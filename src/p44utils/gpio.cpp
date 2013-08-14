//
//  gpio.cpp
//  p44utils
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

#include "gpio.h" // NS9XXX GPIO header, included in project

#include "logger.hpp"
#include "mainloop.hpp"

using namespace p44;


#pragma mark - GPIO via modern kernel support

#define GPIO_SYS_CLASS_PATH "/sys/class/gpio"


GpioPin::GpioPin(int aGpioNo, bool aOutput, bool aInitialState) :
  gpioNo(aGpioNo),
  output(aOutput),
  pinState(aInitialState),
  gpioFD(-1)
{
  int tempFd;
  string name;
  string s = string_format("%d", aGpioNo);
  // have the kernel export the pin
  name = string_format("%s/export",GPIO_SYS_CLASS_PATH);
  tempFd = open(name.c_str(), O_WRONLY);
  if (tempFd<0) { LOG(LOG_ERR,"Cannot open GPIO export file %s: %s\n", name.c_str(), strerror(errno)); return; }
  write(tempFd, s.c_str(), s.length());
  close(tempFd);
  // save base path
  string basePath = string_format("%s/gpio%d", GPIO_SYS_CLASS_PATH, aGpioNo);
  // configure
  name = basePath + "/direction";
  tempFd = open(name.c_str(), O_RDWR);
  if (tempFd<0) { LOG(LOG_ERR,"Cannot open GPIO direction file %s: %s\n", name.c_str(), strerror(errno)); return; }
  if (output) {
    // output
    // - set output with initial value
    s = pinState ? "high" : "low";
    write(tempFd, s.c_str(), s.length());
  }
  else {
    // input
    // - set input
    s = "in";
    write(tempFd, s.c_str(), s.length());
  }
  close(tempFd);
  // now keep the value FD open
  name = basePath + "/value";
  gpioFD = open(name.c_str(), O_RDWR);
  if (gpioFD<0) { LOG(LOG_ERR,"Cannot open GPIO value file %s: %s\n", name.c_str(), strerror(errno)); return; }
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
  else {
    // is input
    if (gpioFD<0)
      return false; // non-working pins always return false
    else {
      // read from input
      char buf[2];
      lseek(gpioFD, 0, SEEK_SET);
      if (read(gpioFD, buf, 1)>0) {
        return buf[0]!='0';
      }
    }
  }
  return false;
}


void GpioPin::setState(bool aState)
{
  if (!output) return; // non-outputs cannot be set
  if (gpioFD<0) return; // non-existing pins cannot be set
  pinState = aState;
  // - set value
  char buf[2];
  buf[0] = pinState ? '1' : '0';
  buf[1] = 0;
  write(gpioFD, buf, 1);
}


//  Sysfs Interface for Userspace (OPTIONAL)
//  ========================================
//  Platforms which use the "gpiolib" implementors framework may choose to
//  configure a sysfs user interface to GPIOs.  This is different from the
//  debugfs interface, since it provides control over GPIO direction and
//  value instead of just showing a gpio state summary.  Plus, it could be
//  present on production systems without debugging support.
//
//  Given appropriate hardware documentation for the system, userspace could
//  know for example that GPIO #23 controls the write protect line used to
//  protect boot loader segments in flash memory.  System upgrade procedures
//  may need to temporarily remove that protection, first importing a GPIO,
//  then changing its output state, then updating the code before re-enabling
//  the write protection.  In normal use, GPIO #23 would never be touched,
//  and the kernel would have no need to know about it.
//
//  Again depending on appropriate hardware documentation, on some systems
//  userspace GPIO can be used to determine system configuration data that
//  standard kernels won't know about.  And for some tasks, simple userspace
//  GPIO drivers could be all that the system really needs.
//
//  Note that standard kernel drivers exist for common "LEDs and Buttons"
//  GPIO tasks:  "leds-gpio" and "gpio_keys", respectively.  Use those
//  instead of talking directly to the GPIOs; they integrate with kernel
//  frameworks better than your userspace code could.
//
//
//  Paths in Sysfs
//  --------------
//  There are three kinds of entry in /sys/class/gpio:
//
//     -  Control interfaces used to get userspace control over GPIOs;
//
//     -  GPIOs themselves; and
//
//     -  GPIO controllers ("gpio_chip" instances).
//
//  That's in addition to standard files including the "device" symlink.
//
//  The control interfaces are write-only:
//
//      /sys/class/gpio/
//
//        "export" ... Userspace may ask the kernel to export control of
//      a GPIO to userspace by writing its number to this file.
//
//      Example:  "echo 19 > export" will create a "gpio19" node
//      for GPIO #19, if that's not requested by kernel code.
//
//        "unexport" ... Reverses the effect of exporting to userspace.
//
//      Example:  "echo 19 > unexport" will remove a "gpio19"
//      node exported using the "export" file.
//
//  GPIO signals have paths like /sys/class/gpio/gpio42/ (for GPIO #42)
//  and have the following read/write attributes:
//
//      /sys/class/gpio/gpioN/
//
//    "direction" ... reads as either "in" or "out".  This value may
//      normally be written.  Writing as "out" defaults to
//      initializing the value as low.  To ensure glitch free
//      operation, values "low" and "high" may be written to
//      configure the GPIO as an output with that initial value.
//
//      Note that this attribute *will not exist* if the kernel
//      doesn't support changing the direction of a GPIO, or
//      it was exported by kernel code that didn't explicitly
//      allow userspace to reconfigure this GPIO's direction.
//
//    "value" ... reads as either 0 (low) or 1 (high).  If the GPIO
//      is configured as an output, this value may be written;
//      any nonzero value is treated as high.
//
//      If the pin can be configured as interrupt-generating interrupt
//      and if it has been configured to generate interrupts (see the
//      description of "edge"), you can poll(2) on that file and
//      poll(2) will return whenever the interrupt was triggered. If
//      you use poll(2), set the events POLLPRI and POLLERR. If you
//      use select(2), set the file descriptor in exceptfds. After
//      poll(2) returns, either lseek(2) to the beginning of the sysfs
//      file and read the new value or close the file and re-open it
//      to read the value.
//
//    "edge" ... reads as either "none", "rising", "falling", or
//      "both". Write these strings to select the signal edge(s)
//      that will make poll(2) on the "value" file return.
//
//      This file exists only if the pin can be configured as an
//      interrupt generating input pin.
//
//    "active_low" ... reads as either 0 (false) or 1 (true).  Write
//      any nonzero value to invert the value attribute both
//      for reading and writing.  Existing and subsequent
//      poll(2) support configuration via the edge attribute
//      for "rising" and "falling" edges will follow this
//      setting.
//
//  GPIO controllers have paths like /sys/class/gpio/gpiochip42/ (for the
//  controller implementing GPIOs starting at #42) and have the following
//  read-only attributes:
//
//      /sys/class/gpio/gpiochipN/
//
//        "base" ... same as N, the first GPIO managed by this chip
//
//        "label" ... provided for diagnostics (not always unique)
//
//        "ngpio" ... how many GPIOs this manges (N to N + ngpio - 1)
//
//  Board documentation should in most cases cover what GPIOs are used for
//  what purposes.  However, those numbers are not always stable; GPIOs on
//  a daughtercard might be different depending on the base board being used,
//  or other cards in the stack.  In such cases, you may need to use the
//  gpiochip nodes (possibly in conjunction with schematics) to determine
//  the correct GPIO number to use for a given signal.
//
//
//  Exporting from Kernel code
//  --------------------------
//  Kernel code can explicitly manage exports of GPIOs which have already been
//  requested using gpio_request():
//
//    /* export the GPIO to userspace */
//    int gpio_export(unsigned gpio, bool direction_may_change);
//
//    /* reverse gpio_export() */
//    void gpio_unexport();
//
//    /* create a sysfs link to an exported GPIO node */
//    int gpio_export_link(struct device *dev, const char *name,
//      unsigned gpio)
//
//    /* change the polarity of a GPIO node in sysfs */
//    int gpio_sysfs_set_active_low(unsigned gpio, int value);
//
//  After a kernel driver requests a GPIO, it may only be made available in
//  the sysfs interface by gpio_export().  The driver can control whether the
//  signal direction may change.  This helps drivers prevent userspace code
//  from accidentally clobbering important system state.
//
//  This explicit exporting can help with debugging (by making some kinds
//  of experiments easier), or can provide an always-there interface that's
//  suitable for documenting as part of a board support package.
//
//  After the GPIO has been exported, gpio_export_link() allows creating
//  symlinks from elsewhere in sysfs to the GPIO sysfs node.  Drivers can
//  use this to provide the interface under their own device in sysfs with
//  a descriptive name.
//
//  Drivers can use gpio_sysfs_set_active_low() to hide GPIO line polarity
//  differences between boards from user space.  This only affects the
//  sysfs interface.  Polarity change can be done both before and after
//  gpio_export(), and previously enabled poll(2) support for either
//  rising or falling edge will be reconfigured to follow this setting.




#pragma mark - GPIO for NS9xxx (Digi ME 9210 LX)

GpioNS9XXXPin::GpioNS9XXXPin(const char* aGpioName, bool aOutput, bool aInitialState) :
  gpioFD(-1),
  pinState(false)
{
  // save params
  output = aOutput;
  name = aGpioName;
  pinState = aInitialState; // set even for inputs

  int ret_val;
  // open device
  string gpiopath(GPION9XXX_DEVICES_BASEPATH);
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


GpioNS9XXXPin::~GpioNS9XXXPin()
{
  if (gpioFD>0) {
    close(gpioFD);
  }
}



bool GpioNS9XXXPin::getState()
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


void GpioNS9XXXPin::setState(bool aState)
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
