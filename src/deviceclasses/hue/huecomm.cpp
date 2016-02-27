//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 7

#include "huecomm.hpp"

#if ENABLE_HUE

using namespace p44;


#pragma mark - HueApiOperation

HueApiOperation::HueApiOperation(HueComm &aHueComm, HttpMethods aMethod, const char* aUrl, JsonObjectPtr aData, HueApiResultCB aResultHandler) :
  hueComm(aHueComm),
  method(aMethod),
  url(aUrl),
  data(aData),
  resultHandler(aResultHandler),
  completed(false)
{
}



HueApiOperation::~HueApiOperation()
{

}



bool HueApiOperation::initiate()
{
  if (!canInitiate())
    return false;
  // initiate the web request
  const char *methodStr;
  switch (method) {
    case httpMethodPOST : methodStr = "POST"; break;
    case httpMethodPUT : methodStr = "PUT"; break;
    case httpMethodDELETE : methodStr = "DELETE"; break;
    default : methodStr = "GET"; data.reset(); break;
  }
  hueComm.bridgeAPIComm.jsonRequest(url.c_str(), boost::bind(&HueApiOperation::processAnswer, this, _1, _2), methodStr, data);
  // executed
  return inherited::initiate();
}



void HueApiOperation::processAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  error = aError;
  if (Error::isOK(error)) {
    // pre-process response in case of non-GET
    if (method!=httpMethodGET) {
      // Expected:
      //  [{"error":{"type":xxx,"address":"yyy","description":"zzz"}}]
      // or
      //  [{"success": { "xxx": "xxxxxxxx" }]
      int errCode = HueCommErrorInvalidResponse;
      string errMessage = "invalid response";
      for (int i=0; i<aJsonResponse->arrayLength(); i++) {
        JsonObjectPtr responseItem = aJsonResponse->arrayGet(i);
        responseItem->resetKeyIteration();
        JsonObjectPtr responseParams;
        string statusToken;
        if (responseItem->nextKeyValue(statusToken, responseParams)) {
          if (statusToken=="success" && responseParams) {
            // apparently successful, return entire response
            // Note: use getSuccessItem() to get success details
            data = aJsonResponse;
            errCode = HueCommErrorOK; // ok
            break;
          }
          else if (statusToken=="error" && responseParams) {
            // make Error object out of it
            JsonObjectPtr e = responseParams->get("type");
            if (e)
              errCode = e->int32Value();
            e = responseParams->get("description");
            if (e)
              errMessage = e->stringValue();
            break;
          }
        }
      } // for
      if (errCode!=HueCommErrorOK) {
        error = ErrorPtr(new HueCommError(errCode, errMessage));
      }
    }
    else {
      // GET, just return entire data
      data = aJsonResponse;
    }
  }
  // done
  completed = true;
  // have queue reprocessed
  hueComm.processOperations();
}



bool HueApiOperation::hasCompleted()
{
  return completed;
}



OperationPtr HueApiOperation::finalize(p44::OperationQueue *aQueueP)
{
  if (resultHandler) {
    resultHandler(data, error);
    resultHandler = NULL; // call once only
  }
  return OperationPtr(); // no operation to insert
}



void HueApiOperation::abortOperation(ErrorPtr aError)
{
  if (!aborted) {
    if (!completed) {
      hueComm.bridgeAPIComm.cancelRequest();
    }
    if (resultHandler) {
      resultHandler(JsonObjectPtr(), aError);
      resultHandler = NULL; // call once only
    }
  }
}




#pragma mark - BridgeFinder

// string used in place of UUID when using fixed hue API URL
#define PSEUDO_UUID_FOR_FIXED_API "fixed_api_base_URL"

class p44::BridgeFinder : public P44Obj
{
  HueComm &hueComm;
  HueComm::HueBridgeFindCB callback;

  BridgeFinderPtr keepAlive;

public:

  // discovery
  bool refind;
  SsdpSearchPtr bridgeDetector;
  typedef map<string, string> StringStringMap;
  StringStringMap bridgeCandiates; ///< possible candidates for hue bridges, key=uuid, value=description URL
  StringStringMap::iterator currentBridgeCandidate; ///< next candidate for bridge
  MLMicroSeconds authTimeWindow;
  StringStringMap authCandidates; ///< bridges to try auth with, key=uuid, value=baseURL
  StringStringMap::iterator currentAuthCandidate; ///< next auth candiate
  MLMicroSeconds startedAuth; ///< when auth was started
  long retryLoginTicket;

  // params and results
  string uuid; ///< the UUID for searching the hue bridge via SSDP
  string userName; ///< the user name / token
  string baseURL; ///< base URL for API calls
  string deviceType; ///< app description for login

  BridgeFinder(HueComm &aHueComm, HueComm::HueBridgeFindCB aFindHandler) :
    callback(aFindHandler),
    hueComm(aHueComm),
    startedAuth(Never),
    retryLoginTicket(0)
  {
    bridgeDetector = SsdpSearchPtr(new SsdpSearch(MainLoop::currentMainLoop()));
  }

  virtual ~BridgeFinder()
  {
    MainLoop::currentMainLoop().cancelExecutionTicket(retryLoginTicket);
  }

  void findNewBridge(const char *aDeviceType, MLMicroSeconds aAuthTimeWindow, HueComm::HueBridgeFindCB aFindHandler)
  {
    refind = false;
    callback = aFindHandler;
    userName.clear();
    deviceType = nonNullCStr(aDeviceType);
    authTimeWindow = aAuthTimeWindow;
    if (hueComm.fixedBaseURL.empty()) {
      // actually search for a bridge
      keepAlive = BridgeFinderPtr(this);
      bridgeDetector->startSearch(boost::bind(&BridgeFinder::bridgeDiscoveryHandler, this, _1, _2), NULL);
    }
    else {
      // we have a pre-known base URL for the hue API, use this without any find operation
      keepAlive = BridgeFinderPtr(this);
      // - just put it in as the only auth candidate
      authCandidates.clear();
      authCandidates[PSEUDO_UUID_FOR_FIXED_API] = hueComm.fixedBaseURL;
      startedAuth = MainLoop::now();
      attemptPairingWithCandidates();
    }
  };


  void refindBridge(HueComm::HueBridgeFindCB aFindHandler)
  {
    refind = true;
    callback = aFindHandler;
    uuid = hueComm.uuid;;
    userName = hueComm.userName;
    if (hueComm.fixedBaseURL.empty() && uuid!=PSEUDO_UUID_FOR_FIXED_API) {
      // actually search for bridge
      keepAlive = BridgeFinderPtr(this);
      bridgeDetector->startSearch(boost::bind(&BridgeFinder::bridgeRefindHandler, this, _1, _2), uuid.c_str());
    }
    else {
      // we have a pre-known base URL for the hue API, use this without any find operation
      hueComm.baseURL = hueComm.fixedBaseURL; // use it
      hueComm.apiReady = true; // can use API now
      FOCUSLOG("Using fixed API URL to access hue Bridge %s: %s", hueComm.uuid.c_str(), hueComm.baseURL.c_str());
      callback(ErrorPtr()); // success
    }
  };


  void bridgeRefindHandler(SsdpSearchPtr aSsdpSearch, ErrorPtr aError)
  {
    if (!Error::isOK(aError)) {
      // could not find bridge, return error
      callback(ErrorPtr(new HueCommError(HueCommErrorUuidNotFound)));
      keepAlive.reset(); // will delete object if nobody else keeps it
      return; // done
    }
    else {
      // found, now get description to get baseURL
      // - put it into queue as the only candidate
      bridgeCandiates.clear();
      bridgeCandiates[aSsdpSearch->uuid.c_str()] = aSsdpSearch->locationURL.c_str();
      // process the candidate
      currentBridgeCandidate = bridgeCandiates.begin();
      processCurrentBridgeCandidate();
    }
  }


  void bridgeDiscoveryHandler(SsdpSearchPtr aSsdpSearch, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      // check device for possibility of being a hue bridge
      if (aSsdpSearch->server.find("IpBridge")!=string::npos) {
        LOG(LOG_INFO, "hue bridge candidate device found at %s, server=%s, uuid=%s", aSsdpSearch->locationURL.c_str(), aSsdpSearch->server.c_str(), aSsdpSearch->uuid.c_str());
        // put into map
        bridgeCandiates[aSsdpSearch->uuid.c_str()] = aSsdpSearch->locationURL.c_str();
      }
    }
    else {
      FOCUSLOG("discovery ended, error = %s (usually: timeout)", aError->description().c_str());
      aSsdpSearch->stopSearch();
      // now process the results
      currentBridgeCandidate = bridgeCandiates.begin();
      processCurrentBridgeCandidate();
    }
  }


  void processCurrentBridgeCandidate()
  {
    if (currentBridgeCandidate!=bridgeCandiates.end()) {
      // request description XML
      hueComm.bridgeAPIComm.httpRequest(
        (currentBridgeCandidate->second).c_str(),
        boost::bind(&BridgeFinder::handleServiceDescriptionAnswer, this, _1, _2),
        "GET"
      );
    }
    else {
      // done with all candidates
      if (refind) {
        // failed getting description, return error
        callback(ErrorPtr(new HueCommError(HueCommErrorDescription)));
        keepAlive.reset(); // will delete object if nobody else keeps it
        return; // done
      }
      else {
        // finding new bridges - attempt user login
        bridgeCandiates.clear();
        // now attempt to pair with one of the candidates
        startedAuth = MainLoop::now();
        attemptPairingWithCandidates();
      }
    }
  }


  void handleServiceDescriptionAnswer(const string &aResponse, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      // show
      //FOCUSLOG("Received bridge description:\n%s", aResponse.c_str());
      FOCUSLOG("Received service description XML");
      // TODO: this is poor man's XML scanning, use some real XML parser eventually
      // do some basic checking for model
      size_t i = aResponse.find("<manufacturer>Royal Philips Electronics</manufacturer>");
      if (i!=string::npos) {
        // is from Philips
        // - check model number
        i = aResponse.find("<modelNumber>929000226503</modelNumber>"); // original 2012 hue bridge, FreeRTOS
        if (i==string::npos) i = aResponse.find("<modelNumber>BSB002</modelNumber>"); // hue bridge 2015 with homekit, Linux
        if (i!=string::npos) {
          // is the right model
          // - get base URL
          string token = "<URLBase>";
          i = aResponse.find(token);
          if (i!=string::npos) {
            i += token.size();
            size_t e = aResponse.find("</URLBase>", i);
            if (e!=string::npos) {
              // create the base address for the API
              string url = aResponse.substr(i,e-i) + "api";
              if (refind) {
                // that's my known hue bridge, save the URL and report success
                hueComm.baseURL = url; // save it
                hueComm.apiReady = true; // can use API now
                FOCUSLOG("pre-known hue Bridge %s found at %s", hueComm.uuid.c_str(), hueComm.baseURL.c_str());
                callback(ErrorPtr()); // success
                keepAlive.reset(); // will delete object if nobody else keeps it
                return; // done
              }
              else {
                // that's a hue bridge, remember it for trying to authorize
                FOCUSLOG("- Seems to be a hue bridge at %s", url.c_str());
                authCandidates[currentBridgeCandidate->first] = url;
              }
            }
          }
        }
      }
    }
    else {
      FOCUSLOG("Error accessing bridge description: %s", aError->description().c_str());
    }
    // try next
    ++currentBridgeCandidate;
    processCurrentBridgeCandidate(); // process next, if any
  }


  void attemptPairingWithCandidates()
  {
    currentAuthCandidate = authCandidates.begin();
    processCurrentAuthCandidate();
  }


  void processCurrentAuthCandidate()
  {
    if (currentAuthCandidate!=authCandidates.end() && hueComm.findInProgress) {
      // try to authorize
      FOCUSLOG("Auth candidate: uuid=%s, baseURL=%s -> try creating user", currentAuthCandidate->first.c_str(), currentAuthCandidate->second.c_str());
      JsonObjectPtr request = JsonObject::newObj();
      request->add("devicetype", JsonObject::newString(deviceType));
      hueComm.apiAction(httpMethodPOST, currentAuthCandidate->second.c_str(), request, boost::bind(&BridgeFinder::handleCreateUserAnswer, this, _1, _2), true);
    }
    else {
      // done with all candidates (or find aborted in hueComm)
      if (authCandidates.size()>0 && MainLoop::now()<startedAuth+authTimeWindow && hueComm.findInProgress) {
        // we have still candidates and time to do a retry in a second, and find is not aborted
        retryLoginTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&BridgeFinder::attemptPairingWithCandidates, this), 1*Second);
        return;
      }
      else {
        // all candidates tried, nothing found in given time
        LOG(LOG_NOTICE, "Could not register with a hue bridge");
        hueComm.findInProgress = false;
        callback(ErrorPtr(new HueCommError(HueCommErrorNoRegistration, "No hue bridge found ready to register")));
        // done!
        keepAlive.reset(); // will delete object if nobody else keeps it
        return;
      }
    }
  }


  void handleCreateUserAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      FOCUSLOG("Received success answer:\n%s", aJsonResponse->json_c_str());
      JsonObjectPtr s = HueComm::getSuccessItem(aJsonResponse);
      // apparently successful, extract user name
      if (s) {
        JsonObjectPtr u = s->get("username");
        if (u) {
          hueComm.userName = u->stringValue();
          hueComm.uuid = currentAuthCandidate->first;
          hueComm.baseURL = currentAuthCandidate->second;
          hueComm.apiReady = true; // can use API now
          FOCUSLOG("hue Bridge %s @ %s: successfully registered as user %s", hueComm.uuid.c_str(), hueComm.baseURL.c_str(), hueComm.userName.c_str());
          // successfully registered with hue bridge, let caller know
          callback(ErrorPtr());
          // done!
          keepAlive.reset(); // will delete object if nobody else keeps it
          return;
        }
      }
    }
    else {
      LOG(LOG_ERR, "hue Bridge: Error creating user: %s", aError->description().c_str());
    }
    // try next
    ++currentAuthCandidate;
    processCurrentAuthCandidate(); // process next, if any
  }

}; // BridgeFinder



#pragma mark - hueComm


HueComm::HueComm() :
  inherited(MainLoop::currentMainLoop()),
  bridgeAPIComm(MainLoop::currentMainLoop()),
  findInProgress(false),
  apiReady(false)
{
}


HueComm::~HueComm()
{
}


void HueComm::apiQuery(const char* aUrlSuffix, HueApiResultCB aResultHandler)
{
  apiAction(httpMethodGET, aUrlSuffix, JsonObjectPtr(), aResultHandler);
}


void HueComm::apiAction(HttpMethods aMethod, const char* aUrlSuffix, JsonObjectPtr aData, HueApiResultCB aResultHandler, bool aNoAutoURL)
{
  if (!apiReady && !aNoAutoURL) {
    if (aResultHandler) aResultHandler(JsonObjectPtr(), ErrorPtr(new HueCommError(HueCommErrorApiNotReady)));
  }
  string url;
  if (aNoAutoURL) {
    url = aUrlSuffix;
  }
  else {
    url = baseURL;
    if (userName.length()>0)
      url += "/" + userName;
    url += nonNullCStr(aUrlSuffix);
  }
  HueApiOperationPtr op = HueApiOperationPtr(new HueApiOperation(*this, aMethod, url.c_str(), aData, aResultHandler));
  queueOperation(op);
  // process operations
  processOperations();
}


JsonObjectPtr HueComm::getSuccessItem(JsonObjectPtr aResult, int aIndex)
{
  if (aResult && aIndex<aResult->arrayLength()) {
    JsonObjectPtr responseItem = aResult->arrayGet(aIndex);
    JsonObjectPtr successItem;
    if (responseItem && responseItem->get("success", successItem)) {
      return successItem;
    }
  }
  return JsonObjectPtr();
}



void HueComm::findNewBridge(const char *aDeviceType, MLMicroSeconds aAuthTimeWindow, HueBridgeFindCB aFindHandler)
{
  findInProgress = true;
  BridgeFinderPtr bridgeFinder = BridgeFinderPtr(new BridgeFinder(*this, aFindHandler));
  bridgeFinder->findNewBridge(aDeviceType, aAuthTimeWindow, aFindHandler);
};


void HueComm::stopFind()
{
  findInProgress = false;
}


void HueComm::refindBridge(HueBridgeFindCB aFindHandler)
{
  apiReady = false; // not yet found, API disabled
  BridgeFinderPtr bridgeFinder = BridgeFinderPtr(new BridgeFinder(*this, aFindHandler));
  bridgeFinder->refindBridge(aFindHandler);
};


#endif // ENABLE_HUE
