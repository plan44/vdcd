//
//  binaryinputbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "binaryinputbehaviour.hpp"

using namespace p44;

BinaryInputBehaviour::BinaryInputBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  configuredInputType(binInpType_none),
  minPushInterval(200*MilliSecond),
  // state
  lastUpdate(Never),
  lastPush(Never),
  currentState(false)
{
  // set dummy default hardware default configuration
  setHardwareInputConfig(binInpType_none, usage_undefined, true, 15*Second);
  // default to joker
  setGroup(group_black_joker);
}


void BinaryInputBehaviour::setHardwareInputConfig(DsBinaryInputType aInputType, DsUsageHint aUsage, bool aReportsChanges, MLMicroSeconds aUpdateInterval)
{
  hardwareInputType = aInputType;
  inputUsage = aUsage;
  reportsChanges = aReportsChanges;
  updateInterval = aUpdateInterval;
  // set default input mode to hardware type
  configuredInputType = hardwareInputType;
}


void BinaryInputBehaviour::updateInputState(bool aNewState)
{
  LOG(LOG_NOTICE, "BinaryInput %s in device %s received new state = %d\n", hardwareName.c_str(),  device.shortDesc().c_str(), aNewState);
  if (aNewState!=currentState) {
    // changed state
    currentState = aNewState;
    MLMicroSeconds now = MainLoop::now();
    lastUpdate = now;
    if (lastPush==Never || now>lastPush+minPushInterval) {
      // push the new value
      device.pushProperty("binaryInputStates", VDC_API_DOMAIN, (int)index);
      lastPush = now;
    }
  }
}


#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *BinaryInputBehaviour::tableName()
{
  return "BinaryInputSettings";
}


// data field definitions

static const size_t numFields = 2;

size_t BinaryInputBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *BinaryInputBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "minPushInterval", SQLITE_INTEGER },
    { "configuredInputType", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void BinaryInputBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  minPushInterval = aRow->get<int>(aIndex++);
  configuredInputType = (DsBinaryInputType)aRow->get<int>(aIndex++);
}


// bind values to passed statement
void BinaryInputBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, (int)minPushInterval);
  aStatement.bind(aIndex++, (int)configuredInputType);
}



#pragma mark - property access

static char binaryInput_key;

// description properties

enum {
  hardwareInputType_key,
  inputUsage_key,
  reportsChanges_key,
  updateInterval_key,
  numDescProperties
};


int BinaryInputBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptor *BinaryInputBehaviour::getDescDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numDescProperties] = {
    { "hardwareSensorFunction", apivalue_uint64, false, hardwareInputType_key+descriptions_key_offset, &binaryInput_key },
    { "inputUsage", apivalue_uint64, false, inputUsage_key+descriptions_key_offset, &binaryInput_key },
    { "inputType", apivalue_bool, false, reportsChanges_key+descriptions_key_offset, &binaryInput_key },
    { "updateInterval", apivalue_double, false, updateInterval_key+descriptions_key_offset, &binaryInput_key },
  };
  return &properties[aPropIndex];
}


// settings properties

enum {
  minPushInterval_key,
  configuredInputType_key,
  numSettingsProperties
};


int BinaryInputBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *BinaryInputBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "minPushInterval", apivalue_double, false, minPushInterval_key+settings_key_offset, &binaryInput_key },
    { "sensorFunction", apivalue_uint64, false, configuredInputType_key+settings_key_offset, &binaryInput_key },
  };
  return &properties[aPropIndex];
}

// state properties

enum {
  value_key,
  age_key,
  numStateProperties
};


int BinaryInputBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptor *BinaryInputBehaviour::getStateDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numStateProperties] = {
    { "value", apivalue_double, false, value_key+states_key_offset, &binaryInput_key },
    { "age", apivalue_double, false, age_key+states_key_offset, &binaryInput_key },
  };
  return &properties[aPropIndex];
}


// access to all fields
bool BinaryInputBehaviour::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&binaryInput_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Description properties
        case hardwareInputType_key+descriptions_key_offset: // aka "hardwareSensorFunction"
          aPropValue->setUint8Value(hardwareInputType);
          return true;
        case inputUsage_key+descriptions_key_offset:
          aPropValue->setUint8Value(inputUsage);
          return true;
        case reportsChanges_key+descriptions_key_offset: // aka "inputType"
          aPropValue->setUint8Value(reportsChanges ? 1 : 0);
          return true;
        case updateInterval_key+descriptions_key_offset:
          aPropValue->setDoubleValue((double)updateInterval/Second);
          return true;
        // Settings properties
        case minPushInterval_key+settings_key_offset:
          aPropValue->setDoubleValue((double)minPushInterval/Second);
          return true;
        case configuredInputType_key+settings_key_offset: // aka "sensorFunction"
          aPropValue->setUint8Value(configuredInputType);
          return true;
        // States properties
        case value_key+states_key_offset:
          // value
          if (lastUpdate==Never)
            aPropValue->setNull();
          else
            aPropValue->setBoolValue(currentState);
          return true;
        case age_key+states_key_offset:
          // age
          if (lastUpdate==Never)
            aPropValue->setNull();
          else
            aPropValue->setDoubleValue(((double)MainLoop::now()-lastUpdate)/Second);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        // Settings properties
        case minPushInterval_key+settings_key_offset:
          minPushInterval = aPropValue->doubleValue()*Second;
          markDirty();
          return true;
        case configuredInputType_key+settings_key_offset: // aka "sensorFunction"
          configuredInputType = (DsBinaryInputType)aPropValue->int32Value();
          markDirty();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}



#pragma mark - description/shortDesc


string BinaryInputBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- binary input type: %d, reportsChanges=%d, interval: %d mS\n", hardwareInputType, reportsChanges, updateInterval/MilliSecond);
  string_format_append(s, "- minimal interval between pushes: %d mS\n", minPushInterval/MilliSecond);
  s.append(inherited::description());
  return s;
}
