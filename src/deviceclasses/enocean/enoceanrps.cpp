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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0

#include "enoceanrps.hpp"

#include "buttonbehaviour.hpp"
#include "binaryinputbehaviour.hpp"


using namespace p44;

#pragma mark - EnoceanRPSDevice

EnoceanRPSDevice::EnoceanRPSDevice(EnoceanDeviceContainer *aClassContainerP) :
  inherited(aClassContainerP)
{
}


#pragma mark - EnoceanRpsHandler

EnoceanRpsHandler::EnoceanRpsHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}


EnoceanDevicePtr EnoceanRpsHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice &aSubDeviceIndex,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aNeedsTeachInResponse
) {
  EnoceanDevicePtr newDev; // none so far
  EnoceanProfile functionProfile = EEP_UNTYPED(aEEProfile);
  if (EEP_PURE(functionProfile)==0xF60200 || EEP_PURE(functionProfile)==0xF60300) {
    // F6-02-xx or F6-03-xx: 2 or 4 rocker switch
    // - we have the standard rocker variant (0) or the separate buttons variant (1)
    // - subdevice index range is always 4 (or 8 for 4-rocker), but for 2-way only every other subdevice index is used
    EnoceanSubDevice numSubDevices = functionProfile==0xF60300 ? 8 : 4;
    if (EEP_VARIANT(aEEProfile)==1) {
      // Custom variant: up and down are treated as separate buttons -> max 4 or 8 dsDevices
      if (aSubDeviceIndex<numSubDevices) {
        // create EnoceanRPSDevice device
        newDev = EnoceanDevicePtr(new EnoceanRPSDevice(aClassContainerP));
        // standard device settings without scene table
        newDev->installSettings();
        // assign channel and address
        newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
        // assign EPP information
        newDev->setEEPInfo(aEEProfile, aEEManufacturer);
        newDev->setFunctionDesc("button");
        // set icon name: 4-rocker have a generic icon, 2-rocker have the 4btn icon in separated mode
        newDev->setIconInfo(functionProfile==0xF60300 ? "enocean_4rkr" : "enocean_4btn", true);
        // RPS switches can be used for anything
        newDev->setPrimaryGroup(group_black_joker);
        // Create single handler, up button for even aSubDevice, down button for odd aSubDevice
        bool isUp = (aSubDeviceIndex & 0x01)==0;
        EnoceanRpsButtonHandlerPtr buttonHandler = EnoceanRpsButtonHandlerPtr(new EnoceanRpsButtonHandler(*newDev.get()));
        buttonHandler->switchIndex = aSubDeviceIndex/2; // subdevices are half-switches, so switch index == subDeviceIndex/2
        buttonHandler->isRockerUp = isUp;
        ButtonBehaviourPtr buttonBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
        buttonBhvr->setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0, true); // fixed mode
        buttonBhvr->setGroup(group_yellow_light); // pre-configure for light
        buttonBhvr->setHardwareName(isUp ? "upper key" : "lower key");
        buttonHandler->behaviour = buttonBhvr;
        newDev->addChannelHandler(buttonHandler);
        // count it
        // - separate buttons use all indices 0,1,2,3...
        aSubDeviceIndex++;
      }
    }
    else {
      // Standard: Up+Down together form a  2-way rocker
      if (aSubDeviceIndex<numSubDevices) {
        // create EnoceanRPSDevice device
        newDev = EnoceanDevicePtr(new EnoceanRPSDevice(aClassContainerP));
        // standard device settings without scene table
        newDev->installSettings();
        // assign channel and address
        newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
        // assign EPP information
        newDev->setEEPInfo(aEEProfile, aEEManufacturer);
        newDev->setFunctionDesc("rocker switch");
        // set icon name: generic 4-rocker or for 2-rocker: even-numbered subdevice is left, odd is right
        newDev->setIconInfo(functionProfile==0xF60300 ? "enocean_4rkr" : (aSubDeviceIndex & 0x02 ? "enocean_br" : "enocean_bl"), true);
        // RPS switches can be used for anything
        newDev->setPrimaryGroup(group_black_joker);
        // Create two handlers, one for the up button, one for the down button
        // - create button input for down key
        EnoceanRpsButtonHandlerPtr downHandler = EnoceanRpsButtonHandlerPtr(new EnoceanRpsButtonHandler(*newDev.get()));
        downHandler->switchIndex = aSubDeviceIndex/2; // subdevices are half-switches, so switch index == subDeviceIndex/2
        downHandler->isRockerUp = false;
        ButtonBehaviourPtr downBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
        downBhvr->setHardwareButtonConfig(0, buttonType_2way, buttonElement_down, false, 1, true); // counterpart up-button has buttonIndex 1, fixed mode
        downBhvr->setGroup(group_yellow_light); // pre-configure for light
        downBhvr->setHardwareName("down key");
        downHandler->behaviour = downBhvr;
        newDev->addChannelHandler(downHandler);
        // - create button input for up key
        EnoceanRpsButtonHandlerPtr upHandler = EnoceanRpsButtonHandlerPtr(new EnoceanRpsButtonHandler(*newDev.get()));
        upHandler->switchIndex = aSubDeviceIndex/2; // subdevices are half-switches, so switch index == subDeviceIndex/2
        upHandler->isRockerUp = true;
        ButtonBehaviourPtr upBhvr = ButtonBehaviourPtr(new ButtonBehaviour(*newDev.get()));
        upBhvr->setGroup(group_yellow_light); // pre-configure for light
        upBhvr->setHardwareButtonConfig(0, buttonType_2way, buttonElement_up, false, 0, true); // counterpart down-button has buttonIndex 0, fixed mode
        upBhvr->setHardwareName("up key");
        upHandler->behaviour = upBhvr;
        newDev->addChannelHandler(upHandler);
        // count it
        // - 2-way rocker switches use indices 0,2,4,6,... to leave room for separate button mode without shifting indices
        aSubDeviceIndex+=2;
      }
    }
  }
  else if (functionProfile==0xF61000 || functionProfile==0xF61001) {
    // F6-10-00/01 : Window handle = single device
    if (aSubDeviceIndex<1) {
      // create EnoceanRPSDevice device
      newDev = EnoceanDevicePtr(new EnoceanRPSDevice(aClassContainerP));
      // standard device settings without scene table
      newDev->installSettings();
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      newDev->setFunctionDesc("window handle");
      // Window handle switches can be used for anything
      newDev->setPrimaryGroup(group_black_joker);
      // Current simple dS mapping: two binary inputs
      // - Input0: 0: Window closed (Handle down position), 1: Window open (all other handle positions)
      EnoceanRpsWindowHandleHandlerPtr newHandler = EnoceanRpsWindowHandleHandlerPtr(new EnoceanRpsWindowHandleHandler(*newDev.get()));
      BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
      bb->setHardwareInputConfig(binInpType_windowOpen, usage_undefined, true, Never);
      bb->setGroup(group_black_joker); // joker by default
      bb->setHardwareName("window open");
      newHandler->isERP2 = EEP_TYPE(functionProfile)==0x01;
      newHandler->isTiltedStatus = false;
      newHandler->behaviour = bb;
      newDev->addChannelHandler(newHandler);
      // - Input1: 0: Window fully open (Handle horizontal left or right), 1: Window tilted (Handle up position)
      newHandler = EnoceanRpsWindowHandleHandlerPtr(new EnoceanRpsWindowHandleHandler(*newDev.get()));
      bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
      bb->setHardwareInputConfig(binInpType_windowTilted, usage_undefined, true, Never);
      bb->setGroup(group_black_joker); // joker by default
      bb->setHardwareName("window tilted");
      newHandler->isERP2 = EEP_TYPE(functionProfile)==0x01;
      newHandler->isTiltedStatus = true;
      newHandler->behaviour = bb;
      newDev->addChannelHandler(newHandler);
      // count it
      aSubDeviceIndex++;
    }
  }
  else if (functionProfile==0xF60400) {
    // F6-04-01, F6-04-02, F6-04-C0 : key card activated switch = single device
    // Note: F6-04-C0 is custom pseudo-EEP for not officially defined Eltako FKC/FKF card switches
    if (aSubDeviceIndex<1) {
      // create EnoceanRPSDevice device
      newDev = EnoceanDevicePtr(new EnoceanRPSDevice(aClassContainerP));
      // standard device settings without scene table
      newDev->installSettings();
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      newDev->setFunctionDesc("key card switch");
      // key card switches can be used for anything
      newDev->setPrimaryGroup(group_black_joker);
      // Current simple dS mapping: one binary input
      // - Input0: 1: card inserted, 0: card extracted
      EnoceanRpsCardKeyHandlerPtr newHandler = EnoceanRpsCardKeyHandlerPtr(new EnoceanRpsCardKeyHandler(*newDev.get()));
      BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
      bb->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
      bb->setGroup(group_black_joker); // joker by default
      bb->setHardwareName("card inserted");
      newHandler->isServiceCardDetector = false;
      newHandler->behaviour = bb;
      newDev->addChannelHandler(newHandler);
      // FKC/FKF can distinguish guest and service cards and have a second input
      if (aEEProfile==0xF604C0) {
        // - Input1: 1: card is service card, 0: card is guest card
        EnoceanRpsCardKeyHandlerPtr newHandler = EnoceanRpsCardKeyHandlerPtr(new EnoceanRpsCardKeyHandler(*newDev.get()));
        BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
        bb->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never);
        bb->setGroup(group_black_joker); // joker by default
        bb->setHardwareName("service card");
        newHandler->isServiceCardDetector = true;
        newHandler->behaviour = bb;
        newDev->addChannelHandler(newHandler);
      }
      // count it
      aSubDeviceIndex++;
    }
  }
  else if (aEEProfile==0xF60501) {
    // F6-05-01 - Liquid Leakage Detector
    if (aSubDeviceIndex<1) {
      // create EnoceanRPSDevice device
      newDev = EnoceanDevicePtr(new EnoceanRPSDevice(aClassContainerP));
      // standard device settings without scene table
      newDev->installSettings();
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      newDev->setFunctionDesc("leakage detector");
      // leakage detectors can be used for anything
      newDev->setPrimaryGroup(group_black_joker);
      // Current simple dS mapping: one binary input for leakage status
      EnoceanRpsLeakageDetectorHandlerPtr newHandler;
      BinaryInputBehaviourPtr bb;
      // - 1: Leakage: 0: no leakage
      newHandler = EnoceanRpsLeakageDetectorHandlerPtr(new EnoceanRpsLeakageDetectorHandler(*newDev.get()));
      bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
      bb->setHardwareInputConfig(binInpType_none, usage_undefined, true, Never); // generic because dS does not have a binary sensor function for leakage yet
      bb->setGroup(group_black_joker); // joker by default
      bb->setHardwareName("leakage detector");
      newHandler->behaviour = bb;
      newDev->addChannelHandler(newHandler);
      // count it
      aSubDeviceIndex++;
    }
  }
  else if (aEEProfile==0xF605C0) {
    // F6-05-xx - EEP for "detectors"
    // F6-05-C0 - custom pseudo-EEP for not yet defined smoke alarm profile (Eltako FRW and alphaEOS GUARD)
    if (aSubDeviceIndex<1) {
      // create EnoceanRPSDevice device
      newDev = EnoceanDevicePtr(new EnoceanRPSDevice(aClassContainerP));
      // standard device settings without scene table
      newDev->installSettings();
      // assign channel and address
      newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
      // assign EPP information
      newDev->setEEPInfo(aEEProfile, aEEManufacturer);
      newDev->setFunctionDesc("smoke detector");
      // smoke detectors can be used for anything
      newDev->setPrimaryGroup(group_black_joker);
      // Current simple dS mapping: one binary input for smoke alarm status, one for low bat status
      EnoceanRpsSmokeDetectorHandlerPtr newHandler;
      BinaryInputBehaviourPtr bb;
      // - Alarm: 1: Alarm, 0: no Alarm
      newHandler = EnoceanRpsSmokeDetectorHandlerPtr(new EnoceanRpsSmokeDetectorHandler(*newDev.get()));
      bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
      bb->setHardwareInputConfig(binInpType_smoke, usage_room, true, Never);
      bb->setGroup(group_black_joker); // joker by default
      bb->setHardwareName("smoke alarm");
      newHandler->behaviour = bb;
      newHandler->isBatteryStatus = false;
      newDev->addChannelHandler(newHandler);
      // - Low Battery: 1: battery low, 0: battery OK
      newHandler = EnoceanRpsSmokeDetectorHandlerPtr(new EnoceanRpsSmokeDetectorHandler(*newDev.get()));
      bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
      bb->setHardwareInputConfig(binInpType_lowBattery, usage_room, true, Never);
      bb->setGroup(group_black_joker); // joker by default
      bb->setHardwareName("low battery");
      newHandler->behaviour = bb;
      newHandler->isBatteryStatus = true;
      newDev->addChannelHandler(newHandler);
      // count it
      aSubDeviceIndex++;
    }
  }
  // RPS never needs a teach-in response
  // return device (or empty if none created)
  return newDev;
}


#pragma mark - button

EnoceanRpsButtonHandler::EnoceanRpsButtonHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
  switchIndex = 0; // default to first
  pressed = false;
}


// device specific radio packet handling
void EnoceanRpsButtonHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  // extract payload data
  uint8_t data = aEsp3PacketPtr->radioUserData()[0];
  uint8_t status = aEsp3PacketPtr->radioStatus();
  LOG(LOG_INFO, "RPS message: data=0x%02X, status=0x%02X, processing in %s (switchIndex=%d, isRockerUp=%d)\n", data, status, device.shortDesc().c_str(), switchIndex, isRockerUp);
  // decode
  if (status & status_NU) {
    // N-Message
    FOCUSLOG("- N-message\n");
    // collect action(s)
    for (int ai=1; ai>=0; ai--) {
      // first action is in DB7..5, second action is in DB3..1 (if DB0==1)
      uint8_t a = (data >> (4*ai+1)) & 0x07;
      if (ai==0 && (data&0x01)==0)
        break; // no second action
      FOCUSLOG("- action #%d = %d\n", 2-ai, a);
      if (((a>>1) & 0x03)==switchIndex) {
        // querying this subdevice/rocker
        FOCUSLOG("- is my switchIndex == %d\n", switchIndex);
        if (((a & 0x01)!=0) == isRockerUp) {
          FOCUSLOG("- is my side (%s) of the switch, isRockerUp == %d\n", isRockerUp ? "Up" : "Down", isRockerUp);
          // my half of the rocker, DB4 is button state (1=pressed, 0=released)
          setButtonState((data & 0x10)!=0);
        }
      }
    }
  }
  else {
    // U-Message
    FOCUSLOG("- U-message\n");
    uint8_t b = (data>>5) & 0x07;
    bool pressed = (data & 0x10);
    FOCUSLOG("- number of buttons still pressed code = %d, action (energy bow) = %s\n", b, pressed ? "PRESSED" : "RELEASED");
    if (!pressed) {
      // report release if all buttons are released now
      if (b==0) {
        // all buttons released, this includes this button
        FOCUSLOG("- released multiple buttons, report RELEASED for all\n");
        setButtonState(false);
      }
    }
    // ignore everything else (more that 2 press actions simultaneously)
  }
}


void EnoceanRpsButtonHandler::setButtonState(bool aPressed)
{
  // only propagate real changes
  if (aPressed!=pressed) {
    // real change, propagate to behaviour
    ButtonBehaviourPtr b = boost::dynamic_pointer_cast<ButtonBehaviour>(behaviour);
    if (b) {
      LOG(LOG_INFO,"Enocean Button %s - %08X, subDevice %d, channel %d: changed state to %s\n", b->getHardwareName().c_str(), device.getAddress(), device.getSubDevice(), channel, aPressed ? "PRESSED" : "RELEASED");
      b->buttonAction(aPressed);
    }
    // update cached status
    pressed = aPressed;
  }
}



string EnoceanRpsButtonHandler::shortDesc()
{
  return "Pushbutton";
}


#pragma mark - window handle


EnoceanRpsWindowHandleHandler::EnoceanRpsWindowHandleHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
  isTiltedStatus = false; // default to "open" status
  isERP2 = false; // default to ERP1
}





// device specific radio packet handling
void EnoceanRpsWindowHandleHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  // extract payload data
  uint8_t data = aEsp3PacketPtr->radioUserData()[0];
  uint8_t status = aEsp3PacketPtr->radioStatus();
  // decode
  bool tilted = false;
  bool closed = false;
  if (isERP2) {
    // extract status (in bits 0..3 for ERP2)
    tilted = (data & 0x0F)==0x0D; // turned up from sideways
    closed = (data & 0x0F)==0x0F; // turned down from sideways
  }
  else if ((status & status_NU)==0 && (status & status_T21)!=0) {
    // Valid ERP1 window handle status change message
    // extract status (in bits 4..7 for ERP1
    tilted = (data & 0xF0)==0xD0; // turned up from sideways
    closed = (data & 0xF0)==0xF0; // turned down from sideways
  }
  else {
    return; // unknown data, don't update binary inputs at all
  }
  // report data for this binary input
  BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(behaviour);
  if (bb) {
    if (isTiltedStatus) {
      LOG(LOG_INFO,"Enocean Window Handle %08X reports state: %s\n", device.getAddress(), closed ? "closed" : (tilted ? "tilted open" : "fully open"));
      bb->updateInputState(tilted); // report the tilted status
    }
    else {
      bb->updateInputState(!closed); // report the open/close status (inverted, because dS definition is binInpType_windowOpen)
    }
  }
}


string EnoceanRpsWindowHandleHandler::shortDesc()
{
  return "Window Handle";
}


#pragma mark - key card switch


EnoceanRpsCardKeyHandler::EnoceanRpsCardKeyHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}


// EEP F6-04-02, ERP1:
//   inserted = status_NU and data = 0x70
//   extracted = !status_NU and data = 0x00

// EEP F6-04-02, ERP2:
//   state of card is in bit 2

// Eltako FKC and FKF (not documented in EEP):
// - FKF just detects cards
// - FKC can detect Guest (KCG) and service (KCS) cards
//   data 0x10, status 0x30 = inserted KCS (Service Card)
//   data 0x00, status 0x20 = extracted
//   data 0x10, status 0x20 = inserted KCG (Guest Card)
//   means:
//   - state of card is in bit 4 (1=inserted)
//   - type of card is status_NU (N=Service, U=Guest)

// device specific radio packet handling
void EnoceanRpsCardKeyHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  bool isInserted = false;
  bool isServiceCard = false;
  // extract payload data
  uint8_t data = aEsp3PacketPtr->radioUserData()[0];
  uint8_t status = aEsp3PacketPtr->radioStatus();
  if (device.getEEProfile()==0xF60402) {
    // Key Card Activated Switch ERP2
    // - just evaluate DB0.2 "State of card"
    isInserted = data & 0x04; // Bit2
  }
  else if (device.getEEProfile()==0xF604C0) {
    // FKC or FKF style switch (no official EEP for this)
    isInserted = data & 0x10; // Bit4
    if (isInserted && ((status & status_NU)!=0)) {
      // Insertion with N-message (status=0x30) means service card
      isServiceCard = true;
    }
  }
  else {
    // Asssume ERP1 Key Card Activated Switch
    isInserted = (status & status_NU)!=0 && data==0x70;
  }
  // report data for this binary input
  BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(behaviour);
  if (bb) {
    if (isServiceCardDetector) {
      LOG(LOG_INFO,"Enocean Key Card Switch %08X reports: %s\n", device.getAddress(), isServiceCard ? "Service Card" : "Guest Card");
      bb->updateInputState(isServiceCard); // report the card type
    }
    else {
      LOG(LOG_INFO,"Enocean Key Card Switch %08X reports state: %s\n", device.getAddress(), isInserted ? "inserted" : "extracted");
      bb->updateInputState(isInserted); // report the status
    }
  }
}


string EnoceanRpsCardKeyHandler::shortDesc()
{
  return "Key Card Switch";
}


#pragma mark - Smoke Detector

EnoceanRpsSmokeDetectorHandler::EnoceanRpsSmokeDetectorHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}



// AlphaEOS GUARD + Eltako FRW
//                          DATA 	STATUS
//  Alarm - Ein             10		30
//  Alarm ­ Aus              00 		20
//  Batterie - ok 7.5 - 9V 	00 		20
//  Batterie - fail (<7.5V) 30 		30


// device specific radio packet handling
void EnoceanRpsSmokeDetectorHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  // extract payload data
  uint8_t data = aEsp3PacketPtr->radioUserData()[0];
  BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(behaviour);
  if (isBatteryStatus) {
    // battery status channel
    bool lowBat = (data & 0x30)==0x30;
    LOG(LOG_INFO,"Enocean Smoke Detector %08X reports state: Battery %s\n", device.getAddress(), lowBat ? "LOW" : "ok");
    bb->updateInputState(lowBat);
  }
  else {
    // smoke alarm status
    bool smokeAlarm = (data & 0x30)==0x10;
    LOG(LOG_INFO,"Enocean Smoke Detector %08X reports state: %s\n", device.getAddress(), smokeAlarm ? "SMOKE ALARM" : "no alarm");
    bb->updateInputState(smokeAlarm);
  }
}


string EnoceanRpsSmokeDetectorHandler::shortDesc()
{
  return "Smoke Detector";
}


#pragma mark - Liquid Leakage Detector

EnoceanRpsLeakageDetectorHandler::EnoceanRpsLeakageDetectorHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}



// F6-05-01
//                          DATA 	STATUS
//  Water detected          11		30 (NU + T21 both set)


// device specific radio packet handling
void EnoceanRpsLeakageDetectorHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  // extract payload data
  uint8_t data = aEsp3PacketPtr->radioUserData()[0];
  BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(behaviour);
  // smoke alarm status
  bool leakage = data==0x11;
  LOG(LOG_INFO,"Enocean Liquid Leakage Detector %08X reports state: %s\n", device.getAddress(), leakage ? "LEAKAGE" : "no leakage");
  bb->updateInputState(leakage);
}


string EnoceanRpsLeakageDetectorHandler::shortDesc()
{
  return "Leakage Detector";
}




#pragma mark - EnoceanRPSDevice profile variants


static const ProfileVariantEntry RPSprofileVariants[] = {
  // dual rocker RPS button alternatives
  { 1, 0x00F602FF, 2, "dual rocker switch (as 2-way rockers)" }, // rocker switches affect 2 indices (of which odd one does not exist in 2-way mode)
  { 1, 0x01F602FF, 2, "dual rocker switch (up and down as separate buttons)" },
  { 1, 0x00F60401, 0, "key card activated switch ERP1" },
  { 1, 0x00F60402, 0, "key card activated switch ERP2" },
  { 1, 0x00F604C0, 0, "key card activated switch FKC/FKF" },
  { 1, 0x00F60501, 0, "Liquid Leakage detector" },
  { 1, 0x00F605C0, 0, "Smoke detector FRW/GUARD" },
  // quad rocker RPS button alternatives
  { 2, 0x00F603FF, 2, "quad rocker switch (as 2-way rockers)" }, // rocker switches affect 2 indices (of which odd one does not exist in 2-way mode)
  { 2, 0x01F603FF, 2, "quad rocker switch (up and down as separate buttons)" },
  { 0, 0, 0, NULL } // terminator
};


const ProfileVariantEntry *EnoceanRPSDevice::profileVariantsTable()
{
  return RPSprofileVariants;
}


