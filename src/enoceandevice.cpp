//
//  enoceandevice.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevice.hpp"

#include "enoceandevicecontainer.hpp"
#include "buttonbehaviour.hpp"

using namespace p44;


EnoceanDevice::EnoceanDevice(EnoceanDeviceContainer *aClassContainerP) :
  Device((DeviceClassContainer *)aClassContainerP),
  eeProfile(eep_profile_unknown),
  eeManufacturer(manufacturer_unknown)
{
}


EnoceanAddress EnoceanDevice::getAddress()
{
  return enoceanAddress;
}


EnoceanChannel EnoceanDevice::getChannel()
{
  return channel;
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
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS);
  // TODO: validate, now we are using the MAC-address class with:
  // - bits 48..51 set to 6
  // - bits 40..47 unused
  // - enOcean address encoded into bits 8..39
  // - bits 2..7 reserved for non-input channels
  // - channel (input index) encoded into bits 0..1
  dsid.setSerialNo(
    0x6000000000000ll+
    ((uint64_t)getAddress()<<8) +
    (getChannel()&0xFF)
  );
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


#pragma mark - profile specific device subclasses


/// RPS switches
class RpsEnoceanDevice : public EnoceanDevice
{
  typedef EnoceanDevice inherited;
  
  int numInputs; // total number of inputs in physical device containing this logical device

  bool pressed[2]; // true if currently pressed, false if released, index: 0=on/down button, 1=off/up button

public:
  RpsEnoceanDevice(EnoceanDeviceContainer *aClassContainerP, int aNumInputs) :
    inherited(aClassContainerP),
    numInputs(aNumInputs)
  {
    pressed[0] = false;
    pressed[1] = false;
  };

  virtual void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer)
  {
    inherited::setEEPInfo(aEEProfile, aEEManufacturer);
    // set the behaviour
    setBehaviour(new ButtonBehaviour(this, true));
  };

  // return number of inputs set at creation
  virtual int getNumInputs() { return numInputs; }

  // the channel corresponds to the dS input
  virtual int getInputIndex() { return getChannel(); }

  /// device specific radio packet handling
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
          // querying this channel
          // Note: this is for application style 1 (as used in EU, with 0-state up mount)
          setButtonState((data & 0x10)!=0, (a & 0x01) ? 1 : 0);
        }
      }
    }
    else {
      // U-Message
      uint8_t b = (data>>5) & 0x07;
      uint8_t numAffectedRockers = 0;
      if (status & status_T21) {
        // 2-rocker
        if (b==0)
          numAffectedRockers = getNumInputs(); // all affected
        else if(b==3)
          numAffectedRockers = 2; // 3 or 4 buttons -> both rockers affected
      }
      else {
        // 4-rocker
        if (b==0)
          numAffectedRockers = getNumInputs();
        else
          numAffectedRockers = (b+1)>>1; // half of buttons affected = switches affected
      }
      if (numAffectedRockers>0) {
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
      ButtonBehaviour *b = dynamic_cast<ButtonBehaviour *>(getDSBehaviour());
      if (b) {
        LOG(LOG_NOTICE,"RpsEnoceanDevice %08X channel %d: Button[%d] changed state to %s\n", getAddress(), getChannel(), aIndex, aPressed ? "pressed" : "released");
        b->buttonAction(aPressed, aIndex!=0);
      }
      // update cached status
      pressed[aIndex] = aPressed;
    }
  }

};



#pragma mark - device factories


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
    // 2 or 4 rocker switch = 2 or 4 channels
    numChannels = functionProfile==0xF60300 ? 4 : 2;
    // create device
    newDev = EnoceanDevicePtr(new RpsEnoceanDevice(aClassContainerP, numChannels));
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aChannel);
    // assign EPP information, device derives behaviour from this
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
  }
  if (aNumChannelsP) *aNumChannelsP = numChannels;
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
    aClassContainerP->addDevice(newDev);
    // - count it
    channel++;
  }
  // return number of devices created
  return channel;
}

