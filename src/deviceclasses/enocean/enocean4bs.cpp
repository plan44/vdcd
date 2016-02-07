//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "enocean4bs.hpp"

#include "enoceandevicecontainer.hpp"

#include "sensorbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "outputbehaviour.hpp"
#include "climatecontrolbehaviour.hpp"

using namespace p44;


#pragma mark - special extraction functions

/// two-range illumination handler, as used in A5-06-01 and A5-06-02
static void illumHandler(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize)
{
  uint16_t value;
  // actual data comes in:
  //  DB(0,0)==0 -> in DB(2), real 9-bit value is DB(2)
  //  DB(0,0)==1 -> in DB(1), real 9-bit value is DB(1)*2
  if (aDataSize<4) return;
  if (aDataP[3-0] & 0x01) {
    // DB(0,0)==1: DB 2 contains low range / higher resolution
    value = aDataP[3-2]; // use as 9 bit value as-is
  }
  else {
    // DB(0,0)==0: DB 1 contains high range / lower resolution
    value = aDataP[3-1]<<1; // multiply by 2 to use as 9 bit value
  }
  if (SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(aBehaviour)) {
    sb->updateEngineeringValue(value);
  }
}

/// power meter data extraction handler
static void powerMeterHandler(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize)
{
  // raw value is in DB3.7..DB1.0 (upper 24 bits)
  uint32_t value =
  (aDataP[0]<<16) +
  (aDataP[1]<<8) +
  aDataP[2];
  // scaling is in bits DB0.1 and DB0.0 : 00=scale1, 01=scale10, 10=scale100, 11=scale1000
  int divisor = 1;
  switch (aDataP[3] & 0x03) {
    case 1: divisor = 10; break; // value scale is 0.1kWh or 0.1W per LSB
    case 2: divisor = 100; break; // value scale is 0.01kWh or 0.01W per LSB
    case 3: divisor = 1000; break; // value scale is 0.001kWh (1Wh) or 0.001W (1mW) per LSB
  }
  if (SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(aBehaviour)) {
    // DB0.2 signals which value it is: 0=cumulative (energy), 1=current value (power)
    if (aDataP[3] & 0x04) {
      // power
      if (sb->getSensorType()==sensorType_power) {
        // we're being called for power, and data is power -> update
        sb->updateSensorValue((double)value/divisor);
      }
    }
    else {
      // energy
      if (sb->getSensorType()==sensorType_energy) {
        // we're being called for energy, and data is energy -> update
        sb->updateSensorValue((double)value/divisor);
      }
    }
  }
}


/// strange irregular fan speed scale as used in A5-10-01,02,04,07,08 and 09
static void fanSpeedHandler(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize)
{
  // extract 8-bit value
  if (SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(aBehaviour)) {
    uint8_t value = EnoceanSensors::bitsExtractor(aSensorDescriptor, aDataP, aDataSize);
    // 255..210 = Auto
    // 209..190 = Speed 0 / OFF
    // 189..165 = Speed 1
    // 164..145 = Speed 2
    // 144..0 = Speed 3 = full speed
    double fanSpeed;
    if (value>=210) fanSpeed = -1; // auto (at full speed, i.e. not limited to lower stage)
    else {
      // get stage
      if (value>=190) fanSpeed = 0; // off
      else if (value>=165) fanSpeed = 1;
      else if (value>=145) fanSpeed = 2;
      else fanSpeed = 3;
      // scale to 0..1
      fanSpeed = fanSpeed/3;
    }
    sb->updateSensorValue(fanSpeed);
  }
}



#pragma mark - sensor mapping table for generic EnoceanSensorHandler

using namespace EnoceanSensors;

const p44::EnoceanSensorDescriptor enocean4BSdescriptors[] = {
  // variant,func,type, SD,primarygroup,  channelGroup,                  behaviourType,         behaviourParam,         usage,              min,  max,MSB,     LSB,  updateIv,aliveSignIv, handler,     typeText, unitText
  // A5-02-xx: Temperature sensors
  // - 40 degree range                 behaviour_binaryinput
  { 0, 0x02, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,    0, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },

  { 0, 0x02, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -30,   10, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x03, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -20,   20, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x04, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10,   30, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x05, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    10,   50, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x07, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    20,   60, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x08, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    30,   70, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x09, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    40,   80, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x0A, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    50,   90, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x0B, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    60,  100, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  // - 80 degree range
  { 0, 0x02, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -60,   20, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -50,   30, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x12, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x13, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -30,   50, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x14, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -20,   60, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x15, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10,   70, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x16, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   80, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x17, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    10,   90, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x18, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    20,  100, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x19, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    30,  110, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x1A, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    40,  120, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x1B, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    50,  130, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  // - 10 bit
  { 0, 0x02, 0x20, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10, 41.2, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0, 0x02, 0x30, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40, 62.3, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  // A5-04-xx: Temperature and Humidity
  // - 0..40 degree indoor, e.g. Alpha Sense
  { 0, 0x04, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler,  tempText, tempUnit },
  { 0, 0x04, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler,  humText,  humUnit  },
  // -20..60 degree outdoor, e.g. Alpha Sense
  { 0, 0x04, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_outdoors,    -20, 61.6, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler,  tempText, tempUnit },
  { 0, 0x04, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_outdoors,      0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler,  humText,  humUnit  },

  // A5-06-xx: Light Sensor
  { 0, 0x06, 0x01, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_outdoors,    600,60000, DB(2,0), DB(1,0), 100, 40*60, &illumHandler,     illumText, illumUnit },
  { 0, 0x06, 0x02, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1024, DB(2,0), DB(1,0), 100, 40*60, &illumHandler,     illumText, illumUnit },
  { 0, 0x06, 0x03, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1024, DB(2,7), DB(1,6), 100, 40*60, &stdSensorHandler, illumText, illumUnit },

  // A5-07-xx: Occupancy Sensor
  // - two slightly different occupancy sensors
  { 0, 0x07, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          0,    1, DB(1,7), DB(1,7), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  { 0, 0x07, 0x02, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          0,    1, DB(0,7), DB(0,7), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  // - occupancy sensor with illumination sensor
  { 0, 0x07, 0x03, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          0,    1, DB(0,7), DB(0,7), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  { 0, 0x07, 0x03, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1024, DB(2,7), DB(1,6), 100, 40*60, &stdSensorHandler, illumText, illumUnit },

  // A5-08-01: Light, Temperature and Occupancy sensor
  // - e.g. Eltako FBH
  { 0, 0x08, 0x01, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0,  510, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, illumText, illumUnit },
  { 0, 0x08, 0x01, 0, group_black_joker,  group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   51, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x08, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          1,    0, DB(0,1), DB(0,1), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  { 0, 0x08, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },


  // A5-10-01: Room Control Panel with Temperature Sensor, Set Point, Fan Speed and Occupancy button
  // Note: fan speed negative range denotes "automatic" (210..255 -> -0.215311..-0)
  // - e.g. Siemens QAX95.4..98.4, Thermokon SR06 LCD 4T type 2
  { 0, 0x10, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,         -1,    1, DB(3,7), DB(3,0), 100, 40*60, &fanSpeedHandler,  fanSpeedText, unityUnit },
  { 0, 0x10, 0x01, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-02: Room Control Panel with Temperature Sensor, Set Point, Fan Speed and Day/Night Control
  // - e.g. Thermokon Thanos
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,         -1,    1, DB(3,7), DB(3,0), 100, 40*60, &fanSpeedHandler, fanSpeedText, unityUnit },
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-03: Room Control Panel with Temperature Sensor and Set Point Control
  // - e.g. Eltako FTR78S, Thermokon SR06 LCD 2T
  { 0, 0x10, 0x03, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x03, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },

  // A5-10-04: Room Control Panel with Temperature Sensor, Set Point, Fan Speed
  // - e.g. Thermokon SR06 LCD 4T type 1
  { 0, 0x10, 0x04, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x04, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x04, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,         -1,    1, DB(3,7), DB(3,0), 100, 40*60, &fanSpeedHandler,  fanSpeedText, unityUnit },

  // A5-10-05: Room Control Panel with Temperature Sensor, Set Point and Occupancy button
  // - e.g. Siemens QAX95.4..98.4, Thermokon SR06 LCD 4T type 3
  { 0, 0x10, 0x05, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x05, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x05, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-06: Room Panel with Temperature Sensor, Set Point Control, Day/Night Control
  { 0, 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },
  // A5-10-06: Variant with Set Point Control as temperature scaled 0..40 degrees
  // - e.g. Eltako FTR55D
  { 1, 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 1, 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_user,          0,   40, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, tempSetPt, tempUnit },
  { 1, 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-07: Room Control Panel with Temperature Sensor, Fan Speed
  { 0, 0x10, 0x07, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x07, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,         -1,    1, DB(3,7), DB(3,0), 100, 40*60, &fanSpeedHandler,  fanSpeedText, unityUnit },

  // A5-10-08: Room Control Panel with Temperature Sensor, Fan Speed and Occupancy button
  { 0, 0x10, 0x08, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x08, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,         -1,    1, DB(3,7), DB(3,0), 100, 40*60, &fanSpeedHandler,  fanSpeedText, unityUnit },
  { 0, 0x10, 0x08, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-09: Room Control Panel with Temperature Sensor, Fan Speed and day/night control
  { 0, 0x10, 0x09, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x09, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,         -1,    1, DB(3,7), DB(3,0), 100, 40*60, &fanSpeedHandler,  fanSpeedText, unityUnit },
  { 0, 0x10, 0x09, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-0A: Room Control Panel with Temperature Sensor, Set Point and single contact
  { 0, 0x10, 0x0A, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x0A, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x0A, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_none,        usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  contactText, binaryUnit },

  // A5-10-0B: Temperature Sensor and single contact
  { 0, 0x10, 0x0B, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x0B, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_none,        usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  contactText, binaryUnit },

  // A5-10-0C: Temperature Sensor and Occupancy button
  { 0, 0x10, 0x0C, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x0C, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-0D: Temperature Sensor and day/night control
  { 0, 0x10, 0x0D, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x0D, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },


  // A5-10-10: Room Control Panel with Temperature Sensor, Set Point, Humidity and Occupancy button
  // - e.g. Thermokon SR06 LCD 4T rh type 3
  { 0, 0x10, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x10, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-11: Room Panel with Temperature Sensor, Set Point Control, Humidity and day/night control
  // - e.g. Thermokon Thanos
  { 0, 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-12: Room Panel with Temperature Sensor, Set Point Control, Humidity
  // - e.g. Thermokon SR06 LCD 2T rh
  { 0, 0x10, 0x12, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x12, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x12, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },

  // A5-10-13: Room Panel with Temperature Sensor, Humidity and day/night control
  { 0, 0x10, 0x13, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x13, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x13, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-14: Room Panel with Temperature Sensor, Humidity and day/night control
  { 0, 0x10, 0x14, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x14, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x14, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-15: Room Panel with 10 bit Temperature Sensor, 6 bit set point
  { 0, 0x10, 0x15, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10, 41.2, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x15, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,2), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },

  // A5-10-16: Room Panel with 10 bit Temperature Sensor, 6 bit set point and Occupancy button
  { 0, 0x10, 0x16, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10, 41.2, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x16, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,2), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x16, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-17: Room Panel with 10 bit Temperature Sensor and Occupancy button
  { 0, 0x10, 0x17, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10, 41.2, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x17, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-18..1F seem quite exotic, and Occupancy enable/button bits are curiously swapped in A5-10-19 compared to all other similar profiles (typo or real?)
  //  // INCOMPLETE: A5-10-18: Room Panel with Temperature Sensor, Temperature set point, fan speed and Occupancy button and disable
  //  { 0, 0x10, 0x18, 0, group_blue_heating, group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1020, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, illumText, illumUnit },
  //  { 0, 0x10, 0x18, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_user,          0,   40, DB(2,7), DB(2,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  //  { 0, 0x10, 0x18, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempSetPt, tempUnit },

  // A5-10-20 and A5-10-21 (by MSR/Viessmann) are currently too exotic as well, so left off for now

  // A5-10-22: Room Panel with Temperature Sensor, Humitity, Set Point and Fan control
  // - e.g. Thermokon SR06 LCD 4T rh type 1
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,  -0.333333,    2, DB(0,7), DB(0,5), 100, 40*60, &stdSensorHandler, fanSpeedText, unityUnit },

  // A5-10-23: Room Panel with Temperature Sensor, Humitity, Set Point, Fan control and Occupancy button
  // - e.g. Thermokon SR06 LCD 4T rh type 2
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,  -0.333333,    2, DB(0,7), DB(0,5), 100, 40*60, &stdSensorHandler, fanSpeedText, unityUnit },
  { 0, 0x10, 0x13, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },


  // A5-12-01: Energy meter
  // - e.g. Eltako FWZ12-16A
  { 0, 0x12, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_sensor,      sensorType_power,       usage_room,          0, 2500, DB(3,7), DB(1,0), 600, 40*60, &powerMeterHandler, "Power", "W" },
  { 0, 0x12, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_sensor,      sensorType_energy,      usage_room,          0, 16e9, DB(3,7), DB(1,0), 600, 40*60, &powerMeterHandler, "Energy", "kWh" },

  // terminator
  { 0, 0,    0,    0, group_black_joker,  group_black_joker,             behaviour_undefined, 0, usage_undefined, 0, 0, 0, 0, 0, 0, NULL /* NULL for extractor function terminates list */, NULL, NULL },
};



#pragma mark - Enocean4BSDevice


Enocean4BSDevice::Enocean4BSDevice(EnoceanDeviceContainer *aClassContainerP) :
  inherited(aClassContainerP)
{
}


static const ProfileVariantEntry profileVariants4BS[] = {
  // dual rocker RPS button alternatives
  { 1, 0x00A52001, 0, "heating valve" },
  { 1, 0x01A52001, 0, "heating valve (with temperature sensor)" },
  { 1, 0x02A52001, 0, "heating valve with binary output adjustment (e.g. MD10-FTL)" },
  { 2, 0x00A51006, 0, "standard profile" },
  { 2, 0x01A51006, 0, "set point interpreted as 0..40°C (e.g. FTR55D)" },
  { 0, 0, 0, NULL } // terminator
};


const ProfileVariantEntry *Enocean4BSDevice::profileVariantsTable()
{
  return profileVariants4BS;
}



void Enocean4BSDevice::sendTeachInResponse()
{
  Esp3PacketPtr responsePacket = Esp3PacketPtr(new Esp3Packet);
  responsePacket->initForRorg(rorg_4BS);
  // TODO: implement other 4BS teach-in variants
  if (EEP_FUNC(getEEProfile())==0x20) {
    // A5-20-xx, just mirror back the learn request's EEP
    responsePacket->set4BSTeachInEEP(getEEProfile());
    // Note: manufacturer not set for now (is 0)
    // Set learn response flags
    //               D[3]
    //   7   6   5   4   3   2   1   0
    //
    //  LRN EEP LRN LRN LRN  x   x   x
    //  typ res res sta bit
    responsePacket->radioUserData()[3] =
      (1<<7) | // LRN type = 1=with EEP
      (1<<6) | // 1=EEP is supported
      (1<<5) | // 1=sender ID stored
      (1<<4) | // 1=is LRN response
      (0<<3); // 0=is LRN packet
    // set destination
    responsePacket->setRadioDestination(getAddress());
    // now send
    LOG(LOG_INFO, "Sending 4BS teach-in response for EEP %06X", EEP_PURE(getEEProfile()));
    getEnoceanDeviceContainer().enoceanComm.sendCommand(responsePacket, NULL);
  }
}



// static device creator function
EnoceanDevicePtr create4BSDeviceFunc(EnoceanDeviceContainer *aClassContainerP)
{
  return EnoceanDevicePtr(new Enocean4BSDevice(aClassContainerP));
}


// static factory method
EnoceanDevicePtr Enocean4BSDevice::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice &aSubDeviceIndex,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  EnoceanDevicePtr newDev; // none so far
  // check for specialized handlers for certain profiles first
  if (EEP_PURE(aEEProfile)==0xA52001) {
    // Note: Profile has variants (with and without temperature sensor)
    // use specialized handler for output functions of heating valve (valve value, summer/winter, prophylaxis)
    newDev = EnoceanA52001Handler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
  }
  else if (aEEProfile==0xA51301) {
    // use specialized handler for multi-telegram sensor
    newDev = EnoceanA5130XHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
  }
  else {
    // check table based sensors, might create more than one device
    newDev = EnoceanSensorHandler::newDevice(aClassContainerP, create4BSDeviceFunc, enocean4BSdescriptors, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
  }
  return newDev;
}


void Enocean4BSDevice::prepare4BSpacket(Esp3PacketPtr &aOutgoingPacket, uint32_t &a4BSdata)
{
  if (!aOutgoingPacket) {
    aOutgoingPacket = Esp3PacketPtr(new Esp3Packet());
    aOutgoingPacket->initForRorg(rorg_4BS);
    // new packet, start with zero data except for LRN bit (D0.3) which must be set for ALL non-learn data
    a4BSdata = LRN_BIT_MASK;
  }
  else {
    // packet exists, get already collected data to modify
    a4BSdata = aOutgoingPacket->get4BSdata();
  }
}






#pragma mark - EnoceanA52001Handler


EnoceanA52001Handler::EnoceanA52001Handler(EnoceanDevice &aDevice) :
  inherited(aDevice),
  serviceState(service_idle),
  lastActualValvePos(50), // assume centered
  lastRequestedValvePos(50) // assume centered
{
}


// static factory method
EnoceanDevicePtr EnoceanA52001Handler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice &aSubDeviceIndex, // current subdeviceindex, factory returns NULL when no device can be created for this subdevice index
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  // A5-20-01: heating valve actuator
  // - e.g. thermokon SAB 02 or Kieback+Peter MD15-FTL, MD10-FTL
  // configuration for included sensor channels
  static const p44::EnoceanSensorDescriptor tempSensor =
    { 0, 0x20, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room, 0, 40, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText,      tempUnit };
  static const p44::EnoceanSensorDescriptor lowBatInput =
    { 0, 0x20, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_lowBattery,  usage_room, 1,  0, DB(2,4), DB(2,4), 100, 40*60, &stdInputHandler,  "Low Battery", binaryUnit };
  // create device
  EnoceanDevicePtr newDev; // none so far
  if (aSubDeviceIndex<1) {
    // only one device
    newDev = EnoceanDevicePtr(new Enocean4BSDevice(aClassContainerP));
    // valve needs climate control scene table (ClimateControlScene)
    newDev->installSettings(DeviceSettingsPtr(new ClimateDeviceSettings(*newDev)));
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
    // assign EPP information
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
    // is heating
    newDev->setPrimaryGroup(group_blue_heating);
    // function
    newDev->setFunctionDesc("heating valve actuator");
    // climate control output, use special behaviour (with has already set its specific default group membership)
    ClimateControlBehaviourPtr cb = ClimateControlBehaviourPtr(new ClimateControlBehaviour(*newDev.get()));
    cb->setHardwareOutputConfig(outputFunction_positional, outputmode_gradual, usage_room, false, 0);
    cb->setHardwareName("valve");
    // - create A5-20-01 specific handler for output
    EnoceanChannelHandlerPtr newHandler = EnoceanChannelHandlerPtr(new EnoceanA52001Handler(*newDev.get()));
    newHandler->behaviour = cb;
    newDev->addChannelHandler(newHandler);
    if (EEP_VARIANT(aEEProfile)!=0) {
      // profile variants with valve sensor enabled - add built-in temp sensor
      EnoceanSensorHandler::addSensorChannel(newDev, tempSensor, false);
    }
    // report low bat status as a binary input
    EnoceanSensorHandler::addSensorChannel(newDev, lowBatInput, false);
    // A5-20-01 need teach-in response if requested (i.e. if this device creation is caused by learn-in, not reinstantiation from DB)
    if (aSendTeachInResponse) {
      newDev->sendTeachInResponse();
    }
    newDev->setUpdateAtEveryReceive();
    // count it
    aSubDeviceIndex++;
  }
  // return device (or empty if none created)
  return newDev;
}



// handle incoming data from device and extract data for this channel
void EnoceanA52001Handler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->radioHasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eepRorg()==rorg_4BS && aEsp3PacketPtr->radioUserDataLength()==4) {
      // only look at 4BS packets of correct length
      // sensor inputs will be checked by separate handlers, check error bits only, most fatal first
      // - check actuator obstructed
      uint32_t data = aEsp3PacketPtr->get4BSdata();
      if ((data & DBMASK(2,0))!=0) {
        HLOG(LOG_ERR, "EnOcean valve error: actuator obstructed");
        behaviour->setHardwareError(hardwareError_overload);
      }
      else if ((data & DBMASK(2,4))==0 && (data & DBMASK(2,5))==0) {
        HLOG(LOG_ERR, "EnOcean valve error: energy storage AND battery are low");
        behaviour->setHardwareError(hardwareError_lowBattery);
      }
      // show general status
      HLOG(LOG_NOTICE,
        "EnOcean valve actual set point: %d%% open\n"
        "- Service %s, Energy input %s, Energy storage %scharged, Battery %s, Cover %s, Sensor %s, Detected window %s, Actuator %s",
        (data>>DB(3,0)) & 0xFF, // get data from DB(3,0..7), range is 0..100% (NOT 0..255!)
        data & DBMASK(2,7) ? "ON" : "off",
        data & DBMASK(2,6) ? "enabled" : "disabled",
        data & DBMASK(2,5) ? "" : "NOT ",
        data & DBMASK(2,4) ? "ok" : "LOW",
        data & DBMASK(2,3) ? "OPEN" : "closed",
        data & DBMASK(2,2) ? "FAILURE" : "ok",
        data & DBMASK(2,1) ? "open" : "closed",
        data & DBMASK(2,0) ? "OBSTRUCTED" : "ok"
      );
    }
  }
}



/// collect data for outgoing message from this channel
/// @param aEsp3PacketPtr must be set to a suitable packet if it is empty, or packet data must be augmented with
///   channel's data when packet already exists
/// @note non-outputs will do nothing in this method
void EnoceanA52001Handler::collectOutgoingMessageData(Esp3PacketPtr &aEsp3PacketPtr)
{
  ClimateControlBehaviourPtr cb = boost::dynamic_pointer_cast<ClimateControlBehaviour>(behaviour);
  if (cb) {
    // get the right channel
    ChannelBehaviourPtr ch = cb->getChannelByIndex(dsChannelIndex);
    // prepare 4BS packet (create packet if none created already)
    uint32_t data;
    Enocean4BSDevice::prepare4BSpacket(aEsp3PacketPtr, data);
    // check for pending service cycle
    if (cb->shouldRunProphylaxis() && serviceState==service_idle) {
      // needs to initiate a prophylaxis cycle (only if not already one running)
      serviceState = service_openvalve; // first fully open
    }
    if (serviceState!=service_idle) {
      // process pending service steps
      // - DB(1,0) set to 1 = normal operation (not service)
      data |= DBMASK(1,0); // service on
      if (serviceState==service_openvalve) {
        // trigger force full open
        LOG(LOG_NOTICE, "- EnOcean valve prophylaxis operation: fully opening valve");
        data |= DBMASK(1,5); // service: open
        // next is closing
        serviceState = service_closevalve;
        device.needOutgoingUpdate();
      }
      else if (serviceState==service_closevalve) {
        // trigger force fully closed
        LOG(LOG_NOTICE, "- EnOcean valve prophylaxis operation: fully closing valve");
        data |= DBMASK(1,4); // service: close
        // next is normal operation again
        serviceState = service_idle;
        device.needOutgoingUpdate();
      }
    }
    else {
      // Normal operation
      // - DB(1,0) left 0 = normal operation (not service)
      // - DB(1,1) left 0 = no inverted set value
      // - DB(1,2) left 0 = sending valve position
      // - DB(3,7)..DB(3,0) is valve position 0..100% (0..255 is only for temperature set point mode!)
      // Note: value is always positive even for cooling, because climateControlBehaviour checks outputfunction and sees this is a unipolar valve
      int8_t newValue = cb->outputValueAccordingToMode(ch->getChannelValue(), ch->getChannelIndex());
      // Still: limit to 0..100 to make sure
      if (newValue<0) newValue = 0;
      else if (newValue>100) newValue=100;
      // Special transformation in case valve is binary
      if (EEP_VARIANT(device.getEEProfile())==2) {
        // this valve can only adjust output by about 4k around the mechanically preset set point
        if (newValue>lastRequestedValvePos) {
          // increase -> open to at least 51%
          LOG(LOG_NOTICE, "- Binary valve: requested set point has increased from %d%% to %d%% -> open to 51%% or more", lastRequestedValvePos, newValue);
          lastRequestedValvePos = newValue;
          if (newValue<=50) newValue = 51;
        }
        else if (newValue<lastRequestedValvePos) {
          // decrease -> close to at least 49%
          LOG(LOG_NOTICE, "- Binary valve: requested set point has decreased from %d%% to %d%% -> close to 49%% or less", lastRequestedValvePos, newValue);
          lastRequestedValvePos = newValue;
          if (newValue>=50) newValue = 49;
        }
        else {
          // no change, just repeat last valve position
          LOG(LOG_NOTICE, "- Binary valve: requested set point has not changed (%d%%) -> send last actual value (%d%%) again", lastRequestedValvePos, lastActualValvePos);
          newValue = lastActualValvePos;
        }
      }
      // remember last actually transmitted value
      lastActualValvePos = newValue;
      // - DB3 is set point with range 0..100 (0..255 is only for temperature set point)
      data |= (newValue<<DB(3,0)); // insert data into DB(3,0..7)
      // - DB(1,3) is summer mode
      LOG(LOG_NOTICE, "- EnOcean valve, requesting new set point: %d%% open", newValue);
      if (cb->isClimateControlIdle()) {
        data |= DBMASK(1,3);
        LOG(LOG_NOTICE, "- valve is in IDLE mode (slow updates)");
      }
    }
    // save data
    aEsp3PacketPtr->set4BSdata(data);
    // value from this channel is applied to the outgoing telegram
    ch->channelValueApplied(true); // applied even if channel did not have needsApplying() status before
  }
}



string EnoceanA52001Handler::shortDesc()
{
  return string_format("valve output, 0..100 %%");
}



#pragma mark - EnoceanA5130XHandler

// configuration for A5-13-0X sensor channels
// - A5-13-01 telegram
static const p44::EnoceanSensorDescriptor A513dawnSensor =
  { 0, 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 999, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, illumText, illumUnit };
static const p44::EnoceanSensorDescriptor A513outdoorTemp =
  { 0, 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_temperature, usage_outdoors, -40, 80, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit };
static const p44::EnoceanSensorDescriptor A513windSpeed =
  { 0, 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_wind_speed, usage_outdoors, 0, 70, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, "wind speed", "m/s" };
static const p44::EnoceanSensorDescriptor A513dayIndicator =
  { 0, 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_binaryinput, binInpType_none,  usage_outdoors, 1,  0, DB(0,2), DB(0,2), 100, 40*60, &stdInputHandler,  "Day indicator", binaryUnit };
static const p44::EnoceanSensorDescriptor A513rainIndicator =
  { 0, 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_binaryinput, binInpType_rain,  usage_outdoors, 0,  1, DB(0,1), DB(0,1), 100, 40*60, &stdInputHandler,  "Rain indicator", binaryUnit };
// - A5-13-02 telegram
static const p44::EnoceanSensorDescriptor A513sunWest =
  { 0, 0x13, 0x02, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 150000, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, "Sun west", illumUnit };
static const p44::EnoceanSensorDescriptor A513sunSouth =
  { 0, 0x13, 0x02, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 150000, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, "Sun south", illumUnit };
static const p44::EnoceanSensorDescriptor A513sunEast =
  { 0, 0x13, 0x02, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 150000, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, "Sun east", illumUnit };

EnoceanA5130XHandler::EnoceanA5130XHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}


// static factory method
EnoceanDevicePtr EnoceanA5130XHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice &aSubDeviceIndex, // current subdeviceindex, factory returns NULL when no device can be created for this subdevice index
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  // A5-13-01..06 (actually used 01,02): environmental sensor
  // - e.g. Eltako Multisensor MS with FWS61
  // create device
  EnoceanDevicePtr newDev; // none so far
  if (aSubDeviceIndex<1) {
    // only one device
    newDev = EnoceanDevicePtr(new Enocean4BSDevice(aClassContainerP));
    // sensor only, standard settings without scene table
    newDev->installSettings();
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
    // assign EPP information
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
    // is joker (AKM type)
    newDev->setPrimaryGroup(group_black_joker);
    // function
    newDev->setFunctionDesc("environmental multisensor");
    // - create A5-13-0X specific handler (which handles all sensors)
    EnoceanA5130XHandlerPtr newHandler = EnoceanA5130XHandlerPtr(new EnoceanA5130XHandler(*newDev.get()));
    // - Add channel-built-in behaviour
    newHandler->behaviour = EnoceanSensorHandler::newSensorBehaviour(A513dawnSensor, newDev);
    // - register the handler and the default behaviour
    newDev->addChannelHandler(newHandler);
    // - Add extra behaviours for A5-13-01
    newHandler->outdoorTemp = EnoceanSensorHandler::newSensorBehaviour(A513outdoorTemp, newDev);
    newDev->addBehaviour(newHandler->outdoorTemp);
    newHandler->windSpeed = EnoceanSensorHandler::newSensorBehaviour(A513windSpeed, newDev);
    newDev->addBehaviour(newHandler->windSpeed);
    newHandler->dayIndicator = EnoceanSensorHandler::newSensorBehaviour(A513dayIndicator, newDev);
    newDev->addBehaviour(newHandler->dayIndicator);
    newHandler->rainIndicator = EnoceanSensorHandler::newSensorBehaviour(A513rainIndicator, newDev);
    newDev->addBehaviour(newHandler->rainIndicator);
    // - Add extra behaviours for A5-13-02
    newHandler->sunWest = EnoceanSensorHandler::newSensorBehaviour(A513sunWest, newDev);
    newDev->addBehaviour(newHandler->sunWest);
    newHandler->sunSouth = EnoceanSensorHandler::newSensorBehaviour(A513sunSouth, newDev);
    newDev->addBehaviour(newHandler->sunSouth);
    newHandler->sunEast = EnoceanSensorHandler::newSensorBehaviour(A513sunEast, newDev);
    newDev->addBehaviour(newHandler->sunEast);
    // count it
    aSubDeviceIndex++;
  }
  // return device (or empty if none created)
  return newDev;
}



// handle incoming data from device and extract data for this channel
void EnoceanA5130XHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->radioHasTeachInfo()) {
    // only look at non-teach-in packets
    uint8_t *dataP = aEsp3PacketPtr->radioUserData();
    int datasize = (int)aEsp3PacketPtr->radioUserDataLength();
    if (datasize!=4) return; // wrong data size
    // - check identifier in DB0.7..DB0.4 to see what info we got
    uint8_t identifier = (dataP[3]>>4) & 0x0F;
    switch (identifier) {
      case 1:
        // A5-13-01
        handleBitField(A513dawnSensor, behaviour, dataP, datasize);
        handleBitField(A513outdoorTemp, outdoorTemp, dataP, datasize);
        handleBitField(A513windSpeed, windSpeed, dataP, datasize);
        handleBitField(A513dayIndicator, dayIndicator, dataP, datasize);
        handleBitField(A513rainIndicator, rainIndicator, dataP, datasize);
        break;
      case 2:
        // A5-13-02
        handleBitField(A513sunWest, sunWest, dataP, datasize);
        handleBitField(A513sunSouth, sunSouth, dataP, datasize);
        handleBitField(A513sunEast, sunEast, dataP, datasize);
        break;
      default:
        // A5-13-03..06 are not supported
        break;
    }
  }
}



string EnoceanA5130XHandler::shortDesc()
{
  return string_format("Dawn/Temp/Wind/Rain/Sun outdoor sensor");
}







