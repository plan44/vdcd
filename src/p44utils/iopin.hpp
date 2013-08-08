//
//  iopin.hpp
//
//  Created by Lukas Zeller on 10.07.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__iopin__
#define __p44utils__iopin__

#include "p44_common.hpp"

#include "consolekey.hpp"

using namespace std;

namespace p44 {

  /// abstract wrapper class for digital I/O pin
  class IOPin
  {
  public:

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState() = 0;

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState) = 0;
  };
  typedef boost::shared_ptr<IOPin> IOPinPtr;
  
  

  /// simulated I/O pin
  class SimPin : public IOPin
  {
    ConsoleKeyPtr consoleKey;
    bool output;
    string name;
    bool pinState;

  public:
    // create a simulated pin (using console I/O)
    SimPin(const char *aName, bool aOutput, bool aInitialState);

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState();

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);
  };


} // namespace

#endif /* defined(__p44utils__iopin__) */
