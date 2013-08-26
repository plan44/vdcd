//
//  enocean4bs.cpp
//  vdcd
//
//  Created by Lukas Zeller on 26.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enocean4bs.hpp"

#include "sensorbehaviour.hpp"
#include "binaryinputbehaviour.hpp"
#include "outputbehaviour.hpp"

using namespace p44;


/// decoder function
/// @param aDescriptor descriptor for data to extract
/// @param a4BSdata the 4BS data as 32-bit value, MSB=enocean DB_3, LSB=enocean DB_0
typedef void (*BitFieldHandlerFunc)(const Enocean4bsHandler &aHandler, bool aForSend, uint32_t &a4BSdata);


/// enocean sensor value descriptor
typedef struct p44::Enocean4BSDescriptor {
  uint8_t func; ///< the function code from the EPP signature
  uint8_t type; ///< the type code from the EPP signature
  uint8_t subDevice; ///< subdevice index, in case enOcean device needs to be split into multiple logical vdSDs
  DsGroup group; ///< the dS group for this channel
  BehaviourType behaviourType; ///< the behaviour type
  uint8_t behaviourParam; ///< DsSensorType, DsBinaryInputType or DsOutputFunction resp., depending on behaviourType
  float min; ///< min value
  float max; ///< max value
  uint8_t msBit; ///< most significant bit of sensor value field in 4BS 32-bit word (31=Bit7 of DB_3, 0=Bit0 of DB_0)
  uint8_t lsBit; ///< least significant bit of sensor value field in 4BS 32-bit word (31=Bit7 of DB_3, 0=Bit0 of DB_0)
  double updateInterval; ///< normal update interval (average) in seconds
  BitFieldHandlerFunc bitFieldHandler; ///< function used to convert between bit field in 4BS telegram and engineering value for the behaviour
  const char *typeText;
  const char *unitText;
} Enocean4BSDescriptor;


/// enocean bit specification to bit number macro
#define DB(byte,bit) (byte*8+bit)

#pragma mark - bit field handlers

/// standard bitfield extractor function for sensor behaviours (read only)
static void stdSensorHandler(const Enocean4bsHandler &aHandler, bool aForSend, uint32_t &a4BSdata)
{
  const Enocean4BSDescriptor *descP = aHandler.channelDescriptorP;
  if (descP && !aForSend) {
    uint32_t value = a4BSdata>>descP->lsBit;
    int numBits = descP->msBit-descP->lsBit+1;
    long mask = (1l<<numBits)-1;
    value &= mask;
    // now pass to behaviour
    SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(aHandler.behaviour);
    if (sb) {
      sb->updateEngineeringValue(value);
    }
  }
}

/// inverted bitfield extractor function
static void invSensorHandler(const Enocean4bsHandler &aHandler, bool aForSend, uint32_t &a4BSdata)
{
  if (!aForSend) {
    uint32_t data = ~a4BSdata;
    stdSensorHandler(aHandler, false, data);
  }
}


/// standard binary input handler
static void stdInputHandler(const Enocean4bsHandler &aHandler, bool aForSend, uint32_t &a4BSdata)
{
  const Enocean4BSDescriptor *descP = aHandler.channelDescriptorP;
  if (descP && !aForSend) {
    bool newRawState = (a4BSdata>>descP->lsBit) & 0x01;
    bool newState;
    if (newRawState)
      newState = (bool)descP->max; // true: report value for max
    else
      newState = (bool)descP->min; // false: report value for min
    // now pass to behaviour
    BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(aHandler.behaviour);
    if (bb) {
      bb->updateInputState(newState);
    }
  }
}



static const char *tempText = "Temperature";
static const char *tempUnit = "°C";

static const Enocean4BSDescriptor enocean4BSdescriptors[] = {
  // Temperature sensors
  // - 40 degree range                 behaviour_binaryinput
  { 0x02, 0x01, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -40,    0, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x02, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -30,   10, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x03, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -20,   20, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x04, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -10,   30, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x05, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,   0,   40, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x06, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  10,   50, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x07, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  20,   60, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x08, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  30,   70, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x09, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  40,   80, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x0A, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  50,   90, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x0B, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  60,  100, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  // - 80 degree range
  { 0x02, 0x10, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -60,   20, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x11, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -50,   30, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x12, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -40,   40, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x13, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -30,   50, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x14, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -20,   60, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x15, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -10,   70, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x16, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,   0,   80, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x17, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  10,   90, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x18, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  20,  100, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x19, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  30,  110, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x1A, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  40,  120, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x1B, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,  50,  130, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  // - 10 bit
  { 0x02, 0x20, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -10, 42.2, DB(2,1), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x02, 0x30, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature, -40, 62.3, DB(2,1), DB(1,0), 100, &invSensorHandler, tempText, tempUnit },
  // Temperature and Humidity
  { 0x04, 0x01, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,   0,   40, DB(2,7), DB(2,0), 100, &invSensorHandler, tempText, tempUnit },
  { 0x04, 0x01, 0, group_blue_climate, behaviour_sensor,      sensorType_humitity,      0,   40, DB(2,7), DB(2,0), 100, &invSensorHandler, tempText, tempUnit },



  // Room Panels
  { 0x10, 0x06, 0, group_blue_climate, behaviour_sensor,      sensorType_temperature,   0,  40, DB(1,7), DB(1,0),  100, &invSensorHandler, tempText, tempUnit },
  { 0x10, 0x06, 0, group_blue_climate, behaviour_sensor,      sensorType_set_point,     0,   1, DB(2,7), DB(2,0),  100, &stdSensorHandler, "Set Point", "1" },
  { 0x10, 0x06, 0, group_green_access, behaviour_binaryinput, sensorType_none,          0,   1, DB(0,0), DB(0,0),  100, &stdInputHandler, "Day/Night", "1" },

  // terminator
  { 0, 0, 0, group_black_joker, behaviour_undefined, 0, 0, 0, 0, 0, 0, NULL /* NULL for extractor function terminates list */, NULL, NULL },
};


Enocean4bsHandler::Enocean4bsHandler(EnoceanDevice &aDevice) :
  EnoceanChannelHandler(aDevice),
  channelDescriptorP(NULL)
{
}



EnoceanDevicePtr Enocean4bsHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress, EnoceanSubDevice aSubDevice,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  EnoceanSubDevice *aNumSubdevicesP
) {
  EepFunc func = EEP_FUNC(aEEProfile);
  EepType type = EEP_TYPE(aEEProfile);
  EnoceanDevicePtr newDev; // none so far
  int numSubDevices = 0; // none found
  int numDescriptors = 0;
  // Search descriptors for this EEP, number of subdevices for this EEP and the start of channels for this subdevice
  const Enocean4BSDescriptor *descP = enocean4BSdescriptors;
  const Enocean4BSDescriptor *subdeviceDescP = NULL;
  while (descP->bitFieldHandler!=NULL) {
    if (descP->func==func && descP->type==type) {
      // update subdevice count
      numSubDevices = descP->subDevice+1;
      // remember if this is the subdevice we are looking for
      if (descP->subDevice==aSubDevice) {
        if (!subdeviceDescP) subdeviceDescP = descP; // remember the first descriptor of this subdevice
        numDescriptors++; // count descriptors for this subdevice
      }
    }
    descP++;
  }
  // Create device and channels
  while (numDescriptors>0) {
    // more channels for this subdevice number
    if (!newDev) {
      // standard EnoceanDevice is ok for 4BS
      newDev = EnoceanDevicePtr(new EnoceanDevice(aClassContainerP, numSubDevices));
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDevice);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      // first descriptor defines device color
      newDev->setPrimaryGroup(subdeviceDescP->group);
    }
    // create channel handler
    Enocean4bsHandlerPtr newHandler = Enocean4bsHandlerPtr(new Enocean4bsHandler(*newDev.get()));
    // assign descriptor
    newHandler->channelDescriptorP = subdeviceDescP;
    // create the behaviour
    switch (subdeviceDescP->behaviourType) {
      case behaviour_sensor: {
        SensorBehaviourPtr sb = SensorBehaviourPtr(new SensorBehaviour(*newDev.get()));
        int numBits = (subdeviceDescP->msBit-subdeviceDescP->lsBit)+1; // number of bits
        double resolution = (subdeviceDescP->max-subdeviceDescP->min) / ((1<<numBits)-1); // units per LSB
        sb->setHardwareSensorConfig((DsSensorType)subdeviceDescP->behaviourParam, subdeviceDescP->min, subdeviceDescP->max, resolution, subdeviceDescP->updateInterval*Second);
        sb->setGroup(subdeviceDescP->group);
        sb->setHardwareName(newHandler->shortDesc());
        newHandler->behaviour = sb;
        break;
      }
      case behaviour_binaryinput: {
        BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
        bb->setHardwareInputConfig((DsBinaryInputType)subdeviceDescP->behaviourParam, true, subdeviceDescP->updateInterval*Second);
        bb->setGroup(subdeviceDescP->group);
        bb->setHardwareName(newHandler->shortDesc());
        newHandler->behaviour = bb;
        break;
      }
      case behaviour_output: {
        // for now, plain output without scenes
        OutputBehaviourPtr ob = OutputBehaviourPtr(new OutputBehaviour(*newDev.get()));
        ob->setHardwareOutputConfig((DsOutputFunction)subdeviceDescP->behaviourParam, false, 0);
        ob->setHardwareName(newHandler->shortDesc());
        newHandler->behaviour = ob;
        break;
      }
      default: {
        break;
      }
    }
    // add channel to device
    newDev->addChannelHandler(newHandler);
    // next descriptor
    subdeviceDescP++;
    numDescriptors--;
  }
  // return updated total of subdevices for this profile
  if (aNumSubdevicesP) *aNumSubdevicesP = numSubDevices;
  // return device (or empty if none created)
  return newDev;
}


// handle incoming data from device and extract data for this channel
void Enocean4bsHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->eep_hasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eep_rorg()==rorg_4BS && aEsp3PacketPtr->radio_userDataLength()==4) {
      // only look at 4BS packets of correct length
      if (channelDescriptorP && channelDescriptorP->bitFieldHandler) {
        // create 32bit data word
        uint32_t data =
          (aEsp3PacketPtr->radio_userData()[0]<<24) |
          (aEsp3PacketPtr->radio_userData()[1]<<16) |
          (aEsp3PacketPtr->radio_userData()[2]<<8) |
          aEsp3PacketPtr->radio_userData()[3];
        // call bit field handler, will pass result to behaviour
        channelDescriptorP->bitFieldHandler(*this, false, data);
      }
    }
  }
};



string Enocean4bsHandler::shortDesc()
{
  return string_format("%s, %0.3f..%0.3f %s", channelDescriptorP->typeText, channelDescriptorP->min, channelDescriptorP->max, channelDescriptorP->unitText);
}





