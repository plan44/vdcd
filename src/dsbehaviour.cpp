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


#pragma mark - DsBehaviourSettings

DsBehaviourSettings::DsBehaviourSettings(ParamStore &aParamStore, DsBehaviour &aBehaviour) :
  inherited(aParamStore),
  behaviour(aBehaviour)
{
}


string parentID();




#pragma mark - DsBehaviour

DsBehaviour::DsBehaviour(Device &aDevice, size_t aIndex) :
  index(aIndex),
  device(aDevice),
  hardwareName("<undefined>")
{
}


DsBehaviour::~DsBehaviour()
{
}


string DsBehaviourSettings::getDbKey()
{
  return string_format("%s_%d",behaviour.device.dsid.getString().c_str(),behaviour.index);
}


ErrorPtr DsBehaviourSettings::load()
{
  return loadFromStore(getDbKey().c_str());
}


ErrorPtr DsBehaviourSettings::save()
{
  return saveToStore(getDbKey().c_str());
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
static const PropertyDescriptor dsBehaviourDescProperties[numDsBehaviourDescProperties] = {
  { "name", ptype_string, false, name_key+descriptions_key_offset, &dsBehaviour_Key },
  { "type", ptype_charptr, false, type_key+descriptions_key_offset, &dsBehaviour_Key },
};



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
  return inherited::numProps(aDomain)+numLocalProps(aDomain);
}


const PropertyDescriptor *DsBehaviour::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  if (aPropIndex>=numLocalProps(aDomain))
    return NULL;
  switch (aDomain) {
    case VDC_API_BHVR_DESC:
      // check for generic description properties
      if (aPropIndex<numDsBehaviourDescProperties)
        return &dsBehaviourDescProperties[aPropIndex];
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
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex); // let base class handle it
}








