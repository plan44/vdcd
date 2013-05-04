//
//  gpio.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__gpio__
#define __p44bridged__gpio__

#include "p44bridged_common.hpp"

#ifndef GPIO_DEVICES_BASEPATH
#define GPIO_DEVICES_BASEPATH "/dev/gpio/"
#endif


#ifdef __APPLE__
#define GPIO_SIMULATION 1
#endif


using namespace std;

namespace p44 {

  /// Wrapper for General Purpose I/O pin accessed via SysFS from Userland
  class Gpio
  {
    int gpioFD;
    bool pinState;
    bool output;
    bool inverted;
    string name;
    #if GPIO_SIMULATION
    int inputBitNo;
    #endif
  public:
    /// Create general purpose I/O
    /// @param aGpioName name of the GPIO (files found in GPIO_DEVICES_BASEPATH)
    /// @param aOutput use as output
    /// @param aInverted inverted polarity (output high level is treated as logic false)
    /// @param aInitialState initial state (to set for output, to expect without triggering change for input)
    ///   Note: aInitialState is logic state (pin state will be inverse if aInverted is set)
    Gpio(const char* aGpioName, bool aOutput, bool aInverted = false, bool aInitialState = false);
    ~Gpio();
    /// set state of output (NOP for inputs)
    /// @param aState new state to set output to
    void setState(bool aState);
    /// set state of output (NOP for inputs)
    /// @return current state (actual level on pin for inputs, last set state for outputs)
    bool getState();
  };


  /// GPIO used as pushbutton
  class ButtonInput : public Gpio
  {
    typedef Gpio inherited;

  public:
    /// button event handler
    /// @param aButtonP the button
    /// @param aNewState the current state of the button (relevant when handler was installed with aPressAndRelease set)
    /// @param aTimestamp the main loop timestamp of the button action
    typedef boost::function<void (ButtonInput *aButtonP, bool aNewState, MLMicroSeconds aTimestamp)> ButtonHandlerCB;

  private:
    bool lastState;
    MLMicroSeconds lastChangeTime;
    bool reportPressAndRelease;
    ButtonHandlerCB buttonHandler;

    bool poll(MLMicroSeconds aTimestamp);
    
  public:
    /// Craete pushbutton
    /// @param aGpioName name of the GPIO where the pushbutton is connected
    /// @param aInverted inverted polarity (output high level is treated as logic false)
    ButtonInput(const char* aGpioName, bool aInverted);

    /// set handler to be called on pushbutton events
    /// @param aButtonHandler handler for pushbutton events
    /// @param aPressAndRelease if set, both pressing and releasing button generates event.
    ///   Otherwise, only one event is issued per button press (on button release)
    void setButtonHandler(ButtonHandlerCB aButtonHandler, bool aPressAndRelease);
    
  };




} // namespace p44

#endif /* defined(__p44bridged__gpio__) */
