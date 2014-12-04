//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


#ifndef __vdcd__roombadevice__
#define __vdcd__roombadevice__

#include "device.hpp"

#include "jsonwebclient.hpp"
#include "outputbehaviour.hpp"
#include "staticdevicecontainer.hpp"


using namespace std;

namespace p44 {

  class StaticDeviceContainer;
  class SparkIoDevice;


  typedef boost::intrusive_ptr<SparkIoDevice> SparkIoDevicePtr;
  class RoombaDevice : public StaticDevice
  {
    typedef StaticDevice inherited;

    string roombaIPAddress;
    JsonWebClient roombaJSON;

  public:
    RoombaDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "roomba"; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts (usually just after collecting devices)
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset);

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
    virtual void applyChannelValues(DoneCB aDoneCB, bool aForDimming);

    /// synchronize channel values by reading them back from the device's hardware (if possible)
    /// @param aDoneCB will be called when values are updated with actual hardware values
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    /// @note implementation must use channel's syncChannelValue() method
    virtual void syncChannelValues(DoneCB aDoneCB);

    /// call scene on this device
    /// @param aSceneNo the scene to call.
    virtual void callScene(SceneNo aSceneNo, bool aForce);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// @return human readable model name/short description
    virtual string modelName() { return "Roomba vacuum cleaner"; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID() { return string_format("roomba:%s", roombaIPAddress.c_str()); }

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    virtual string vendorId() { return "vendorname:roomba"; };

    /// @}

  protected:

    void deriveDsUid();

  private:

    void statusReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void runStatusReceived(DoneCB aDoneCB, bool aShouldRun, JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void firstCommandSent(DoneCB aDoneCB, bool aShouldRun, JsonObjectPtr aJsonResponse, ErrorPtr aError);
    void sendFinalCommand(DoneCB aDoneCB, bool aShouldRun);
    void finalCommandSent(DoneCB aDoneCB, bool aShouldRun);
    void startSong(SocketCommPtr sock);
    void songDone(SocketCommPtr sock);

  };
  
} // namespace p44

#endif /* defined(__vdcd__roombadevice__) */
