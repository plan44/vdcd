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
  pressed[0] = false;
  pressed[1] = false;
}



EnoceanDevicePtr EnoceanRpsHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress, EnoceanSubDevice aSubDevice,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  EnoceanSubDevice *aNumSubdevicesP
) {
  EnoceanDevicePtr newDev; // none so far
  int numSubDevices = 1; // default to one
  EnoceanProfile functionProfile = aEEProfile & eep_ignore_type_mask;
  if (functionProfile==0xF60200 || functionProfile==0xF60300) {
    // 2 or 4 rocker switch = 2 or 4 dsDevices
    numSubDevices = functionProfile==0xF60300 ? 4 : 2;
    // create device, standard EnoceanDevice is ok for 4BS
    newDev = EnoceanDevicePtr(new EnoceanDevice(aClassContainerP, numSubDevices));
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aSubDevice);
    // assign EPP information
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
    // Create two handlers, one for the up button, one for the down button
    // - create button input for down key
    EnoceanRpsHandlerPtr downHandler = EnoceanRpsHandlerPtr(new EnoceanRpsHandler(*newDev.get()));
    downHandler->switchIndex = aSubDevice; // each switch gets its own subdevice
    ButtonBehaviourPtr downBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
    downBhvr->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false);
    downBhvr->setHardwareName("Down key");
    downHandler->behaviour = downBhvr;
    newDev->addChannelHandler(downHandler);
    // - create button input for up key
    EnoceanRpsHandlerPtr upHandler = EnoceanRpsHandlerPtr(new EnoceanRpsHandler(*newDev.get()));
    upHandler->switchIndex = aSubDevice; // each switch gets its own subdevice
    ButtonBehaviourPtr upBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
    upBhvr->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false);
    upBhvr->setHardwareName("Up key");
    upHandler->behaviour = upBhvr;
    newDev->addChannelHandler(upHandler);
  }
  // return updated total of subdevices for this profile
  if (aNumSubdevicesP) *aNumSubdevicesP = numSubDevices;
  // return device (or empty if none created)
  return newDev;
}


// device specific radio packet handling
void EnoceanRpsHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
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
      if (((a>>1) & 0x03)==switchIndex) {
        // querying this subdevice/rocker
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
}


void EnoceanRpsHandler::setButtonState(bool aPressed, int aIndex)
{
  // only propagate real changes
  if (aPressed!=pressed[aIndex]) {
    // real change, propagate to behaviour
    ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(behaviour);
    if (b) {
      LOG(LOG_NOTICE,"RpsEnoceanDevice %08X, subDevice %d, channel %d: Button[%d] changed state to %s\n", device.getAddress(), device.getSubDevice(), channel, aIndex, aPressed ? "pressed" : "released");
      b->buttonAction(aPressed);
    }
    // update cached status
    pressed[aIndex] = aPressed;
  }
}



string EnoceanRpsHandler::shortDesc()
{
  return "Pushbutton";
}


