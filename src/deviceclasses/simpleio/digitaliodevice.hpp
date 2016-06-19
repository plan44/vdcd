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

#ifndef __vdcd__digitaliodevice__
#define __vdcd__digitaliodevice__

#include "device.hpp"

#if ENABLE_STATIC

#include "digitalio.hpp"
#include "staticdevicecontainer.hpp"

using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class DigitalIODevice;
  typedef boost::intrusive_ptr<DigitalIODevice> DigitalIODevicePtr;
  class DigitalIODevice : public StaticDevice
  {
    typedef StaticDevice inherited;

    typedef enum {
      digitalio_unknown,
      digitalio_button, // button input
      digitalio_input, // binary input
      digitalio_light, // light output
      digitalio_relay, // general purpose relay output
      digitalio_blind, // blind output
    } DigitalIoType;


		ButtonInputPtr buttonInput;
    DigitalIoPtr digitalInput;
    IndicatorOutputPtr indicatorOutput;
    DigitalIoPtr blindsOutputUp;
    DigitalIoPtr blindsOutputDown;

    DigitalIoType digitalIoType;
    
    int movingDirection; ///< currently moving direction 0=stopped, -1=moving down, +1=moving up
    long commandTicket;
    int nextDirection; ///< currently moving direction 0=stopped, -1=moving down, +1=moving up
    SimpleCB moveCB;

  public:
    DigitalIODevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);
    
    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "digitalio"; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// Get extra info (plan44 specific) to describe the addressable in more detail
    /// @return string, single line extra info describing aspects of the device not visible elsewhere
    virtual string getExtraInfo();
      
    virtual void dimChannel(DsChannelType aChannelType, DsDimMode aDimMode);

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
      
    /// synchronize channel values by reading them back from the device's hardware (if possible)
    /// @param aDoneCB will be called when values are updated with actual hardware values
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    /// @note implementation must use channel's syncChannelValue() method
    virtual void syncChannelValues(SimpleCB aDoneCB);
    
    void changeMovement(SimpleCB aDoneCB, int aNewDirection);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @}

  protected:

    void deriveDsUid();
		
	private:

    void buttonHandler(bool aNewState, MLMicroSeconds aTimestamp);
    void inputHandler(bool aNewState);
    string blindsName() const;
    void delayedBlindAction();
    void applyBlind(int aNewDirection);

  };

} // namespace p44

#endif // ENABLE_STATIC
#endif // __vdcd__digitaliodevice__
