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



#pragma mark - profile specific device subclasses


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
    // 2 or 4 rocker switch = 2 or 4 dsDevices
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
    aClassContainerP->addAndRemeberDevice(newDev);
    // - count it
    channel++;
  }
  // return number of devices created
  return channel;
}

