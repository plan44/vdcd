//
//  deviceclasscontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "deviceclasscontainer.hpp"

#include "device.hpp"


DeviceClassContainer::DeviceClassContainer() :
  deviceContainerP(NULL)
{
}


void DeviceClassContainer::setDeviceContainer(DeviceContainer *aDeviceContainerP)
{
  deviceContainerP = aDeviceContainerP;
}


DeviceContainer *DeviceClassContainer::getDeviceContainerP()
{
  return deviceContainerP;
}



// deviceclass container instance identifier
string DeviceClassContainer::deviceClassContainerInstanceIdentifier()
{
  string s(deviceClassIdentifier());
  s.append(deviceContainerP->deviceContainerInstanceIdentifier());
  return s;
}


// add a newly collected device
void DeviceClassContainer::addCollectedDevice(DevicePtr aDevice)
{
  devices.insert(make_pair(aDevice->dsid, aDevice));
  // TODO: trigger registration??
}
