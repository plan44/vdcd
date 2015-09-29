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

#ifndef __p44utils__iopin__
#define __p44utils__iopin__

#include "p44_common.hpp"

#include "consolekey.hpp"

using namespace std;

namespace p44 {

  #pragma mark - digital pins

  /// callback for input pin state change reporting
  /// @param aNewState the new state of the Input pin
  typedef boost::function<void (bool aNewState)> InputChangedCB;

  /// wrapper class for digital I/O pin
  class IOPin : public P44Obj
  {

  protected:

    InputChangedCB inputChangedCB;
    bool currentState;
    bool invertedReporting;
    long pollTicket;
    MLMicroSeconds pollInterval;

    MLMicroSeconds debounceTime;
    MLMicroSeconds lastReportedChange;
    long debounceTicket;

    /// this can be called by derived classes' input change detection or polling routine to report the current input state
    /// Note: the current state does not necessarily need to be "new", inputHasChangedTo() prevents re-reporting the same state
    void inputHasChangedTo(bool aCurrentState);

  public:

    IOPin();
    virtual ~IOPin();

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState() = 0;

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

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState) = 0;

  private:

    void clearChangeHandling();
    bool idlepoll();
    void timedpoll();
    void debounceSample();

  };
  typedef boost::intrusive_ptr<IOPin> IOPinPtr;
  
  

  /// simulated digital I/O pin
  class SimPin : public IOPin
  {
    typedef IOPin inherited;

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


  /// missing (dummy) digital I/O pin
  class MissingPin : public IOPin
  {
    typedef IOPin inherited;

    bool pinState;

  public:
    // create a missing pin (not connected, just keeping state)
    MissingPin(bool aInitialState) : pinState(aInitialState) {};

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState() { return pinState; } // return state (which is initialstate or state set with setState later on)

    /// set state of pin (NOP)
    /// @param aState new state (changes initial state)
    virtual void setState(bool aState) { pinState = aState; }; // remember
  };


  /// Digital System Command I/O pin
  class SysCommandPin : public IOPin
  {
    typedef IOPin inherited;

    string onCommand;
    string offCommand;
    bool pinState;
    bool output;
    bool changing;
    bool changePending;

  public:
    // create a pin using a command line to act
    SysCommandPin(const char *aConfig, bool aOutput, bool aInitialState);

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState() { return pinState; } // return state (which is initialstate or state set with setState later on)

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);

  private:

    string stateSetCommand(bool aState);
    void applyState(bool aState);
    void stateUpdated(ErrorPtr aError, const string &aOutputString);

  };



  #pragma mark - analog pins

  /// abstract wrapper class for analog I/O pin
  class AnalogIOPin : public P44Obj
  {
  public:

    /// get value of pin
    /// @return current value (from actual pin for inputs, from last set state for outputs)
    virtual double getValue() = 0;

    /// set value of pin (NOP for inputs)
    /// @param aValue new value to set output to
    virtual void setValue(double aValue) = 0;
  };
  typedef boost::intrusive_ptr<AnalogIOPin> AnalogIOPinPtr;



  /// simulated I/O pin
  class AnalogSimPin : public AnalogIOPin
  {
    typedef AnalogIOPin inherited;

    ConsoleKeyPtr consoleKey;
    bool output;
    string name;
    double pinValue;

  public:
    // create a simulated pin (using console I/O)
    AnalogSimPin(const char *aName, bool aOutput, double aInitialValue);

    /// get value of pin
    /// @return current value (from actual pin for inputs, from last set state for outputs)
    virtual double getValue();

    /// set value of pin (NOP for inputs)
    /// @param aValue new value to set output to
    virtual void setValue(double aValue);
  };


  /// missing (dummy) I/O pin
  class AnalogMissingPin : public AnalogIOPin
  {
    typedef AnalogIOPin inherited;

    double pinValue;

  public:
    // create a missing pin (not connected, just keeping state)
    AnalogMissingPin(double aInitialValue) : pinValue(aInitialValue) {};

    /// get value of pin
    /// @return current value (from actual pin for inputs, from last set state for outputs)
    virtual double getValue() { return pinValue; } // return value (which is initial value or value set with setValue later on)

    /// set value of pin (NOP for inputs)
    /// @param aValue new value to set output to
    virtual void setValue(double aValue) { pinValue = aValue; } // remember
  };



  /// Digital System Command I/O pin
  class AnalogSysCommandPin : public AnalogIOPin
  {
    typedef AnalogIOPin inherited;

    string setCommand;
    double pinValue;
    int range;
    bool output;
    bool changing;
    bool changePending;

  public:
    // create a pin using a command line to act
    AnalogSysCommandPin(const char *aConfig, bool aOutput, double aInitialValue);

    /// get value of pin
    /// @return current value (from actual pin for inputs, from last set state for outputs)
    virtual double getValue() { return pinValue; } // return value (which is initial value or value set with setValue later on)

    /// set value of pin (NOP for inputs)
    /// @param aValue new value to set output to
    virtual void setValue(double aValue);

  private:

    string valueSetCommand(double aValue);
    void applyValue(double aValue);
    void valueUpdated(ErrorPtr aError, const string &aOutputString);
    
  };



} // namespace

#endif /* defined(__p44utils__iopin__) */
