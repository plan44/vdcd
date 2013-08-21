//
//  GpioDevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__gpiodevice__
#define __vdcd__gpiodevice__

#include "device.hpp"

#include "digitalio.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class GpioDevice;
  typedef boost::shared_ptr<GpioDevice> GpioDevicePtr;
  class GpioDevice : public Device
  {
    typedef Device inherited;
		ButtonInputPtr buttonInput;
    IndicatorOutputPtr indicatorOutput;
    bool isOutput;
    bool inverted;

  public:
    GpioDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// get currently set output value from device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    virtual int16_t getOutputValue(int aChannel);

    /// set new output value on device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int aChannel, int16_t aValue, MLMicroSeconds aTransitionTime=0);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "plan44 GPIO/I2C digital I/O based device"; }

    /// @}

  protected:

    void deriveDSID();
		
	private:

    void buttonHandler(bool aNewState, MLMicroSeconds aTimestamp);
		
  };

} // namespace p44

#endif /* defined(__vdcd__gpiodevice__) */
