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

using namespace p44;

#if GPIO_SIMULATION

#include "mainloop.hpp"

#include <sys/select.h>
#include <termios.h>

static int kbhit() {
  static const int STDIN = 0;
  static bool initialized = false;

  if (! initialized) {
    // Use termios to turn off line buffering
    termios term;
    tcgetattr(STDIN, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(STDIN, TCSANOW, &term);
    setbuf(stdin, NULL);
    initialized = true;
  }

  int bytesWaiting;
  ioctl(STDIN, FIONREAD, &bytesWaiting);
  return bytesWaiting;
}


static int numSimKeys = 0;
uint32_t inputBits = 0;

static bool simInputPoll()
{
  if (kbhit()) {
    char  c = getchar();
    c -= '1';
    if (c>=0 && c<=9) {
      inputBits = inputBits ^ (1<<c);
      printf(">>> GPIO '%c' toggled to %d\n", c+'1', (inputBits & (1<<c))>0);
    }
  }
  return true; // done for this cycle
}


static int initSimInput(bool aInitialState)
{
  if (numSimKeys==0) {
    // first simulated GPIO input
    MainLoop::currentMainLoop()->registerIdleHandler(NULL, boost::bind(&simInputPoll));
  }
  if (aInitialState) inputBits |= (1<<numSimKeys); // set initial state if not zero
  printf("- initial input state is %d, press '%c' key to toggle state\n", aInitialState, '1'+numSimKeys);
  return numSimKeys++;
}

#endif // GPIO_SIMULATION


Gpio::Gpio(const char* aGpioName, bool aOutput, bool aInverted, bool aInitialState) :
  gpioFD(-1),
  pinState(false)
{
  // save params
  output = aOutput;
  inverted = aInverted;
  name = aGpioName;
  pinState = aInitialState; // set even for inputs

  #if GPIO_SIMULATION
  printf("Initialized GPIO %s as %s%s\n", name.c_str(), aInverted ? "inverted " : "", aOutput ? "output" : "input");
  if (!output) {
    inputBitNo = initSimInput(pinState);
  }
  #else
  int ret_val;
  // open device
  //#if 0
  #ifdef __APPLE__
  DBGLOG(LOG_ERR,"No GPIOs supported on Mac OS X\n");
  #else
  string gpiopath("/dev/gpio/");
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
    setState(aInitialState);
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


bool Gpio::getState()
{
  if (output)
    return pinState != inverted; // just return last set state
  #if GPIO_SIMULATION
  return (bool)(inputBits & 1<<inputBitNo) != inverted;
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


void Gpio::setState(bool aState)
{
  if (!output) return; // non-outputs cannot be set
  #if GPIO_SIMULATION
  pinState = aState != inverted;
  printf(">>> GPIO %s set to %d\n", name.c_str(), pinState);
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
