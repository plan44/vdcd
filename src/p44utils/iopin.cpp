//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#include "iopin.hpp"

using namespace p44;

SimPin::SimPin(const char *aName, bool aOutput, bool aInitialState) :
  name(aName),
  output(aOutput),
  pinState(aInitialState)
{
  LOG(LOG_ALERT, "Initialized SimPin \"%s\" as %s with initial state %s\n", name.c_str(), aOutput ? "output" : "input", pinState ? "HI" : "LO");
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
    LOG(LOG_ALERT, ">>> SimPin \"%s\" set to %s\n", name.c_str(), pinState ? "HI" : "LO");
  }
}