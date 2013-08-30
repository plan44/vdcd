//
//  outputbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "outputbehaviour.hpp"

using namespace p44;

OutputBehaviour::OutputBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  outputMode(outputmode_disabled), // none by default, hardware should set a default matching the actual HW capabilities
  outputUpdatePending(false), // no output update pending
  cachedOutputValue(0), // output value cache
  outputLastSent(Never), // we don't known nor have we sent the output state
  nextTransitionTime(0), // none
  pushChanges(false) // do not push changes
{
  // set default hardware default configuration
  setHardwareOutputConfig(outputFunction_switch, false, -1);
  // default to joker
  setGroup(group_black_joker);
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
      break;
    case outputFunction_dimmer:
      outputMode = outputmode_gradual;
      break;
    default:
      outputMode = outputmode_disabled;
      break;
  }
}


int32_t OutputBehaviour::getOutputValue()
{
  return cachedOutputValue;
}


// only used at startup to get the inital value FROM the hardware
// NOT to be used to change the hardware output value!
void OutputBehaviour::initOutputValue(uint32_t aActualOutputValue)
{
  cachedOutputValue = aActualOutputValue;
  outputValueApplied(); // now we know that we are in sync
}


void OutputBehaviour::setOutputValue(int32_t aNewValue, MLMicroSeconds aTransitionTime)
{
  if (aNewValue!=cachedOutputValue) {
    cachedOutputValue = aNewValue;
    nextTransitionTime = aTransitionTime;
    outputUpdatePending = true; // pending to be sent to the device
    outputLastSent = Never; // cachedOutputValue is no longer applied (does not correspond with actual hardware)
    // let device know to hardware can update actual output
    device.updateOutputValue(*this);
  }
}


void OutputBehaviour::outputValueApplied()
{
  outputUpdatePending = false; // applied
  outputLastSent = MainLoop::now(); // now we know that we are in sync
}


#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *OutputBehaviour::tableName()
{
  return "OutputSettings";
}


// data field definitions

static const size_t numFields = 2;

size_t OutputBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *OutputBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
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
  mode_key,
  pushChanges_key,
  numSettingsProperties
};


int OutputBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *OutputBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "mode", ptype_int8, false, mode_key+settings_key_offset, &output_key },
    { "pushChanges_key", ptype_bool, false, pushChanges_key+settings_key_offset, &output_key },
  };
  return &properties[aPropIndex];
}

// state properties

enum {
  value_key,
  age_key,
  numStateProperties
};


int OutputBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptor *OutputBehaviour::getStateDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numStateProperties] = {
    { "value", ptype_int32, false, value_key+states_key_offset, &output_key },
    { "age", ptype_double, false, age_key+states_key_offset, &output_key },
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
        case mode_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(outputMode);
          return true;
        case pushChanges_key+settings_key_offset:
          aPropValue = JsonObject::newBool(pushChanges);
          return true;
        // States properties
        case value_key+states_key_offset:
          aPropValue = JsonObject::newInt32(getOutputValue());
          return true;
        case age_key+states_key_offset:
          if (outputLastSent==Never)
            aPropValue = JsonObject::newNull(); // no value known
          else
            aPropValue = JsonObject::newDouble(outputLastSent); // when value was last applied to hardware
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        // Settings properties
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
          setOutputValue(aPropValue->int32Value());
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
  string_format_append(s, "- hardware output function: %d (%s)\n", outputFunction, outputFunction==outputFunction_dimmer ? "dimmer" : (outputFunction==outputFunction_switch ? "switch" : "other"));
  string_format_append(s, "- output mode: %d\n", outputMode);
  s.append(inherited::description());
  return s;
}

