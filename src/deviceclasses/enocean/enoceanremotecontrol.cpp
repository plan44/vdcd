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
#define FOCUSLOGLEVEL 6

#include "enoceanremotecontrol.hpp"

#include "shadowbehaviour.hpp"
#include "lightbehaviour.hpp"
#include "enoceandevicecontainer.hpp"

using namespace p44;

#pragma mark - EnoceanRemoteControlDevice

#define TEACH_IN_TIME (300*MilliSecond) // how long the teach-in simulated button press should last


EnoceanRemoteControlDevice::EnoceanRemoteControlDevice(EnoceanDeviceContainer *aClassContainerP, uint8_t aDsuidIndexStep) :
  inherited(aClassContainerP)
{
}


uint8_t EnoceanRemoteControlDevice::teachInSignal(int8_t aVariant)
{
  if (EEP_FUNC(getEEProfile())==PSEUDO_FUNC_SWITCHCONTROL && aVariant<4) {
    // issue simulated buttom press - variant: 0=left button up, 1=left button down, 2=right button up, 3=right button down
    if (aVariant<0) return 4; // only query: we have 4 teach-in variants
    bool right = aVariant & 0x2;
    bool up = !(aVariant & 0x1);
    buttonAction(right, up, true); // press
    MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanRemoteControlDevice::sendSwitchBeaconRelease, this, right, up), TEACH_IN_TIME);
    return 4;
  }
  return inherited::teachInSignal(aVariant);
}


void EnoceanRemoteControlDevice::sendSwitchBeaconRelease(bool aRight, bool aUp)
{
  buttonAction(aRight, aUp, false); // release
}


void EnoceanRemoteControlDevice::buttonAction(bool aRight, bool aUp, bool aPress)
{
  FOCUSLOG("- %s simulated %s %s button\n", aPress ? "PRESSING" : "RELEASING", aRight ? "RIGHT" : "LEFT", aUp ? "UP" : "DOWN");
  Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
  packet->initForRorg(rorg_RPS);
  packet->setRadioDestination(EnoceanBroadcast);
  if (aPress) {
    uint8_t d = 0x10; // energy bow: pressed
    if (aUp) d |= 0x20; // "up"
    if (aRight) d |= 0x40; // B = right button
    packet->radioUserData()[0] = d;
    packet->setRadioStatus(status_NU|status_T21); // pressed
  }
  else {
    packet->radioUserData()[0] = 0x00; // release
    packet->setRadioStatus(status_T21); // released
  }
  packet->setRadioSender(getAddress()); // my own ID base derived address that is learned into this actor
  getEnoceanDeviceContainer().enoceanComm.sendCommand(packet, NULL);
}


void EnoceanRemoteControlDevice::markUsedBaseOffsets(string &aUsedOffsetsMap)
{
  int offs = getAddress() & 0x7F;
  if (offs<aUsedOffsetsMap.size()) {
    aUsedOffsetsMap[offs]='1';
  }
}


#pragma mark - EnoceanRemoteControlHandler

// Simple on/off controller
#define BUTTON_PRESS_TIME (200*MilliSecond) // how long the simulated button press should last
#define BUTTON_PRESS_PAUSE_TIME (300*MilliSecond) // how long the pause between simulated button actions should last

// Blind controller
// - hardware timing
#define LONGPRESS_TIME (1*Second)
#define SHORTPRESS_TIME (200*MilliSecond)
#define PAUSE_TIME (300*MilliSecond)
// - derived timing
#define MIN_MOVE_TIME SHORTPRESS_TIME
#define MAX_SHORT_MOVE_TIME (LONGPRESS_TIME/2)
#define MIN_LONG_MOVE_TIME (LONGPRESS_TIME+SHORTPRESS_TIME)



EnoceanRemoteControlHandler::EnoceanRemoteControlHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}


EnoceanDevicePtr EnoceanRemoteControlHandler::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice aSubDeviceIndex,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aNeedsTeachInResponse
) {
  EnoceanDevicePtr newDev; // none so far
  if (EEP_RORG(aEEProfile)==PSEUDO_RORG_REMOTECONTROL) {
    // is a remote control device
    if (EEP_FUNC(aEEProfile)==PSEUDO_FUNC_SWITCHCONTROL && aSubDeviceIndex<1) {
      // device using F6 RPS messages to control actors
      if (EEP_TYPE(aEEProfile)==PSEUDO_TYPE_ON_OFF) {
        // simple on/off relay device
        newDev = EnoceanDevicePtr(new EnoceanRelayControlDevice(aClassContainerP));
        // standard single-value scene table (SimpleScene)
        newDev->installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*newDev)));
        // assign channel and address
        newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
        // assign EPP information
        newDev->setEEPInfo(aEEProfile, aEEManufacturer);
        // is joker
        newDev->setPrimaryGroup(group_black_joker);
        // function
        newDev->setFunctionDesc("on/off relay");
        // is always updateable (no need to wait for incoming data)
        newDev->setAlwaysUpdateable();
        // - add standard output behaviour
        OutputBehaviourPtr o = OutputBehaviourPtr(new OutputBehaviour(*newDev.get()));
        o->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
        o->setGroupMembership(group_black_joker, true); // put into joker group by default
        o->addChannel(ChannelBehaviourPtr(new DigitalChannel(*o)));
        // does not need a channel handler at all, just add behaviour
        newDev->addBehaviour(o);
      }
      else if (EEP_TYPE(aEEProfile)==PSEUDO_TYPE_SWITCHED_LIGHT) {
        // simple on/off relay device
        newDev = EnoceanDevicePtr(new EnoceanRelayControlDevice(aClassContainerP));
        // light device scene
        newDev->installSettings(DeviceSettingsPtr(new LightDeviceSettings(*newDev)));
        // assign channel and address
        newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
        // assign EPP information
        newDev->setEEPInfo(aEEProfile, aEEManufacturer);
        // is light
        newDev->setPrimaryGroup(group_yellow_light);
        // function
        newDev->setFunctionDesc("on/off light");
        // is always updateable (no need to wait for incoming data)
        newDev->setAlwaysUpdateable();
        // - add standard output behaviour
        LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*newDev.get()));
        l->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
        // does not need a channel handler at all, just add behaviour
        newDev->addBehaviour(l);
      }
      else if (EEP_TYPE(aEEProfile)==PSEUDO_TYPE_BLIND) {
        // full-featured blind controller
        newDev = EnoceanDevicePtr(new EnoceanBlindControlDevice(aClassContainerP));
        // standard single-value scene table (SimpleScene) with Shadow defaults
        newDev->installSettings(DeviceSettingsPtr(new ShadowDeviceSettings(*newDev)));
        // assign channel and address
        newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
        // assign EPP information
        newDev->setEEPInfo(aEEProfile, aEEManufacturer);
        // is shadow
        newDev->setPrimaryGroup(group_grey_shadow);
        // function
        newDev->setFunctionDesc("blind control");
        // is always updateable (no need to wait for incoming data)
        newDev->setAlwaysUpdateable();
        // - add shadow behaviour
        ShadowBehaviourPtr sb = ShadowBehaviourPtr(new ShadowBehaviour(*newDev.get()));
        sb->setHardwareOutputConfig(outputFunction_positional, usage_undefined, false, -1);
        sb->setHardwareName("blind");
        sb->setDeviceParams(shadowdevice_jalousie, MIN_MOVE_TIME, MAX_SHORT_MOVE_TIME, MIN_LONG_MOVE_TIME);
        sb->position->syncChannelValue(100); // assume fully up at beginning
        sb->angle->syncChannelValue(100); // assume fully open at beginning
        // does not need a channel handler at all, just add behaviour
        newDev->addBehaviour(sb);
      }
    }
  }
  // remote control devices never need a teach-in response
  // return device (or empty if none created)
  return newDev;
}


#pragma mark - relay device


EnoceanRelayControlDevice::EnoceanRelayControlDevice(EnoceanDeviceContainer *aClassContainerP, uint8_t aDsuidIndexStep) :
  inherited(aClassContainerP, aDsuidIndexStep)
{
}



void EnoceanRelayControlDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  // standard output behaviour
  if (output) {
    ChannelBehaviourPtr ch = output->getChannelByType(channeltype_default);
    if (ch->needsApplying()) {
      bool up = ch->getChannelValue() >= (ch->getMax()-ch->getMin())/2;
      buttonAction(false, up, true);
      MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanRelayControlDevice::sendReleaseTelegram, this, aDoneCB, up), BUTTON_PRESS_TIME);
      ch->channelValueApplied();
    }
  }
}


void EnoceanRelayControlDevice::sendReleaseTelegram(SimpleCB aDoneCB, bool aUp)
{
  commandTicket = 0;
  // just release
  buttonAction(false, aUp, false);
  // schedule callback if set
  if (aDoneCB) {
    MainLoop::currentMainLoop().executeOnce(boost::bind(aDoneCB), BUTTON_PRESS_PAUSE_TIME);
  }
}



#pragma mark - time controlled blind device


EnoceanBlindControlDevice::EnoceanBlindControlDevice(EnoceanDeviceContainer *aClassContainerP, uint8_t aDsuidIndexStep) :
  inherited(aClassContainerP, aDsuidIndexStep),
  movingDirection(0),
  commandTicket(0),
  missedUpdate(false)
{
};



void EnoceanBlindControlDevice::syncChannelValues(SimpleCB aDoneCB)
{
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb) {
    sb->syncBlindState();
  }
  if (aDoneCB) aDoneCB();
}



void EnoceanBlindControlDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  // shadow behaviour
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb) {
    // ask shadow behaviour to start movement sequence
    sb->applyBlindChannels(boost::bind(&EnoceanBlindControlDevice::changeMovement, this, _1, _2), aDoneCB, aForDimming);
  }
}


// optimized blinds dimming implementation
void EnoceanBlindControlDevice::dimChannel(DsChannelType aChannelType, DsDimMode aDimMode)
{
  // start dimming
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(output);
  if (sb) {
    // no channel check, there's only global dimming of the blind, no separate position/angle
    sb->dimBlind(boost::bind(&EnoceanBlindControlDevice::changeMovement, this, _1, _2), aDimMode);
  }
}




void EnoceanBlindControlDevice::changeMovement(SimpleCB aDoneCB, int aNewDirection)
{
  // - 0=stopped, -1=moving down, +1=moving up
  FOCUSLOG("blind action requested: %d (current: %d)\n", aNewDirection, movingDirection);
  if (aNewDirection!=movingDirection) {
    int previousDirection = movingDirection;
    movingDirection = aNewDirection;
    // needs change
    FOCUSLOG("- needs action:\n");
    if (movingDirection==0) {
      // requesting stop:
      if (commandTicket) {
        // start button still pressed
        // - cancel releasing it after logpress time
        MainLoop::currentMainLoop().cancelExecutionTicket(commandTicket);
        // - but release it right now
        buttonAction(false, previousDirection>0, false);
        // - and exit normally to confirm done immediately
      }
      else {
        // issue short command in current moving direction - if already at end of move,
        // this will not change anything, otherwise the movement will stop
        // - press button
        buttonAction(false, previousDirection>0, true);
        commandTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanBlindControlDevice::sendReleaseTelegram, this, aDoneCB), SHORTPRESS_TIME);
        // callback only later when button is released
        return;
      }
    }
    else {
      // requesting start
      // - press button
      buttonAction(false, movingDirection>0, true);
      // - release latest after blind has entered permanent move mode (but maybe earlier)
      commandTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanBlindControlDevice::sendReleaseTelegram, this, SimpleCB()), LONGPRESS_TIME);
      // - but as movement has actualy started, exit normally to confirm done immediately
    }
  }
  // normal exit, confirm it done
  if (aDoneCB) aDoneCB();
}


void EnoceanBlindControlDevice::sendReleaseTelegram(SimpleCB aDoneCB)
{
  commandTicket = 0;
  // just release
  buttonAction(false, false, false);
  // schedule callback if set
  if (aDoneCB) {
    MainLoop::currentMainLoop().executeOnce(boost::bind(aDoneCB), PAUSE_TIME);
  }
}




