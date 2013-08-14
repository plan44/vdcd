//
//  i2c.h
//  p44utils
//
//  Created by Lukas Zeller on 10.07.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__i2c__
#define __p44utils__i2c__

#include "p44_common.hpp"

#include "iopin.hpp"

using namespace std;

namespace p44 {

  class I2CManager;
  class I2CBus;

  class I2CDevice
  {
    friend class I2CBus;
  protected:
    I2CBus *i2cbus;
    uint8_t deviceAddress;

  public:

    /// @return fully qualified device identifier (deviceType@hexaddress)
    string deviceID();

    /// @return device type identifier
    virtual const char *deviceType() { return "generic"; };

    /// @return true if this device or one of it's ancestors is of the given type
    virtual bool isKindOf(const char *aDeviceType);

    /// create device
    /// @param aDeviceAddress slave address of the device
    /// @param aBusP I2CBus object
    I2CDevice(uint8_t aDeviceAddress, I2CBus *aBusP);

  };
  typedef boost::shared_ptr<I2CDevice> I2CDevicePtr;

  typedef std::map<string, I2CDevicePtr> I2CDeviceMap;



  class I2CBitPortDevice : public I2CDevice
  {
    typedef I2CDevice inherited;


  protected:
    uint32_t outputEnableMask; ///< bit set = pin is output
    uint32_t pinStateMask; ///< state of pins 0..31(max)
    uint32_t outputStateMask; ///< state of outputs 0..31(max)

    virtual void updateInputState(int aForBitNo) = 0;
    virtual void updateOutputs(int aForBitNo) = 0;
    virtual void updateDirection(int aForBitNo) = 0;

  public:
    /// create device
    /// @param aDeviceAddress slave address of the device
    /// @param aBusP I2CBus object
    I2CBitPortDevice(uint8_t aDeviceAddress, I2CBus *aBusP);

    /// @return device type identifier
    virtual const char *deviceType() { return "BitPort"; };

    /// @return true if this device or one of it's ancestors is of the given type
    virtual bool isKindOf(const char *aDeviceType);

    bool getBitState(int aBitNo);
    void setBitState(int aBitNo, bool aState);
    void setAsOutput(int aBitNo, bool aOutput, bool aInitialState);

  };
  typedef boost::shared_ptr<I2CBitPortDevice> I2CBitPortDevicePtr;



  class TCA9555 : public I2CBitPortDevice
  {
    typedef I2CBitPortDevice inherited;

  protected:

    virtual void updateInputState(int aForBitNo);
    virtual void updateOutputs(int aForBitNo);
    virtual void updateDirection(int aForBitNo);


  public:

    /// create device
    /// @param aDeviceAddress slave address of the device
    /// @param aBusP I2CBus object
    TCA9555(uint8_t aDeviceAddress, I2CBus *aBusP);

    /// @return device type identifier
    virtual const char *deviceType() { return "TCA9555"; };

    /// @return true if this device or one of it's ancestors is of the given type
    virtual bool isKindOf(const char *aDeviceType);

  };




  class I2CBus
  {
    friend class I2CManager;

    int busNumber;
    I2CDeviceMap deviceMap;

    int busFD;
    int lastDeviceAddress;

  protected:
    /// create i2c bus
    /// @param aBusNumber i2c bus number in the system
    I2CBus(int aBusNumber);

    /// register new I2CDevice
    /// @param the device to register
    void registerDevice(I2CDevicePtr aDevice);

    /// get device registered for address
    /// @param aDeviceID the device ID string in fully qualified "devicetype@2digithexaddr" form
    /// @return the device registered for this type/address or empty pointer if none registered
    I2CDevicePtr getDevice(const char *aDeviceID);

  public:

    virtual ~I2CBus();

    /// read byte/word
    /// @param aDevice device to access
    /// @param aRegister register/command to access
    /// @param aByte/aWord will receive result
    /// @return true if successful
    bool readByte(I2CDevice *aDeviceP, uint8_t aRegister, uint8_t &aByte);
    bool readWord(I2CDevice *aDeviceP, uint8_t aRegister, uint16_t &aWord);

    /// write byte/word
    /// @param aDevice device to access
    /// @param aRegister register/command to access
    /// @param aByte/aWord to write
    /// @return true if successful
    bool writeByte(I2CDevice *aDeviceP, uint8_t aRegister, uint8_t aByte);
    bool writeWord(I2CDevice *aDeviceP, uint8_t aRegister, uint16_t aWord);

  private:
    bool accessDevice(I2CDevice *aDeviceP);
    bool accessBus();
    void closeBus();

  };
  typedef boost::shared_ptr<I2CBus> I2CBusPtr;



  typedef std::map<int, I2CBusPtr> I2CBusMap;

  class I2CManager
  {
    I2CBusMap busMap;

    I2CManager();
  public:
    virtual ~I2CManager();

    /// get shared instance of manager
    static I2CManager *sharedManager();

    /// get device
    /// @param aBusNumber the i2c bus number in the system to use
    /// @param aDeviceID the device name identifying address and type of device
    ///   like "tca9555@25" meaning TCA9555 chip based IO at HEX!! bus address 25
    /// @return a device of proper type or empty pointer if none could be found
    I2CDevicePtr getDevice(int aBusNumber, const char *aDeviceID);

  };



  /// abstract wrapper class for digital I/O pin
  class I2CPin : public IOPin
  {
    I2CBitPortDevicePtr bitPortDevice;
    int pinNumber;
    bool output;
    bool lastSetState;

  public:

    /// create i2c based digital input or output pin
    I2CPin(int aBusNumber, const char *aDeviceId, int aPinNumber, bool aOutput, bool aInitialState);

    /// get state of pin
    /// @return current state (from actual GPIO pin for inputs, from last set state for outputs)
    virtual bool getState();

    /// set state of pin (NOP for inputs)
    /// @param aState new state to set output to
    virtual void setState(bool aState);
  };  
  
} // namespace

#endif /* defined(__p44utils__i2c__) */
