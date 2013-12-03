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


DeviceClassContainer::DeviceClassContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP) :
  inherited(aDeviceContainerP),
  instanceNumber(aInstanceNumber)
{
}


void DeviceClassContainer::addClassToDeviceContainer()
{
  // derive dSUID first, as it will be mapped by dSUID in the device container 
  deriveDsUid();
  // add to container
  getDeviceContainer().addDeviceClassContainer(DeviceClassContainerPtr(this));
}




void DeviceClassContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // done
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


void DeviceClassContainer::deriveDsUid()
{
  // Note: device class containers ALWAYS have a modern dSUID, as these are not exposed in systms with old dsids at all
  // - class containers have v5 UUIDs based on the device container's master UUID as namespace
  string name = string_format("%s.%d", deviceClassIdentifier(), getInstanceNumber()); // name is class identifier plus instance number: classID.instNo
  dSUID.setNameInSpace(name, getDeviceContainer().dSUID); // domain is dSUID of device container
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


void DeviceClassContainer::removeDevices(bool aForget)
{
	for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    DevicePtr dev = *pos;
    deviceContainerP->removeDevice(dev, aForget);
  }
  // clear my own list
  devices.clear();
}






#pragma mark - property access

static char deviceclass_key;

enum {
  devices_key,
  numClassContainerProperties
};



int DeviceClassContainer::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numClassContainerProperties;
}


const PropertyDescriptor *DeviceClassContainer::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numClassContainerProperties] = {
    { "devices", apivalue_string, true, devices_key, &deviceclass_key }
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}



bool DeviceClassContainer::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&deviceclass_key) {
    if (!aForWrite) {
      // read only
      if (aPropertyDescriptor.accessKey==devices_key) {
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
  }
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}


#pragma mark - description/shortDesc


string DeviceClassContainer::description()
{
  string d = string_format("%s #%d: %s\n", deviceClassIdentifier(), getInstanceNumber(), shortDesc().c_str());
  string_format_append(d, "- contains %d devices:\n", devices.size());
  for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
    d.append((*pos)->description());
  }
  return d;
}



