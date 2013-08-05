//
//  ConsoleDevice.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__consoledevice__
#define __p44bridged__consoledevice__

#include "device.hpp"

#include "consolekey.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class ConsoleDevice;
  typedef boost::shared_ptr<ConsoleDevice> ConsoleDevicePtr;
  class ConsoleDevice : public Device
  {
    typedef Device inherited;
    bool hasButton;
    bool hasOutput;
    string name;
    ConsoleKeyPtr consoleKey;
    uint16_t outputValue;

  public:
    ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
		/// single or no input
		virtual int getNumButtons() { return hasButton ? 1 : 0; }

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



  protected:

    void deriveDSID();
		
	private:

    void buttonHandler(bool aState, MLMicroSeconds aTimeStamp);
		
  };

} // namespace p44

#endif /* defined(__p44bridged__consoledevice__) */
