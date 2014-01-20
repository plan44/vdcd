//
//  digitalio.hpp
//  p44utils
//
//  Copyright (c) 2013-2014 plan44.ch. All rights reserved.
//

#ifndef __p44utils__digitalio__
#define __p44utils__digitalio__

#include "p44_common.hpp"

#include "iopin.hpp"

using namespace std;

namespace p44 {

  /// Generic digital I/O
  class DigitalIo : public P44Obj
  {
    IOPinPtr ioPin; ///< the actual hardware interface to the pin

    string name;
    bool output;
    bool inverted;
  public:
    /// Create general purpose I/O
    /// @param aGpioName name of the IO (in form bus.device.pin, where bus & device can be omitted for normal GPIOs)
    /// @param aOutput use as output
    /// @param aInverted inverted polarity (output high level is treated as logic false)
    /// @param aInitialState initial state (to set for output, to expect without triggering change for input)
    ///   Note: aInitialState is logic state (pin state will be inverse if aInverted is set)
    DigitalIo(const char* aName, bool aOutput, bool aInverted = false, bool aInitialState = false);
    virtual ~DigitalIo();

		/// get name
		const char *getName() { return name.c_str(); };

    /// check for output
    bool isOutput() { return output; };
		
    /// get state of GPIO
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
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
	typedef boost::intrusive_ptr<DigitalIo> DigitalIoPtr;
	
	

  /// GPIO used as pushbutton
  class ButtonInput : public DigitalIo
  {
    typedef DigitalIo inherited;

  public:
    /// button event handler
    /// @param aButtonP the button
    /// @param aState the current state of the button (relevant when handler was installed with aPressAndRelease set)
    /// @param aHasChanged set when reporting a state change, cleared when reporting the same state again (when repeatActiveReport set)
    /// @param aTimeSincePreviousChange time passed since previous button state change (to easily detect long press actions etc.)
    typedef boost::function<void (ButtonInput *aButtonP, bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)> ButtonHandlerCB;

  private:
    bool lastState;
    MLMicroSeconds lastChangeTime;
    bool reportPressAndRelease;
    ButtonHandlerCB buttonHandler;
    MLMicroSeconds repeatActiveReport;
    MLMicroSeconds lastActiveReport;

    bool poll(MLMicroSeconds aTimestamp);
    
  public:
    /// Create pushbutton
    /// @param aGpioName name of the GPIO where the pushbutton is connected
    /// @param aInverted inverted polarity (output high level is treated as logic false)
    ButtonInput(const char* aGpioName, bool aInverted);

    /// destructor
    virtual ~ButtonInput();


    /// set handler to be called on pushbutton events
    /// @param aButtonHandler handler for pushbutton events
    /// @param aPressAndRelease if set, both pressing and releasing button generates event.
    ///   Otherwise, only one event is issued per button press (on button release)
    /// @param aRepeatActiveReport time after which a still pressed button is reported again
    void setButtonHandler(ButtonHandlerCB aButtonHandler, bool aPressAndRelease, MLMicroSeconds aRepeatActiveReport=p44::Never);
    
  };
	typedef boost::intrusive_ptr<ButtonInput> ButtonInputPtr;

	
	
  /// GPIO used for indicator (e.g. LED)
  class IndicatorOutput : public DigitalIo
  {
    typedef DigitalIo inherited;

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

    /// destructor
    virtual ~IndicatorOutput();

    /// activate the output for a certain time period, then switch off again
    /// @param aOnTime how long indicator should stay active
    void onFor(MLMicroSeconds aOnTime);

    /// blink indicator for a certain time period, with a given blink period and on ratio
    /// @param aOnTime how long indicator should stay active, or p44::infinite to keep blinking
    /// @param aBlinkPeriod how fast the blinking should be
    /// @param aOnRatioPercent how many percents of aBlinkPeriod the indicator should be on
    void blinkFor(MLMicroSeconds aOnTime, MLMicroSeconds aBlinkPeriod = 600*MilliSecond, int aOnRatioPercent = 50);

    /// stop blinking/timed activation immediately
    void stop();

    /// stop blinking/timed activation immediately and turn indicator off
    void steadyOff();

    /// stop blinking/timed activation immediately and turn indicator on
    void steadyOn();


  };
	typedef boost::intrusive_ptr<IndicatorOutput> IndicatorOutputPtr;



} // namespace p44

#endif /* defined(__p44utils__digitalio__) */
