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

#include "dsbehaviour.hpp"

#include "dsparams.hpp"

#include "device.hpp"

using namespace p44;



#pragma mark - DsBehaviour

DsBehaviour::DsBehaviour(Device &aDevice) :
  inheritedParams(aDevice.getDeviceContainer().getDsParamStore()),
  index(0),
  device(aDevice),
  hardwareName("<undefined>"),
  hardwareError(hardwareError_none),
  hardwareErrorUpdated(p44::Never)
{
}


DsBehaviour::~DsBehaviour()
{
}


void DsBehaviour::setHardwareError(DsHardwareError aHardwareError)
{
  if (aHardwareError!=hardwareError) {
    // error status has changed
    hardwareError = aHardwareError;
    hardwareErrorUpdated = MainLoop::now();
    // push the error status change
    device.pushProperty(string(getTypeName()).append("States"), VDC_API_DOMAIN, (int)index);
  }
}


void DsBehaviour::setGroup(DsGroup aGroup)
{
  if (group!=aGroup) {
    group = aGroup;
  }
}



string DsBehaviour::getDbKey()
{
  return string_format("%s_%d",device.dSUID.getString().c_str(),index);
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
  // Note: this must be the prefix for the xxxDescriptions, xxxSettings and xxxStates properties
  switch (getType()) {
    case behaviour_button : return "buttonInput";
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


enum {
  error_key,
  numDsBehaviourStateProperties
};



static char dsBehaviour_Key;

int DsBehaviour::numLocalProps(int aDomain)
{
  switch (aDomain) {
    case VDC_API_BHVR_DESC: return numDescProps()+numDsBehaviourDescProperties;
    case VDC_API_BHVR_SETTINGS: return numSettingsProps()+numDsBehaviourSettingsProperties;
    case VDC_API_BHVR_STATES: return numStateProps()+numDsBehaviourStateProperties;
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
    { "name", apivalue_string, false, name_key+descriptions_key_offset, &dsBehaviour_Key },
    { "type", apivalue_string, false, type_key+descriptions_key_offset, &dsBehaviour_Key },
  };
  static const PropertyDescriptor settingsProperties[numDsBehaviourSettingsProperties] = {
    { "group", apivalue_uint64, false, group_key+settings_key_offset, &dsBehaviour_Key },
  };
  static const PropertyDescriptor stateProperties[numDsBehaviourStateProperties] = {
    { "error", apivalue_uint64, false, error_key+states_key_offset, &dsBehaviour_Key },
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
      // check for generic state properties
      if (aPropIndex<numDsBehaviourStateProperties)
        return &stateProperties[aPropIndex];
      aPropIndex -= numDsBehaviourStateProperties;
      // check type-specific states
      return getStateDescriptor(aPropIndex);
    default:
      return NULL;
  }
}


bool DsBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&dsBehaviour_Key) {
    if (aMode==access_read) {
      // Read
      switch (aPropertyDescriptor.accessKey) {
        // descriptions
        case name_key+descriptions_key_offset: aPropValue->setStringValue(hardwareName); return true;
        case type_key+descriptions_key_offset: aPropValue->setStringValue(getTypeName()); return true;
        // settings
        case group_key+settings_key_offset: aPropValue->setUint16Value(group); return true;
        // state
        case error_key+states_key_offset: aPropValue->setUint16Value(hardwareError); return true;
      }
    }
    else {
      // Write
      switch (aPropertyDescriptor.accessKey) {
        // settings
        case group_key+settings_key_offset:
          setGroup((DsGroup)aPropValue->int32Value());
          markDirty();
          return true;
      }

    }
  }
  return inheritedProps::accessField(aMode, aPropValue, aPropertyDescriptor, aIndex); // let base class handle it
}



#pragma mark - description/shortDesc


string DsBehaviour::description()
{
  string s = string_format("- behaviour hardware name: '%s'\n", hardwareName.c_str());
  string_format_append(s, "- group: %d, hardwareError: %d\n", group, hardwareError);
  return s;
}





