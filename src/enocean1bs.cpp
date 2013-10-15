//
//  enocean1bs.cpp
//  vdcd
//
//  Created by Lukas Zeller on 15.10.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enocean1bs.hpp"

#include "enoceandevicecontainer.hpp"

#include "binaryinputbehaviour.hpp"

using namespace p44;


Enocean1bsHandler::Enocean1bsHandler(EnoceanDevice &aDevice) :
  EnoceanChannelHandler(aDevice)
{
}


EnoceanDevicePtr Enocean1bsHandler::newDevice(
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
  // At this time, only the "single input contact" profile is defined in EEP: D5-00-01
  if (func==0x00 && type==0x01) {
    // single input contact
    aNumSubdevices = 1; // always has a single subdevice
    // create device
    newDev = EnoceanDevicePtr(new EnoceanDevice(aClassContainerP, aNumSubdevices));
    // assign channel and address
    newDev->setAddressingInfo(aAddress, aSubDevice);
    // assign EPP information
    newDev->setEEPInfo(aEEProfile, aEEManufacturer);
    // joker by default, we don't know what kind of contact this is
    newDev->setPrimaryGroup(group_black_joker);
    // create channel handler
    Enocean1bsHandlerPtr newHandler = Enocean1bsHandlerPtr(new Enocean1bsHandler(*newDev.get()));
    // create the behaviour
    BinaryInputBehaviourPtr bb = BinaryInputBehaviourPtr(new BinaryInputBehaviour(*newDev.get()));
    bb->setHardwareInputConfig(binInpType_none, usage_undefined, true, 15*Minute);
    bb->setGroup(group_black_joker); // joker by default
    bb->setHardwareName(newHandler->shortDesc());
    newHandler->behaviour = bb;
    // add channel to device
    newDev->addChannelHandler(newHandler);
  }
  // return device (or empty if none created)
  return newDev;
}


// handle incoming data from device and extract data for this channel
void Enocean1bsHandler::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  if (!aEsp3PacketPtr->eepHasTeachInfo()) {
    // only look at non-teach-in packets
    if (aEsp3PacketPtr->eepRorg()==rorg_1BS && aEsp3PacketPtr->radioUserDataLength()==1) {
      // only look at 1BS packets of correct length
      uint8_t data = aEsp3PacketPtr->radioUserData()[0];
      // report contact state to binaryInputBehaviour
      BinaryInputBehaviourPtr bb = boost::dynamic_pointer_cast<BinaryInputBehaviour>(behaviour);
      if (bb) {
        bb->updateInputState(data & 0x01); // Bit 0 is the contact
      }
    }
  }
}


string Enocean1bsHandler::shortDesc()
{
  return "Single Contact";
}
