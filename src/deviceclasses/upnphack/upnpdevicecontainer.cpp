//
//  upnpdeviceContainer.cpp
//  vdcd
//

#include "upnpdevicecontainer.hpp"
#include "ssdpsearch.hpp"
#include "upnpdevice.hpp"

using namespace p44;


UpnpDeviceContainer::UpnpDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
  m_dmr_search = SsdpSearchPtr(new SsdpSearch(MainLoop::currentMainLoop()));
}


// device class name
const char *UpnpDeviceContainer::deviceClassIdentifier() const
{
  return "UPnP_Device_Container";
}

void UpnpDeviceContainer::collectHandler(CompletedCB aCompletedCB, SsdpSearchPtr aSsdpSearch, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // found a result (not just timeout)
    printf("SSDP discovery:\n%s\n", aSsdpSearch->response.c_str());
    // this is an answer to our M-SEARCH, so it has the right device type and should be added
    DevicePtr newDev = DevicePtr(new UpnpDevice(this, aSsdpSearch->locationURL, aSsdpSearch->uuid));
    addDevice(newDev);
  }
  else {
    // search timed out with no results
    // - collection time over
    aCompletedCB(ErrorPtr());
    // - from now on, keep monitoring UPnP notifications for further new devices
    // TODO: actually implement this, and filter results by NT 
  }
}


#define COLLECTING_TIME (15*Second)

/// collect devices for this device class
void UpnpDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  // incrementally collecting Demo devices makes no sense, they are statically created at startup
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(false);
  }
  // start a search (which times out after a while)
  m_dmr_search->startSearchForTarget(
    boost::bind(&UpnpDeviceContainer::collectHandler, this, aCompletedCB, _1, _2),
    "urn:schemas-upnp-org:device:MediaRenderer:1",
    false, // not single device, we want to get all of them
    true // but we only want those that match the ST
  );
}

