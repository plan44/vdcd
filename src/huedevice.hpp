//
//  huedevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__huedevice__
#define __vdcd__huedevice__

#include "device.hpp"

using namespace std;

namespace p44 {

  class HueDeviceContainer;
  class HueDevice;
  typedef boost::intrusive_ptr<HueDevice> HueDevicePtr;
  class HueDevice : public Device
  {
    typedef Device inherited;

    int lampNumber;

  public:
    HueDevice(HueDeviceContainer *aClassContainerP, int aLampNumber);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   output behaviours to collect data for an outgoing message.
    virtual void updateOutputValue(OutputBehaviour &aOutputBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "hue color light device"; }

    /// @}

  protected:

    void deriveDSID();

  };
  
} // namespace p44


#endif /* defined(__vdcd__huedevice__) */
