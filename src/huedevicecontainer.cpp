//
//  huedevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huedevicecontainer.hpp"

#include "huedevice.hpp"

using namespace p44;


HueDeviceContainer::HueDeviceContainer(int aInstanceNumber) :
  inherited(aInstanceNumber),
  bridgeAPIComm(SyncIOMainLoop::currentMainLoop())
{
}

const char *HueDeviceContainer::deviceClassIdentifier() const
{
  return "hue_Lights_Container";
}


void HueDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  collectedHandler = aCompletedCB;
  // if we have uuid and token of a bridge, try to re-find it
  if (ssdpUuid.length()>0 && apiToken.length()>0) {
    // search bridge by uuid
    bridgeSearcher = SsdpSearchPtr(new SsdpSearch(SyncIOMainLoop::currentMainLoop()));
    bridgeSearcher->startSearch(boost::bind(&HueDeviceContainer::bridgeRefindHandler, this, _1, _2), ssdpUuid.c_str());
  }
}


void HueDeviceContainer::bridgeRefindHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // apparently found bridge, extract base URL
    baseURL = aSsdpSearchP->locationURL;
    LOG(LOG_NOTICE, "Found my hue bridge by uuid: %s\n", baseURL.c_str());
    #warning "// TODO: verify that this is a hue bridge"
    collectedHandler(ErrorPtr()); // ok
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "Error refinding hue bridge with uuid %s, error = %s\n", baseURL.c_str(), aError->description().c_str());
    collectedHandler(ErrorPtr()); // no hue bridge to collect lights from (but this is not a collect error)
  }
  aSsdpSearchP->stopSearch();
  bridgeSearcher.reset();
}



void HueDeviceContainer::setLearnMode(bool aEnableLearning)
{
  if (aEnableLearning) {
    // search for any device
    bridgeSearcher = SsdpSearchPtr(new SsdpSearch(SyncIOMainLoop::currentMainLoop()));
    bridgeSearcher->startSearch(boost::bind(&HueDeviceContainer::bridgeDiscoveryHandler, this, _1, _2), NULL);
  }
  else {
    // stop learning
    if (bridgeSearcher) {
      // if discovery still active, stop it
      bridgeSearcher->stopSearch();
      bridgeSearcher.reset();
    }
    #warning "for now, extend search beyond learning period"
//    // forget all candidates
//    currentBridgeCandidate = bridgeCandiates.end();
//    currentAuthCandidate = authCandidates.end();
//    authCandidates.clear();
//    bridgeCandiates.clear();
  }
}



void HueDeviceContainer::bridgeDiscoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
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
    bridgeSearcher.reset();
    // now process the results
    currentBridgeCandidate = bridgeCandiates.begin();
    processCurrentBridgeCandidate();
  }
}


void HueDeviceContainer::processCurrentBridgeCandidate()
{
  if (currentBridgeCandidate!=bridgeCandiates.end()) {
    // request description XML
    bridgeAPIComm.httpRequest(
      (currentBridgeCandidate->second).c_str(),
      boost::bind(&HueDeviceContainer::handleBridgeDescriptionAnswer, this, _2, _3),
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



void HueDeviceContainer::handleBridgeDescriptionAnswer(const string &aResponse, ErrorPtr aError)
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



void HueDeviceContainer::processCurrentAuthCandidate()
{
  if (currentAuthCandidate!=authCandidates.end()) {
    // try to authorize
    DBGLOG(LOG_DEBUG, "%%% auth candidate: uuid=%s, baseURL=%s", currentAuthCandidate->first.c_str(), currentAuthCandidate->second.c_str());
    JsonObjectPtr request = JsonObject::newObj();
    request->add("username", JsonObject::newString(getDeviceContainer().dsid.getString()));
    request->add("devicetype", JsonObject::newString(getDeviceContainer().modelName()));
    bridgeAPIComm.jsonRequest(currentAuthCandidate->second.c_str(), boost::bind(&HueDeviceContainer::handleBridgeAuthAnswer, this, _2, _3), "POST", request);
  }
  else {
    // done with all candidates
    authCandidates.clear();
  }
}



void HueDeviceContainer::handleBridgeAuthAnswer(JsonObjectPtr aJsonResponse, ErrorPtr aError)
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




