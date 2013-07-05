//
//  gpio.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "gpio.hpp"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "gpio.h"

#include "logger.hpp"

#include "mainloop.hpp"

using namespace p44;


#pragma mark - GPIO


Gpio::Gpio(const char* aGpioName, bool aOutput, bool aInverted, bool aInitialState) :
  gpioFD(-1),
  pinState(false)
{
  // save params
  output = aOutput;
  inverted = aInverted;
  name = aGpioName;
  pinState = aInitialState!=inverted; // set even for inputs

  #if GPIO_SIMULATION
  static char nextGpioSimKey = 'a';
  printf("Initialized GPIO %s as %s%s with initial state %s\n", name.c_str(), aInverted ? "inverted " : "", aOutput ? "output" : "input", pinState ? "HI" : "LO");
  if (!output) {
    consoleKey = ConsoleKeyManager::sharedKeyManager()->newConsoleKey(
      nextGpioSimKey++,
      name.c_str(),
      pinState
    );
  }
  #else
  int ret_val;
  // open device
  //#if 0
  #ifdef __APPLE__
  DBGLOG(LOG_ERR,"No GPIOs supported on Mac OS X\n");
  #else
  string gpiopath(GPIO_DEVICES_BASEPATH);
  gpiopath.append(name);
  gpioFD = open(gpiopath.c_str(), O_RDWR);
  if (gpioFD<0) {
    DBGLOG(LOG_ERR,"Cannot open GPIO device %s: %s\n", name.c_str(), strerror(errno));
    return;
  }
  // configure
  if (output) {
    // output
    if ((ret_val = ioctl(gpioFD, GPIO_CONFIG_AS_OUT)) < 0) {
      DBGLOG(LOG_ERR,"GPIO_CONFIG_AS_OUT failed for %s: %s\n", name.c_str(), strerror(errno));
      return;
    }
    // set state immediately
    set(aInitialState);
  }
  else {
    // input
    if ((ret_val = ioctl(gpioFD, GPIO_CONFIG_AS_INP)) < 0) {
      DBGLOG(LOG_ERR,"GPIO_CONFIG_AS_INP failed for %s: %s\n", name.c_str(), strerror(errno));
      return;
    }
  }
  #endif // Apple
  #endif
}


Gpio::~Gpio()
{
  #if !GPIO_SIMULATION
  if (gpioFD>0) {
    close(gpioFD);
  }
  #endif
}


bool Gpio::isSet()
{
  if (output)
    return pinState != inverted; // just return last set state
  #if GPIO_SIMULATION
  return (bool)consoleKey->isSet() != inverted;
  #else
  if (gpioFD<0)
    return false; // non-working pins always return false
  else {
    // read from input
    int inval;
    int ret_val;
    if ((ret_val = ioctl(gpioFD, GPIO_READ_PIN_VAL, &inval)) < 0) {
      DBGLOG(LOG_ERR,"GPIO_READ_PIN_VAL failed for %s: %s\n", name.c_str(), strerror(errno));
      return false;
    }
    return (bool)inval != inverted;
  }
  #endif
}


void Gpio::set(bool aState)
{
  if (!output) return; // non-outputs cannot be set
  #if GPIO_SIMULATION
  pinState = aState != inverted;
  printf(">>> GPIO %s set to %s\n", name.c_str(), pinState ? "HI" : "LO");
  #else
  if (gpioFD<0) return; // non-existing pins cannot be set
  pinState = aState != inverted;
  // - set value
  int setval = pinState;
  int ret_val;
  if ((ret_val = ioctl(gpioFD, GPIO_WRITE_PIN_VAL, &setval)) < 0) {
    DBGLOG(LOG_ERR,"GPIO_WRITE_PIN_VAL failed for %s: %s\n", name.c_str(), strerror(errno));
    return;
  }
  #endif
}


void Gpio::on()
{
  set(true);
}


void Gpio::off()
{
  set(false);
}


bool Gpio::toggle()
{
  bool state = isSet();
  if (output) {
    state = !state;
    set(state);
  }
  return state;
}


#pragma mark - Button input


ButtonInput::ButtonInput(const char* aGpioName, bool aInverted) :
  Gpio(aGpioName, false, aInverted, false)
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

IndicatorOutput::IndicatorOutput(const char* aGpioName, bool aInverted, bool aInitiallyOn) :
  Gpio(aGpioName, true, aInverted, aInitiallyOn),
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
