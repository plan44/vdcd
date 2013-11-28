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


int DeviceClassContainer::getInstanceNumber() const
{
	return instanceNumber;
}


string DeviceClassContainer::deviceClassContainerInstanceIdentifier() const
{
  string s(deviceClassIdentifier());
  string_format_append(s, ".%d@", getInstanceNumber());
  if (deviceContainerP->usingDsUids()) {
    // with modern dSUIDs, device container is always identified via its dSUID
    s.append(deviceContainerP->dSUID.getString());
  }
  else {
    // with classic dsids, device container was identified by its MAC address, so we simulate that to
    // avoid generating another set of different dSUIDs (classic dsids are beta only anyway).
    s.append(deviceContainerP->macAddressString());
  }
  return s;
}


// add a device
bool DeviceClassContainer::addDevice(DevicePtr aDevice)
{
  // announce to global device container
  if (deviceContainerP->addDevice(aDevice)) {
    // not a duplicate
    // - save in my own list
    devices.push_back(aDevice);
    // added
    return true;
  }
  return false;
}



// remove a device
void DeviceClassContainer::removeDevice(DevicePtr aDevice, bool aForget)
{
	// find and remove from my list.
	for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
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
	for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
		if (pos->get()==aDeviceP) {
      dev = *pos;
			break;
		}
	}
  return dev;
}



void DeviceClassContainer::removeDevices(bool aForget)
{
	for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    DevicePtr dev = *pos;
    deviceContainerP->removeDevice(dev, aForget);
  }
  // clear my own list
  devices.clear();
}




string DeviceClassContainer::description()
{
  string d = string_format("Deviceclass Container '%s' contains %d devices:\n", deviceClassIdentifier(), devices.size());
  for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    d.append((*pos)->description());
  }
  return d;
}


#pragma mark - property access

enum {
  deviceClassInstance_key,
  dsuids_key,
  numClassContainerProperties
};



int DeviceClassContainer::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numClassContainerProperties;
}


const PropertyDescriptor *DeviceClassContainer::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numClassContainerProperties] = {
    { "deviceClassInstance", apivalue_string, false, deviceClassInstance_key },
    { "dsuids", apivalue_string, true, dsuids_key }
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}



bool DeviceClassContainer::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (!aForWrite) {
    // read only
    if (aPropertyDescriptor.accessKey==deviceClassInstance_key) {
      // return dSUID of contained devices
      aPropValue->setStringValue(deviceClassContainerInstanceIdentifier());
      return true;
    }
    else if (aPropertyDescriptor.accessKey==dsuids_key) {
      if (aIndex==PROP_ARRAY_SIZE) {
        // return size of array
        aPropValue->setUint32Value((uint32_t)devices.size());
        return true;
      }
      else if (aIndex<devices.size()) {
        // return dSUID of contained devices
        aPropValue->setStringValue(devices[aIndex]->dSUID.getString());
        return true;
      }
    }
  }
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}

