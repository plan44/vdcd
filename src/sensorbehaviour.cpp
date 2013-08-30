//
//  sensorbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "sensorbehaviour.hpp"

using namespace p44;

SensorBehaviour::SensorBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  minPushInterval(15*Second),
  // state
  lastUpdate(Never),
  lastPush(Never),
  currentValue(0)
{
  // set dummy default hardware default configuration
  setHardwareSensorConfig(sensorType_none, 0, 100, 1, 15*Second);
  // default to joker
  setGroup(group_black_joker);
}


void SensorBehaviour::setHardwareSensorConfig(DsSensorType aType, double aMin, double aMax, double aResolution, MLMicroSeconds aUpdateInterval)
{
  sensorType = aType;
  min = aMin;
  max = aMax;
  resolution = aResolution;
  updateInterval = aUpdateInterval;
}


void SensorBehaviour::updateEngineeringValue(long aEngineeringValue)
{
  double newCurrentValue = min+(aEngineeringValue*resolution);
  LOG(LOG_INFO,
    "Sensor %s in device %s received engineering value %d = physical units value %0.3f\n",
    hardwareName.c_str(),  device.shortDesc().c_str(), aEngineeringValue, newCurrentValue
  );
  if (newCurrentValue!=currentValue) {
    // changed value
    MLMicroSeconds now = MainLoop::now();
    currentValue = newCurrentValue;
    lastUpdate = now;
    if (lastPush==Never || now>lastPush+minPushInterval) {
      // push the new value
      device.pushProperty("sensorStates", VDC_API_DOMAIN, (int)index);
      lastPush = now;
    }
  }
}


#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *SensorBehaviour::tableName()
{
  return "SensorSettings";
}


// data field definitions

static const size_t numFields = 1;

size_t SensorBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *SensorBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "minPushInterval", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void SensorBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  minPushInterval = aRow->get<int>(aIndex++);
}


// bind values to passed statement
void SensorBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, (int)minPushInterval);
}



#pragma mark - property access

static char sensor_key;

// description properties

enum {
  sensorType_key,
  min_key,
  max_key,
  resolution_key,
  updateInterval_key,
  numDescProperties
};


int SensorBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptor *SensorBehaviour::getDescDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numDescProperties] = {
    { "sensorType", ptype_int8, false, sensorType_key+descriptions_key_offset, &sensor_key },
    { "min", ptype_double, false, min_key+descriptions_key_offset, &sensor_key },
    { "max", ptype_double, false, max_key+descriptions_key_offset, &sensor_key },
    { "resolution", ptype_double, false, resolution_key+descriptions_key_offset, &sensor_key },
    { "updateInterval", ptype_double, false, updateInterval_key+descriptions_key_offset, &sensor_key },
  };
  return &properties[aPropIndex];
}


// settings properties

enum {
  minPushInterval_key,
  numSettingsProperties
};


int SensorBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *SensorBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "minPushInterval", ptype_double, false, minPushInterval_key+settings_key_offset, &sensor_key },
  };
  return &properties[aPropIndex];
}

// state properties

enum {
  value_key,
  age_key,
  numStateProperties
};


int SensorBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptor *SensorBehaviour::getStateDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numStateProperties] = {
    { "value", ptype_double, false, value_key+states_key_offset, &sensor_key },
    { "age", ptype_double, false, age_key+states_key_offset, &sensor_key },
  };
  return &properties[aPropIndex];
}


// access to all fields

bool SensorBehaviour::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&sensor_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Description properties
        case sensorType_key+descriptions_key_offset:
          aPropValue = JsonObject::newInt32(sensorType);
          return true;
        case min_key+descriptions_key_offset:
          aPropValue = JsonObject::newDouble(min);
          return true;
        case max_key+descriptions_key_offset:
          aPropValue = JsonObject::newDouble(max);
          return true;
        case resolution_key+descriptions_key_offset:
          aPropValue = JsonObject::newDouble(resolution);
          return true;
        case updateInterval_key+descriptions_key_offset:
          aPropValue = JsonObject::newDouble((double)updateInterval/Second);
          return true;
        // Settings properties
        case minPushInterval_key+settings_key_offset:
          aPropValue = JsonObject::newDouble((double)minPushInterval/Second);
          return true;
        // States properties
        case value_key+states_key_offset:
          // value
          if (lastUpdate==Never)
            aPropValue = JsonObject::newNull();
          else
            aPropValue = JsonObject::newDouble(currentValue);
          return true;
        case age_key+states_key_offset:
          // age
          if (lastUpdate==Never)
            aPropValue = JsonObject::newNull();
          else
            aPropValue = JsonObject::newDouble(((double)(MainLoop::now()-lastUpdate))/Second);
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
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}



#pragma mark - description/shortDesc


string SensorBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- sensor type: %d, min: %0.1f, max: %0.1f, resulution: %0.3f, interval: %d mS\n", sensorType, min, max, updateInterval/MilliSecond);
  string_format_append(s, "- minimal interval between pushes: %d mS\n", minPushInterval/MilliSecond);
  s.append(inherited::description());
  return s;
}

