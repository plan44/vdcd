//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 1
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL LOG_DEBUG


#include "i2c.hpp"

#if (defined(__APPLE__) || defined(DIGI_ESP)) && !defined(RASPBERRYPI)
#define DISABLE_I2C 1
#endif

#ifndef DISABLE_I2C
#include <linux/i2c-dev.h>
#else
#warning "No i2C supported on this platform - just showing calls in output"
#endif

using namespace p44;

#pragma mark - I2C Manager

static I2CManager *sharedI2CManager = NULL;


I2CManager::I2CManager()
{
}

I2CManager::~I2CManager()
{
}


I2CManager *I2CManager::sharedManager()
{
  if (!sharedI2CManager) {
    sharedI2CManager = new I2CManager();
  }
  return sharedI2CManager;
}




I2CDevicePtr I2CManager::getDevice(int aBusNumber, const char *aDeviceID)
{
  // find or create bus
  I2CBusMap::iterator pos = busMap.find(aBusNumber);
  I2CBusPtr bus;
  if (pos!=busMap.end()) {
    bus = pos->second;
  }
  else {
    // bus does not exist yet, create it
    bus = I2CBusPtr(new I2CBus(aBusNumber));
    busMap[aBusNumber] = bus;
  }
  // dissect device ID into type and busAddress
  // - type string
  string typeString = "generic";
  string s = aDeviceID;
  size_t i = s.find_first_of('@');
  if (i!=string::npos) {
    typeString = s.substr(0,i);
    s.erase(0,i+1);
  }
  // - device address (hex)
  int deviceAddress = 0;
  sscanf(s.c_str(), "%x", &deviceAddress);
  // reconstruct fully qualified device name for searching
  s = string_format("%s@%02X", typeString.c_str(), deviceAddress);
  // get possibly already existing device of correct type at that address
  I2CDevicePtr dev = bus->getDevice(s.c_str());
  if (!dev) {
    // create device from typestring
    if (typeString=="TCA9555")
      dev = I2CDevicePtr(new TCA9555(deviceAddress, bus.get()));
    else if (typeString=="PCF8574")
      dev = I2CDevicePtr(new PCF8574(deviceAddress, bus.get()));
    // TODO: add more device types
    // Register new device
    if (dev) {
      bus->registerDevice(dev);
    }
  }
  return dev;
}


#pragma mark - I2CBus


I2CBus::I2CBus(int aBusNumber) :
  busFD(-1),
  busNumber(aBusNumber),
  lastDeviceAddress(-1)
{
}


I2CBus::~I2CBus()
{
  closeBus();
}


void I2CBus::registerDevice(I2CDevicePtr aDevice)
{
  deviceMap[aDevice->deviceID()] = aDevice;
}


I2CDevicePtr I2CBus::getDevice(const char *aDeviceID)
{
  I2CDeviceMap::iterator pos = deviceMap.find(aDeviceID);
  if (pos!=deviceMap.end())
    return pos->second;
  return I2CDevicePtr();
}



bool I2CBus::I2CReadByte(I2CDevice *aDeviceP, uint8_t &aByte)
{
  if (!accessDevice(aDeviceP)) return false; // cannot read
  #ifndef DISABLE_I2C
  int res = i2c_smbus_read_byte(busFD);
  #else
  int res = 0x42; // dummy
  #endif
  FOCUSLOG("i2c_smbus_read_byte() = %d / 0x%02X\n", res, res);
  if (res<0) return false;
  aByte = (uint8_t)res;
  return true;
}


bool I2CBus::I2CWriteByte(I2CDevice *aDeviceP, uint8_t aByte)
{
  if (!accessDevice(aDeviceP)) return false; // cannot write
  #ifndef DISABLE_I2C
  int res = i2c_smbus_write_byte(busFD, aByte);
  #else
  int res = 1; // ok
  #endif
  FOCUSLOG("i2c_smbus_write_byte(0x%02X) = %d\n", aByte, res);
  return (res>=0);
}



bool I2CBus::SMBusReadByte(I2CDevice *aDeviceP, uint8_t aRegister, uint8_t &aByte)
{
  if (!accessDevice(aDeviceP)) return false; // cannot read
  #ifndef DISABLE_I2C
  int res = i2c_smbus_read_byte_data(busFD, aRegister);
  #else
  int res = 0x42; // dummy
  #endif
  FOCUSLOG("i2c_smbus_read_byte_data(0x%02X) = %d / 0x%02X\n", aRegister, res, res);
  if (res<0) return false;
  aByte = (uint8_t)res;
  return true;
}


bool I2CBus::SMBusReadWord(I2CDevice *aDeviceP, uint8_t aRegister, uint16_t &aWord)
{
  if (!accessDevice(aDeviceP)) return false; // cannot read
  #ifndef DISABLE_I2C
  int res = i2c_smbus_read_word_data(busFD, aRegister);
  if (res<0) return false;
  #else
  int res = 0x4242; // dummy
  #endif
  FOCUSLOG("i2c_smbus_read_word_data(0x%02X) = %d / 0x%04X\n", aRegister, res, res);
  if (res<0) return false;
  aWord = (uint16_t)res;
  return true;
}


bool I2CBus::SMBusWriteByte(I2CDevice *aDeviceP, uint8_t aRegister, uint8_t aByte)
{
  if (!accessDevice(aDeviceP)) return false; // cannot write
  #ifndef DISABLE_I2C
  int res = i2c_smbus_write_byte_data(busFD, aRegister, aByte);
  #else
  int res = 1; // ok
  #endif
  FOCUSLOG("i2c_smbus_write_byte_data(0x%02X, 0x%02X) = %d\n", aRegister, aByte, res);
  return (res>=0);
}


bool I2CBus::SMBusWriteWord(I2CDevice *aDeviceP, uint8_t aRegister, uint16_t aWord)
{
  if (!accessDevice(aDeviceP)) return false; // cannot write
  #ifndef DISABLE_I2C
  int res = i2c_smbus_write_word_data(busFD, aRegister, aWord);
  #else
  int res = 1; // ok
  #endif
  FOCUSLOG("i2c_smbus_write_word_data(0x%02X, 0x%04X) = %d\n", aRegister, aWord, res);
  return (res>=0);
}



bool I2CBus::accessDevice(I2CDevice *aDeviceP)
{
  if (!accessBus())
    return false;
  if (aDeviceP->deviceAddress == lastDeviceAddress)
    return true; // already set to access that device
  // address the device
  #ifndef DISABLE_I2C
  if (ioctl(busFD, I2C_SLAVE, aDeviceP->deviceAddress) < 0) {
    LOG(LOG_ERR,"Error: Cannot access device '%s' on bus %d\n", aDeviceP->deviceID().c_str(), busNumber);
    lastDeviceAddress = -1; // invalidate
    return false;
  }
  #endif
  FOCUSLOG("ioctl(busFD, I2C_SLAVE, 0x%02X)\n", aDeviceP->deviceAddress);
  // remember
  lastDeviceAddress = aDeviceP->deviceAddress;
  return true; // ok
}


bool I2CBus::accessBus()
{
  if (busFD>=0)
    return true; // already open
  // need to open
  string busDevName = string_format("/dev/i2c-%d", busNumber);
  #ifndef DISABLE_I2C
  busFD = open(busDevName.c_str(), O_RDWR);
  if (busFD<0) {
    LOG(LOG_ERR,"Error: Cannot open i2c bus device '%s'\n",busDevName.c_str());
    return false;
  }
  #else
  busFD = 1; // dummy, signalling open
  #endif
  FOCUSLOG("open(\"%s\", O_RDWR) = %d\n", busDevName.c_str(), busFD);
  return true;
}



void I2CBus::closeBus()
{
  if (busFD>=0) {
    #ifndef __APPLE__
    close(busFD);
    #endif
    busFD = -1;
  }
}



#pragma mark - I2CDevice


I2CDevice::I2CDevice(uint8_t aDeviceAddress, I2CBus *aBusP)
{
  i2cbus = aBusP;
  deviceAddress = aDeviceAddress;
}


string I2CDevice::deviceID()
{
  return string_format("%s@%02X", deviceType(), deviceAddress);
}




bool I2CDevice::isKindOf(const char *aDeviceType)
{
  return (strcmp(deviceType(),aDeviceType)==0);
}



#pragma mark - I2CBitPortDevice


I2CBitPortDevice::I2CBitPortDevice(uint8_t aDeviceAddress, I2CBus *aBusP) :
  inherited(aDeviceAddress, aBusP),
  outputEnableMask(0),
  pinStateMask(0)
{
}



bool I2CBitPortDevice::isKindOf(const char *aDeviceType)
{
  if (strcmp(deviceType(),aDeviceType)==0)
    return true;
  else
    return inherited::isKindOf(aDeviceType);
}


bool I2CBitPortDevice::getBitState(int aBitNo)
{
  uint32_t bitMask = 1<<aBitNo;
  if (outputEnableMask & bitMask) {
    // is output, just return the last set state
    return (outputStateMask & bitMask)!=0;
  }
  else {
    // is input, get actual input state
    updateInputState(aBitNo); // update
    return (pinStateMask & bitMask)!=0;
  }
}


void I2CBitPortDevice::setBitState(int aBitNo, bool aState)
{
  uint32_t bitMask = 1<<aBitNo;
  if (outputEnableMask & bitMask) {
    // is output, set new state (always, even if seemingly already set)
    if (aState)
      outputStateMask |= bitMask;
    else
      outputStateMask &= ~bitMask;
    // update hardware
    updateOutputs(aBitNo);
  }
}


void I2CBitPortDevice::setAsOutput(int aBitNo, bool aOutput, bool aInitialState)
{
  uint32_t bitMask = 1<<aBitNo;
  if (aOutput)
    outputEnableMask |= bitMask;
  else
    outputEnableMask &= ~bitMask;
  // before actually updating direction, set initial value
  setBitState(aBitNo, aInitialState);
  // now update direction
  updateDirection(aBitNo);
}



#pragma mark - TCA9555


TCA9555::TCA9555(uint8_t aDeviceAddress, I2CBus *aBusP) :
  inherited(aDeviceAddress, aBusP)
{
  // make sure we have all inputs
  updateDirection(0); // port 0
  updateDirection(8); // port 1
  // reset polarity inverter
  i2cbus->SMBusWriteByte(this, 4, 0); // reset polarity inversion port 0
  i2cbus->SMBusWriteByte(this, 5, 0); // reset polarity inversion port 1
}


bool TCA9555::isKindOf(const char *aDeviceType)
{
  if (strcmp(deviceType(),aDeviceType)==0)
    return true;
  else
    return inherited::isKindOf(aDeviceType);
}


void TCA9555::updateInputState(int aForBitNo)
{
  if (aForBitNo>15) return;
  uint8_t port = aForBitNo >> 3; // calculate port No
  uint8_t shift = 8*port;
  uint8_t data;
  i2cbus->SMBusReadByte(this, port, data); // get input byte
  pinStateMask = (pinStateMask & (~((uint32_t)0xFF) << shift)) | ((uint32_t)data << shift);
}


void TCA9555::updateOutputs(int aForBitNo)
{
  if (aForBitNo>15) return;
  uint8_t port = aForBitNo >> 3; // calculate port No
  uint8_t shift = 8*port;
  i2cbus->SMBusWriteByte(this, port+2, (outputStateMask >> shift) & 0xFF); // write output byte
}



void TCA9555::updateDirection(int aForBitNo)
{
  if (aForBitNo>15) return;
  updateOutputs(aForBitNo); // make sure output register has the correct value
  uint8_t port = aForBitNo >> 3; // calculate port No
  uint8_t shift = 8*port;
  uint8_t data = ~((outputEnableMask >> shift) & 0xFF); // TCA9555 config register has 1 for inputs, 0 for outputs
  i2cbus->SMBusWriteByte(this, port+6, data); // set input enable flags in reg 6 or 7
}


#pragma mark - PCF8574


PCF8574::PCF8574(uint8_t aDeviceAddress, I2CBus *aBusP) :
  inherited(aDeviceAddress, aBusP)
{
  // make sure we have all inputs
  updateDirection(0); // port 0
}


bool PCF8574::isKindOf(const char *aDeviceType)
{
  if (strcmp(deviceType(),aDeviceType)==0)
    return true;
  else
    return inherited::isKindOf(aDeviceType);
}


void PCF8574::updateInputState(int aForBitNo)
{
  if (aForBitNo>7) return;
  uint8_t data;
  if (i2cbus->I2CReadByte(this, data)) {
    pinStateMask = data;
  }
}


void PCF8574::updateOutputs(int aForBitNo)
{
  if (aForBitNo>7) return;
  // PCF8574 does not have a direction register, but reading just senses the pin level.
  // With output set to H, the pin is OC and can be set to Low
  // -> pins to be used as inputs must always be high
  uint8_t b =
    ((~outputEnableMask) & 0xFF) | // pins used as input must have output state High
    (outputStateMask & 0xFF); // pins used as output will have the correct state from beginning
  i2cbus->I2CWriteByte(this, b);
}



void PCF8574::updateDirection(int aForBitNo)
{
  // There is no difference in updating outputs or updating direction for the primitive PCF8574
  updateOutputs(aForBitNo);
}



#pragma mark - I2Cpin


/// create i2c based digital input or output pin
I2CPin::I2CPin(int aBusNumber, const char *aDeviceId, int aPinNumber, bool aOutput, bool aInitialState) :
  output(false),
  lastSetState(false)
{
  pinNumber = aPinNumber;
  output = aOutput;
  I2CDevicePtr dev = I2CManager::sharedManager()->getDevice(aBusNumber, aDeviceId);
  bitPortDevice = boost::dynamic_pointer_cast<I2CBitPortDevice>(dev);
  if (bitPortDevice) {
    bitPortDevice->setAsOutput(pinNumber, output, aInitialState);
    lastSetState = aInitialState;
  }
}


/// get state of pin
/// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
bool I2CPin::getState()
{
  if (bitPortDevice) {
    if (output)
      return lastSetState;
    else
      return bitPortDevice->getBitState(pinNumber);
  }
  return false;
}


/// set state of pin (NOP for inputs)
/// @param aState new state to set output to
void I2CPin::setState(bool aState)
{
  if (bitPortDevice && output)
    bitPortDevice->setBitState(pinNumber, aState);
  lastSetState = aState;
}





//  Usually, i2c devices are controlled by a kernel driver. But it is also
//  possible to access all devices on an adapter from userspace, through
//  the /dev interface. You need to load module i2c-dev for this.
//
//  Each registered i2c adapter gets a number, counting from 0. You can
//  examine /sys/class/i2c-dev/ to see what number corresponds to which adapter.
//  Alternatively, you can run "i2cdetect -l" to obtain a formated list of all
//  i2c adapters present on your system at a given time. i2cdetect is part of
//  the i2c-tools package.
//
//  I2C device files are character device files with major device number 89
//  and a minor device number corresponding to the number assigned as
//  explained above. They should be called "i2c-%d" (i2c-0, i2c-1, ...,
//  i2c-10, ...). All 256 minor device numbers are reserved for i2c.
//
//
//  C example
//  =========
//
//  So let's say you want to access an i2c adapter from a C program. The
//  first thing to do is "#include <linux/i2c-dev.h>". Please note that
//  there are two files named "i2c-dev.h" out there, one is distributed
//  with the Linux kernel and is meant to be included from kernel
//  driver code, the other one is distributed with i2c-tools and is
//  meant to be included from user-space programs. You obviously want
//  the second one here.
//
//  Now, you have to decide which adapter you want to access. You should
//  inspect /sys/class/i2c-dev/ or run "i2cdetect -l" to decide this.
//  Adapter numbers are assigned somewhat dynamically, so you can not
//  assume much about them. They can even change from one boot to the next.
//
//  Next thing, open the device file, as follows:
//
//    int file;
//    int adapter_nr = 2; /* probably dynamically determined */
//    char filename[20];
//
//    snprintf(filename, 19, "/dev/i2c-%d", adapter_nr);
//    file = open(filename, O_RDWR);
//    if (file < 0) {
//      /* ERROR HANDLING; you can check errno to see what went wrong */
//      exit(1);
//    }
//
//  When you have opened the device, you must specify with what device
//  address you want to communicate:
//
//    int addr = 0x40; /* The I2C address */
//
//    if (ioctl(file, I2C_SLAVE, addr) < 0) {
//      /* ERROR HANDLING; you can check errno to see what went wrong */
//      exit(1);
//    }
//
//  Well, you are all set up now. You can now use SMBus commands or plain
//  I2C to communicate with your device. SMBus commands are preferred if
//  the device supports them. Both are illustrated below.
//
//    __u8 register = 0x10; /* Device register to access */
//    __s32 res;
//    char buf[10];
//
//    /* Using SMBus commands */
//    res = i2c_smbus_read_word_data(file, register);
//    if (res < 0) {
//      /* ERROR HANDLING: i2c transaction failed */
//    } else {
//      /* res contains the read word */
//    }
//
//    /* Using I2C Write, equivalent of
//       i2c_smbus_write_word_data(file, register, 0x6543) */
//    buf[0] = register;
//    buf[1] = 0x43;
//    buf[2] = 0x65;
//    if (write(file, buf, 3) ! =3) {
//      /* ERROR HANDLING: i2c transaction failed */
//    }
//
//    /* Using I2C Read, equivalent of i2c_smbus_read_byte(file) */
//    if (read(file, buf, 1) != 1) {
//      /* ERROR HANDLING: i2c transaction failed */
//    } else {
//      /* buf[0] contains the read byte */
//    }
//
//  Note that only a subset of the I2C and SMBus protocols can be achieved by
//  the means of read() and write() calls. In particular, so-called combined
//  transactions (mixing read and write messages in the same transaction)
//  aren't supported. For this reason, this interface is almost never used by
//  user-space programs.
//
//  IMPORTANT: because of the use of inline functions, you *have* to use
//  '-O' or some variation when you compile your program!
//
//
//  Full interface description
//  ==========================
//
//  The following IOCTLs are defined:
//
//  ioctl(file, I2C_SLAVE, long addr)
//    Change slave address. The address is passed in the 7 lower bits of the
//    argument (except for 10 bit addresses, passed in the 10 lower bits in this
//    case).
//
//  ioctl(file, I2C_TENBIT, long select)
//    Selects ten bit addresses if select not equals 0, selects normal 7 bit
//    addresses if select equals 0. Default 0.  This request is only valid
//    if the adapter has I2C_FUNC_10BIT_ADDR.
//
//  ioctl(file, I2C_PEC, long select)
//    Selects SMBus PEC (packet error checking) generation and verification
//    if select not equals 0, disables if select equals 0. Default 0.
//    Used only for SMBus transactions.  This request only has an effect if the
//    the adapter has I2C_FUNC_SMBUS_PEC; it is still safe if not, it just
//    doesn't have any effect.
//
//  ioctl(file, I2C_FUNCS, unsigned long *funcs)
//    Gets the adapter functionality and puts it in *funcs.
//
//  ioctl(file, I2C_RDWR, struct i2c_rdwr_ioctl_data *msgset)
//    Do combined read/write transaction without stop in between.
//    Only valid if the adapter has I2C_FUNC_I2C.  The argument is
//    a pointer to a
//
//    struct i2c_rdwr_ioctl_data {
//        struct i2c_msg *msgs;  /* ptr to array of simple messages */
//        int nmsgs;             /* number of messages to exchange */
//    }
//
//    The msgs[] themselves contain further pointers into data buffers.
//    The function will write or read data to or from that buffers depending
//    on whether the I2C_M_RD flag is set in a particular message or not.
//    The slave address and whether to use ten bit address mode has to be
//    set in each message, overriding the values set with the above ioctl's.
//
//  ioctl(file, I2C_SMBUS, struct i2c_smbus_ioctl_data *args)
//    Not meant to be called  directly; instead, use the access functions
//    below.
//
//  You can do plain i2c transactions by using read(2) and write(2) calls.
//  You do not need to pass the address byte; instead, set it through
//  ioctl I2C_SLAVE before you try to access the device.
//
//  You can do SMBus level transactions (see documentation file smbus-protocol
//  for details) through the following functions:
//    __s32 i2c_smbus_write_quick(int file, __u8 value);
//    __s32 i2c_smbus_read_byte(int file);
//    __s32 i2c_smbus_write_byte(int file, __u8 value);
//    __s32 i2c_smbus_read_byte_data(int file, __u8 command);
//    __s32 i2c_smbus_write_byte_data(int file, __u8 command, __u8 value);
//    __s32 i2c_smbus_read_word_data(int file, __u8 command);
//    __s32 i2c_smbus_write_word_data(int file, __u8 command, __u16 value);
//    __s32 i2c_smbus_process_call(int file, __u8 command, __u16 value);
//    __s32 i2c_smbus_read_block_data(int file, __u8 command, __u8 *values);
//    __s32 i2c_smbus_write_block_data(int file, __u8 command, __u8 length,
//                                     __u8 *values);
//  All these transactions return -1 on failure; you can read errno to see
//  what happened. The 'write' transactions return 0 on success; the
//  'read' transactions return the read value, except for read_block, which
//  returns the number of values read. The block buffers need not be longer
//  than 32 bytes.
//
//  The above functions are all inline functions, that resolve to calls to
//  the i2c_smbus_access function, that on its turn calls a specific ioctl
//  with the data in a specific format. Read the source code if you
//  want to know what happens behind the screens.
//
//
//  Implementation details
//  ======================
//
//  For the interested, here's the code flow which happens inside the kernel
//  when you use the /dev interface to I2C:
//
//  1* Your program opens /dev/i2c-N and calls ioctl() on it, as described in
//  section "C example" above.
//
//  2* These open() and ioctl() calls are handled by the i2c-dev kernel
//  driver: see i2c-dev.c:i2cdev_open() and i2c-dev.c:i2cdev_ioctl(),
//  respectively. You can think of i2c-dev as a generic I2C chip driver
//  that can be programmed from user-space.
//
//  3* Some ioctl() calls are for administrative tasks and are handled by
//  i2c-dev directly. Examples include I2C_SLAVE (set the address of the
//  device you want to access) and I2C_PEC (enable or disable SMBus error
//  checking on future transactions.)
//
//  4* Other ioctl() calls are converted to in-kernel function calls by
//  i2c-dev. Examples include I2C_FUNCS, which queries the I2C adapter
//  functionality using i2c.h:i2c_get_functionality(), and I2C_SMBUS, which
//  performs an SMBus transaction using i2c-core.c:i2c_smbus_xfer().
//
//  The i2c-dev driver is responsible for checking all the parameters that
//  come from user-space for validity. After this point, there is no
//  difference between these calls that came from user-space through i2c-dev
//  and calls that would have been performed by kernel I2C chip drivers
//  directly. This means that I2C bus drivers don't need to implement
//  anything special to support access from user-space.
//
//  5* These i2c-core.c/i2c.h functions are wrappers to the actual
//  implementation of your I2C bus driver. Each adapter must declare
//  callback functions implementing these standard calls.
//  i2c.h:i2c_get_functionality() calls i2c_adapter.algo->functionality(),
//  while i2c-core.c:i2c_smbus_xfer() calls either
//  adapter.algo->smbus_xfer() if it is implemented, or if not,
//  i2c-core.c:i2c_smbus_xfer_emulated() which in turn calls
//  i2c_adapter.algo->master_xfer().
//
//  After your I2C bus driver has processed these requests, execution runs
//  up the call chain, with almost no processing done, except by i2c-dev to
//  package the returned data, if any, in suitable format for the ioctl.
