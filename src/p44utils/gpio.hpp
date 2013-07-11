//
//  gpio.hpp
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__gpio__
#define __p44bridged__gpio__

#include "p44_common.hpp"

#include "iopin.hpp"

#ifndef GPIO_DEVICES_BASEPATH
#define GPIO_DEVICES_BASEPATH "/dev/gpio/"
#endif


using namespace std;

namespace p44 {

  /// Wrapper for General Purpose I/O pin accessed via SysFS from Userland
  class GpioPin : public IOPin
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
    GpioPin(const char* aGpioName, bool aOutput, bool aInitialState);
    virtual ~GpioPin();

    /// get state of GPIO
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState();

    /// set state of output (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);

  };

	
} // namespace p44

#endif /* defined(__p44bridged__gpio__) */
