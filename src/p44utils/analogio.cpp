//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "analogio.hpp"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "iopin.hpp"
#include "gpio.hpp"
#include "i2c.hpp"

#include "logger.hpp"
#include "mainloop.hpp"

using namespace p44;

AnalogIo::AnalogIo(const char* aAnalogIoName, bool aOutput, double aInitialValue)
{
  // save params
  output = aOutput;
  name = aAnalogIoName;
  // check for missing pin (no pin, just silently keeping value)
  if (name=="missing") {
    ioPin = AnalogIOPinPtr(new AnalogMissingPin(aInitialValue));
    return;
  }
  // dissect name into bus, device, pin
  string busName;
  string deviceName;
  string pinName;
  size_t i = name.find(".");
  if (i==string::npos) {
    // no structured name, NOP
    return;
  }
  else {
    busName = name.substr(0,i);
    // rest is device + pinname or just pinname
    pinName = name.substr(i+1,string::npos);
    i = pinName.find(".");
    if (i!=string::npos) {
      // separate device and pin names
      // - extract device name
      deviceName = pinName.substr(0,i);
      // - remove device name from pin name string
      pinName.erase(0,i+1);
    }
  }
  // now create appropriate pin
  DBGLOG(LOG_DEBUG, "AnalogIo: bus name = '%s'\n", busName.c_str());
  if (busName.substr(0,3)=="i2c") {
    // i2c<busnum>.<devicespec>.<pinnum>
    int busNumber = atoi(busName.c_str()+3);
    int pinNumber = atoi(pinName.c_str());
    ioPin = AnalogIOPinPtr(new AnalogI2CPin(busNumber, deviceName.c_str(), pinNumber, output, aInitialValue));
  }
  else {
    // all other/unknown bus names default to simulated pin
    ioPin = AnalogIOPinPtr(new AnalogSimPin(name.c_str(), output, aInitialValue)); // set even for inputs
  }
}


AnalogIo::~AnalogIo()
{
}


double AnalogIo::value()
{
  return ioPin->getValue();
}


void AnalogIo::setValue(double aValue)
{
  ioPin->setValue(aValue);
}
