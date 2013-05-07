//
//  enoceandevicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevicecontainer.hpp"



using namespace p44;


EnoceanDeviceContainer::EnoceanDeviceContainer(int aInstanceNumber) :
  DeviceClassContainer(aInstanceNumber),
	enoceanComm(SyncIOMainLoop::currentMainLoop())
{
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanDeviceContainer::handleRadioPacket, this, _2, _3));
}



const char *EnoceanDeviceContainer::deviceClassIdentifier() const
{
  return "EnOcean_Bus_Container";
}



void EnoceanDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  // TODO: actually collect
  // - read learned-in enOcean button IDs from DB
  // - create EnoceanDevice objects
  // - addCollectedDevice(enoceanDevice);
  // %%% for now, just return ok
  aCompletedCB(ErrorPtr());
}



void EnoceanDeviceContainer::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  if (isLearning()) {
    // in learn mode, check if strong signal and if so, learn/unlearn
    // TODO: refine
    if (aEsp3PacketPtr->radio_dBm()>-60) {
      ///
    }
  }
}


void EnoceanDeviceContainer::learnSwitchDevice(CompletedCB aCompletedCB, MLMicroSeconds aLearnTimeout)
{
  if (isLearning()) return; // already learning -> NOP
  // start timer for timeout
  learningCompleteHandler = aCompletedCB;
  MainLoop::currentMainLoop()->executeOnce(bind(&EnoceanDeviceContainer::stopLearning, this), aLearnTimeout);
}


bool EnoceanDeviceContainer::isLearning()
{
  return !learningCompleteHandler.empty();
}


void EnoceanDeviceContainer::stopLearning()
{
  MainLoop::currentMainLoop()->cancelExecutionsFrom(this); // cancel timeout
  if (isLearning()) {
    CompletedCB cb = learningCompleteHandler;
    learningCompleteHandler = NULL;
    cb(ErrorPtr(new EnoceanError(EnoceanLearnAborted)));
  }
}
