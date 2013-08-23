//
//  outputbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "outputbehaviour.hpp"

using namespace p44;

OutputBehaviour::OutputBehaviour(Device &aDevice, size_t aIndex) :
  inherited(aDevice, aIndex),
  // persistent settings
  outputGroup(group_yellow_light), // default to light
  outputMode(outputmode_disabled), // none by default, hardware should set a default matching the actual HW capabilities
  pushChanges(false) // do not push changes
{
  // set default hardware default configuration
  setHardwareOutputConfig(outputFunction_switch, false, -1);
}


void OutputBehaviour::setHardwareOutputConfig(DsOutputFunction aOutputFunction, bool aVariableRamp, double aMaxPower)
{
  outputFunction = aOutputFunction;
  variableRamp = aVariableRamp;
  maxPower = aMaxPower;
  // determine default output mode
  switch (outputFunction) {
    case outputFunction_switch:
      outputMode = outputmode_binary;
    case outputFunction_dimmer:
      outputMode = outputmode_gradual;
    default:
      outputMode = outputmode_disabled;
  }
}


#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *OutputBehaviour::tableName()
{
  return "OutputSettings";
}


// data field definitions

static const size_t numFields = 3;

size_t OutputBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *OutputBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "outputGroup", SQLITE_INTEGER },
    { "outputMode", SQLITE_INTEGER },
    { "outputFlags", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  outputflag_pushChanges = 0x0001,
};

/// load values from passed row
void OutputBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  outputGroup  = (DsGroup)aRow->get<int>(aIndex++);
  outputMode = (DsOutputMode)aRow->get<int>(aIndex++);
  int flags = aRow->get<int>(aIndex++);
  // decode the flags
  pushChanges = flags & outputflag_pushChanges;
}


// bind values to passed statement
void OutputBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  int flags = 0;
  if (pushChanges) flags |= outputflag_pushChanges;
  // bind the fields
  aStatement.bind(aIndex++, outputGroup);
  aStatement.bind(aIndex++, outputMode);
  aStatement.bind(aIndex++, flags);
}



#pragma mark - property access

static char output_key;

// description properties

enum {
  outputFunction_key,
  variableRamp_key,
  maxPower_key,
  numDescProperties
};


int OutputBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptor *OutputBehaviour::getDescDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numDescProperties] = {
    { "outputFunction", ptype_int8, false, outputFunction_key+descriptions_key_offset, &output_key },
    { "variableRamp", ptype_bool, false, variableRamp_key+descriptions_key_offset, &output_key },
    { "maxPower", ptype_double, false, maxPower_key+descriptions_key_offset, &output_key },
  };
  return &properties[aPropIndex];
}


// settings properties

enum {
  group_key,
  mode_key,
  pushChanges_key,
  numSettingsProperties
};


int OutputBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *OutputBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "group", ptype_int8, false, group_key+settings_key_offset, &output_key },
    { "mode", ptype_int8, false, mode_key+settings_key_offset, &output_key },
    { "pushChanges_key", ptype_bool, false, pushChanges_key+settings_key_offset, &output_key },
  };
  return &properties[aPropIndex];
}

// state properties

enum {
  value_key,
  error_key,
  numStateProperties
};


int OutputBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptor *OutputBehaviour::getStateDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numStateProperties] = {
    { "value", ptype_int32, false, value_key+states_key_offset, &output_key },
    { "error", ptype_int32, false, error_key+states_key_offset, &output_key },
  };
  return &properties[aPropIndex];
}


// access to all fields

bool OutputBehaviour::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&output_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Description properties
        case outputFunction_key+descriptions_key_offset:
          aPropValue = JsonObject::newInt32(outputFunction);
          return true;
        case variableRamp_key+descriptions_key_offset:
          aPropValue = JsonObject::newBool(variableRamp);
          return true;
        case maxPower_key+descriptions_key_offset:
          aPropValue = JsonObject::newDouble(maxPower);
          return true;
        // Settings properties
        case group_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(outputGroup);
          return true;
        case mode_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(outputMode);
          return true;
        case pushChanges_key+settings_key_offset:
          aPropValue = JsonObject::newBool(pushChanges);
          return true;
        // States properties
        case value_key+states_key_offset:
          aPropValue = JsonObject::newInt32(device.getOutputValue(*this));
          return true;
        case error_key+states_key_offset:
          aPropValue = JsonObject::newInt32(device.getOutputError(*this));
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        // Settings properties
        case group_key+settings_key_offset:
          outputGroup = (DsGroup)aPropValue->int32Value();
          markDirty();
          return true;
        case mode_key+settings_key_offset:
          outputMode = (DsOutputMode)aPropValue->int32Value();
          markDirty();
          return true;
        case pushChanges_key+settings_key_offset:
          pushChanges = aPropValue->boolValue();
          markDirty();
          return true;
        // States properties
        case value_key+states_key_offset:
          device.setOutputValue(*this, aPropValue->int32Value());
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}



#pragma mark - description/shortDesc


string OutputBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- hardware function: %d (%s)\n", outputFunction, outputFunction==outputFunction_dimmer ? "dimmer" : (outputFunction==outputFunction_switch ? "switch" : "other"));
  string_format_append(s, "- group: %d, output mode: %d\n", outputGroup, outputMode);
  s.append(inherited::description());
  return s;
}

