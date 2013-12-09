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

#ifndef __p44utils__gpio__
#define __p44utils__gpio__

#include "p44_common.hpp"

#include "iopin.hpp"

#ifndef GPION9XXX_DEVICES_BASEPATH
#define GPION9XXX_DEVICES_BASEPATH "/dev/gpio/"
#endif


using namespace std;

namespace p44 {


  /// Wrapper for General Purpose I/O pin as accessed via
  /// generic Linux kernel SysFS support for GPIOs (
  class GpioPin : public IOPin
  {
    bool pinState;
    bool output;
    int gpioNo;
    int gpioFD;
  public:

    /// Create general purpose I/O pin
    /// @param aGpioName name of the GPIO (files found in GPIO_DEVICES_BASEPATH)
    /// @param aOutput use as output
    /// @param aInitialState initial state assumed for inputs and enforced for outputs
    GpioPin(int aGpioNo, bool aOutput, bool aInitialState);
    virtual ~GpioPin();

    /// get state of GPIO
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState();

    /// set state of output (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);
    
  };



  /// Wrapper for General Purpose I/O pin as accessed via NS9XXX kernel module
  /// and SysFS from Userland (Digi ME 9210 LX)
  class GpioNS9XXXPin : public IOPin
  {
    int gpioFD;
    bool pinState;
    bool output;
    string name;
  public:

    /// Create general purpose I/O pin
    /// @param aGpioName name of the GPIO (files found in GPIO_DEVICES_BASEPATH)
    /// @param aOutput use as output
    /// @param aInitialState initial state assumed for inputs and enforced for outputs
    GpioNS9XXXPin(const char* aGpioName, bool aOutput, bool aInitialState);
    virtual ~GpioNS9XXXPin();

    /// get state of GPIO
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState();

    /// set state of output (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);

  };

	
} // namespace p44

#endif /* defined(__p44utils__gpio__) */
