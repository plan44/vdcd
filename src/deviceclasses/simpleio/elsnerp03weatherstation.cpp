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

#include "elsnerp03weatherstation.hpp"

#include "fnv.hpp"

using namespace p44;


ElsnerP03WeatherStation::ElsnerP03WeatherStation(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  StaticDevice((DeviceClassContainer *)aClassContainerP),
  rs485devicename(aDeviceConfig)
{
  // assign name for showing on console and for creating dSUID from
  // create I/O
  // Standard device settings without scene table
  primaryGroup = group_black_joker;
  installSettings();
  // - create temp sensor input
  temperatureSensor = SensorBehaviourPtr(new SensorBehaviour(*this));
  temperatureSensor->setHardwareSensorConfig(sensorType_temperature, usage_outdoors, -40, 80, 0.1, 3*Second, 5*Minute, 5*Minute);
  temperatureSensor->setGroup(group_blue_heating);
  temperatureSensor->setHardwareName("Outdoor Temperature -40..80 °C");
  addBehaviour(temperatureSensor);
  // - create sun sensor input
  sun1Sensor = SensorBehaviourPtr(new SensorBehaviour(*this));
  sun1Sensor->setHardwareSensorConfig(sensorType_illumination, usage_outdoors, 0, 99000, 1000, 3*Second, 5*Minute, 5*Minute);
  sun1Sensor->setGroup(group_yellow_light);
  sun1Sensor->setHardwareName("sunlight in lux");
  addBehaviour(sun1Sensor);
  // - create twilight binary input
  twilightInput = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
  twilightInput->setGroup(group_yellow_light);
  twilightInput->setHardwareInputConfig(binInpType_twilight, usage_outdoors, true, Never);
  twilightInput->setHardwareName("Twilight indicator");
  addBehaviour(twilightInput);
  // - create sun sensor input
  daylightSensor = SensorBehaviourPtr(new SensorBehaviour(*this));
  daylightSensor->setHardwareSensorConfig(sensorType_illumination, usage_outdoors, 0, 99000, 100, 3*Second, 5*Minute, 5*Minute);
  daylightSensor->setGroup(group_yellow_light);
  daylightSensor->setHardwareName("daylight in lux");
  addBehaviour(daylightSensor);
  // - create sun sensor input
  windSensor = SensorBehaviourPtr(new SensorBehaviour(*this));
  windSensor->setHardwareSensorConfig(sensorType_wind_speed, usage_outdoors, 0, 70, 4, 3*Second, 5*Minute, 5*Minute);
  windSensor->setGroup(group_blue_heating);
  windSensor->setHardwareName("wind in m/s");
  addBehaviour(windSensor);
  // - create rain binary input
  rainInput = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*this));
  rainInput->setGroup(group_yellow_light);
  rainInput->setHardwareInputConfig(binInpType_rain, usage_outdoors, true, Never);
  rainInput->setHardwareName("rain");
  addBehaviour(rainInput);
  // done, calculate dSUID
  deriveDsUid();
}


string ElsnerP03WeatherStation::modelName()
{
  return "Elsner P03";
}



void ElsnerP03WeatherStation::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  serial = SerialCommPtr(new SerialComm(MainLoop::currentMainLoop()));
  serial->setConnectionSpecification(rs485devicename.c_str(), 2103, 19200);
  serial->setReceiveHandler(boost::bind(&ElsnerP03WeatherStation::serialReceiveHandler, this, _1));
  serial->establishConnection();
  telegramIndex = -1;
  // done
  if (aCompletedCB) aCompletedCB(ErrorPtr());
}


void ElsnerP03WeatherStation::serialReceiveHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    size_t n = serial->numBytesReady();
    if (n>0) {
      uint8_t byte;
      serial->receiveBytes(1, &byte, aError);
      if (Error::isOK(aError)) {
        if (telegramIndex<0) {
          // wait for start of telegram
          if (byte=='W') {
            telegramIndex = 0;
          }
        }
        if (telegramIndex>=0 && telegramIndex<numTelegramBytes) {
          telegram[telegramIndex++] = byte;
          if (telegramIndex>=numTelegramBytes) {
            // evaluate
            // TODO: checksum
            // - temperature
            double temp =
              (telegram[2]-'0')*10 +
              (telegram[3]-'0')*1 +
              (telegram[5]-'0')*0.1;
            temperatureSensor->updateSensorValue(temp);
            // - sun
            double sun =
              (telegram[6]-'0')*10000 +
              (telegram[7]-'0')*1000;
            sun1Sensor->updateSensorValue(sun);
            // - twilight
            bool isTwilight = telegram[12]=='J';
            twilightInput->updateInputState(isTwilight);
            // - daylight
            double daylight =
              (telegram[13]-'0')*10000 +
              (telegram[14]-'0')*1000 +
              (telegram[15]-'0')*100;
            daylightSensor->updateSensorValue(daylight);
            // - wind
            double wind =
              (telegram[16]-'0')*10 +
              (telegram[17]-'0')*1 +
              (telegram[19]-'0')*0.1;
            windSensor->updateSensorValue(wind);
            // - rain
            bool rain = telegram[20]=='J';
            rainInput->updateInputState(rain);
            // reset index
            telegramIndex = 0;
          }
        }
      }
    }
  }
}



void ElsnerP03WeatherStation::deriveDsUid()
{
  // vDC implementation specific UUID:
  //   UUIDv5 with name = classcontainerinstanceid::consoledevicename
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s = classContainerP->deviceClassContainerInstanceIdentifier();
  s += "::" + rs485devicename;
  dSUID.setNameInSpace(s, vdcNamespace);
}


string ElsnerP03WeatherStation::description()
{
  string s = inherited::description();
  string_format_append(s, "\n- drives a Elsner P03 weather station");
  return s;
}
