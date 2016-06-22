//
//  Copyright (c) 2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


#ifndef __vdcd__mystromdevice__
#define __vdcd__mystromdevice__

#include "device.hpp"

#if ENABLE_STATIC

#include "jsonwebclient.hpp"
#include "colorlightbehaviour.hpp"
#include "staticdevicecontainer.hpp"


using namespace std;

namespace p44 {


  class MyStromDevice : public StaticDevice
  {
    typedef StaticDevice inherited;

    string deviceHostName;
    string deviceToken;
    JsonWebClient myStromComm;

  public:
    MyStromDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "spark_io"; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts (usually just after collecting devices)
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(StatusCB aCompletedCB, bool aFactoryReset);

    /// check presence of this addressable
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @param aDoneCB if not NULL, must be called when values are applied
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

    /// synchronize channel values by reading them back from the device's hardware (if possible)
    /// @param aDoneCB will be called when values are updated with actual hardware values
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    /// @note implementation must use channel's syncChannelValue() method
    virtual void syncChannelValues(SimpleCB aDoneCB);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "myStrom switch"; }

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    virtual string vendorId() { return "vendorname:mystrom.ch"; };

    /// @}

  protected:

    void deriveDsUid();

  private:

    bool myStromApiQuery(JsonWebClientCB aResponseCB, string aPathAndArgs);
    bool myStromApiAction(HttpCommCB aResponseCB, string aPathAndArgs);

    void initialStateReceived(StatusCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void presenceStateReceived(PresenceCB aPresenceResultHandler, JsonObjectPtr aDeviceInfo, ErrorPtr aError);

    void channelValuesSent(SimpleCB aDoneCB, string aResponse, ErrorPtr aError);
    void channelValuesReceived(SimpleCB aDoneCB, JsonObjectPtr aJsonResponse, ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<MyStromDevice> MyStromDevicePtr;


} // namespace p44


#endif // ENABLE_STATIC
#endif // __vdcd__mystromdevice__
