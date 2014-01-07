//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


/// decoder function
/// @param aDescriptor descriptor for data to extract
/// @param a4BSdata the 4BS data as 32-bit value, MSB=enocean DB_3, LSB=enocean DB_0
typedef void (*BitFieldHandlerFunc)(const Enocean4bsHandler &aHandler, bool aForSend, uint32_t &a4BSdata);

namespace p44 {

  /// descriptor flags
  typedef enum {
    dflag_none = 0,
    dflag_NeedsTeachInResponse = 0x1,
    dflag_alwaysUpdateable = 0x2, ///< device is always updateable (not only within 1sec after having sent a message)
    dflag_climatecontrolbehaviour = 0x4, ///< device should get climatecontrolbehaviour
  } DescriptorFlags;


  /// enocean sensor value descriptor
  typedef struct Enocean4BSDescriptor {
    uint8_t func; ///< the function code from the EPP signature
    uint8_t type; ///< the type code from the EPP signature
    uint8_t subDevice; ///< subdevice index, in case enOcean device needs to be split into multiple logical vdSDs
    DsGroup group; ///< the dS group for this channel
    BehaviourType behaviourType; ///< the behaviour type
    uint8_t behaviourParam; ///< DsSensorType, DsBinaryInputType or DsOutputFunction resp., depending on behaviourType
    DsUsageHint usage; ///< usage hint
    float min; ///< min value
    float max; ///< max value
    uint8_t msBit; ///< most significant bit of sensor value field in 4BS 32-bit word (31=Bit7 of DB_3, 0=Bit0 of DB_0)
    uint8_t lsBit; ///< least significant bit of sensor value field in 4BS 32-bit word (31=Bit7 of DB_3, 0=Bit0 of DB_0)
    double updateInterval; ///< normal update interval (average) in seconds
    BitFieldHandlerFunc bitFieldHandler; ///< function used to convert between bit field in 4BS telegram and engineering value for the behaviour
    const char *typeText;
    const char *unitText;
    uint32_t flags; ///< flags for special functions of device or channel
  } Enocean4BSDescriptor;

} // namespace p44

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
  // read only
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


/// standard output handler
static void stdOutputHandler(const Enocean4bsHandler &aHandler, bool aForSend, uint32_t &a4BSdata)
{
  const Enocean4BSDescriptor *descP = aHandler.channelDescriptorP;
  // write only
  if (descP && aForSend) {
    OutputBehaviourPtr ob = boost::dynamic_pointer_cast<OutputBehaviour>(aHandler.behaviour);
    if (ob) {
      int32_t newValue = ob->valueForHardware();
      // insert output into 32bit data
      int numBits = descP->msBit-descP->lsBit+1;
      long mask = ((1l<<numBits)-1)<<descP->lsBit;
      // - clear out previous bits
      a4BSdata &= ~mask;
      // - prepare new bits
      newValue = (newValue<<descP->lsBit) & mask;
      // - combine
      a4BSdata |= newValue;
    }
  }
}



static const char *tempText = "Temperature";
static const char *tempUnit = "°C";
static const char *humText = "Humidity";
static const char *humUnit = "%";

static const p44::Enocean4BSDescriptor enocean4BSdescriptors[] = {
  // Temperature sensors
  // - 40 degree range                 behaviour_binaryinput
  { 0x02, 0x01, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,    0, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x02, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -30,   10, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x03, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -20,   20, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x04, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,        -10,   30, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x05, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,          0,   40, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x06, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    10,   50, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x07, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    20,   60, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x08, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    30,   70, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x09, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    40,   80, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x0A, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    50,   90, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x0B, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    60,  100, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  // - 80 degree range
  { 0x02, 0x10, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -60,   20, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x11, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -50,   30, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x12, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40,   40, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x13, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -30,   50, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x14, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -20,   60, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x15, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,        -10,   70, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x16, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,          0,   80, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x17, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    10,   90, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x18, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    20,  100, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x19, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    30,  110, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x1A, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    40,  120, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x1B, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,    50,  130, DB(1,7), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  // - 10 bit
  { 0x02, 0x20, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,        -10, 42.2, DB(2,1), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x02, 0x30, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_undefined,   -40, 62.3, DB(2,1), DB(1,0), 100, &invSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  // Temperature and Humidity
  // - e.g. Alpha Sense
  { 0x04, 0x01, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,          0, 40.8, DB(1,7), DB(1,0), 100, &stdSensorHandler, tempText, tempUnit, dflag_climatecontrolbehaviour },
  { 0x04, 0x01, 0, group_blue_heating, behaviour_sensor,      sensorType_humitity,    usage_room,          0,  102, DB(2,7), DB(2,0), 100, &stdSensorHandler, humText,  humUnit,  dflag_climatecontrolbehaviour },

  // Room Panels
  // - e.g. Eltako FTR55D
  { 0x10, 0x06, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,          0,  40, DB(1,7), DB(1,0),  100, &invSensorHandler, tempText, tempUnit },
  { 0x10, 0x06, 0, group_blue_heating, behaviour_sensor,      sensorType_set_point,   usage_user,          0,   1, DB(2,7), DB(2,0),  100, &stdSensorHandler, "Set Point", "1", dflag_climatecontrolbehaviour },
  { 0x10, 0x06, 0, group_blue_heating, behaviour_binaryinput, binInpType_none,        usage_user,          0,   1, DB(0,0), DB(0,0),  100, &stdInputHandler, "Day/Night", "1", dflag_climatecontrolbehaviour },

  // HVAC heating valve actuators
  // - e.g. thermokon SAB 02 or Kieback+Peter MD15-FTL
  { 0x20, 0x01, 0, group_blue_heating, behaviour_sensor,      sensorType_temperature, usage_room,          0,  40, DB(1,7), DB(1,0),  100, &stdSensorHandler, tempText, tempUnit, dflag_NeedsTeachInResponse|dflag_climatecontrolbehaviour },
  { 0x20, 0x01, 0, group_blue_heating, behaviour_output,      outputFunction_positional, usage_room,       0, 100, DB(3,7), DB(3,0),  100, &stdOutputHandler, "Valve", "", dflag_NeedsTeachInResponse|dflag_climatecontrolbehaviour },

  // terminator
  { 0, 0, 0, group_black_joker, behaviour_undefined, 0, usage_undefined, 0, 0, 0, 0, 0, NULL /* NULL for extractor function terminates list */, NULL, NULL },
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
  EnoceanSubDevice &aNumSubdevices,
  bool aSendTeachInResponse
) {
  EepFunc func = EEP_FUNC(aEEProfile);
  EepType type = EEP_TYPE(aEEProfile);
  EnoceanDevicePtr newDev; // none so far
  aNumSubdevices = 0; // none found
  int numDescriptors = 0;
  // Search descriptors for this EEP, number of subdevices for this EEP and the start of channels for this subdevice
  const Enocean4BSDescriptor *descP = enocean4BSdescriptors;
  const Enocean4BSDescriptor *subdeviceDescP = NULL;
  while (descP->bitFieldHandler!=NULL) {
    if (descP->func==func && descP->type==type) {
      // update subdevice count
      aNumSubdevices = descP->subDevice+1;
      // remember if this is the subdevice we are looking for
      if (descP->subDevice==aSubDevice) {
        if (!subdeviceDescP) subdeviceDescP = descP; // remember the first descriptor of this subdevice
        numDescriptors++; // count descriptors for this subdevice
      }
    }
    descP++;
  }
  // Create device and channels
  bool needsTeachInResponse = false;
  while (numDescriptors>0) {
    // more channels for this subdevice number
    if (!newDev) {
      // create device
      newDev = EnoceanDevicePtr(new Enocean4BSDevice(aClassContainerP, aNumSubdevices));
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDevice);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      // first descriptor defines device primary color
      newDev->setPrimaryGroup(subdeviceDescP->group);
      // set flag for teach-in response
      if (subdeviceDescP->flags & dflag_NeedsTeachInResponse) {
        // needs a teach-in response (will be created after installing handlers)
        needsTeachInResponse = true;
      }
    }
    // set updateable status
    if (subdeviceDescP->flags & dflag_alwaysUpdateable) {
      newDev->setAlwaysUpdateable();
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
        sb->setHardwareSensorConfig((DsSensorType)subdeviceDescP->behaviourParam, subdeviceDescP->usage, subdeviceDescP->min, subdeviceDescP->max, resolution, subdeviceDescP->updateInterval*Second);
        if (subdeviceDescP->flags & dflag_climatecontrolbehaviour)
          sb->setGroup(group_roomtemperature_control);
        else
          sb->setGroup(subdeviceDescP->group);
        sb->setHardwareName(newHandler->shortDesc());
        newHandler->behaviour = sb;
        break;
      }
      case behaviour_binaryinput: {
        BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
        bb->setHardwareInputConfig((DsBinaryInputType)subdeviceDescP->behaviourParam, subdeviceDescP->usage, true, subdeviceDescP->updateInterval*Second);
        bb->setGroup(subdeviceDescP->group);
        bb->setHardwareName(newHandler->shortDesc());
        newHandler->behaviour = bb;
        break;
      }
      case behaviour_output: {
        OutputBehaviourPtr ob;
        if (subdeviceDescP->flags & dflag_climatecontrolbehaviour) {
          // climate control output, use special behaviour
          ob = OutputBehaviourPtr(new ClimateControlBehaviour(*newDev.get()));
          ob->setGroup(group_roomtemperature_control); // put into room temperature control group by default
        }
        else {
          // generic output, no scenes or special behaviour
          ob = OutputBehaviourPtr(new OutputBehaviour(*newDev.get()));
          ob->setGroup(subdeviceDescP->group); // same group as device
        }
        ob->setHardwareOutputConfig((DsOutputFunction)subdeviceDescP->behaviourParam, subdeviceDescP->usage, false, 0);
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
  // create the teach-in response if one is required
  if (newDev && aSendTeachInResponse && needsTeachInResponse) {
    newDev->sendTeachInResponse();
  }
  // return device (or empty if none created)
  return newDev;
}


// handle incoming data from device and extract data for this channel
void Enocean4bsHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->eepHasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eepRorg()==rorg_4BS && aEsp3PacketPtr->radioUserDataLength()==4) {
      // only look at 4BS packets of correct length
      if (channelDescriptorP && channelDescriptorP->bitFieldHandler) {
        // create 32bit data word
        uint32_t data = aEsp3PacketPtr->get4BSdata();
        // call bit field handler, will pass result to behaviour
        channelDescriptorP->bitFieldHandler(*this, false, data);
      }
    }
  }
};



/// collect data for outgoing message from this channel
/// @param aEsp3PacketPtr must be set to a suitable packet if it is empty, or packet data must be augmented with
///   channel's data when packet already exists
/// @note non-outputs will do nothing in this method
void Enocean4bsHandler::collectOutgoingMessageData(Esp3PacketPtr &aEsp3PacketPtr)
{
  OutputBehaviourPtr ob = boost::dynamic_pointer_cast<OutputBehaviour>(behaviour);
  if (ob) {
    // create packet if none created already
    uint32_t data;
    if (!aEsp3PacketPtr) {
      aEsp3PacketPtr = Esp3PacketPtr(new Esp3Packet());
      aEsp3PacketPtr->initForRorg(rorg_4BS);
      // new packet, start with zero data except for LRN bit (D0.3) which must be set for ALL non-learn data
      data = LRN_BIT_MASK;
    }
    else {
      // packet exists, get already collected data to modify
      data = aEsp3PacketPtr->get4BSdata();
    }
    // call bit field handler, will insert the bits into the output
    channelDescriptorP->bitFieldHandler(*this, true, data);
    // save data
    aEsp3PacketPtr->set4BSdata(data);
    // value from this channel is applied to the outgoing telegram
    ob->outputValueApplied();
  }
}



string Enocean4bsHandler::shortDesc()
{
  return string_format("%s, %0.3f..%0.3f %s", channelDescriptorP->typeText, channelDescriptorP->min, channelDescriptorP->max, channelDescriptorP->unitText);
}


#pragma mark - Enocean4BSDevice

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
    getEnoceanDeviceContainer().enoceanComm.sendPacket(responsePacket);
  }

}




