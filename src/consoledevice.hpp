//
//  ConsoleDevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__consoledevice__
#define __vdcd__consoledevice__

#include "device.hpp"

#include "consolekey.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class ConsoleDevice;
  typedef boost::intrusive_ptr<ConsoleDevice> ConsoleDevicePtr;
  class ConsoleDevice : public Device
  {
    typedef Device inherited;
    bool hasButton;
    bool hasOutput;
    string name;
    ConsoleKeyPtr consoleKey;
    int32_t outputValue;

  public:
    ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
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
    virtual string modelName() { return "plan44 console-based debug device"; }

    /// @}

  protected:

    void deriveDSID();
		
	private:

    void buttonHandler(bool aState, MLMicroSeconds aTimeStamp);
		
  };

} // namespace p44

#endif /* defined(__vdcd__consoledevice__) */
