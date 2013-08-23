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


void DsBehaviour::setGroup(DsGroup aGroup)
{
  if (group!=aGroup) {
    device.setGroupMembership(group, false);
    group = aGroup;
    device.setGroupMembership(group, true);
  }
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


#pragma mark - persistence implementation


// Note: no tablename - this is an abstract class

// data field definitions

static const size_t numFields = 1;

size_t DsBehaviour::numFieldDefs()
{
  return inheritedParams::numFieldDefs()+numFields;
}


const FieldDefinition *DsBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "dsGroup", SQLITE_INTEGER } // Note: don't call a SQL field "group"!
  };
  if (aIndex<inheritedParams::numFieldDefs())
    return inheritedParams::getFieldDef(aIndex);
  aIndex -= inheritedParams::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  buttonflag_setsLocalPriority = 0x0001,
  buttonflag_callsPresent = 0x0002
};

/// load values from passed row
void DsBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inheritedParams::loadFromRow(aRow, aIndex);
  // get the fields
  group  = (DsGroup)aRow->get<int>(aIndex++);
}


// bind values to passed statement
void DsBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inheritedParams::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, group);
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


enum {
  group_key,
  numDsBehaviourSettingsProperties
};


static char dsBehaviour_Key;

int DsBehaviour::numLocalProps(int aDomain)
{
  switch (aDomain) {
    case VDC_API_BHVR_DESC: return numDescProps()+numDsBehaviourDescProperties;
    case VDC_API_BHVR_SETTINGS: return numSettingsProps()+numDsBehaviourSettingsProperties;
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
  static const PropertyDescriptor descProperties[numDsBehaviourDescProperties] = {
    { "name", ptype_string, false, name_key+descriptions_key_offset, &dsBehaviour_Key },
    { "type", ptype_charptr, false, type_key+descriptions_key_offset, &dsBehaviour_Key },
  };
  static const PropertyDescriptor settingsProperties[numDsBehaviourSettingsProperties] = {
    { "group", ptype_int8, false, group_key+settings_key_offset, &dsBehaviour_Key },
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
        return &descProperties[aPropIndex];
      aPropIndex -= numDsBehaviourDescProperties;
      // check type-specific descriptions
      return getDescDescriptor(aPropIndex);
    case VDC_API_BHVR_SETTINGS:
      // check for generic settings properties
      if (aPropIndex<numDsBehaviourSettingsProperties)
        return &settingsProperties[aPropIndex];
      aPropIndex -= numDsBehaviourSettingsProperties;
      // check type-specific settings
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
        // descriptions
        case name_key+descriptions_key_offset: aPropValue = JsonObject::newString(hardwareName); return true;
        case type_key+descriptions_key_offset: aPropValue = JsonObject::newString(getTypeName()); return true;
        // settings
        case group_key+settings_key_offset: aPropValue = JsonObject::newInt32(group); return true;
      }
    }
    else {
      switch (aPropertyDescriptor.accessKey) {
        // settings
        case group_key+settings_key_offset:
          setGroup((DsGroup)aPropValue->int32Value());
          markDirty();
          return true;
      }

    }
  }
  return inheritedProps::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex); // let base class handle it
}








