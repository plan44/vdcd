//
//  enoceanrps.cpp
//  vdcd
//
//  Created by Lukas Zeller on 26.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceanrps.hpp"

#include "buttonbehaviour.hpp"


using namespace p44;

EnoceanRpsHandler::EnoceanRpsHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
  switchIndex = 0; // default to first
  pressed = false;
}



EnoceanDevicePtr EnoceanRpsHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress, EnoceanSubDevice aSubDevice,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  EnoceanSubDevice &aNumSubdevices,
  bool aNeedsTeachInResponse
) {
  EnoceanDevicePtr newDev; // none so far
  aNumSubdevices = 1; // default to one
  EnoceanProfile functionProfile = aEEProfile & eep_ignore_type_mask;
  if (functionProfile==0xF60200 || functionProfile==0xF60300) {
    // 2 or 4 rocker switch = 2 or 4 dsDevices
    aNumSubdevices = functionProfile==0xF60300 ? 4 : 2;
    // create device, standard EnoceanDevice is ok for 4BS
    newDev = EnoceanDevicePtr(new EnoceanDevice(aClassContainerP, aNumSubdevices));
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aSubDevice);
    // assign EPP information
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
    // RPS switches can be used for anything
    newDev->setPrimaryGroup(group_black_joker);
    // Create two handlers, one for the up button, one for the down button
    // - create button input for down key
    EnoceanRpsHandlerPtr downHandler = EnoceanRpsHandlerPtr(new EnoceanRpsHandler(*newDev.get()));
    downHandler->switchIndex = aSubDevice; // each switch gets its own subdevice
    ButtonBehaviourPtr downBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
    downBhvr->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false);
    downBhvr->setGroup(group_yellow_light); // pre-configure for light
    downBhvr->setHardwareName("Down key");
    downHandler->behaviour = downBhvr;
    newDev->addChannelHandler(downHandler);
    // - create button input for up key
    EnoceanRpsHandlerPtr upHandler = EnoceanRpsHandlerPtr(new EnoceanRpsHandler(*newDev.get()));
    upHandler->switchIndex = aSubDevice; // each switch gets its own subdevice
    ButtonBehaviourPtr upBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
    upBhvr->setGroup(group_yellow_light); // pre-configure for light
    upBhvr->setHardwareButtonConfig(0, buttonType_2way, buttonElement_up, false);
    upBhvr->setHardwareName("Up key");
    upHandler->behaviour = upBhvr;
    newDev->addChannelHandler(upHandler);
  }
  // RPS never needs a teach-in response
  // return device (or empty if none created)
  return newDev;
}


// device specific radio packet handling
void EnoceanRpsHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  // extract payload data
  uint8_t data = aEsp3PacketPtr->radioUserData()[0];
  uint8_t status = aEsp3PacketPtr->radioStatus();
  // decode
  if (status & status_NU) {
    // N-Message
    // collect action(s)
    for (int ai=1; ai>=0; ai--) {
      uint8_t a = (data >> (4*ai+1)) & 0x07;
      if (ai==0 && (data&0x01)==0)
        break; // no second action
      if (((a>>1) & 0x03)==switchIndex) {
        // querying this subdevice/rocker
        if (((a & 0x01)!=0) == ((channel & 0x1)!=0))
          // my half of the rocker
          setButtonState((data & 0x10)!=0);
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
        setButtonState(false);
      }
    }
  }
}


void EnoceanRpsHandler::setButtonState(bool aPressed)
{
  // only propagate real changes
  if (aPressed!=pressed) {
    // real change, propagate to behaviour
    ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(behaviour);
    if (b) {
      LOG(LOG_NOTICE,"RpsEnoceanDevice %08X, subDevice %d, channel %d: changed state to %s\n", device.getAddress(), device.getSubDevice(), channel, aPressed ? "pressed" : "released");
      b->buttonAction(aPressed);
    }
    // update cached status
    pressed = aPressed;
  }
}



string EnoceanRpsHandler::shortDesc()
{
  return "Pushbutton";
}


