//
//  GpioDevice.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__gpiodevice__
#define __p44bridged__gpiodevice__

#include "device.hpp"

#include "gpio.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class GpioDevice;
  typedef boost::shared_ptr<GpioDevice> GpioDevicePtr;
  class GpioDevice : public Device
  {
    typedef Device inherited;
		ButtonInputPtr buttonInput;

  public:
    GpioDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
		/// single input
		virtual int getNumInputs() { return 1; }

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    void deriveDSID();
		
	private:
		
    void buttonHandler(bool aNewState, MLMicroSeconds aTimestamp);
		
  };

} // namespace p44

#endif /* defined(__p44bridged__gpiodevice__) */
