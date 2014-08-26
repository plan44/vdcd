//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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
    
    bool hasButton;
    bool hasOutput;
    bool hasColor;
    bool isValve;
    ConsoleKeyPtr consoleKey;

  public:
    ConsoleDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
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
    virtual void applyChannelValues(DoneCB aDoneCB, bool aForDimming);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "plan44 console-based debug device"; }

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    virtual string vendorId() { return "vendorname:plan44.ch"; };

    /// @}

  protected:

    void deriveDsUid();
		
	private:

    void buttonHandler(bool aState, MLMicroSeconds aTimeStamp);
    void binaryInputHandler(bool aState, MLMicroSeconds aTimeStamp);
    void sensorValueHandler(double aValue, MLMicroSeconds aTimeStamp);
    void sensorJitter(bool aState, MLMicroSeconds aTimeStamp);

  };

} // namespace p44

#endif /* defined(__vdcd__consoledevice__) */
