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
#include "enoceandevicecontainer.hpp"

using namespace p44;

#pragma mark - EnoceanRemoteControlDevice

EnoceanRemoteControlDevice::EnoceanRemoteControlDevice(EnoceanDeviceContainer *aClassContainerP, uint8_t aDsuidIndexStep) :
  inherited(aClassContainerP)
{
}


bool EnoceanRemoteControlDevice::sendTeachInSignal()
{
  if (EEP_FUNC(getEEProfile())==PSEUDO_FUNC_SWITCHCONTROL) {
    // issue simulated left up switch press
    Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
    packet->initForRorg(rorg_RPS);
    packet->setRadioDestination(EnoceanBroadcast);
    packet->radioUserData()[0] = 0x30; // pressing left button, up
    packet->setRadioStatus(status_NU|status_T21); // pressed
    packet->setRadioSender(getAddress()); // my own ID base derived address that is learned into this actor
    getEnoceanDeviceContainer().enoceanComm.sendPacket(packet);
    MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanRemoteControlDevice::sendSwitchBeaconRelease, this), 300*MilliSecond);
    return true;
  }
  return inherited::sendTeachInSignal();
}


void EnoceanRemoteControlDevice::sendSwitchBeaconRelease()
{
  Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
  packet->initForRorg(rorg_RPS);
  packet->setRadioDestination(EnoceanBroadcast);
  packet->radioUserData()[0] = 0x00; // release
  packet->setRadioStatus(status_T21); // released
  packet->setRadioSender(getAddress()); // my own ID base derived address that is learned into this actor
  getEnoceanDeviceContainer().enoceanComm.sendPacket(packet);
}


void EnoceanRemoteControlDevice::markUsedBaseOffsets(string &aUsedOffsetsMap)
{
  int offs = getAddress() & 0x7F;
  if (offs<aUsedOffsetsMap.size()) {
    aUsedOffsetsMap[offs]='1';
  }
}


// insert into knowndevices (enoceanAddress, subdevice, eeProfile, eeManufacturer) values (0xFFDC0D00, 0, 0xFFF6FF, 0xFFFF);
// insert into knowndevices (enoceanAddress, subdevice, eeProfile, eeManufacturer) values (4292611328, 0, 16774911, 65535);

#pragma mark - EnoceanRemoteControlHandler

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
      if (EEP_TYPE(aEEProfile)==PSEUDO_TYPE_SIMPLEBLIND) {
        // simple blind controller
        newDev = EnoceanDevicePtr(new EnoceanRemoteControlDevice(aClassContainerP));
        // standard single-value scene table (SimpleScene)
        newDev->installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*newDev)));
        // assign channel and address
        newDev->setAddressingInfo(aAddress, aSubDeviceIndex);
        // assign EPP information
        newDev->setEEPInfo(aEEProfile, aEEManufacturer);
        // is shadow
        newDev->setPrimaryGroup(group_grey_shadow);
        // function
        newDev->setFunctionDesc("simple blind control");
        // is always updateable (no need to wait for incoming data)
        newDev->setAlwaysUpdateable();
        // - add generic output behaviour
        OutputBehaviourPtr ob = OutputBehaviourPtr(new OutputBehaviour(*newDev.get()));
        ob->setHardwareOutputConfig(outputFunction_switch, usage_undefined, false, -1);
        ob->setHardwareName("blind");
        ob->setGroupMembership(group_grey_shadow, true); // put into shadow group by default
        ob->addChannel(ChannelBehaviourPtr(new DigitalChannel(*ob)));
        // - create PSEUDO_TYPE_SIMPLEBLIND specific handler for output
        EnoceanSimpleBlindHandlerPtr newHandler = EnoceanSimpleBlindHandlerPtr(new EnoceanSimpleBlindHandler(*newDev.get()));
        newHandler->behaviour = ob;
        newDev->addChannelHandler(newHandler);
      }
      else if (EEP_TYPE(aEEProfile)==PSEUDO_TYPE_BLIND) {
        // simple blind controller
        newDev = EnoceanDevicePtr(new EnoceanRemoteControlDevice(aClassContainerP));
        // standard single-value scene table (SimpleScene)
        newDev->installSettings(DeviceSettingsPtr(new SceneDeviceSettings(*newDev)));
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
        sb->position->syncChannelValue(100); // assume fully up at beginning
        sb->angle->syncChannelValue(100); // assume fully open at beginning
        // - create PSEUDO_TYPE_SIMPLEBLIND specific handler for output
        EnoceanBlindHandlerPtr newHandler = EnoceanBlindHandlerPtr(new EnoceanBlindHandler(*newDev.get()));
        newHandler->behaviour = sb;
        newDev->addChannelHandler(newHandler);
      }
    }
  }
  // remote control devices never need a teach-in response
  // return device (or empty if none created)
  return newDev;
}


#pragma mark - simple all-up/all-down blind controller

EnoceanSimpleBlindHandler::EnoceanSimpleBlindHandler(EnoceanDevice &aDevice) :
  inherited(aDevice)
{
}


string EnoceanSimpleBlindHandler::shortDesc()
{
  return "Simple Blind";
}


void EnoceanSimpleBlindHandler::issueDirectChannelActions(SimpleCB aDoneCB)
{
  // veeery simplistic behaviour: value>0.5 means: blind down, otherwise: blind up
  OutputBehaviourPtr ob = boost::dynamic_pointer_cast<OutputBehaviour>(behaviour);
  if (ob) {
    // get the right channel
    ChannelBehaviourPtr ch = ob->getChannelByIndex(dsChannelIndex);
    // get value
    bool blindUp = ch->getChannelValue()>0.5;
    // simulate long press of my button
    Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
    packet->initForRorg(rorg_RPS);
    packet->setRadioDestination(EnoceanBroadcast);
    packet->radioUserData()[0] = blindUp ? 0x30 : 0x10; // pressing left button, up or down
    packet->setRadioStatus(status_NU|status_T21); // pressed
    packet->setRadioSender(device.getAddress()); // my own ID base derived address that is learned into this actor
    device.getEnoceanDeviceContainer().enoceanComm.sendPacket(packet);
    MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanSimpleBlindHandler::sendReleaseTelegram, this, aDoneCB), 1*Second);
  }
}


void EnoceanSimpleBlindHandler::sendReleaseTelegram(SimpleCB aDoneCB)
{
  Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
  packet->initForRorg(rorg_RPS);
  packet->setRadioDestination(EnoceanBroadcast);
  packet->radioUserData()[0] = 0x00; // release
  packet->setRadioStatus(status_T21); // released
  packet->setRadioSender(device.getAddress()); // my own ID base derived address that is learned into this actor
  device.getEnoceanDeviceContainer().enoceanComm.sendPacket(packet);
  if (aDoneCB) aDoneCB();
}


#pragma mark - time controller blind handler


EnoceanBlindHandler::EnoceanBlindHandler(EnoceanDevice &aDevice) :
  inherited(aDevice),
  movingDirection(0),
  commandTicket(0),
  missedUpdate(false)
{
}


string EnoceanBlindHandler::shortDesc()
{
  return "Blind Control";
}


#define LONGPRESS_TIME (1*Second)
#define SHORTPRESS_TIME (200*MilliSecond)


void EnoceanBlindHandler::issueDirectChannelActions(SimpleCB aDoneCB)
{
  // shadow behaviour
  ShadowBehaviourPtr sb = boost::dynamic_pointer_cast<ShadowBehaviour>(behaviour);
  if (sb) {
    // ask shadow behaviour for current movement direction
    // - 0=stopped, -1=moving down, +1=moving up
    int newDirection = sb->currentMovingDirection();
    FOCUSLOG("blind action requested: %d (current: %d)\n", newDirection, movingDirection);
    if (newDirection!=movingDirection) {
      int previousDirection = movingDirection;
      movingDirection = newDirection;
      // needs change
      FOCUSLOG("- needs action:\n");
      if (newDirection==0) {
        // requesting stop:
        if (commandTicket) {
          // start button still pressed
          // - cancel releasing it after logpress time
          MainLoop::currentMainLoop().cancelExecutionTicket(commandTicket);
          // - but release it right now
          buttonAction(newDirection>0, false);
          // - and exit normally (with callback executed)
        }
        else {
          // issue short command in current moving direction - if already at end of move,
          // this will not change anything, otherwise the movement will stop
          // - press button
          buttonAction(previousDirection>0, true);
          commandTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanBlindHandler::sendReleaseTelegram, this, aDoneCB), SHORTPRESS_TIME);
          // callback only later when button is released
          return;
        }
      }
      else {
        // requesting start
        // - press button
        buttonAction(newDirection>0, true);
        // - release latest after blind has entered permanent move mode (but maybe earlier)
        commandTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&EnoceanBlindHandler::sendReleaseTelegram, this, SimpleCB()), LONGPRESS_TIME);
        // - but consider done after shortest possible press (earlier)
        //   Note: we will NOT get called again until aDoneCB has been called, so this holds off actions for at least SHORTPRESS_TIME
        MainLoop::currentMainLoop().executeOnce(boost::bind(aDoneCB), SHORTPRESS_TIME);
        // callback only later when button is released
        return;
      }
    }
  }
  // normal exit, confirm it done
  if (aDoneCB) aDoneCB();
}


void EnoceanBlindHandler::sendReleaseTelegram(SimpleCB aDoneCB)
{
  commandTicket = 0;
  // just release
  buttonAction(false, false);
  // callback if set
  if (aDoneCB) aDoneCB();
}



void EnoceanBlindHandler::buttonAction(bool aBlindUp, bool aPress)
{
  FOCUSLOG("- %s simulated blind %s button\n", aPress ? "PRESSING" : "RELEASING", aBlindUp ? "UP" : "DOWN");
  Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
  packet->initForRorg(rorg_RPS);
  packet->setRadioDestination(EnoceanBroadcast);
  if (aPress) {
    packet->radioUserData()[0] = aBlindUp ? 0x30 : 0x10; // pressing left button, up or down
    packet->setRadioStatus(status_NU|status_T21); // pressed
  }
  else {
    packet->radioUserData()[0] = 0x00; // release
    packet->setRadioStatus(status_T21); // released
  }
  packet->setRadioSender(device.getAddress()); // my own ID base derived address that is learned into this actor
  device.getEnoceanDeviceContainer().enoceanComm.sendPacket(packet);
}






