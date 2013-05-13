//
//  deviceclasscontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "deviceclasscontainer.hpp"

#include "device.hpp"

using namespace p44;


DeviceClassContainer::DeviceClassContainer(int aInstanceNumber) :
  deviceContainerP(NULL),
  instanceNumber(aInstanceNumber)
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


void DeviceClassContainer::initialize(CompletedCB aCompletedCB)
{
	aCompletedCB(ErrorPtr()); // default to error-free initialisation
}


void DeviceClassContainer::setPersistentDataDir(const char *aPersistentDataDir)
{
	persistentDataDir = nonNullCStr(aPersistentDataDir);
	if (!persistentDataDir.empty() && persistentDataDir[persistentDataDir.length()-1]!='/') {
		persistentDataDir.append("/");
	}
}


const char *DeviceClassContainer::getPersistentDataDir()
{
	return persistentDataDir.c_str();
}


int DeviceClassContainer::getInstanceNumber()
{
	return instanceNumber;
}




// deviceclass container instance identifier
string DeviceClassContainer::deviceClassContainerInstanceIdentifier() const
{
  string s(deviceClassIdentifier());
  s.append("@");
  s.append(deviceContainerP->deviceContainerInstanceIdentifier());
  return s;
}


// add a newly collected device
void DeviceClassContainer::addCollectedDevice(DevicePtr aDevice)
{
  // save in my own list
  devices.push_back(aDevice);
  // announce to global device container
  deviceContainerP->addCollectedDevice(aDevice);
}


void DeviceClassContainer::forgetCollectedDevices()
{
  devices.clear();
}




string DeviceClassContainer::description()
{
  string d = string_format("Deviceclass Container '%s' contains %d devices:\n", deviceClassIdentifier(), devices.size());
  for (DeviceList::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    d.append((*pos)->description());
  }
  return d;
}
