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
  setHardwareOutputConfig(outputFunction_switch, channeltype_undefined, usage_undefined, false, -1);
  // default to joker
  setGroup(group_black_joker);
}


void OutputBehaviour::setHardwareOutputConfig(DsOutputFunction aOutputFunction, DsChannelType aDefaultChannel, DsUsageHint aUsage, bool aVariableRamp, double aMaxPower)
{
  outputFunction = aOutputFunction;
  defaultChannel = aDefaultChannel;
  channel = defaultChannel;
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

static const size_t numFields = 3;

size_t OutputBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *OutputBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "outputMode", SQLITE_INTEGER },
    { "outputFlags", SQLITE_INTEGER },
    { "outputChannel", SQLITE_INTEGER }
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
  channel = (DsChannelType)aRow->get<int>(aIndex++);
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
  aStatement.bind(aIndex++, channel);
}



#pragma mark - property access

static char output_key;

// description properties

enum {
  outputFunction_key,
  defaultChannel_key,
  outputUsage_key,
  variableRamp_key,
  maxPower_key,
  numDescProperties
};


int OutputBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptorPtr OutputBehaviour::getDescDescriptorByIndex(int aPropIndex)
{
  static const PropertyDescription properties[numDescProperties] = {
    { "function", apivalue_uint64, outputFunction_key+descriptions_key_offset, OKEY(output_key) },
    { "channel", apivalue_uint64, defaultChannel_key+descriptions_key_offset, OKEY(output_key) },
    { "outputUsage", apivalue_uint64, outputUsage_key+descriptions_key_offset, OKEY(output_key) },
    { "variableRamp", apivalue_bool, variableRamp_key+descriptions_key_offset, OKEY(output_key) },
    { "maxPower", apivalue_double, maxPower_key+descriptions_key_offset, OKEY(output_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex]));
}


// settings properties

enum {
  mode_key,
  channel_key,
  pushChanges_key,
  numSettingsProperties
};


int OutputBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptorPtr OutputBehaviour::getSettingsDescriptorByIndex(int aPropIndex)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "mode", apivalue_uint64, mode_key+settings_key_offset, OKEY(output_key) },
    { "channel", apivalue_uint64, channel_key+settings_key_offset, OKEY(output_key) },
    { "pushChanges", apivalue_bool, pushChanges_key+settings_key_offset, OKEY(output_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex]));
}

// state properties

enum {
  value_key,
  age_key,
  numStateProperties
};


int OutputBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptorPtr OutputBehaviour::getStateDescriptorByIndex(int aPropIndex)
{
  static const PropertyDescription properties[numStateProperties] = {
    { "value", apivalue_uint64, value_key+states_key_offset, OKEY(output_key) }, // note: so far, pbuf API requires uint here
    { "age", apivalue_double, age_key+states_key_offset, OKEY(output_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex]));
}


// access to all fields

bool OutputBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(output_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Description properties
        case outputFunction_key+descriptions_key_offset:
          aPropValue->setUint8Value(outputFunction);
          return true;
        case defaultChannel_key+descriptions_key_offset:
          aPropValue->setUint8Value(defaultChannel);
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
        case channel_key+settings_key_offset:
          aPropValue->setUint8Value(channel);
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
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case mode_key+settings_key_offset:
          outputMode = (DsOutputMode)aPropValue->int32Value();
          markDirty();
          return true;
        case channel_key+settings_key_offset:
          channel = (DsChannelType)aPropValue->int32Value();
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
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}



#pragma mark - description/shortDesc


string OutputBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- hardware output function: %d (%s)\n", outputFunction, outputFunction==outputFunction_dimmer ? "dimmer" : (outputFunction==outputFunction_switch ? "switch" : "other"));
  string_format_append(s, "- hardware-defined channel: %d, channel: %d, output mode: %d\n", defaultChannel, channel, outputMode);
  s.append(inherited::description());
  return s;
}

