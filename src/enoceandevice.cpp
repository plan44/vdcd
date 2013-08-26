//
//  enoceandevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevice.hpp"

#include "enoceandevicecontainer.hpp"

#include "buttonbehaviour.hpp"
#include "sensorbehaviour.hpp"


using namespace p44;


EnoceanDevice::EnoceanDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanChannel aNumChannels) :
  Device((DeviceClassContainer *)aClassContainerP),
  eeProfile(eep_profile_unknown),
  eeManufacturer(manufacturer_unknown),
	numChannels(aNumChannels)
{
}


EnoceanDeviceContainer &EnoceanDevice::getEnoceanDeviceContainer()
{
  return *(static_cast<EnoceanDeviceContainer *>(classContainerP));
}


EnoceanAddress EnoceanDevice::getAddress()
{
  return enoceanAddress;
}


EnoceanChannel EnoceanDevice::getChannel()
{
  return channel;
}


EnoceanChannel EnoceanDevice::getNumChannels()
{
	return numChannels;
}



void EnoceanDevice::setAddressingInfo(EnoceanAddress aAddress, EnoceanChannel aChannel)
{
  enoceanAddress = aAddress;
  channel = aChannel;
  deriveDSID();
}


void EnoceanDevice::setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer)
{
  eeProfile = aEEProfile;
  eeManufacturer = aEEManufacturer;
}


EnoceanProfile EnoceanDevice::getEEProfile()
{
  return eeProfile;
}


EnoceanManufacturer EnoceanDevice::getEEManufacturer()
{
  return eeManufacturer;
}



void EnoceanDevice::deriveDSID()
{
  #if FAKE_REAL_DSD_IDS
  dsid.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
  dsid.setSerialNo(
    ((uint64_t)getAddress()<<4) + // 32 upper bits, 4..35
    (getChannel()&0x0F) // 4 lower bits for up to 16 channels
  );
  #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
  #else
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS);
  // TODO: validate, now we are using the MAC-address class with:
  // - bits 48..51 set to 6
  // - bits 40..47 unused
  // - enOcean address encoded into bits 8..39
  // - channel encoded into bits 0..7 (max 255 channels)
	//   Note: this conforms to the dS convention which mandates that multi-input physical
	//   devices (up to 4) must have adjacent dsids.
  dsid.setSerialNo(
    0x6000000000000ll+
    ((uint64_t)getAddress()<<8) +
    (getChannel()&0xFF)
  );
  #endif
}


string EnoceanDevice::hardwareGUID()
{
  // GTIN is 24bit company prefix + 20bit item reference, SGTIN adds a 48bit serial number as third element: urn:epc:id:sgtin:COMPANYPREFIX.ITEMREF.SERIALNO
  // TODO: create a GTIN if there is an official scheme for it
  return string_format("enoceanaddress:%ld", getAddress());
}




string EnoceanDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- Enocean Address = 0x%08lX, channel=%d\n", enoceanAddress, channel);
  string_format_append(s,
    "- EEP RORG/FUNC/TYPE: %02X %02X %02X, Manufacturer Code = %03X\n",
    (eeProfile>>16) & 0xFF,
    (eeProfile>>8) & 0xFF,
    eeProfile & 0xFF,
    eeManufacturer
  );
  return s;
}



void EnoceanDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // clear learn-in data from DB
  getEnoceanDeviceContainer().db.executef("DELETE FROM knownDevices WHERE enoceanAddress=%d AND channel=%d", getAddress(), getChannel());
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}



#pragma mark - RpsEnoceanDevice - rocker switches (RPS = Repeated Switch)


/// RPS switches
class RpsEnoceanDevice : public EnoceanDevice
{
  typedef EnoceanDevice inherited;
  
  bool pressed[2]; // true if currently pressed, false if released, index: 0=on/down button, 1=off/up button

public:
  RpsEnoceanDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanChannel aNumChannels) :
    inherited(aClassContainerP, aNumChannels)
  {
    pressed[0] = false;
    pressed[1] = false;
  };

  virtual void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer)
  {
    inherited::setEEPInfo(aEEProfile, aEEManufacturer);
    // create two behaviours, one for the up button, one for the down button
    // - create button input for down key
    ButtonBehaviourPtr bd = ButtonBehaviourPtr(new ButtonBehaviour(*this,buttons.size()));
    bd->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false);
    bd->setHardwareName("Down key");
    buttons.push_back(bd);
    // - create button input for up key
    ButtonBehaviourPtr bu = ButtonBehaviourPtr(new ButtonBehaviour(*this,buttons.size()));
    bu->setHardwareButtonConfig(0, buttonType_2way, buttonElement_up, false);
    bu->setHardwareName("Up key");
    buttons.push_back(bu);
  };

  // device specific radio packet handling
  virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
  {
    // extract payload data
    uint8_t data = aEsp3PacketPtr->radio_userData()[0];
    uint8_t status = aEsp3PacketPtr->radio_status();
    // decode
    if (status & status_NU) {
      // N-Message
      // collect action(s)
      for (int ai=1; ai>=0; ai--) {
        uint8_t a = (data >> (4*ai+1)) & 0x07;
        if (ai==0 && (data&0x01)==0)
          break; // no second action
        if (((a>>1) & 0x03)==getChannel()) {
          // querying this channel/rocker
          setButtonState((data & 0x10)!=0, (a & 0x01) ? 1 : 0);
        }
      }
    }
    else {
      // U-Message
      uint8_t b = (data>>5) & 0x07;
      bool affectsMe = false;
      if (status & status_T21) {
        // 2-rocker
        if (b==0 || b==3)
          affectsMe = true; // all buttons or explicitly 3/4 affected
      }
      else {
        // 4-rocker
        if (b==0 || ((b+1)>>1)>0)
          affectsMe = true; // all or half of buttons affected = switches affected
      }
      if (affectsMe) {
        // releasing -> affect all
        // pressing -> ignore
        // Note: rationale is that pressing should create individual actions, while releasing does not
        if ((data & 0x10)!=0) {
          // pressed
          // NOP, ignore ambiguous pressing of more buttons
        }
        else {
          // released
          // assume both buttons (both sides of the rocker) released
          setButtonState(false, 0);
          setButtonState(false, 1);
        }
      }
    }
  };

private:
  void setButtonState(bool aPressed, int aIndex)
  {
    // only propagate real changes
    if (aPressed!=pressed[aIndex]) {
      // real change, propagate to behaviour
      ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(buttons[aIndex]);
      if (b) {
        LOG(LOG_NOTICE,"RpsEnoceanDevice %08X channel %d: Button[%d] changed state to %s\n", getAddress(), getChannel(), aIndex, aPressed ? "pressed" : "released");
        b->buttonAction(aPressed);
      }
      // update cached status
      pressed[aIndex] = aPressed;
    }
  }

};


#pragma mark - TempSensorEnoceanDevice - single temperature sensors

typedef const struct {
  uint8_t type; ///< enocean type
  float min; ///< min temperature in degrees celsius
  float max; ///< max temperature in degrees celsius
  uint8_t numbits; ///< number of bits used for the range
} TempSensorDesc;

static const TempSensorDesc tempSensorDesc[] = {
  // 40 degree range
  { 0x01, -40, 0, 8 },
  { 0x02, -30, 10, 8 },
  { 0x03, -20, 20, 8 },
  { 0x04, -10, 30, 8 },
  { 0x05, 0, 40, 8 },
  { 0x06, 10, 50, 8 },
  { 0x07, 20, 60, 8 },
  { 0x08, 30, 70, 8 },
  { 0x09, 40, 80, 8 },
  { 0x0A, 50, 90, 8 },
  { 0x0B, 60, 100, 8 },
  // 80 degree range
  { 0x10, -60, 20, 8 },
  { 0x11, -50, 30, 8 },
  { 0x12, -40, 40, 8 },
  { 0x13, -30, 50, 8 },
  { 0x14, -20, 60, 8 },
  { 0x15, -10, 70, 8 },
  { 0x16, 0, 80, 8 },
  { 0x17, 10, 90, 8 },
  { 0x18, 20, 100, 8 },
  { 0x19, 30, 110, 8 },
  { 0x1A, 40, 120, 8 },
  { 0x1B, 50, 130, 8 },
  // 10 bit
  { 0x20, -10, 42.2, 10 },
  { 0x30, -40, 62.3, 10 },
  { 0, 0, 0, 0 } // terminator
};

/// Temperature sensors
class TempSensorEnoceanDevice : public EnoceanDevice
{
  typedef EnoceanDevice inherited;


public:
  TempSensorEnoceanDevice(EnoceanDeviceContainer *aClassContainerP) :
    inherited(aClassContainerP, 1)
  {
    // temp sensor devices are climate-related
    setPrimaryGroup(group_blue_climate);
  };

  virtual void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer)
  {
    inherited::setEEPInfo(aEEProfile, aEEManufacturer);
    // create one sensor behaviour
    SensorBehaviourPtr sb = SensorBehaviourPtr(new SensorBehaviour(*this,sensors.size()));
    // find the ranges
    const TempSensorDesc *tP = tempSensorDesc;
    while (tP->numbits!=0) {
      if (tP->type == getEEPType())
        break; // found
      tP++;
    }
    if (tP->numbits!=0) {
      // found sensor type
      double resolution = (tP->max-tP->min) / ((1<<tP->numbits)-1); // units per LSB
      sb->setHardwareSensorConfig(sensorType_temperature, tP->min, tP->max, resolution, 15*Second);
      sb->setGroup(group_blue_climate); 
    }
    sb->setHardwareName(string_format("Temperature %d..%d °C",(int)tP->min,(int)tP->max));
    sensors.push_back(sb);
  };

  // device specific radio packet handling
  virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
  {
    if (!aEsp3PacketPtr->eep_hasTeachInfo()) {
      // only look at non-teach-in packets
      // extract payload data
      uint16_t engValue;
      if (getEEPType()>=0x20) {
        // 10 bit sensors with MSB in data[1] and LSB in data[2]
        engValue =
          (~aEsp3PacketPtr->radio_userData()[2] & 0xFF) +
          (((uint16_t)(~aEsp3PacketPtr->radio_userData()[1] & 0x03))<<8);
      }
      else {
        // 8 bit sensors with inverted value in data[2]
        engValue = (~aEsp3PacketPtr->radio_userData()[2])&0xFF;
      }
      // report value
      LOG(LOG_DEBUG,"Temperature sensor reported engineering value %d\n", engValue);
      if (sensors.size()>0) {
        SensorBehaviourPtr sb = boost::dynamic_pointer_cast<SensorBehaviour>(sensors[0]);
        if (sb) {
          sb->updateEngineeringValue(engValue);
        }
      }
    }
  };

};


#pragma mark - RoompanelSensorEnoceanDevice - room operating panel devices





#pragma mark - device factory


EnoceanDevicePtr EnoceanDevice::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress, EnoceanChannel aChannel,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  int *aNumChannelsP
) {
  int numChannels = 1; // default to one
  EnoceanDevicePtr newDev;
  EnoceanProfile functionProfile = aEEProfile & eep_ignore_type_mask;
  if (functionProfile==0xF60200 || functionProfile==0xF60300) {
    // 2 or 4 rocker switch = 2 or 4 dsDevices
    numChannels = functionProfile==0xF60300 ? 4 : 2;
    // create device
    newDev = EnoceanDevicePtr(new RpsEnoceanDevice(aClassContainerP, numChannels));
  }
  else if (functionProfile==0xA50200) {
    // temperature sensors are single channel
    numChannels = 1;
    // temperature sensor devices
    newDev = EnoceanDevicePtr(new TempSensorEnoceanDevice(aClassContainerP));
  }
  // now assign
  if (newDev) {
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aChannel);
    // assign EPP information, device derives behaviour from this
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
  }
  // return updated total of channels
  if (aNumChannelsP) *aNumChannelsP = numChannels;
  // return device (or empty if none created)
  return newDev;
}



int EnoceanDevice::createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, Esp3PacketPtr aLearnInPacket)
{
  int totalChannels = 1; // at least one
  int channel = 0;
  while (channel<totalChannels) {
    EnoceanDevicePtr newDev = newDevice(
      aClassContainerP,
      aLearnInPacket->radio_sender(), channel,
      aLearnInPacket->eep_profile(), aLearnInPacket->eep_manufacturer(),
      &totalChannels // possibly update total
    );
    if (!newDev) {
      // could not create a device
      break;
    }
    // created device
    // - add it to the container
    aClassContainerP->addAndRemeberDevice(newDev);
    // - count it
    channel++;
  }
  // return number of devices created
  return channel;
}

