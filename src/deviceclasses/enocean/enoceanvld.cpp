//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "enoceanvld.hpp"

#include "enoceandevicecontainer.hpp"


using namespace p44;


#pragma mark - VLD device specifications


using namespace EnoceanSensors;


#warning "%%% no real profiles yet"
const p44::EnoceanSensorDescriptor enoceanVLDdescriptors[] = {
  // variant,func,type, SD,primarygroup,  channelGroup,                  behaviourType,         behaviourParam,         usage,              min,  max,MSB,     LSB,  updateIv,aliveSignIv, handler,     typeText, unitText
  // A5-02-xx: Temperature sensors
  // - 40 degree range                 behaviour_binaryinput
  { 0, 0x02, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,    0, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },

  // A5-10-02: Room Control Panel with Temperature Sensor, Set Point, Fan Speed and Day/Night Control
  // - e.g. Thermokon Thanos
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,  -0.215311,    1, DB(3,7), DB(3,0), 100, 40*60, &invSensorHandler, fanSpeedText, unityUnit },
  { 0, 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-05: Room Control Panel with Temperature Sensor, Set Point and Occupancy button
  // - e.g. Siemens QAX95.4..98.4, Thermokon SR06 LCD 4T type 3
  { 0, 0x10, 0x05, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x05, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x05, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-0B: Temperature Sensor and single contact
  { 0, 0x10, 0x0B, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x0B, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_none,        usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  contactText, binaryUnit },

  // A5-10-10: Room Control Panel with Temperature Sensor, Set Point, Humidity and Occupancy button
  // - e.g. Thermokon SR06 LCD 4T rh type 3
  { 0, 0x10, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x10, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-23: Room Panel with Temperature Sensor, Humitity, Set Point, Fan control and Occupancy button
  // - e.g. Thermokon SR06 LCD 4T rh type 2
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0, 0x10, 0x22, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,  -0.333333,    2, DB(0,7), DB(0,5), 100, 40*60, &stdSensorHandler, fanSpeedText, unityUnit },
  { 0, 0x10, 0x13, 0, group_blue_heating, group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // terminator
  { 0, 0,    0,    0, group_black_joker,  group_black_joker,             behaviour_undefined, 0, usage_undefined, 0, 0, 0, 0, 0, 0, NULL /* NULL for extractor function terminates list */, NULL, NULL },
};




#pragma mark - EnoceanVLDDevice


EnoceanVLDDevice::EnoceanVLDDevice(EnoceanDeviceContainer *aClassContainerP) :
  inherited(aClassContainerP)
{
}


// static device creator function
EnoceanDevicePtr createVLDDeviceFunc(EnoceanDeviceContainer *aClassContainerP)
{
  return EnoceanDevicePtr(new EnoceanVLDDevice(aClassContainerP));
}


// static factory method
EnoceanDevicePtr EnoceanVLDDevice::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice &aSubDeviceIndex,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  EnoceanDevicePtr newDev; // none so far
  // check for specialized handlers for certain profiles first
//  if (EEP_PURE(aEEProfile)==0xA52001) {
//    // Note: Profile has variants (with and without temperature sensor)
//    // use specialized handler for output functions of heating valve (valve value, summer/winter, prophylaxis)
//    newDev = EnoceanA52001Handler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
//  }
//  else
  {
    // check table based sensors, might create more than one device
#warning "%%% no real profiles yet"
//    newDev = EnoceanSensorHandler::newDevice(aClassContainerP, createVLDDeviceFunc, enoceanVLDdescriptors, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
  }
  return newDev;
}




#pragma mark - EnoceanVLDDevice profile variants


//static const ProfileVariantEntry profileVariantsVLD[] = {
//  // dual rocker RPS button alternatives
//  { 1, 0x00A52001, 0, "heating valve" },
//  { 1, 0x01A52001, 0, "heating valve (with temperature sensor)" },
//  { 1, 0x02A52001, 0, "heating valve with binary output adjustment (e.g. MD10-FTL)" },
//  { 2, 0x00A51006, 0, "standard profile" },
//  { 2, 0x01A51006, 0, "set point interpreted as 0..40°C (e.g. FTR55D)" },
//  { 0, 0, 0, NULL } // terminator
//};


const ProfileVariantEntry *EnoceanVLDDevice::profileVariantsTable()
{
  return NULL; // none for now
  // return profileVariantsVLD;
}







