//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#include "deviceclasscontainer.hpp"

#include "device.hpp"

using namespace p44;


DeviceClassContainer::DeviceClassContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  inherited(aDeviceContainerP),
  inheritedParams(aDeviceContainerP->getDsParamStore()),
  instanceNumber(aInstanceNumber),
  defaultZoneID(0),
  vdcFlags(0),
  tag(aTag)
{
}



string DeviceClassContainer::modelUID()
{
  // use device class identifier as modelID
  DsUid vdcNamespace(DSUID_P44VDC_MODELUID_UUID);
  // now make UUIDv5 type dSUID out of it
  DsUid modelUID;
  modelUID.setNameInSpace(deviceClassIdentifier(), vdcNamespace);
  return modelUID.getString();
}


void DeviceClassContainer::setName(const string &aName)
{
  if (aName!=getName()) {
    // has changed
    inherited::setName(aName);
    // make sure it will be saved
    markDirty();
  }
}


ErrorPtr DeviceClassContainer::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-collectDevices") {
    // (re)collect devices of this particular device class
    bool incremental = true;
    bool exhaustive = false;
    checkBoolParam(aParams, "incremental", incremental);
    checkBoolParam(aParams, "exhaustive", exhaustive);
    collectDevices(boost::bind(&DeviceClassContainer::collectDevicesMethodComplete, this, aRequest, _1), incremental, exhaustive);
  }
  else {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}


void DeviceClassContainer::collectDevicesMethodComplete(VdcApiRequestPtr aRequest, ErrorPtr aError)
{
  // devices re-collected, return ok (empty response)
  if (Error::isOK(aError)) {
    // collected ok
    aRequest->sendResult(ApiValuePtr());
  }
  else {
    // collected with error, report it
    aRequest->sendError(aError);
  }
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


void DeviceClassContainer::selfTest(CompletedCB aCompletedCB)
{
  // by default, assume everything ok
  aCompletedCB(ErrorPtr());
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
  // class containers have v5 UUIDs based on the device container's master UUID as namespace
  string name = string_format("%s.%d", deviceClassIdentifier(), getInstanceNumber()); // name is class identifier plus instance number: classID.instNo
  dSUID.setNameInSpace(name, getDeviceContainer().dSUID); // domain is dSUID of device container
}


string DeviceClassContainer::deviceClassContainerInstanceIdentifier() const
{
  string s(deviceClassIdentifier());
  string_format_append(s, ".%d@", getInstanceNumber());
  s.append(deviceContainerP->dSUID.getString());
  return s;
}


bool DeviceClassContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vdc", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}




// add a device
bool DeviceClassContainer::addDevice(DevicePtr aDevice)
{
  // announce to global device container
  aDevice->willBeAdded();
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
  // remove from global device container
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



#pragma mark - persistent vdc level params


ErrorPtr DeviceClassContainer::load()
{
  ErrorPtr err;
  // load the vdc settings
  err = loadFromStore(dSUID.getString().c_str());
  if (!Error::isOK(err)) LOG(LOG_ERR,"Error loading settings for vdc %s: %s", shortDesc().c_str(), err->description().c_str());
  return ErrorPtr();
}


ErrorPtr DeviceClassContainer::save()
{
  ErrorPtr err;
  // save the vdc settings
  err = saveToStore(dSUID.getString().c_str(), false); // only one record per vdc
  return ErrorPtr();
}


ErrorPtr DeviceClassContainer::forget()
{
  // delete the vdc settings
  deleteFromStore();
  return ErrorPtr();
}



#pragma mark - property access

static char deviceclass_key;
static char device_container_key;
static char capabilities_container_key;
static char device_key;

enum {
  webui_url_key,
  defaultzone_key,
  capabilities_key,
  devices_key,
  deviceclassidentifier_key,
  instancenumber_key,
  numClassContainerProperties
};


enum {
  capability_metering_key,
  numCapabilities
};



int DeviceClassContainer::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->hasObjectKey(device_container_key)) {
    return (int)devices.size();
  }
  else if (aParentDescriptor && aParentDescriptor->hasObjectKey(capabilities_container_key)) {
    return numCapabilities;
  }
  return inherited::numProps(aDomain, aParentDescriptor)+numClassContainerProperties;
}


PropertyDescriptorPtr DeviceClassContainer::getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->hasObjectKey(device_container_key)) {
    // accessing one of the devices by numeric index
    return getDescriptorByNumericName(
      aPropMatch, aStartIndex, aDomain, aParentDescriptor,
      OKEY(device_key)
    );
  }
  // None of the containers within Device - let base class handle vdc-Level properties
  return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aParentDescriptor);
}


PropertyContainerPtr DeviceClassContainer::getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  if (aPropertyDescriptor->isArrayContainer()) {
    // local container
    return PropertyContainerPtr(this); // handle myself
  }
  else if (aPropertyDescriptor->hasObjectKey(device_key)) {
    // - get device
    PropertyContainerPtr container = devices[aPropertyDescriptor->fieldKey()];
    aPropertyDescriptor.reset(); // next level is "root" again (is a DsAddressable)
    return container;
  }
  // unknown here
  return NULL;
}



// note: is only called when getDescriptorByName does not resolve the name
PropertyDescriptorPtr DeviceClassContainer::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->hasObjectKey(capabilities_container_key)) {
    // capabilities level
    static const PropertyDescription capability_props[numClassContainerProperties] = {
      { "metering", apivalue_bool, capability_metering_key, OKEY(capabilities_container_key) },
    };
    // simple, all on this level
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&capability_props[aPropIndex], aParentDescriptor));
  }
  else {
    // vdc level
    static const PropertyDescription properties[numClassContainerProperties] = {
      { "configURL", apivalue_string, webui_url_key, OKEY(deviceclass_key) },
      { "zoneID", apivalue_uint64, defaultzone_key, OKEY(deviceclass_key) },
      { "capabilities", apivalue_object+propflag_container, capabilities_key, OKEY(capabilities_container_key) },
      { "x-p44-devices", apivalue_object+propflag_container, devices_key, OKEY(device_container_key) },
      { "x-p44-deviceClass", apivalue_string, deviceclassidentifier_key, OKEY(deviceclass_key) },
      { "x-p44-instanceNo", apivalue_uint64, instancenumber_key, OKEY(deviceclass_key) }
    };
    int n = inherited::numProps(aDomain, aParentDescriptor);
    if (aPropIndex<n)
      return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
    aPropIndex -= n; // rebase to 0 for my own first property
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
  }
}



bool DeviceClassContainer::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(deviceclass_key)) {
    // vdc level properties
    if (aMode==access_read) {
      // read
      switch (aPropertyDescriptor->fieldKey()) {
        case webui_url_key:
          aPropValue->setStringValue(webuiURLString());
          return true;
        case defaultzone_key:
          aPropValue->setInt32Value(defaultZoneID);
          return true;
        case deviceclassidentifier_key:
          aPropValue->setStringValue(deviceClassIdentifier());
          return true;
        case instancenumber_key:
          aPropValue->setInt32Value(getInstanceNumber());
          return true;
      }
    }
    else {
      // write
      switch (aPropertyDescriptor->fieldKey()) {
        case defaultzone_key:
          defaultZoneID = aPropValue->int32Value();
          markDirty();
          return true;
      }
    }
  }
  else if (aPropertyDescriptor->hasObjectKey(capabilities_container_key)) {
    // capabilities
    if (aMode==access_read) {
      switch (aPropertyDescriptor->fieldKey()) {
        case capability_metering_key: aPropValue->setBoolValue(false); return true; // TODO: implement actual metering flag
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - persistence implementation

// SQLIte3 table name to store these parameters to
const char *DeviceClassContainer::tableName()
{
  return "VdcSettings";
}


// data field definitions

static const size_t numFields = 3;

size_t DeviceClassContainer::numFieldDefs()
{
  return inheritedParams::numFieldDefs()+numFields;
}


const FieldDefinition *DeviceClassContainer::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "vdcFlags", SQLITE_INTEGER },
    { "vdcName", SQLITE_TEXT },
    { "defaultZoneID", SQLITE_INTEGER }
  };
  if (aIndex<inheritedParams::numFieldDefs())
    return inheritedParams::getFieldDef(aIndex);
  aIndex -= inheritedParams::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void DeviceClassContainer::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inheritedParams::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the field value
  vdcFlags = aRow->get<int>(aIndex++);
  setName(nonNullCStr(aRow->get<const char *>(aIndex++)));
  defaultZoneID = aRow->get<int>(aIndex++);
}


// bind values to passed statement
void DeviceClassContainer::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inheritedParams::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, vdcFlags);
  aStatement.bind(aIndex++, getName().c_str());
  aStatement.bind(aIndex++, defaultZoneID);
}

#pragma mark - description/shortDesc


string DeviceClassContainer::description()
{
  string d = string_format("%s #%d: %s (%ld devices)\n", deviceClassIdentifier(), getInstanceNumber(), shortDesc().c_str(), (long)devices.size());
  return d;
}



