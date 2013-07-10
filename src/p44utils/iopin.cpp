//
//  iopin.cpp
//
//  Created by Lukas Zeller on 10.07.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "iopin.hpp"

using namespace p44;

SimPin::SimPin(const char *aName, bool aOutput, bool aInitialState) :
  name(aName),
  output(aOutput),
  pinState(aInitialState)
{
  printf("Initialized SimPin \"%s\" as %s with initial state %s\n", name.c_str(), aOutput ? "output" : "input", pinState ? "HI" : "LO");
  if (!aOutput) {
    static char nextGpioSimKey = 'a';
    if (!output) {
      consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(
        nextGpioSimKey++,
        name.c_str(),
        pinState
      );
    }
  }
}


bool SimPin::getState()
{
  if (output)
    return pinState; // just return last set state
  else
    return (bool)consoleKey->isSet();
}


void SimPin::setState(bool aState)
{
  if (!output) return; // non-outputs cannot be set
  if (pinState!=aState) {
    pinState = aState;
    printf(">>> SimPin \"%s\" set to %s\n", name.c_str(), pinState ? "HI" : "LO");
  }
}