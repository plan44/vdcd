//
//  deviceclasscontainer.cpp
//  vdcd
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


DeviceContainer &DeviceClassContainer::getDeviceContainer() const
{
  return *deviceContainerP;
}


void DeviceClassContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
	aCompletedCB(ErrorPtr()); // default to error-free initialisation
}


const char *DeviceClassContainer::getPersistentDataDir()
{
	return deviceContainerP->getPersistentDataDir();
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


// add a device
void DeviceClassContainer::addDevice(DevicePtr aDevice)
{
  // save in my own list
  devices.push_back(aDevice);
  // announce to global device container
  deviceContainerP->addDevice(aDevice);
}



// remove a device
void DeviceClassContainer::removeDevice(DevicePtr aDevice, bool aForget)
{
	// find and remove from my list.
	for (DeviceList::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
		if (*pos==aDevice) {
			devices.erase(pos);
			break;
		}
	}
  // announce to global device container
  deviceContainerP->removeDevice(aDevice, aForget);
}


// get device by instance pointer
DevicePtr DeviceClassContainer::getDevicePtrForInstance(Device *aDeviceP)
{
	// find shared pointer in my list
  DevicePtr dev;
	for (DeviceList::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
		if (pos->get()==aDeviceP) {
      dev = *pos;
			break;
		}
	}
  return dev;
}



void DeviceClassContainer::removeDevices(bool aForget)
{
	for (DeviceList::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    DevicePtr dev = *pos;
    deviceContainerP->removeDevice(dev, aForget);
  }
  // clear my own list
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
