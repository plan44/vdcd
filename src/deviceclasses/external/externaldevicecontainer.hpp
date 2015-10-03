//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__externaldevicecontainer__
#define __vdcd__externaldevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"
#include "device.hpp"
#include "jsoncomm.hpp"

#include "buttonbehaviour.hpp"

using namespace std;

namespace p44 {

  class ExternalDeviceContainer;
  class ExternalDevice;


  typedef boost::intrusive_ptr<ExternalDevice> ExternalDevicePtr;
  class ExternalDevice : public Device
  {
    typedef Device inherited;
    friend class ExternalDeviceContainer;

    JsonCommPtr deviceConnection;
    bool configured; ///< set when device is configured (init message received and device added to vdc)
    bool simpletext; ///< if set, device communication uses very simple text messages rather than JSON
    bool useMovement; ///< if set, device communication uses MV/move command for dimming and shadow device operation
    bool querySync; ///< if set, device is asked for synchronizing actual values of channels when needed (e.g. before saveScene)

    SimpleCB syncedCB; ///< will be called when device confirms "SYNC" message with "SYNCED" response

  public:

    ExternalDevice(DeviceClassContainer *aClassContainerP, JsonCommPtr aDeviceConnection);
    virtual ~ExternalDevice();

    ExternalDeviceContainer &getExternalDeviceContainer();

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "external"; };

    /// @return human readable model name/short description
    virtual string modelName() { return "external device"; }

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// apply all pending channel value updates to the device's hardware
    /// @param aDoneCB will called when values are actually applied, or hardware reports an error/timeout
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    ///   In addition, if the device output hardware has distinct disabled/enabled states, output->isEnabled() must be checked and applied.
    /// @note the implementation must capture the channel values to be applied before returning from this method call,
    ///   because channel values might change again before a delayed apply mechanism calls aDoneCB.
    /// @note this method will NOT be called again until aCompletedCB is called, even if that takes a long time.
    ///   Device::requestApplyingChannels() provides an implementation that serializes calls to applyChannelValues and syncChannelValues
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

    /// synchronize channel values by reading them back from the device's hardware (if possible)
    /// @param aDoneCB will be called when values are updated with actual hardware values
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    /// @note implementation must use channel's syncChannelValue() method
    virtual void syncChannelValues(SimpleCB aDoneCB);

    /// start or stop dimming channel of this device. Usually implemented in device specific manner in subclasses.
    /// @param aChannel the channelType to start or stop dimming for
    /// @param aDimMode according to DsDimMode: 1=start dimming up, -1=start dimming down, 0=stop dimming
    /// @note unlike the vDC API "dimChannel" command, which must be repeated for dimming operations >5sec, this
    ///   method MUST NOT terminate dimming automatically except when reaching the minimum or maximum level
    ///   available for the device. Dimming timeouts are implemented at the device level and cause calling
    ///   dimChannel() with aDimMode=0 when timeout happens.
    /// @note this method can rely on a clean start-stop sequence in all cases, which means it will be called once to
    ///   start a dimming process, and once again to stop it. There are no repeated start commands or missing stops - Device
    ///   class makes sure these cases (which may occur at the vDC API level) are not passed on to dimChannel()
    virtual void dimChannel(DsChannelType aChannelType, DsDimMode aDimMode);


  private:

    void closeConnection();

    void handleDeviceConnectionStatus(ErrorPtr aError);
    void handleDeviceApiJsonMessage(ErrorPtr aError, JsonObjectPtr aMessage);
    void handleDeviceApiSimpleMessage(ErrorPtr aError, string aMessage);
    void sendDeviceApiJsonMessage(JsonObjectPtr aMessage);
    void sendDeviceApiSimpleMessage(string aMessage);
    void sendDeviceApiStatusMessage(ErrorPtr aError);

    ErrorPtr processJsonMessage(string aMessageType, JsonObjectPtr aMessage);
    ErrorPtr processSimpleMessage(string aMessageType, string aValue);
    ErrorPtr configureDevice(JsonObjectPtr aInitParams);
    ErrorPtr processInputJson(char aInputType, JsonObjectPtr aParams);

    ErrorPtr processInput(char aInputType, uint32_t aIndex, double aValue);

    void changeChannelMovement(size_t aChannelIndex, SimpleCB aDoneCB, int aNewDirection);
    void releaseButton(ButtonBehaviourPtr aButtonBehaviour);

  };



  typedef boost::intrusive_ptr<ExternalDeviceContainer> ExternalDeviceContainerPtr;
  class ExternalDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
    friend class ExternalDevice;

    SocketCommPtr externalDeviceApiServer;

  public:
    ExternalDeviceContainer(int aInstanceNumber, const string &aSocketPathOrPort, bool aNonLocal, DeviceContainer *aDeviceContainerP, int aTag);

    void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    virtual const char *deviceClassIdentifier() const;

    /// collect devices from this device class
    /// @param aCompletedCB will be called when device scan for this device class has been completed
    virtual void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    /// @param aForget if set, all parameters stored for the device (if any) will be deleted. Note however that
    ///   the devices are not disconnected (=unlearned) by this.
    virtual void removeDevices(bool aForget);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "external"; }

    /// External device container should not be announced when it has no devices
    /// @return if true, this device class should not be announced towards the dS system when it has no devices
    virtual bool invisibleWhenEmpty() { return true; }

    /// get supported rescan modes for this device class. This indicates (usually to a web-UI) which
    /// of the flags to collectDevices() make sense for this device class.
    /// @return a combination of rescanmode_xxx bits
    virtual int getRescanModes() const { return rescanmode_exhaustive; }; // only exhaustive makes sense

  private:

    SocketCommPtr deviceApiConnectionHandler(SocketCommPtr aServerSocketCommP);

  };

} // namespace p44


#endif /* defined(__vdcd__externaldevicecontainer__) */
