//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __vdcd__consoledevice__
#define __vdcd__consoledevice__

#include "device.hpp"

#if ENABLE_STATIC

#include "consolekey.hpp"
#include "staticdevicecontainer.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class ConsoleDevice;
  typedef boost::intrusive_ptr<ConsoleDevice> ConsoleDevicePtr;
  class ConsoleDevice : public StaticDevice
  {
    typedef StaticDevice inherited;
    
    typedef enum {
      consoleio_unknown,
      consoleio_button,
      consoleio_rocker,
      consoleio_input,
      consoleio_sensor,
      consoleio_dimmer,
      consoleio_colordimmer,
      consoleio_valve,
    } ConsoleIoType;

    ConsoleIoType consoleIoType;
    ConsoleKeyPtr consoleKey1;
    ConsoleKeyPtr consoleKey2;
    string consoleName;

  public:
    ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "console"; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @param aDoneCB if not NULL, must be called when values are applied
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @}

  protected:

    void deriveDsUid();
		
	private:

    void buttonHandler(int aButtonIndex, bool aState, MLMicroSeconds aTimestamp);
    void sensorHandler(int aButtonIndex, bool aState, MLMicroSeconds aTimestamp);
    void binaryInputHandler(bool aState, MLMicroSeconds aTimeStamp);
    void sensorValueHandler(double aValue, MLMicroSeconds aTimeStamp);
    void sensorJitter(bool aState, MLMicroSeconds aTimeStamp);

  };

} // namespace p44

#endif // ENABLE_STATIC
#endif // __vdcd__consoledevice__
