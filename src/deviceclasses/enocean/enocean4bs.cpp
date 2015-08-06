//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

/// enocean bit specification to bit number macro
#define DB(byte,bit) (byte*8+bit)
/// enocean bit specification to bit mask macro
#define DBMASK(byte,bit) ((uint32_t)1<<DB(byte,bit))


#pragma mark - Enocean4BSDevice


Enocean4BSDevice::Enocean4BSDevice(EnoceanDeviceContainer *aClassContainerP) :
  inherited(aClassContainerP)
{
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
    LOG(LOG_INFO, "Sending 4BS teach-in response for EEP %06X\n", EEP_PURE(getEEProfile()));
    getEnoceanDeviceContainer().enoceanComm.sendCommand(responsePacket, NULL);
  }
}



#pragma mark - Enocean4BSHandler


// static factory method
EnoceanDevicePtr Enocean4bsHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice aSubDeviceIndex,
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
    newDev = Enocean4bsSensorHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
  }
  return newDev;
}


void Enocean4bsHandler::prepare4BSpacket(Esp3PacketPtr &aOutgoingPacket, uint32_t &a4BSdata)
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



#pragma mark - generic table driven sensor handler




namespace p44 {

  struct Enocean4BSSensorDescriptor;

  /// decoder function
  /// @param aDescriptor descriptor for data to extract
  /// @param a4BSdata the 4BS data as 32-bit value, MSB=enocean DB_3, LSB=enocean DB_0
  typedef void (*BitFieldHandlerFunc)(const struct Enocean4BSSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, bool aForSend, uint32_t &a4BSdata);


  /// enocean sensor value descriptor
  typedef struct Enocean4BSSensorDescriptor {
    uint8_t func; ///< the function code from the EPP signature
    uint8_t type; ///< the type code from the EPP signature
    uint8_t subDevice; ///< subdevice index, in case EnOcean device needs to be split into multiple logical vdSDs
    DsGroup primaryGroup; ///< the dS group for the entire device
    DsGroup channelGroup; ///< the dS group for this channel
    BehaviourType behaviourType; ///< the behaviour type
    uint8_t behaviourParam; ///< DsSensorType, DsBinaryInputType or DsOutputFunction resp., depending on behaviourType
    DsUsageHint usage; ///< usage hint
    float min; ///< min value
    float max; ///< max value
    uint8_t msBit; ///< most significant bit of sensor value field in 4BS 32-bit word (31=Bit7 of DB_3, 0=Bit0 of DB_0)
    uint8_t lsBit; ///< least significant bit of sensor value field in 4BS 32-bit word (31=Bit7 of DB_3, 0=Bit0 of DB_0)
    double updateInterval; ///< normal update interval (average time resolution) in seconds
    double aliveSignInterval; ///< maximum interval between two reports of a sensor. If sensor does not push a value for longer than that, it should be considered out-of-order
    BitFieldHandlerFunc bitFieldHandler; ///< function used to convert between bit field in 4BS telegram and engineering value for the behaviour
    const char *typeText;
    const char *unitText;
  } Enocean4BSSensorDescriptor;

} // namespace p44


#pragma mark - bit field handlers for Enocean4bsSensorHandler

/// standard bitfield extractor function for sensor behaviours (read only)
static void stdSensorHandler(const struct Enocean4BSSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, bool aForSend, uint32_t &a4BSdata)
{
  if (!aForSend) {
    uint32_t value = a4BSdata>>aSensorDescriptor.lsBit;
    int numBits = aSensorDescriptor.msBit-aSensorDescriptor.lsBit+1;
    long mask = (1l<<numBits)-1;
    value &= mask;
    // now pass to behaviour
    SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(aBehaviour);
    if (sb) {
      sb->updateEngineeringValue(value);
    }
  }
}

/// inverted bitfield extractor function
static void invSensorHandler(const struct Enocean4BSSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, bool aForSend, uint32_t &a4BSdata)
{
  if (!aForSend) {
    uint32_t data = ~a4BSdata;
    stdSensorHandler(aSensorDescriptor, aBehaviour, false, data);
  }
}


/// two-range illumination handler, as used in A5-06-01 and A5-06-02
static void illumHandler(const struct Enocean4BSSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, bool aForSend, uint32_t &a4BSdata)
{
  uint32_t data = 0;
  if (!aForSend) {
    // actual data comes in:
    //  DB(0,0)==0 -> in DB(2), real 9-bit value is DB(2)
    //  DB(0,0)==1 -> in DB(1), real 9-bit value is DB(1)*2
    // Convert this to an always 9-bit value in DB(2,0)..DB(1,0)
    if (a4BSdata % 0x01) {
      // DB(0,0)==1: put DB(2) into DB(1,7)..DB(1,0)
      data = (a4BSdata>>8) & 0x0000FF00; // normal range, full resolution
    }
    else {
      // DB(0,0)==1: put DB(1) into DB(2,0)..DB(1,1)
      data = (a4BSdata<<1) & 0x0001FE00; // double range, half resolution
    }
    stdSensorHandler(aSensorDescriptor, aBehaviour, false, data);
  }
}


static void powerMeterHandler(const struct Enocean4BSSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, bool aForSend, uint32_t &a4BSdata)
{
  if (!aForSend) {
    // raw value is in DB3.7..DB1.0 (upper 24 bits)
    uint32_t value = a4BSdata>>8;
    // scaling is in bits DB0.1 and DB0.0 : 00=scale1, 01=scale10, 10=scale100, 11=scale1000
    int divisor = 1;
    switch (a4BSdata & 0x03) {
      case 1: divisor = 10; break; // value scale is 0.1kWh or 0.1W per LSB
      case 2: divisor = 100; break; // value scale is 0.01kWh or 0.01W per LSB
      case 3: divisor = 1000; break; // value scale is 0.001kWh (1Wh) or 0.001W (1mW) per LSB
    }
    SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(aBehaviour);
    if (sb) {
      // DB0.2 signals which value it is: 0=cumulative (energy), 1=current value (power)
      if (a4BSdata & 0x04) {
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
}



/// standard binary input handler
static void stdInputHandler(const struct Enocean4BSSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, bool aForSend, uint32_t &a4BSdata)
{
  // read only
  if (!aForSend) {
    bool newRawState = (a4BSdata>>aSensorDescriptor.lsBit) & 0x01;
    bool newState;
    if (newRawState)
      newState = (bool)aSensorDescriptor.max; // true: report value for max
    else
      newState = (bool)aSensorDescriptor.min; // false: report value for min
    // now pass to behaviour
    BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(aBehaviour);
    if (bb) {
      bb->updateInputState(newState);
    }
  }
}



#pragma mark - sensor mapping table for Enocean4bsSensorHandler


static const char *tempText = "Temperature";
static const char *tempUnit = "°C";

static const char *humText = "Humidity";
static const char *humUnit = "%";

static const char *illumText = "Illumination";
static const char *illumUnit = "lx";

static const char *occupText = "Occupancy";

static const char *motionText = "Motion";

static const char *unityUnit = "units"; // undefined unit, but not just 0/1 (binary)

static const char *binaryUnit = ""; // binary, only 0 or 1

static const char *setPointText = "Set Point";
static const char *fanSpeedText = "Fan Speed";
static const char *dayNightText = "Day/Night";



static const p44::Enocean4BSSensorDescriptor enocean4BSdescriptors[] = {
  // func,type, SD,primarygroup,       channelGroup,                  behaviourType,          behaviourParam,        usage,              min,  max,  MSB,  LSB,  updateIv, aliveSignIv, handler,      typeText, unitText, flags
  // A5-02-xx: Temperature sensors
  // - 40 degree range                 behaviour_binaryinput
  { 0x02, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,    0, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -30,   10, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x03, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -20,   20, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x04, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10,   30, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x05, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    10,   50, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x07, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    20,   60, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x08, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    30,   70, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x09, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    40,   80, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x0A, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    50,   90, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x0B, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    60,  100, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  // - 80 degree range
  { 0x02, 0x10, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -60,   20, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -50,   30, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x12, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x13, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -30,   50, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x14, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -20,   60, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x15, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10,   70, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x16, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   80, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x17, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    10,   90, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x18, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    20,  100, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x19, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    30,  110, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x1A, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    40,  120, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x1B, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,    50,  130, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  // - 10 bit
  { 0x02, 0x20, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,        -10, 42.2, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  { 0x02, 0x30, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40, 62.3, DB(2,1), DB(1,0), 100, 40*60, &invSensorHandler,  tempText, tempUnit },
  // A5-04-xx: Temperature and Humidity
  // - 0..40 degree indoor, e.g. Alpha Sense
  { 0x04, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler,  tempText, tempUnit },
  { 0x04, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler,  humText,  humUnit  },
  // -20..60 degree outdoor, e.g. Alpha Sense
  { 0x04, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_outdoors,    -20, 61.6, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler,  tempText, tempUnit },
  { 0x04, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_outdoors,      0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler,  humText,  humUnit  },

  // A5-06-xx: Light Sensor
  { 0x06, 0x01, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_outdoors,    600,60000, DB(2,0), DB(1,0), 100, 40*60, &illumHandler,     illumText, illumUnit },
  { 0x06, 0x02, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1024, DB(2,0), DB(1,0), 100, 40*60, &illumHandler,     illumText, illumUnit },
  { 0x06, 0x03, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1024, DB(2,7), DB(1,6), 100, 40*60, &stdSensorHandler, illumText, illumUnit },

  // A5-07-xx: Occupancy Sensor
  // - two slightly different occupancy sensors
  { 0x07, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          0,    1, DB(1,7), DB(1,7), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  { 0x07, 0x02, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          0,    1, DB(0,7), DB(0,7), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  // - occupancy sensor with illumination sensor
  { 0x07, 0x03, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          0,    1, DB(0,7), DB(0,7), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  { 0x07, 0x03, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0, 1024, DB(2,7), DB(1,6), 100, 40*60, &stdSensorHandler, illumText, illumUnit },

  // A5-08-01: Light, Temperature and Occupancy sensor
  // - e.g. Eltako FBH
  { 0x08, 0x01, 0, group_black_joker,  group_yellow_light,            behaviour_sensor,      sensorType_illumination,usage_room,          0,  510, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, illumText, illumUnit },
  { 0x08, 0x01, 0, group_black_joker,  group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   51, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0x08, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_motion,      usage_room,          1,    0, DB(0,1), DB(0,1), 100, 40*60, &stdInputHandler,  motionText, binaryUnit },
  { 0x08, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_binaryinput, binInpType_presence,    usage_user,          1,    0, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  occupText, binaryUnit },

  // A5-10-02: Room Control Panel with Temperature Sensor, Set Point, Fan Speed and Day/Night Control
  // Note: fan speed negative range denotes "automatic" (210..255 -> -0.215311..-0)
  // - e.g. Thermokon Thanos
  { 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_fan_speed,   usage_room,  -0.215311,    1, DB(3,7), DB(3,0), 100, 40*60, &invSensorHandler, fanSpeedText, unityUnit },
  { 0x10, 0x02, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-03: Room Control Panel with Temperature Sensor and Set Point Control
  // - e.g. Eltako FTR78S
  { 0x10, 0x03, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0x10, 0x03, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },

  // A5-10-06: Room Panel with Temperature Sensor, Set Point Control, Day/Night Control
  // - e.g. Eltako FTR55D
  { 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, 40*60, &invSensorHandler, tempText, tempUnit },
  { 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0x10, 0x06, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-10-11: Room Panel with Temperature Sensor, Set Point Control, Humidity
  // - e.g. Thermokon Thanos
  { 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_set_point,   usage_user,          0,    1, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, setPointText, unityUnit },
  { 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_humidity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, humText,  humUnit },
  { 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit },
  { 0x10, 0x11, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_none,        usage_user,          0,    1, DB(0,0), DB(0,0), 100, 40*60, &stdInputHandler,  dayNightText, binaryUnit },

  // A5-12-01: Energy meter
  // - e.g. Eltako FWZ12-16A
  { 0x12, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_sensor,      sensorType_power,       usage_room,          0, 2500, DB(3,7), DB(1,0), 600, 40*60, &powerMeterHandler, "Power", "W" },
  { 0x12, 0x01, 0, group_black_joker,  group_black_joker,             behaviour_sensor,      sensorType_energy,      usage_room,          0, 16e9, DB(3,7), DB(1,0), 600, 40*60, &powerMeterHandler, "Energy", "kWh" },

  // terminator
  { 0,    0,    0, group_black_joker,  group_black_joker,             behaviour_undefined, 0, usage_undefined, 0, 0, 0, 0, 0, 0, NULL /* NULL for extractor function terminates list */, NULL, NULL },
};




Enocean4bsSensorHandler::Enocean4bsSensorHandler(EnoceanDevice &aDevice) :
  inherited(aDevice),
  sensorChannelDescriptorP(NULL)
{

}



#define TIMEOUT_FACTOR_FOR_INACTIVE 4

bool Enocean4bsSensorHandler::isAlive()
{
  if (sensorChannelDescriptorP->aliveSignInterval<=0)
    return true; // no alive sign interval to check, assume alive
  // check if gotten no message for longer than aliveSignInterval
  if (MainLoop::now()-device.getLastPacketTime() < sensorChannelDescriptorP->aliveSignInterval*Second*TIMEOUT_FACTOR_FOR_INACTIVE)
    return true;
  // timed out
  return false;
}


// static factory method
EnoceanDevicePtr Enocean4bsSensorHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice aSubDeviceIndex, // current subdeviceindex, factory returns NULL when no device can be created for this subdevice index
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  EepFunc func = EEP_FUNC(aEEProfile);
  EepType type = EEP_TYPE(aEEProfile);
  EnoceanDevicePtr newDev; // none so far

  // create device from matching with sensor table
  int numDescriptors = 0; // number of descriptors
  // Search descriptors for this EEP and for the start of channels for this aSubDeviceIndex (in case sensors in one physical devices are split into multiple vdSDs)
  const Enocean4BSSensorDescriptor *subdeviceDescP = NULL;
  const Enocean4BSSensorDescriptor *descP = enocean4BSdescriptors;
  while (descP->bitFieldHandler!=NULL) {
    if (descP->func==func && descP->type==type) {
      // remember if this is the subdevice we are looking for
      if (descP->subDevice==aSubDeviceIndex) {
        if (!subdeviceDescP) subdeviceDescP = descP; // remember the first descriptor of this subdevice as starting point for creating handlers below
        numDescriptors++; // count descriptors for this subdevice as a limit for creating handlers below
      }
    }
    descP++;
  }
  // Create device and channels
  bool needsTeachInResponse = false;
  bool firstDescriptorForDevice = true;
  while (numDescriptors>0) {
    // more channels to create for this subdevice number
    if (!newDev) {
      // device not yet created, create it now
      newDev = EnoceanDevicePtr(new Enocean4BSDevice(aClassContainerP));
      // sensor devices don't need scenes
      newDev->installSettings(); // no scenes
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      // first descriptor defines device primary color
      newDev->setPrimaryGroup(subdeviceDescP->primaryGroup);
    }
    // now add the channel
    addSensorChannel(newDev, *subdeviceDescP, firstDescriptorForDevice);
    // next descriptor
    firstDescriptorForDevice = false;
    subdeviceDescP++;
    numDescriptors--;
  }
  // create the teach-in response if one is required
  if (newDev && aSendTeachInResponse && needsTeachInResponse) {
    newDev->sendTeachInResponse();
  }
  // return device (or empty if none created)
  return newDev;
}


// static factory method
void Enocean4bsSensorHandler::addSensorChannel(
  EnoceanDevicePtr aDevice,
  const Enocean4BSSensorDescriptor &aSensorDescriptor,
  bool aSetDeviceDescription
) {
  // create channel handler
  Enocean4bsSensorHandlerPtr newHandler = Enocean4bsSensorHandlerPtr(new Enocean4bsSensorHandler(*aDevice.get()));
  // assign descriptor
  newHandler->sensorChannelDescriptorP = &aSensorDescriptor;
  // create the behaviour
  newHandler->behaviour = Enocean4bsSensorHandler::newSensorBehaviour(aSensorDescriptor, aDevice);
  switch (aSensorDescriptor.behaviourType) {
    case behaviour_sensor: {
      if (aSetDeviceDescription) {
        aDevice->setFunctionDesc(string(aSensorDescriptor.typeText) + " sensor");
        aDevice->setIconInfo("enocean_sensor", true);
      }
      break;
    }
    case behaviour_binaryinput: {
      if (aSetDeviceDescription) {
        aDevice->setFunctionDesc(string(aSensorDescriptor.typeText) + " input");
      }
      break;
    }
    default: {
      break;
    }
  }
  // add channel to device
  aDevice->addChannelHandler(newHandler);
}



// static factory method
DsBehaviourPtr Enocean4bsSensorHandler::newSensorBehaviour(const Enocean4BSSensorDescriptor &aSensorDescriptor, DevicePtr aDevice) {
  // create the behaviour
  switch (aSensorDescriptor.behaviourType) {
    case behaviour_sensor: {
      SensorBehaviourPtr sb = SensorBehaviourPtr(new SensorBehaviour(*aDevice.get()));
      int numBits = (aSensorDescriptor.msBit-aSensorDescriptor.lsBit)+1; // number of bits
      double resolution = (aSensorDescriptor.max-aSensorDescriptor.min) / ((1<<numBits)-1); // units per LSB
      sb->setHardwareSensorConfig((DsSensorType)aSensorDescriptor.behaviourParam, aSensorDescriptor.usage, aSensorDescriptor.min, aSensorDescriptor.max, resolution, aSensorDescriptor.updateInterval*Second, aSensorDescriptor.aliveSignInterval*Second);
      sb->setGroup(aSensorDescriptor.channelGroup);
      sb->setHardwareName(Enocean4bsSensorHandler::sensorDesc(aSensorDescriptor));
      return sb;
    }
    case behaviour_binaryinput: {
      BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*aDevice.get()));
      bb->setHardwareInputConfig((DsBinaryInputType)aSensorDescriptor.behaviourParam, aSensorDescriptor.usage, true, aSensorDescriptor.updateInterval*Second);
      bb->setGroup(aSensorDescriptor.channelGroup);
      bb->setHardwareName(Enocean4bsSensorHandler::sensorDesc(aSensorDescriptor));
      return bb;
    }
    default: {
      break;
    }
  }
  // none
  return DsBehaviourPtr();
}





// handle incoming data from device and extract data for this channel
void Enocean4bsSensorHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->eepHasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eepRorg()==rorg_4BS && aEsp3PacketPtr->radioUserDataLength()==4) {
      // only look at 4BS packets of correct length
      if (sensorChannelDescriptorP && sensorChannelDescriptorP->bitFieldHandler) {
        // create 32bit data word
        uint32_t data = aEsp3PacketPtr->get4BSdata();
        // call bit field handler, will pass result to behaviour
        sensorChannelDescriptorP->bitFieldHandler(*sensorChannelDescriptorP, behaviour, false, data);
      }
    }
  }
}


string Enocean4bsSensorHandler::shortDesc()
{
  return Enocean4bsSensorHandler::sensorDesc(*sensorChannelDescriptorP);
}


string Enocean4bsSensorHandler::sensorDesc(const Enocean4BSSensorDescriptor &aSensorDescriptor)
{
  const char *unitText = aSensorDescriptor.unitText;
  if (unitText==binaryUnit) {
    // binary input
    return string_format("%s", aSensorDescriptor.typeText);
  }
  else {
    // sensor with a value
    int numBits = (aSensorDescriptor.msBit-aSensorDescriptor.lsBit)+1; // number of bits
    double resolution = (aSensorDescriptor.max-aSensorDescriptor.min) / ((1<<numBits)-1); // units per LSB
    int fracDigits = (int)(-log(resolution)/log(10)+0.99);
    if (fracDigits<0) fracDigits=0;
    return string_format("%s, %0.*f..%0.*f %s", aSensorDescriptor.typeText, fracDigits, aSensorDescriptor.min, fracDigits, aSensorDescriptor.max, unitText);
  }
}




#pragma mark - EnoceanA52001Handler


EnoceanA52001Handler::EnoceanA52001Handler(EnoceanDevice &aDevice) :
  inherited(aDevice),
  serviceState(service_idle)
{
}


// static factory method
EnoceanDevicePtr EnoceanA52001Handler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice aSubDeviceIndex, // current subdeviceindex, factory returns NULL when no device can be created for this subdevice index
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  // A5-20-01: heating valve actuator
  // - e.g. thermokon SAB 02 or Kieback+Peter MD15-FTL, MD10-FTL
  // configuration for included sensor channels
  static const p44::Enocean4BSSensorDescriptor tempSensor =
    { 0x20, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_sensor,      sensorType_temperature, usage_room, 0, 40, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, tempText,      tempUnit };
  static const p44::Enocean4BSSensorDescriptor lowBatInput =
    { 0x20, 0x01, 0, group_blue_heating, group_roomtemperature_control, behaviour_binaryinput, binInpType_lowBattery,  usage_room, 1,  0, DB(2,4), DB(2,4), 100, 40*60, &stdInputHandler,  "Low Battery", binaryUnit };
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
    OutputBehaviourPtr ob = OutputBehaviourPtr(new ClimateControlBehaviour(*newDev.get()));
    ob->setHardwareOutputConfig(outputFunction_positional, usage_room, false, 0);
    ob->setHardwareName("valve");
    // - create A5-20-01 specific handler for output
    Enocean4bsHandlerPtr newHandler = Enocean4bsHandlerPtr(new EnoceanA52001Handler(*newDev.get()));
    newHandler->behaviour = ob;
    newDev->addChannelHandler(newHandler);
    if (EEP_VARIANT(aEEProfile)==1) {
      // profile variant with valve sensor enabled - add built-in temp sensor
      Enocean4bsSensorHandler::addSensorChannel(newDev, tempSensor, false);
    }
    // report low bat status as a binary input
    Enocean4bsSensorHandler::addSensorChannel(newDev, lowBatInput, false);
    // A5-20-01 need teach-in response if requested (i.e. if this device creation is caused by learn-in, not reinstantiation from DB)
    if (aSendTeachInResponse) {
      newDev->sendTeachInResponse();
    }
    newDev->setUpdateAtEveryReceive();
  }
  // return device (or empty if none created)
  return newDev;
}



// handle incoming data from device and extract data for this channel
void EnoceanA52001Handler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->eepHasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eepRorg()==rorg_4BS && aEsp3PacketPtr->radioUserDataLength()==4) {
      // only look at 4BS packets of correct length
      // sensor inputs will be checked by separate handlers, check error bits only, most fatal first
      // - check actuator obstructed
      uint32_t data = aEsp3PacketPtr->get4BSdata();
      if ((data & DBMASK(2,0))!=0) {
        LOG(LOG_ERR, "EnOcean valve %s error: actuator obstructed\n", shortDesc().c_str());
        behaviour->setHardwareError(hardwareError_overload);
      }
      else if ((data & DBMASK(2,4))==0 && (data & DBMASK(2,5))==0) {
        LOG(LOG_ERR, "EnOcean valve %s error: energy storage AND battery are low\n", shortDesc().c_str());
        behaviour->setHardwareError(hardwareError_lowBattery);
      }
      // show general status if not fully ok
      LOG(LOG_INFO,
        "EnOcean valve %s status: Service %s, Energy input %s, Energy storage %scharged, Battery %s, Cover %s, Sensor %s, Detected window %s, Actuator %s\n",
        shortDesc().c_str(),
        data & DBMASK(2,7) ? "ON" : "off",
        data & DBMASK(2,6) ? "enabled" : "disabled",
        data & DBMASK(2,5) ? "" : "NOT ",
        data & DBMASK(2,4) ? "ok" : "LOW",
        data & DBMASK(2,3) ? "closed" : "OPEN",
        data & DBMASK(2,2) ? "FAILURE" : "ok",
        data & DBMASK(2,2) ? "open" : "closed",
        data & DBMASK(2,2) ? "OBSTRUCTED" : "ok"
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
    prepare4BSpacket(aEsp3PacketPtr, data);
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
        LOG(LOG_NOTICE,"EnOcean valve %s prophylaxis operation: fully opening valve\n", shortDesc().c_str());
        data |= DBMASK(1,5); // service: open
        // next is closing
        serviceState = service_closevalve;
        device.needOutgoingUpdate();
      }
      else if (serviceState==service_closevalve) {
        // trigger force fully closed
        LOG(LOG_NOTICE,"EnOcean valve %s prophylaxis operation: fully closing valve\n", shortDesc().c_str());
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
      // - DB(3,7)..DB(3,0) is valve position 0..255
      int32_t newValue = ch->getChannelValue()*255.0/100.0; // channel is 0..100 -> scale to 0..255
      data |= newValue<<DB(3,0); // insert data into DB(3,0..7)
      // - DB(1,3) is summer mode
      if (cb->isSummerMode()) {
        data |= DBMASK(1,3);
        LOG(LOG_INFO,"EnOcean valve %s is in SUMMER mode (slow updates)\n", shortDesc().c_str());
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
static const p44::Enocean4BSSensorDescriptor A513dawnSensor =
  { 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 999, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, illumText, illumUnit };
static const p44::Enocean4BSSensorDescriptor A513outdoorTemp =
  { 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_temperature, usage_outdoors, -40, 80, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, tempText, tempUnit };
static const p44::Enocean4BSSensorDescriptor A513windSpeed =
  { 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_wind_speed, usage_outdoors, 0, 70, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, "wind speed", "m/s" };
static const p44::Enocean4BSSensorDescriptor A513dayIndicator =
  { 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_binaryinput, binInpType_none,  usage_outdoors, 1,  0, DB(0,2), DB(0,2), 100, 40*60, &stdInputHandler,  "Day indicator", binaryUnit };
static const p44::Enocean4BSSensorDescriptor A513rainIndicator =
  { 0x13, 0x01, 0, group_black_joker, group_black_joker, behaviour_binaryinput, binInpType_rain,  usage_outdoors, 0,  1, DB(0,1), DB(0,1), 100, 40*60, &stdInputHandler,  "Rain indicator", binaryUnit };
// - A5-13-02 telegram
static const p44::Enocean4BSSensorDescriptor A513sunWest =
  { 0x13, 0x02, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 150000, DB(3,7), DB(3,0), 100, 40*60, &stdSensorHandler, "Sun west", illumUnit };
static const p44::Enocean4BSSensorDescriptor A513sunSouth =
  { 0x13, 0x02, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 150000, DB(2,7), DB(2,0), 100, 40*60, &stdSensorHandler, "Sun south", illumUnit };
static const p44::Enocean4BSSensorDescriptor A513sunEast =
  { 0x13, 0x02, 0, group_black_joker, group_black_joker, behaviour_sensor, sensorType_illumination, usage_outdoors, 0, 150000, DB(1,7), DB(1,0), 100, 40*60, &stdSensorHandler, "Sun east", illumUnit };

EnoceanA5130XHandler::EnoceanA5130XHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}


// static factory method
EnoceanDevicePtr EnoceanA5130XHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice aSubDeviceIndex, // current subdeviceindex, factory returns NULL when no device can be created for this subdevice index
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
    newHandler->behaviour = Enocean4bsSensorHandler::newSensorBehaviour(A513dawnSensor, newDev);
    // - register the handler and the default behaviour
    newDev->addChannelHandler(newHandler);
    // - Add extra behaviours for A5-13-01
    newHandler->outdoorTemp = Enocean4bsSensorHandler::newSensorBehaviour(A513outdoorTemp, newDev);
    newDev->addBehaviour(newHandler->outdoorTemp);
    newHandler->windSpeed = Enocean4bsSensorHandler::newSensorBehaviour(A513windSpeed, newDev);
    newDev->addBehaviour(newHandler->windSpeed);
    newHandler->dayIndicator = Enocean4bsSensorHandler::newSensorBehaviour(A513dayIndicator, newDev);
    newDev->addBehaviour(newHandler->dayIndicator);
    newHandler->rainIndicator = Enocean4bsSensorHandler::newSensorBehaviour(A513rainIndicator, newDev);
    newDev->addBehaviour(newHandler->rainIndicator);
    // - Add extra behaviours for A5-13-02
    newHandler->sunWest = Enocean4bsSensorHandler::newSensorBehaviour(A513sunWest, newDev);
    newDev->addBehaviour(newHandler->sunWest);
    newHandler->sunSouth = Enocean4bsSensorHandler::newSensorBehaviour(A513sunSouth, newDev);
    newDev->addBehaviour(newHandler->sunSouth);
    newHandler->sunEast = Enocean4bsSensorHandler::newSensorBehaviour(A513sunEast, newDev);
    newDev->addBehaviour(newHandler->sunEast);
  }
  // return device (or empty if none created)
  return newDev;
}



// handle incoming data from device and extract data for this channel
void EnoceanA5130XHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->eepHasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eepRorg()==rorg_4BS && aEsp3PacketPtr->radioUserDataLength()==4) {
      // only look at 4BS packets of correct length
      // - check identifier to see what info we got
      uint32_t data = aEsp3PacketPtr->get4BSdata();
      uint8_t identifier = (data>>4) & 0x0F;
      switch (identifier) {
        case 1:
          // A5-13-01
          A513dawnSensor.bitFieldHandler(A513dawnSensor, behaviour, false, data);
          A513dawnSensor.bitFieldHandler(A513outdoorTemp, outdoorTemp, false, data);
          A513dawnSensor.bitFieldHandler(A513windSpeed, windSpeed, false, data);
          A513dawnSensor.bitFieldHandler(A513dayIndicator, dayIndicator, false, data);
          A513dawnSensor.bitFieldHandler(A513rainIndicator, rainIndicator, false, data);
          break;
        case 2:
          // A5-13-02
          A513sunWest.bitFieldHandler(A513sunWest, sunWest, false, data);
          A513sunSouth.bitFieldHandler(A513sunSouth, sunSouth, false, data);
          A513sunEast.bitFieldHandler(A513sunEast, sunEast, false, data);
          break;
        default:
          // A5-13-03..06 are not supported
          break;
      }
    }
  }
}



string EnoceanA5130XHandler::shortDesc()
{
  return string_format("Dawn/Temp/Wind/Rain/Sun outdoor sensor");
}



#pragma mark - Enocean4BSDevice profile variants


static const profileVariantEntry RPSprofileVariants[] = {
  // dual rocker RPS button alternatives
  { 1, 0x00A52001, "heating valve" },
  { 1, 0x01A52001, "heating valve (with temperature sensor)" },
  { 0, 0, NULL } // terminator
};


const profileVariantEntry *Enocean4BSDevice::profileVariantsTable()
{
  return RPSprofileVariants;
}







