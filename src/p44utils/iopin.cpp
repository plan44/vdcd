//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


static char nextIoSimKey = 'a';

#pragma mark - digital I/O

SimPin::SimPin(const char *aName, bool aOutput, bool aInitialState) :
  name(aName),
  output(aOutput),
  pinState(aInitialState)
{
  LOG(LOG_ALERT, "Initialized SimPin \"%s\" as %s with initial state %s\n", name.c_str(), aOutput ? "output" : "input", pinState ? "HI" : "LO");
  if (!aOutput) {
    if (!output) {
      consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(
        nextIoSimKey++,
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


#pragma mark - analog I/O


AnalogSimPin::AnalogSimPin(const char *aName, bool aOutput, double aInitialValue) :
  name(aName),
  output(aOutput),
  pinValue(aInitialValue)
{
  LOG(LOG_ALERT, "Initialized AnalogSimPin \"%s\" as %s with initial value %.2f\n", name.c_str(), aOutput ? "output" : "input", pinValue);
  if (!aOutput) {
    if (!output) {
      consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(
        nextIoSimKey++,
        name.c_str(),
        false
      );
    }
  }
}


double AnalogSimPin::getValue()
{
  return pinValue; // just return last set value (as set by setValue or modified by key presses)
}


void AnalogSimPin::setValue(double aValue)
{
  if (!output) return; // non-outputs cannot be set
  if (pinValue!=aValue) {
    pinValue = aValue;
    LOG(LOG_ALERT, ">>> AnalogSimPin \"%s\" set to %.2f\n", name.c_str(), pinValue);
  }
}
