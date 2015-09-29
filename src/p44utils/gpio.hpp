//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


  /// Wrapper for LED output accessed via
  /// generic Linux kernel SysFS support for LEDs (
  class GpioLedPin : public IOPin
  {
    typedef IOPin inherited;

    int ledNo;
    int ledFD;
    bool ledState;

  public:

    /// Create general purpose I/O pin
    /// @param aLEDNo number of the LED
    /// @param aInitialState initial state for the LED
    GpioLedPin(int aLEDNo, bool aInitialState);
    virtual ~GpioLedPin();

    /// get state of LED
    /// @return current state of LED
    virtual bool getState();

    /// set state of LED
    /// @param aState new state to set LED to
    virtual void setState(bool aState);
  };


  /// Wrapper for General Purpose I/O pin as accessed via
  /// generic Linux kernel SysFS support for GPIOs (
  class GpioPin : public IOPin
  {
    typedef IOPin inherited;

    bool pinState;
    bool output;
    int gpioNo;
    int gpioFD;

    bool stateChanged(int aPollFlags);

  public:

    /// Create general purpose I/O pin
    /// @param aGpioNo numberof the GPIO
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

    /// install state change detector
    /// @param aInputChangedCB will be called when the input state changes. Passing NULL disables input state change reporting.
    /// @param aInverted if set, the state will be reported inverted to what getState() would report. This is a shortcut
    ///   for efficient implementation of higher level classes (which support inverting), to avoid two stage callbacks
    /// @param aInitialState the initial state (of the pin) assumed present when callback is installed
    /// @param aDebounceTime after a reported state change, next input sampling will take place only after specified interval
    /// @param aPollInterval if <0 (Infinite), the state change detector only works if the input pin supports state change
    ///   detection without polling (e.g. with GPIO edge trigger). If aPollInterval is >=0, and the
    ///   input pin does not support edge detection, the state detection will be implemented via polling
    ///   on the current mainloop - if pollInterval==0 then polling will be done in a mainloop idle handler, otherwise in
    ///   a timed handler according to the specified interval
    /// @return true if input supports the type of state change detection requested
    virtual bool setInputChangedHandler(InputChangedCB aInputChangedCB, bool aInverted, bool aInitialState, MLMicroSeconds aDebounceTime, MLMicroSeconds aPollInterval);

  };



  /// Wrapper for General Purpose I/O pin as accessed via NS9XXX kernel module
  /// and SysFS from Userland (Digi ME 9210 LX)
  class GpioNS9XXXPin : public IOPin
  {
    typedef IOPin inherited;

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
