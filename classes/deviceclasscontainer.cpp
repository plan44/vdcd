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


DeviceContainer *DeviceClassContainer::getDeviceContainerP() const
{
  return deviceContainerP;
}



// deviceclass container instance identifier
string DeviceClassContainer::deviceClassContainerInstanceIdentifier() const
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


string DeviceClassContainer::description()
{
  string d = string_format("Deviceclass Container '%s' contains %d devices: %d\n", deviceClassIdentifier(), devices.size());
  for (DeviceMap::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    d.append(pos->second->description());
  }
  return d;
}
