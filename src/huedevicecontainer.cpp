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
  hueComm()
{
}

const char *HueDeviceContainer::deviceClassIdentifier() const
{
  return "hue_Lights_Container";
}


void HueDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  // TODO: load hue bridge uuid and token
  collectedHandler = aCompletedCB;
  hueComm.refindBridge(boost::bind(&HueDeviceContainer::refindResultHandler, this, _2));
}




void HueDeviceContainer::refindResultHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // found and authenticated bridge, now get lights
    // TODO: %%% implement

    // for now: just say ok
    collectedHandler(ErrorPtr()); // ok
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "Error refinding hue bridge with uuid %s, error = %s\n", hueComm.uuid.c_str(), aError->description().c_str());
    collectedHandler(ErrorPtr()); // no hue bridge to collect lights from (but this is not a collect error)
  }
}


void HueDeviceContainer::setLearnMode(bool aEnableLearning)
{
  if (aEnableLearning) {
    hueComm.findNewBridge(
      getDeviceContainer().dsid.getString().c_str(), // dsid is suitable as hue login name
      getDeviceContainer().modelName().c_str(),
      15*Second, // try to login for 15 secs
      boost::bind(&HueDeviceContainer::learnResultHandler, this, _2)
    );
  }
  else {
    // stop learning
    #warning "for now, extend search beyond learning period"
  }
}


void HueDeviceContainer::learnResultHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // found and authenticated bridge
    LOG(LOG_NOTICE,
      "Hue bridge found and registered with it:\n"
      "- uuid = %s\n",
      "- userName = %s\n",
      "- API base URL = %s\n",
      hueComm.uuid.c_str(),
      hueComm.userName.c_str(),
      hueComm.baseURL.c_str()
    );
    // TODO: persist hue parameters

    // TODO: now get lights

    //%%% for now: just NOP
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "No hue bridge found to register, error = %s\n", aError->description().c_str());
  }
}





