//
//  dsbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 20.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dsbehaviour.hpp"

#include "dsparams.hpp"

#include "device.hpp"

using namespace p44;



#pragma mark - DsBehaviour

DsBehaviour::DsBehaviour(Device &aDevice, size_t aIndex) :
  inheritedParams(aDevice.getDeviceContainer().getDsParamStore()),
  index(aIndex),
  device(aDevice),
  hardwareName("<undefined>")
{
}


DsBehaviour::~DsBehaviour()
{
}


string DsBehaviour::getDbKey()
{
  return string_format("%s_%d",device.dsid.getString().c_str(),index);
}


ErrorPtr DsBehaviour::load()
{
  return loadFromStore(getDbKey().c_str());
}


ErrorPtr DsBehaviour::save()
{
  return saveToStore(getDbKey().c_str());
}


ErrorPtr DsBehaviour::forget()
{
  return deleteFromStore();
}


#pragma mark - property access


const char *DsBehaviour::getTypeName()
{
  switch (getType()) {
    case behaviour_button : return "button";
    case behaviour_binaryinput : return "binaryInput";
    case behaviour_output : return "output";
    case behaviour_sensor : return "sensor";
    default: return "<undefined>";
  }
}


enum {
  name_key,
  type_key,
  numDsBehaviourDescProperties
};

static char dsBehaviour_Key;

int DsBehaviour::numLocalProps(int aDomain)
{
  switch (aDomain) {
    case VDC_API_BHVR_DESC: return numDescProps()+numDsBehaviourDescProperties;
    case VDC_API_BHVR_SETTINGS: return numSettingsProps();
    case VDC_API_BHVR_STATES: return numStateProps();
    default: return 0;
  }
}


int DsBehaviour::numProps(int aDomain)
{
  return inheritedProps::numProps(aDomain)+numLocalProps(aDomain);
}


const PropertyDescriptor *DsBehaviour::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numDsBehaviourDescProperties] = {
    { "name", ptype_string, false, name_key+descriptions_key_offset, &dsBehaviour_Key },
    { "type", ptype_charptr, false, type_key+descriptions_key_offset, &dsBehaviour_Key },
  };
  int n = inheritedProps::numProps(aDomain);
  if (aPropIndex<n)
    return inheritedProps::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  if (aPropIndex>=numLocalProps(aDomain))
    return NULL;
  switch (aDomain) {
    case VDC_API_BHVR_DESC:
      // check for generic description properties
      if (aPropIndex<numDsBehaviourDescProperties)
        return &properties[aPropIndex];
      aPropIndex -= numDsBehaviourDescProperties;
      // check type-specific properties
      return getDescDescriptor(aPropIndex);
    case VDC_API_BHVR_SETTINGS:
      // settings are always type-specific
      return getSettingsDescriptor(aPropIndex);
    case VDC_API_BHVR_STATES:
      // states are always type-specific
      return getStateDescriptor(aPropIndex);
    default: return NULL;
  }
}


bool DsBehaviour::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&dsBehaviour_Key) {
    if (!aForWrite) {
      switch (aPropertyDescriptor.accessKey) {
        case name_key+descriptions_key_offset: aPropValue = JsonObject::newString(hardwareName); return true;
        case type_key+descriptions_key_offset: aPropValue = JsonObject::newString(getTypeName()); return true;
      }
    }
  }
  return inheritedProps::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex); // let base class handle it
}








