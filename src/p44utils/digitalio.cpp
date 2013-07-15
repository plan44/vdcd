//
//  digitalio.cpp
//  p44bridged
//
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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
  if (busName=="gpio") {
    // Linux GPIO
    ioPin = IOPinPtr(new GpioPin(pinName.c_str(), output, aInitialState));
  }
  else if (busName.substr(0,3)=="i2c") {
    // i2c<busnum>.<devicespec>.<pinnum>
    int busNumber = atoi(busName.c_str()+3);
    int pinNumber = atoi(pinName.c_str());
    ioPin = IOPinPtr(new I2CPin(busNumber, deviceName.c_str(), pinNumber, output, aInitialState));
  }
  else {
    // default to simulated pin
    ioPin = IOPinPtr(new SimPin(name.c_str(), output, aInitialState!=inverted)); // set even for inputs
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
  DigitalIo(aName, false, aInverted, false)
{
  // save params
  lastState = false; // assume inactive to start with
  lastChangeTime = MainLoop::now();
}


ButtonInput::~ButtonInput()
{
  MainLoop::currentMainLoop()->unregisterIdleHandlers(this);
}


void ButtonInput::setButtonHandler(ButtonHandlerCB aButtonHandler, bool aPressAndRelease)
{
  reportPressAndRelease = aPressAndRelease;
  buttonHandler = aButtonHandler;
  if (buttonHandler) {
    MainLoop::currentMainLoop()->registerIdleHandler(this, boost::bind(&ButtonInput::poll, this, _2));
  }
  else {
    // unregister
    MainLoop::currentMainLoop()->unregisterIdleHandlers(this);
  }
}


#define DEBOUNCE_TIME 1000 // 1mS

bool ButtonInput::poll(MLMicroSeconds aTimestamp)
{
  bool newState = isSet();
  if (newState!=lastState && aTimestamp-lastChangeTime>DEBOUNCE_TIME) {
    // consider this a state change
    lastState = newState;
    lastChangeTime = aTimestamp;
    // report if needed
    if (!newState || reportPressAndRelease) {
      buttonHandler(this, newState, aTimestamp);
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
  MainLoop::currentMainLoop()->registerIdleHandler(this, boost::bind(&IndicatorOutput::timer, this, _2));
}


IndicatorOutput::~IndicatorOutput()
{
  MainLoop::currentMainLoop()->unregisterIdleHandlers(this);
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
  off();
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
