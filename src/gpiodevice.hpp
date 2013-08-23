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
  typedef boost::intrusive_ptr<GpioDevice> GpioDevicePtr;
  class GpioDevice : public Device
  {
    typedef Device inherited;
		ButtonInputPtr buttonInput;
    IndicatorOutputPtr indicatorOutput;

  public:
    GpioDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// get currently set output value from device
    /// @param aOutputBehaviour the output behaviour which wants to know the output value as set in the hardware
    virtual int16_t getOutputValue(OutputBehaviour &aOutputBehaviour);

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which wants to set the hardware output value
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(OutputBehaviour &aOutputBehaviour, int16_t aValue, MLMicroSeconds aTransitionTime=0);

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
