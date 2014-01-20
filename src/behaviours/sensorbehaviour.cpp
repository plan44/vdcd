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

#include "sensorbehaviour.hpp"

using namespace p44;

SensorBehaviour::SensorBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  minPushInterval(2*Second), // do not push more often than every 2 seconds
  changesOnlyInterval(0), // report every sensor update
  // state
  lastUpdate(Never),
  lastPush(Never),
  currentValue(0)
{
  // set dummy default hardware default configuration
  setHardwareSensorConfig(sensorType_none, usage_undefined, 0, 100, 1, 15*Second);
  // default to joker
  setGroup(group_black_joker);
}


void SensorBehaviour::setHardwareSensorConfig(DsSensorType aType, DsUsageHint aUsage, double aMin, double aMax, double aResolution, MLMicroSeconds aUpdateInterval)
{
  sensorType = aType;
  sensorUsage = aUsage;
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
  // always update age, even if value itself may not have changed
  MLMicroSeconds now = MainLoop::now();
  lastUpdate = now;
  if (newCurrentValue!=currentValue || now>lastPush+changesOnlyInterval) {
    // changed value or last push with same value long enough ago
    currentValue = newCurrentValue;
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

static const size_t numFields = 2;

size_t SensorBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *SensorBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "minPushInterval", SQLITE_INTEGER },
    { "changesOnlyInterval", SQLITE_INTEGER },
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
  sensorUsage_key,
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
    { "sensorType", apivalue_uint64, false, sensorType_key+descriptions_key_offset, &sensor_key },
    { "sensorUsage", apivalue_uint64, false, sensorUsage_key+descriptions_key_offset, &sensor_key },
    { "min", apivalue_double, false, min_key+descriptions_key_offset, &sensor_key },
    { "max", apivalue_double, false, max_key+descriptions_key_offset, &sensor_key },
    { "resolution", apivalue_double, false, resolution_key+descriptions_key_offset, &sensor_key },
    { "updateInterval", apivalue_double, false, updateInterval_key+descriptions_key_offset, &sensor_key },
  };
  return &properties[aPropIndex];
}


// settings properties

enum {
  minPushInterval_key,
  changesOnlyInterval_key,
  numSettingsProperties
};


int SensorBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *SensorBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "minPushInterval", apivalue_double, false, minPushInterval_key+settings_key_offset, &sensor_key },
    { "changesOnlyInterval", apivalue_double, false, changesOnlyInterval_key+settings_key_offset, &sensor_key },
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
    { "value", apivalue_double, false, value_key+states_key_offset, &sensor_key },
    { "age", apivalue_double, false, age_key+states_key_offset, &sensor_key },
  };
  return &properties[aPropIndex];
}


// access to all fields

bool SensorBehaviour::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&sensor_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Description properties
        case sensorType_key+descriptions_key_offset:
          aPropValue->setUint16Value(sensorType);
          return true;
        case sensorUsage_key+descriptions_key_offset:
          aPropValue->setUint16Value(sensorUsage);
          return true;
        case min_key+descriptions_key_offset:
          aPropValue->setDoubleValue(min);
          return true;
        case max_key+descriptions_key_offset:
          aPropValue->setDoubleValue(max);
          return true;
        case resolution_key+descriptions_key_offset:
          aPropValue->setDoubleValue(resolution);
          return true;
        case updateInterval_key+descriptions_key_offset:
          aPropValue->setDoubleValue((double)updateInterval/Second);
          return true;
        // Settings properties
        case minPushInterval_key+settings_key_offset:
          aPropValue->setDoubleValue((double)minPushInterval/Second);
          return true;
        case changesOnlyInterval_key+settings_key_offset:
          aPropValue->setDoubleValue((double)changesOnlyInterval/Second);
          return true;
        // States properties
        case value_key+states_key_offset:
          // value
          if (lastUpdate==Never)
            aPropValue->setNull();
          else
            aPropValue->setDoubleValue(currentValue);
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
        case changesOnlyInterval_key+settings_key_offset:
          changesOnlyInterval = aPropValue->doubleValue()*Second;
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

