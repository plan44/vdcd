//
//  huecomm.cpp
//  vdcd
//
//  Created by Lukas Zeller on 07.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huecomm.hpp"

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
  hueComm.bridgeAPIComm.jsonRequest(url.c_str(), boost::bind(&HueApiOperation::processAnswer, this, _2, _3), methodStr, data);
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
            // apparently successful, return success object
            data = responseParams;
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
    resultHandler(hueComm, data, error);
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
      resultHandler(hueComm, JsonObjectPtr(), aError);
      resultHandler = NULL; // call once only
    }
  }
}




#pragma mark - BridgeFinder


class p44::BridgeFinder : public P44Obj
{
  HueComm &hueComm;
  HueComm::HueBridgeFindCB callback;

  BridgeFinderPtr keepAlive;

public:

  // discovery
  bool refind;
  SsdpSearch bridgeDetector;
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
    retryLoginTicket(0),
    bridgeDetector(SyncIOMainLoop::currentMainLoop())
  {
  }

  virtual ~BridgeFinder()
  {
    MainLoop::currentMainLoop().cancelExecutionTicket(retryLoginTicket);
  }

  void findNewBridge(const char *aUserName, const char *aDeviceType, MLMicroSeconds aAuthTimeWindow, HueComm::HueBridgeFindCB aFindHandler)
  {
    refind = false;
    callback = aFindHandler;
    userName = nonNullCStr(aUserName);
    deviceType = nonNullCStr(aDeviceType);
    authTimeWindow = aAuthTimeWindow;
    keepAlive = BridgeFinderPtr(this);
    bridgeDetector.startSearch(boost::bind(&BridgeFinder::bridgeDiscoveryHandler, this, _1, _2), NULL);
  };


  void refindBridge(HueComm::HueBridgeFindCB aFindHandler)
  {
    refind = true;
    callback = aFindHandler;
    uuid = hueComm.uuid;;
    userName = hueComm.userName;
    keepAlive = BridgeFinderPtr(this);
    bridgeDetector.startSearch(boost::bind(&BridgeFinder::bridgeRefindHandler, this, _1, _2), uuid.c_str());
  };


  void bridgeRefindHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
  {
    if (!Error::isOK(aError)) {
      // could not find bridge, return error
      callback(hueComm, ErrorPtr(new HueCommError(HueCommErrorUuidNotFound)));
      keepAlive.reset(); // will delete object if nobody else keeps it
      return; // done
    }
    else {
      // found, now get description to get baseURL
      // - put it into queue as the only candidate
      bridgeCandiates.clear();
      bridgeCandiates[aSsdpSearchP->uuid.c_str()] = aSsdpSearchP->locationURL.c_str();
      // process the candidate
      currentBridgeCandidate = bridgeCandiates.begin();
      processCurrentBridgeCandidate();
    }
  }


  void bridgeDiscoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      // check device for possibility of being a hue bridge
      if (aSsdpSearchP->server.find("IpBridge")!=string::npos) {
        LOG(LOG_NOTICE, "hue bridge candidate device found at %s, server=%s, uuid=%s\n", aSsdpSearchP->locationURL.c_str(), aSsdpSearchP->server.c_str(), aSsdpSearchP->uuid.c_str());
        // put into map
        bridgeCandiates[aSsdpSearchP->uuid.c_str()] = aSsdpSearchP->locationURL.c_str();
      }
    }
    else {
      DBGLOG(LOG_DEBUG, "discovery ended, error = %s (usually: timeout)\n", aError->description().c_str());
      aSsdpSearchP->stopSearch();
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
        boost::bind(&BridgeFinder::handleServiceDescriptionAnswer, this, _2, _3),
        "GET"
      );
    }
    else {
      // done with all candidates
      if (refind) {
        // failed getting description, return error
        callback(hueComm, ErrorPtr(new HueCommError(HueCommErrorDescription)));
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
      //DBGLOG(LOG_DEBUG, "Received bridge description:\n%s\n", aResponse.c_str());
      DBGLOG(LOG_DEBUG, "Received service description XML\n");
      // TODO: this is poor man's XML scanning, use some real XML parser eventually
      // do some basic checking for model
      size_t i = aResponse.find("<manufacturer>Royal Philips Electronics</manufacturer>");
      if (i!=string::npos) {
        // is from Philips
        // - check model number
        i = aResponse.find("<modelNumber>929000226503</modelNumber>");
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
                callback(hueComm, ErrorPtr()); // success
                keepAlive.reset(); // will delete object if nobody else keeps it
                return; // done
              }
              else {
                // that's a hue bridge, remember it for trying to authorize
                DBGLOG(LOG_DEBUG, "- Seems to be a hue bridge at %s\n", url.c_str());
                authCandidates[currentBridgeCandidate->first] = url;
              }
            }
          }
        }
      }
    }
    else {
      DBGLOG(LOG_DEBUG, "Error accessing bridge description: %s\n", aError->description().c_str());
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
      DBGLOG(LOG_DEBUG, "Auth candidate: uuid=%s, baseURL=%s -> try creating user\n", currentAuthCandidate->first.c_str(), currentAuthCandidate->second.c_str());
      JsonObjectPtr request = JsonObject::newObj();
      request->add("username", JsonObject::newString(userName));
      request->add("devicetype", JsonObject::newString(deviceType));
      hueComm.apiAction(httpMethodPOST, currentAuthCandidate->second.c_str(), request, boost::bind(&BridgeFinder::handleCreateUserAnswer, this, _2, _3), true);
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
        DBGLOG(LOG_DEBUG, "Could not register with a hue bridge\n");
        hueComm.findInProgress = false;
        callback(hueComm, ErrorPtr(new HueCommError(HueCommErrorNoRegistration, "No hue bridge found ready to register")));
        // done!
        keepAlive.reset(); // will delete object if nobody else keeps it
        return;
      }
    }
  }


  void handleCreateUserAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      DBGLOG(LOG_DEBUG, "Received success answer:\n%s\n", aJsonResponse->json_c_str());
      // apparently successful, extract user name
      JsonObjectPtr u = aJsonResponse->get("username");
      if (u) {
        hueComm.userName = u->stringValue();
        hueComm.uuid = currentAuthCandidate->first;
        hueComm.baseURL = currentAuthCandidate->second;
        hueComm.apiReady = true; // can use API now
        DBGLOG(LOG_DEBUG, "Bridge %s @ %s: successfully registered as user %s\n", hueComm.uuid.c_str(), hueComm.baseURL.c_str(), hueComm.userName.c_str());
        // successfully registered with hue bridge, let caller know
        callback(hueComm, ErrorPtr());
        // done!
        keepAlive.reset(); // will delete object if nobody else keeps it
        return;
      }
    }
    else {
      DBGLOG(LOG_DEBUG, "Error creating bridge user: %s\n", aError->description().c_str());
    }
    // try next
    ++currentAuthCandidate;
    processCurrentAuthCandidate(); // process next, if any
  }

}; // BridgeFinder



#pragma mark - hueComm


HueComm::HueComm() :
  inherited(SyncIOMainLoop::currentMainLoop()),
  bridgeAPIComm(SyncIOMainLoop::currentMainLoop()),
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
  if (!apiReady) {
    if (aResultHandler) aResultHandler(*this,JsonObjectPtr(),ErrorPtr(new HueCommError(HueCommErrorApiNotReady)));
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




void HueComm::findNewBridge(const char *aUserName, const char *aDeviceType, MLMicroSeconds aAuthTimeWindow, HueBridgeFindCB aFindHandler)
{
  findInProgress = true;
  BridgeFinderPtr bridgeFinder = BridgeFinderPtr(new BridgeFinder(*this, aFindHandler));
  bridgeFinder->findNewBridge(aUserName, aDeviceType, aAuthTimeWindow, aFindHandler);
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


