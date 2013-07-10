//
//  i2c.h
//
//  Created by Lukas Zeller on 10.07.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__i2c__
#define __vdcd__i2c__

#include "p44_common.hpp"

using namespace std;

namespace p44 {

  /// abstract wrapper class for digital I/O pin
  class I2CPin
  {
  public:

    
    I2CPin(int aBusNumber, const char *aDeviceId, int aPinNumber, bool aOutput, bool aInitialState);

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState();

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);
  };  
  
} // namespace

#endif /* defined(__vdcd__i2c__) */
