//
//  demodevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 2013-11-11
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "demodevicecontainer.hpp"
#include "ssdpsearch.hpp"
#include "demodevice.hpp"

using namespace p44;


DemoDeviceContainer::DemoDeviceContainer(int aInstanceNumber) :
  DeviceClassContainer(aInstanceNumber),
  m_dmr_search(SyncIOMainLoop::currentMainLoop())
{
}


// device class name
const char *DemoDeviceContainer::deviceClassIdentifier() const
{
  return "Demo_Device_Container";
}

void DemoDeviceContainer::discoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
{
    printf("SSDP discovery\n%s\n", aSsdpSearchP->response.c_str());
    DevicePtr newDev = DevicePtr(new DemoDevice(this, aSsdpSearchP->locationURL, aSsdpSearchP->uuid));
    addDevice(newDev);
}

void DemoDeviceContainer::findDevices()
{
    m_dmr_search.startSearchForTarget(boost::bind(&DemoDeviceContainer::discoveryHandler, this, _1, _2), "urn:schemas-upnp-org:device:MediaRenderer:1", false);
}

/// collect devices from this device class
void DemoDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  // incrementally collecting Demo devices makes no sense, they are statically created at startup
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(false);
    // create one single demo device
//    DevicePtr newDev = DevicePtr(new DemoDevice(this));
    // add to container
//    addDevice(newDev);
    findDevices();
  }


  // assume ok
  aCompletedCB(ErrorPtr());
}

