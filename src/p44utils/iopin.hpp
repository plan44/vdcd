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

#ifndef __p44utils__iopin__
#define __p44utils__iopin__

#include "p44_common.hpp"

#include "consolekey.hpp"

using namespace std;

namespace p44 {

  /// abstract wrapper class for digital I/O pin
  class IOPin : public P44Obj
  {
  public:

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState() = 0;

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState) = 0;
  };
  typedef boost::intrusive_ptr<IOPin> IOPinPtr;
  
  

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
