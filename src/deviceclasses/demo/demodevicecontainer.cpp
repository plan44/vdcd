//
//  demodevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 2013-11-11
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "demodevicecontainer.hpp"

#include "demodevice.hpp"

using namespace p44;


DemoDeviceContainer::DemoDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP)
{
}


// device class name
const char *DemoDeviceContainer::deviceClassIdentifier() const
{
  return "Demo_Device_Container";
}


/// collect devices from this device class
void DemoDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  // incrementally collecting Demo devices makes no sense, they are statically created at startup
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(false);
    // create one single demo device
    DevicePtr newDev = DevicePtr(new DemoDevice(this));
    // add to container
    addDevice(newDev);
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}

