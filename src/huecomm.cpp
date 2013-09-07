//
//  huecomm.cpp
//  vdcd
//
//  Created by Lukas Zeller on 07.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huecomm.hpp"

using namespace p44;

#pragma mark - BridgeFinder


class p44::BridgeFinder : public P44Obj
{
  HueComm &hueComm;
  HueComm::HueBridgeFindCB callback;

  BridgeFinderPtr keepAlive;

public:

  // discovery
  SsdpSearch bridgeDetector;
  typedef map<string, string> StringStringMap;
  StringStringMap bridgeCandiates; ///< possible candidates for hue bridges, key=uuid, value=description URL
  StringStringMap::iterator currentBridgeCandidate; ///< next candidate for bridge
  MLMicroSeconds authTimeWindow;
  StringStringMap authCandidates; ///< bridges to try auth with, key=uuid, value=baseURL
  StringStringMap::iterator currentAuthCandidate; ///< next auth candiate


  // params and results
  string uuid; ///< the UUID for searching the hue bridge via SSDP
  string apiToken; ///< the API token
  string baseURL; ///< base URL for API calls
  string loginName; ///< the login name to use (identifier, no spaces)
  string appDescription; ///< app description for login

  BridgeFinder(HueComm &aHueComm, HueComm::HueBridgeFindCB aFindHandler) :
    callback(aFindHandler),
    hueComm(aHueComm),
    bridgeDetector(SyncIOMainLoop::currentMainLoop())
  {
  }


  void findNewBridge(const char *aLoginName, const char *aAppDescription, MLMicroSeconds aAuthTimeWindow, HueComm::HueBridgeFindCB aFindHandler)
  {
    loginName = nonNullCStr(aLoginName);
    appDescription = nonNullCStr(aAppDescription);
    authTimeWindow = aAuthTimeWindow;
    keepAlive = BridgeFinderPtr(this);
    bridgeDetector.startSearch(boost::bind(&BridgeFinder::bridgeDiscoveryHandler, this, _1, _2), NULL);
  };


  void refindBridge(HueComm::HueBridgeFindCB aFindHandler)
  {
    uuid = hueComm.ssdpUuid;;
    apiToken = hueComm.apiToken;
    keepAlive = BridgeFinderPtr(this);
    bridgeDetector.startSearch(boost::bind(&BridgeFinder::bridgeRefindHandler, this, _1, _2), uuid.c_str());
  };


  void bridgeRefindHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
  {
    // TODO: implement %%%
    //%%% release, for now
    keepAlive.reset(); // will delete object if nobody else keeps it
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
        boost::bind(&BridgeFinder::handleBridgeDescriptionAnswer, this, _2, _3),
        "GET"
      );
    }
    else {
      // done with all candidates
      bridgeCandiates.clear();
      // now attempt to authorize
      currentAuthCandidate = authCandidates.begin();
      processCurrentAuthCandidate();
    }
  }


  void handleBridgeDescriptionAnswer(const string &aResponse, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      // show
      DBGLOG(LOG_DEBUG, "Received bridge description:\n%s\n", aResponse.c_str());
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
              // that's a hue bridge, remember it for trying to authorize
              authCandidates[currentBridgeCandidate->first] = url;
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


  void processCurrentAuthCandidate()
  {
    if (currentAuthCandidate!=authCandidates.end()) {
      // try to authorize
      DBGLOG(LOG_DEBUG, "%%% auth candidate: uuid=%s, baseURL=%s", currentAuthCandidate->first.c_str(), currentAuthCandidate->second.c_str());
      JsonObjectPtr request = JsonObject::newObj();
      request->add("username", JsonObject::newString(loginName));
      request->add("devicetype", JsonObject::newString(appDescription));
      hueComm.bridgeAPIComm.jsonRequest(currentAuthCandidate->second.c_str(), boost::bind(&BridgeFinder::handleBridgeAuthAnswer, this, _2, _3), "POST", request);
    }
    else {
      // done with all candidates
      // TODO: check if we should restart auth attempts after a delay (learn mode), otherwise call back with error %%%
      //%%% release, for now
      keepAlive.reset(); // will delete object if nobody else keeps it
    }
  }


  void handleBridgeAuthAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      // show
      DBGLOG(LOG_DEBUG, "Received bridge auth answer:\n%s\n", aJsonResponse->json_c_str());
    }
    else {
      DBGLOG(LOG_DEBUG, "Error doing bridge login: %s\n", aError->description().c_str());
    }
    // try next
    ++currentAuthCandidate;
    processCurrentAuthCandidate(); // process next, if any
  }

}; // BridgeFinder


#pragma mark - hueComm


HueComm::HueComm() :
  inherited(SyncIOMainLoop::currentMainLoop()),
  bridgeAPIComm(SyncIOMainLoop::currentMainLoop())
{
}


HueComm::~HueComm()
{
}


void HueComm::findNewBridge(const char *aLoginName, const char *aAppDescription, MLMicroSeconds aAuthTimeWindow, HueBridgeFindCB aFindHandler)
{
  BridgeFinderPtr bridgeFinder = BridgeFinderPtr(new BridgeFinder(*this, aFindHandler));
  bridgeFinder->findNewBridge(aLoginName, aAppDescription, aAuthTimeWindow, aFindHandler);
};


void HueComm::refindBridge(HueBridgeFindCB aFindHandler)
{
  BridgeFinderPtr bridgeFinder = BridgeFinderPtr(new BridgeFinder(*this, aFindHandler));
  bridgeFinder->refindBridge(aFindHandler);
};


