//
//  device.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "device.hpp"

using namespace p44;


DSBehaviour::DSBehaviour(Device *aDeviceP) :
  deviceP(aDeviceP)
{
}

DSBehaviour::~DSBehaviour()
{
}



Device::Device(DeviceClassContainer *aClassContainerP) :
  registered(false),
  classContainerP(aClassContainerP),
  behaviourP(NULL)
{

}


Device::~Device()
{
  setBehaviour(NULL);
}


void Device::setBehaviour(DSBehaviour *aBehaviour)
{
  if (behaviourP)
    delete behaviourP;
  behaviourP = aBehaviour;
}


string Device::description()
{
  string s = string_format("Device with dsid = %s\n", dsid.getString().c_str());
  if (behaviourP) {
    string_format_append(s, "- Input: %d/%d, DSBehaviour : %s\n", getInputIndex()+1, getNumInputs(), behaviourP->description().c_str());
  }
  return s;
}
