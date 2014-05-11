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
    pushBehaviourState();
  }
}


void DsBehaviour::pushBehaviourState()
{
  VdcApiConnectionPtr api = device.getDeviceContainer().getSessionConnection();
  if (api) {
    ApiValuePtr query = api->newApiValue();
    query->setType(apivalue_object);
    ApiValuePtr subQuery = query->newValue(apivalue_object);
    subQuery->add(string_format("%d",index), subQuery->newValue(apivalue_null));
    query->add(string(getTypeName()).append("States"), subQuery);
    device.pushProperty(query, VDC_API_DOMAIN);
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

int DsBehaviour::numLocalProps(PropertyDescriptorPtr aParentDescriptor)
{
  switch (aParentDescriptor->parentDescriptor->fieldKey()) {
    case descriptions_key_offset: return numDescProps()+numDsBehaviourDescProperties;
    case settings_key_offset: return numSettingsProps()+numDsBehaviourSettingsProperties;
    case states_key_offset: return numStateProps()+numDsBehaviourStateProperties;
    default: return 0;
  }
}


int DsBehaviour::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  return inheritedProps::numProps(aDomain, aParentDescriptor)+numLocalProps(aParentDescriptor);
}


PropertyDescriptorPtr DsBehaviour::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription descProperties[numDsBehaviourDescProperties] = {
    { "name", apivalue_string, name_key+descriptions_key_offset, OKEY(dsBehaviour_Key) },
    { "type", apivalue_string, type_key+descriptions_key_offset, OKEY(dsBehaviour_Key) },
  };
  static const PropertyDescription settingsProperties[numDsBehaviourSettingsProperties] = {
    { "group", apivalue_uint64, group_key+settings_key_offset, OKEY(dsBehaviour_Key) },
  };
  static const PropertyDescription stateProperties[numDsBehaviourStateProperties] = {
    { "error", apivalue_uint64, error_key+states_key_offset, OKEY(dsBehaviour_Key) },
  };
  int n = inheritedProps::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inheritedProps::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  if (aPropIndex>=numLocalProps(aParentDescriptor))
    return NULL;
  switch (aParentDescriptor->parentDescriptor->fieldKey()) {
    case descriptions_key_offset:
      // check for generic description properties
      if (aPropIndex<numDsBehaviourDescProperties)
        return PropertyDescriptorPtr(new StaticPropertyDescriptor(&descProperties[aPropIndex], aParentDescriptor));
      aPropIndex -= numDsBehaviourDescProperties;
      // check type-specific descriptions
      return getDescDescriptorByIndex(aPropIndex);
    case settings_key_offset:
      // check for generic settings properties
      if (aPropIndex<numDsBehaviourSettingsProperties)
        return PropertyDescriptorPtr(new StaticPropertyDescriptor(&settingsProperties[aPropIndex], aParentDescriptor));
      aPropIndex -= numDsBehaviourSettingsProperties;
      // check type-specific settings
      return getSettingsDescriptorByIndex(aPropIndex);
    case states_key_offset:
      // check for generic state properties
      if (aPropIndex<numDsBehaviourStateProperties)
        return PropertyDescriptorPtr(new StaticPropertyDescriptor(&stateProperties[aPropIndex], aParentDescriptor));
      aPropIndex -= numDsBehaviourStateProperties;
      // check type-specific states
      return getStateDescriptorByIndex(aPropIndex);
    default:
      return NULL;
  }
}


bool DsBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(dsBehaviour_Key)) {
    if (aMode==access_read) {
      // Read
      switch (aPropertyDescriptor->fieldKey()) {
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
      switch (aPropertyDescriptor->fieldKey()) {
        // settings
        case group_key+settings_key_offset:
          setGroup((DsGroup)aPropValue->int32Value());
          markDirty();
          return true;
      }

    }
  }
  return inheritedProps::accessField(aMode, aPropValue, aPropertyDescriptor); // let base class handle it
}



#pragma mark - description/shortDesc


string DsBehaviour::description()
{
  string s = string_format("- behaviour hardware name: '%s'\n", hardwareName.c_str());
  string_format_append(s, "- group: %d, hardwareError: %d\n", group, hardwareError);
  return s;
}





