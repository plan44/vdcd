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
  setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
  // default to joker
  setGroup(group_black_joker);
}


void OutputBehaviour::setHardwareOutputConfig(DsOutputFunction aOutputFunction, DsUsageHint aUsage, bool aVariableRamp, double aMaxPower)
{
  outputFunction = aOutputFunction;
  outputUsage = aUsage;
  variableRamp = aVariableRamp;
  maxPower = aMaxPower;
  // determine default output mode
  switch (outputFunction) {
    case outputFunction_switch:
      outputMode = outputmode_binary;
      break;
    case outputFunction_dimmer:
    case outputFunction_positional:
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
  DBGLOG(LOG_DEBUG, ">>>> initOutputValue actualOutputValue=%d\n", aActualOutputValue);
  cachedOutputValue = aActualOutputValue;
  outputValueApplied(); // now we know that we are in sync
}


void OutputBehaviour::setOutputValue(int32_t aNewValue, MLMicroSeconds aTransitionTime)
{
  LOG(LOG_INFO,
    "Output '%s' in device %s: is requested to apply new value %d (transition time=%lld uS), last known value is %d\n",
    hardwareName.c_str(), device.shortDesc().c_str(), aNewValue, aTransitionTime, cachedOutputValue
  );
  if (aNewValue!=cachedOutputValue) {
    cachedOutputValue = aNewValue;
    nextTransitionTime = aTransitionTime;
    outputUpdatePending = true; // pending to be sent to the device
    outputLastSent = Never; // cachedOutputValue is no longer applied (does not correspond with actual hardware)
  }
  // check if output update is pending (might be because of changing the value right above
  // but also when derived class marks update pending because of changed values
  // of secondary outputs (e.g. hue color scene recall)
  if (outputUpdatePending) {
    // let device know so hardware can update actual output
    device.updateOutputValue(*this);
  }
}


void OutputBehaviour::outputValueApplied()
{
  outputUpdatePending = false; // applied
  outputLastSent = MainLoop::now(); // now we know that we are in sync
  LOG(LOG_INFO,
    "Output '%s' in device %s: has applied new value %d to hardware\n",
    hardwareName.c_str(), device.shortDesc().c_str(), cachedOutputValue
  );
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
  outputUsage_key,
  variableRamp_key,
  maxPower_key,
  numDescProperties
};


int OutputBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptor *OutputBehaviour::getDescDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numDescProperties] = {
    { "function", apivalue_uint64, false, outputFunction_key+descriptions_key_offset, &output_key },
    { "outputUsage", apivalue_uint64, false, outputUsage_key+descriptions_key_offset, &output_key },
    { "variableRamp", apivalue_bool, false, variableRamp_key+descriptions_key_offset, &output_key },
    { "maxPower", apivalue_double, false, maxPower_key+descriptions_key_offset, &output_key },
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
    { "mode", apivalue_uint64, false, mode_key+settings_key_offset, &output_key },
    { "pushChanges", apivalue_bool, false, pushChanges_key+settings_key_offset, &output_key },
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
    { "value", apivalue_uint64, false, value_key+states_key_offset, &output_key }, // note: so far, pbuf API requires uint here
    { "age", apivalue_double, false, age_key+states_key_offset, &output_key },
  };
  return &properties[aPropIndex];
}


// access to all fields

bool OutputBehaviour::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&output_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Description properties
        case outputFunction_key+descriptions_key_offset:
          aPropValue->setUint8Value(outputFunction);
          return true;
        case outputUsage_key+descriptions_key_offset:
          aPropValue->setUint16Value(outputUsage);
          return true;
        case variableRamp_key+descriptions_key_offset:
          aPropValue->setBoolValue(variableRamp);
          return true;
        case maxPower_key+descriptions_key_offset:
          aPropValue->setDoubleValue(maxPower);
          return true;
        // Settings properties
        case mode_key+settings_key_offset:
          aPropValue->setUint8Value(outputMode);
          return true;
        case pushChanges_key+settings_key_offset:
          aPropValue->setBoolValue(pushChanges);
          return true;
        // States properties
        case value_key+states_key_offset:
          aPropValue->setUint32Value(getOutputValue());
          return true;
        case age_key+states_key_offset:
          if (outputLastSent==Never)
            aPropValue->setNull(); // no value known
          else
            aPropValue->setDoubleValue(((double)MainLoop::now()-outputLastSent)/Second);
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

