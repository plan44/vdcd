//
//  digitalio.cpp
//  p44utils
//
//  Copyright (c) 2013-2014 plan44.ch. All rights reserved.
//

#include "digitalio.hpp"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "iopin.hpp"
#include "gpio.hpp"
#include "i2c.hpp"

#include "logger.hpp"
#include "mainloop.hpp"

using namespace p44;


DigitalIo::DigitalIo(const char* aName, bool aOutput, bool aInverted, bool aInitialState)
{
  // save params
  output = aOutput;
  inverted = aInverted;
  name = aName;
  bool initialPinState = aInitialState!=inverted;
  // check for missing pin (no pin, just silently keeping state)
  if (name=="missing") {
    ioPin = IOPinPtr(new MissingPin(initialPinState));
    return;
  }
  // dissect name into bus, device, pin
  string busName;
  string deviceName;
  string pinName;
  size_t i = name.find_first_of('.');
  if (i==string::npos) {
    // no structured name, assume GPIO
    busName = "gpio";
  }
  else {
    busName = name.substr(0,i);
    // rest is device + pinname or just pinname
    pinName = name.substr(i+1,string::npos);
    i = pinName.find_first_of('.');
    if (i!=string::npos) {
      // separate device and pin names
      // - extract device name
      deviceName = pinName.substr(0,i);
      // - remove device name from pin name string
      pinName.erase(0,i+1);
    }
  }
  // now create appropriate pin
  DBGLOG(LOG_DEBUG, "DigitalIo: bus name = '%s'\n", busName.c_str());
  #ifndef __APPLE__
  if (busName=="gpio") {
    // Linux generic GPIO
    // gpio.<gpionumber>
    int pinNumber = atoi(pinName.c_str());
    ioPin = IOPinPtr(new GpioPin(pinNumber, output, initialPinState));
  }
  else
  #endif
  #ifdef DIGI_ESP
  if (busName=="gpioNS9XXXX") {
    // gpioNS9XXXX.<pinname>
    // NS9XXX driver based GPIO (Digi ME 9210 LX)
    ioPin = IOPinPtr(new GpioNS9XXXPin(pinName.c_str(), output, initialPinState));
  }
  else
  #endif
  if (busName.substr(0,3)=="i2c") {
    // i2c<busnum>.<devicespec>.<pinnum>
    int busNumber = atoi(busName.c_str()+3);
    int pinNumber = atoi(pinName.c_str());
    ioPin = IOPinPtr(new I2CPin(busNumber, deviceName.c_str(), pinNumber, output, initialPinState));
  }
  else {
    // all other/unknown bus names default to simulated pin
    ioPin = IOPinPtr(new SimPin(name.c_str(), output, initialPinState)); // set even for inputs
  }
}


DigitalIo::~DigitalIo()
{
}


bool DigitalIo::isSet()
{
  return ioPin->getState() != inverted;
}


void DigitalIo::set(bool aState)
{
  ioPin->setState(aState!=inverted);
}


void DigitalIo::on()
{
  set(true);
}


void DigitalIo::off()
{
  set(false);
}


bool DigitalIo::toggle()
{
  bool state = isSet();
  if (output) {
    state = !state;
    set(state);
  }
  return state;
}


#pragma mark - Button input


ButtonInput::ButtonInput(const char* aName, bool aInverted) :
  DigitalIo(aName, false, aInverted, false),
  repeatActiveReport(Never),
  lastActiveReport(Never)
{
  // save params
  lastState = false; // assume inactive to start with
  lastChangeTime = MainLoop::now();
}


ButtonInput::~ButtonInput()
{
  MainLoop::currentMainLoop().unregisterIdleHandlers(this);
}


void ButtonInput::setButtonHandler(ButtonHandlerCB aButtonHandler, bool aPressAndRelease, MLMicroSeconds aRepeatActiveReport)
{
  reportPressAndRelease = aPressAndRelease;
  repeatActiveReport = aRepeatActiveReport;
  buttonHandler = aButtonHandler;
  if (buttonHandler) {
    MainLoop::currentMainLoop().registerIdleHandler(this, boost::bind(&ButtonInput::poll, this, _1));
  }
  else {
    // unregister
    MainLoop::currentMainLoop().unregisterIdleHandlers(this);
  }
}


#define DEBOUNCE_TIME 1000 // 1mS

bool ButtonInput::poll(MLMicroSeconds aTimestamp)
{
  bool newState = isSet();
  if (newState!=lastState && aTimestamp-lastChangeTime>DEBOUNCE_TIME) {
    // report if needed
    if (!newState || reportPressAndRelease) {
      buttonHandler(newState, true, aTimestamp-lastChangeTime);
    }
    // consider this a state change
    lastState = newState;
    lastChangeTime = aTimestamp;
    // active state reported now
    if (newState) lastActiveReport = aTimestamp;
  }
  else {
    // no state change
    // - check if re-reporting pressed button state is required
    if (newState && repeatActiveReport!=Never && aTimestamp-lastActiveReport>=repeatActiveReport) {
      lastActiveReport = aTimestamp;
      // re-report pressed state
      buttonHandler(true, false, aTimestamp-lastChangeTime);
    }
  }
  return true;
}



#pragma mark - Indicator output

IndicatorOutput::IndicatorOutput(const char* aName, bool aInverted, bool aInitiallyOn) :
  DigitalIo(aName, true, aInverted, aInitiallyOn),
  switchOffAt(Never),
  blinkOnTime(Never),
  blinkOffTime(Never)
{
  MainLoop::currentMainLoop().registerIdleHandler(this, boost::bind(&IndicatorOutput::timer, this, _1));
}


IndicatorOutput::~IndicatorOutput()
{
  MainLoop::currentMainLoop().unregisterIdleHandlers(this);
}


void IndicatorOutput::onFor(MLMicroSeconds aOnTime)
{
  blinkOnTime = Never;
  blinkOffTime = Never;
  set(true);
  if (aOnTime>0)
    switchOffAt = MainLoop::now()+aOnTime;
  else
    switchOffAt = Never;
}


void IndicatorOutput::blinkFor(MLMicroSeconds aOnTime, MLMicroSeconds aBlinkPeriod, int aOnRatioPercent)
{
  onFor(aOnTime);
  blinkOnTime =  (aBlinkPeriod*aOnRatioPercent*10)/1000;
  blinkOffTime = aBlinkPeriod - blinkOnTime;
  blinkToggleAt = MainLoop::now()+blinkOnTime;
}


void IndicatorOutput::stop()
{
  blinkOnTime = Never;
  blinkOffTime = Never;
  switchOffAt = Never;
}


void IndicatorOutput::steadyOff()
{
  stop();
  off();
}


void IndicatorOutput::steadyOn()
{
  stop();
  on();
}





bool IndicatorOutput::timer(MLMicroSeconds aTimestamp)
{
  // check off time first
  if (switchOffAt!=Never && aTimestamp>=switchOffAt) {
    stop();
  }
  else if (blinkOnTime!=Never) {
    // blinking enabled
    if (aTimestamp>=blinkToggleAt) {
      if (toggle()) {
        // turned on, blinkOnTime starts
        blinkToggleAt = aTimestamp + blinkOnTime;
      }
      else {
        // turned off, blinkOffTime starts
        blinkToggleAt = aTimestamp + blinkOffTime;
      }
    }
  }
  return true;
}
