//
//  gpio.cpp
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

#include "gpio.h" // Linux GPIO, included in project

#include "logger.hpp"
#include "mainloop.hpp"

using namespace p44;


#pragma mark - GPIO


GpioPin::GpioPin(const char* aGpioName, bool aOutput, bool aInitialState) :
  gpioFD(-1),
  pinState(false)
{
  // save params
  output = aOutput;
  name = aGpioName;
  pinState = aInitialState; // set even for inputs

  int ret_val;
  // open device
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
    setState(pinState);
  }
  else {
    // input
    if ((ret_val = ioctl(gpioFD, GPIO_CONFIG_AS_INP)) < 0) {
      DBGLOG(LOG_ERR,"GPIO_CONFIG_AS_INP failed for %s: %s\n", name.c_str(), strerror(errno));
      return;
    }
  }
}


GpioPin::~GpioPin()
{
  if (gpioFD>0) {
    close(gpioFD);
  }
}



bool GpioPin::getState()
{
  if (output)
    return pinState; // just return last set state
  if (gpioFD<0)
    return false; // non-working pins always return false
  else {
    // read from input
    int inval;
    #ifndef __APPLE__
    int ret_val;
    if ((ret_val = ioctl(gpioFD, GPIO_READ_PIN_VAL, &inval)) < 0) {
      LOG(LOG_ERR,"GPIO_READ_PIN_VAL failed for %s: %s\n", name.c_str(), strerror(errno));
      return false;
    }
    #else
    DBGLOG(LOG_ERR,"ioctl(gpioFD, GPIO_READ_PIN_VAL, &dummy)\n");
    inval = 0;
    #endif
    return (bool)inval;
  }
}


void GpioPin::setState(bool aState)
{
  if (!output) return; // non-outputs cannot be set
  if (gpioFD<0) return; // non-existing pins cannot be set
  pinState = aState;
  // - set value
  int setval = pinState;
  #ifndef __APPLE__
  int ret_val;
  if ((ret_val = ioctl(gpioFD, GPIO_WRITE_PIN_VAL, &setval)) < 0) {
    LOG(LOG_ERR,"GPIO_WRITE_PIN_VAL failed for %s: %s\n", name.c_str(), strerror(errno));
    return;
  }
  #else
  DBGLOG(LOG_ERR,"ioctl(gpioFD, GPIO_WRITE_PIN_VAL, %d)\n", setval);
  #endif
}
