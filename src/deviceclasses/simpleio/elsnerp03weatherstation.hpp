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

#ifndef __vdcd__rs485weather__
#define __vdcd__rs485weather__

#include "device.hpp"

#include "staticdevicecontainer.hpp"
#include "serialcomm.hpp"

#include "sensorbehaviour.hpp"
#include "binaryinputbehaviour.hpp"


using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class ElsnerP03WeatherStation;
  typedef boost::intrusive_ptr<ElsnerP03WeatherStation> ElsnerP03WeatherStationPtr;
  class ElsnerP03WeatherStation : public StaticDevice
  {
    typedef StaticDevice inherited;

    string rs485devicename;

    SerialCommPtr serial;
    int telegramIndex;
    int checksum;
    static const size_t numTelegramBytes = 40;
    uint8_t telegram[numTelegramBytes];

    SensorBehaviourPtr temperatureSensor;
    SensorBehaviourPtr sun1Sensor;
//    SensorBehaviourPtr sun2Sensor;
//    SensorBehaviourPtr sun3Sensor;
    BinaryInputBehaviourPtr twilightInput;
    SensorBehaviourPtr daylightSensor;
    SensorBehaviourPtr windSensor;
    BinaryInputBehaviourPtr rainInput;

  public:
    ElsnerP03WeatherStation(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "weatherstation"; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    void initializeDevice(StatusCB aCompletedCB, bool aFactoryReset);

    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @}

  protected:

    void deriveDsUid();

  private:

    void serialReceiveHandler(ErrorPtr aError);

  };
  
} // namespace p44

#endif /* defined(__vdcd__rs485weather__) */
