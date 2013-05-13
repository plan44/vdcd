//
//  devicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "devicecontainer.hpp"

#include "deviceclasscontainer.hpp"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include "device.hpp"

using namespace p44;


void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers.push_back(aDeviceClassContainerPtr);
  aDeviceClassContainerPtr->setDeviceContainer(this);
}



string DeviceContainer::deviceContainerInstanceIdentifier() const
{
  string identifier;

  //
  struct ifreq ifr;
  struct ifconf ifc;
  char buf[1024];
  int success = 0;

  do {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { /* handle error*/ };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
      break;
    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (; it != end; ++it) {
      strcpy(ifr.ifr_name, it->ifr_name);
      if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
          #ifdef __APPLE__
          #warning MAC address retrieval on OSX not supported
          break;
          #else
          if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            success = 1;
            break;
          }
          #endif
        }
      }
      else
        break;
    }
  } while(false);
  // extract ID if we have one
  if (success) {
    for (int i=0; i<6; ++i) {
      #ifndef __APPLE__
      string_format_append(identifier, "%02X",(uint8_t *)(ifr.ifr_hwaddr.sa_data)[i]);
      #endif
    }
  }
  else {
    identifier = "UnknownMACAddress";
  }
  return identifier;
}


class DeviceClassInitializer
{
  CompletedCB callback;
  list<DeviceClassContainerPtr>::iterator nextContainer;
  DeviceContainer *deviceContainerP;
public:
  static void initialize(DeviceContainer *aDeviceContainerP, CompletedCB aCallback)
  {
    // create new instance, deletes itself when finished
    new DeviceClassInitializer(aDeviceContainerP, aCallback);
  };
private:
  DeviceClassInitializer(DeviceContainer *aDeviceContainerP, CompletedCB aCallback) :
		callback(aCallback),
		deviceContainerP(aDeviceContainerP)
  {
    nextContainer = deviceContainerP->deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }
	
	
  void queryNextContainer(ErrorPtr aError)
  {
    if (!aError && nextContainer!=deviceContainerP->deviceClassContainers.end())
      (*nextContainer)->initialize(boost::bind(&DeviceClassInitializer::containerInitialized, this, _1));
    else
      completed(aError);
  }
	
  void containerInitialized(ErrorPtr aError)
  {
    // check next
    ++nextContainer;
    queryNextContainer(aError);
  }
	
  void completed(ErrorPtr aError)
  {
    callback(aError);
    // done, delete myself
    delete this;
  }
	
};


void DeviceContainer::initialize(CompletedCB aCompletedCB)
{
  DeviceClassInitializer::initialize(this, aCompletedCB);
}





class DeviceClassCollector
{
  CompletedCB callback;
  bool exhaustive;
  list<DeviceClassContainerPtr>::iterator nextContainer;
  DeviceContainer *deviceContainerP;
public:
  static void collectDevices(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aExhaustive)
  {
    // create new instance, deletes itself when finished
    new DeviceClassCollector(aDeviceContainerP, aCallback, aExhaustive);
  };
private:
  DeviceClassCollector(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aExhaustive) :
    callback(aCallback),
    deviceContainerP(aDeviceContainerP),
    exhaustive(aExhaustive)
  {
    nextContainer = deviceContainerP->deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }


  void queryNextContainer(ErrorPtr aError)
  {
    if (!aError && nextContainer!=deviceContainerP->deviceClassContainers.end())
      (*nextContainer)->collectDevices(boost::bind(&DeviceClassCollector::containerQueried, this, _1), exhaustive);
    else
      completed(aError);
  }

  void containerQueried(ErrorPtr aError)
  {
    // check next
    ++nextContainer;
    queryNextContainer(aError);
  }

  void completed(ErrorPtr aError)
  {
    callback(aError);
    // done, delete myself
    delete this;
  }

};


void DeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  dSDevices.clear(); // forget existing ones
  DeviceClassCollector::collectDevices(this, aCompletedCB, aExhaustive);
}


// add a newly collected device
void DeviceContainer::addCollectedDevice(DevicePtr aDevice)
{
  // add to container-wide map of devices
  dSDevices.insert(make_pair(aDevice->dsid, aDevice));
}




string DeviceContainer::description()
{
  string d = string_format("DeviceContainer with %d device classes:\n", deviceClassContainers.size());
  for (ContainerList::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
    d.append((*pos)->description());
  }
  return d;
}



