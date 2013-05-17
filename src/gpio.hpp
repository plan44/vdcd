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
    virtual ~Gpio();
    /// set state of output (NOP for inputs)
    /// @return current state (actual level on pin for inputs, last set state for outputs)
    bool isSet();
    /// set state of output (NOP for inputs)
    /// @param aState new state to set output to
    void set(bool aState);
    /// set state to true
    void on();
    /// set state to false
    void off();
    /// toggle state of output and return new state
    /// @return new state of output after toggling (for inputs, just returns state like isSet() does)
    bool toggle();
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
    /// Create pushbutton
    /// @param aGpioName name of the GPIO where the pushbutton is connected
    /// @param aInverted inverted polarity (output high level is treated as logic false)
    ButtonInput(const char* aGpioName, bool aInverted);

    /// set handler to be called on pushbutton events
    /// @param aButtonHandler handler for pushbutton events
    /// @param aPressAndRelease if set, both pressing and releasing button generates event.
    ///   Otherwise, only one event is issued per button press (on button release)
    void setButtonHandler(ButtonHandlerCB aButtonHandler, bool aPressAndRelease);
    
  };

  /// GPIO used for indicator (e.g. LED)
  class IndicatorOutput : public Gpio
  {
    typedef Gpio inherited;

    MLMicroSeconds switchOffAt;
    MLMicroSeconds blinkOnTime;
    MLMicroSeconds blinkOffTime;
    MLMicroSeconds blinkToggleAt;

    bool timer(MLMicroSeconds aTimestamp);

  public:
    /// Create indicator output
    /// @param aGpioName name of the GPIO where the indicator is connected
    /// @param aInverted inverted polarity (output high level means indicator off)
    /// @param aInitiallyOn initial state (on or off) of the indicator
    IndicatorOutput(const char* aGpioName, bool aInverted, bool aInitiallyOn = false);

    /// activate the output for a certain time period, then switch off again
    /// @param aOnTime how long indicator should stay active
    void onFor(MLMicroSeconds aOnTime);

    /// blink indicator for a certain time period, with a given blink period and on ratio
    /// @param aOnTime how long indicator should stay active, or p44::infinite to keep blinking
    void blinkFor(MLMicroSeconds aOnTime, MLMicroSeconds aBlinkPeriod = 600*MilliSecond, int aOnRatioPercent = 50);

    /// stop blinking/timed activation immediately, turn off
    void stop();

  };



} // namespace p44

#endif /* defined(__p44bridged__gpio__) */
