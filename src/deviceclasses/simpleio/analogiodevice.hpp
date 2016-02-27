//
//  Copyright (c) 2014-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__analogiodevice__
#define __vdcd__analogiodevice__

#include "device.hpp"

#if ENABLE_STATIC

#include "analogio.hpp"
#include "staticdevicecontainer.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class AnalogIODevice;
  typedef boost::intrusive_ptr<AnalogIODevice> AnalogIODevicePtr;

  class AnalogIODevice : public StaticDevice
  {
    typedef StaticDevice inherited;

    typedef enum {
      analogio_unknown,
      analogio_dimmer,
      analogio_rgbdimmer,
      analogio_valve
    } AnalogIoType;

    AnalogIoPtr analogIO; // brighness for single channel, red for RGB
    AnalogIoPtr analogIO2; // green for RGB
    AnalogIoPtr analogIO3; // blue for RGB
    AnalogIoPtr analogIO4; // white for RGBW

    AnalogIoType analogIOType;

    long transitionTicket;

  public:
    AnalogIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "analogio"; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// Get extra info (plan44 specific) to describe the addressable in more detail
    /// @return string, single line extra info describing aspects of the device not visible elsewhere
    virtual string getExtraInfo();

    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @param aCompletedCB if not NULL, must be called when values are applied
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

    virtual void applyChannelValueSteps(bool aForDimming, double aStepSize);

  };

} // namespace p44

#endif // ENABLE_STATIC
#endif // __vdcd__analogiodevice__
