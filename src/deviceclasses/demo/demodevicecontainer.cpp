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
  // check if this is an answer to our M-SEARCH
  DevicePtr newDev = DevicePtr(new DemoDevice(this, aSsdpSearchP->locationURL, aSsdpSearchP->uuid));
  addDevice(newDev);
}


#define RETRY_TIMEOUT_INTERVAL (15*Second)

/// collect devices from this device class
void DemoDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  // incrementally collecting Demo devices makes no sense, they are statically created at startup
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(false);
  }
  // start a search
  m_dmr_search.startSearchForTarget(
    boost::bind(&DemoDeviceContainer::discoveryHandler, this, _1, _2),
    "urn:schemas-upnp-org:device:MediaRenderer:1",
    false, // not single device, we want to get all of them
    true // but we only want those that match the ST
  );
  // collecting in the strict sense ends after a while (but not immediately)
  MainLoop::currentMainLoop().executeOnce(boost::bind(&DemoDeviceContainer::endCollect, this, aCompletedCB), RETRY_TIMEOUT_INTERVAL);
}


void DemoDeviceContainer::endCollect(CompletedCB aCompletedCB)
{
  // collection time over
  aCompletedCB(ErrorPtr());
  // stop explicitly searching
  m_dmr_search.stopSearch();
}

