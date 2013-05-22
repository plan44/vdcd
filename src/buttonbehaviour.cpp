//
//  buttonbehaviour.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "buttonbehaviour.hpp"

using namespace p44;

/// @name functional identification for digitalSTROM system
/// @{

#warning // TODO: for now, we just emulate a SW-TKM210 in a more or less hard-coded way

// from DeviceConfig.py: deviceDefaults["SW-TKM210"] = ( 0x8103, 1234, 257, 0, 0, 0 )

uint16_t ButtonBehaviour::functionId()
{
  return 0x8103;
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


/// @}
