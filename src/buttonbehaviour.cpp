//
//  buttonbehaviour.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "buttonbehaviour.hpp"

using namespace p44;


ButtonBehaviour::ButtonBehaviour(Device *aDeviceP) :
  inherited(aDeviceP)
{
}


void ButtonBehaviour::buttonAction(bool aPressed)
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Button was %s\n", aPressed ? "pressed" : "released");
}



#pragma mark - functional identification for digitalSTROM system

#warning // TODO: for now, we just emulate a SW-TKM2xx in a more or less hard-coded way

// from DeviceConfig.py:
// #  productName (functionId, productId, groupMemberShip, ltMode, outputMode, buttonIdGroup)
// %%% luz: was apparently wrong names, TKM200 is the 4-input version TKM210 the 2-input, I corrected the functionID below:
// deviceDefaults["SW-TKM200"] = ( 0x8103, 1224, 257, 0, 0, 0 ) # 4 inputs
// deviceDefaults["SW-TKM210"] = ( 0x8102, 1234, 257, 0, 0, 0 ) # 2 inputs

// Die Function-ID enthält (für alle dSIDs identisch) in den letzten 2 Bit eine Kennung,
// wieviele Eingänge das Gerät besitzt: 0=keine, 1=1 Eingang, 2=2 Eingänge, 3=4 Eingänge.
// Die dSID muss für den ersten Eingang mit einer durch 4 teilbaren dSID beginnen, die weiteren dSIDs sind fortlaufend.


uint16_t ButtonBehaviour::functionId()
{
  int i = deviceP->getNumInputs();
  return 0x8100 + i>3 ? 3 : i; // 0 = no inputs, 1..2 = 1..2 inputs, 3 = 4 inputs
}



uint16_t ButtonBehaviour::productId()
{
  return 1234;
}


uint16_t ButtonBehaviour::groupMemberShip()
{
  return 257; // all groups
}


uint8_t ButtonBehaviour::ltMode()
{
  return 0;
}


uint8_t ButtonBehaviour::outputMode()
{
  return 0;
}



uint8_t ButtonBehaviour::buttonIdGroup()
{
  return 0;
}



string ButtonBehaviour::description()
{
  return string("Button");
}
