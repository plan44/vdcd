//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__vzughomedevice__
#define __vdcd__vzughomedevice__

#include "device.hpp"

#if ENABLE_VZUGHOME

#include "vzughomedevicecontainer.hpp"

#include "binaryinputbehaviour.hpp"
#include "sensorbehaviour.hpp"
#include "buttonbehaviour.hpp"
#include "simplescene.hpp"

using namespace std;

namespace p44 {


  #define STATUS_BINARY_INPUTS 0
  #define NUM_MESSAGE_BINARY_INPUTS 6

  #define STATUS_BUTTON_CLICKS 1


  typedef map<string, string> StringStringMap;

  class VZugHomeDeviceContainer;


  class VZugHomeDeviceSettings : public CmdSceneDeviceSettings
  {
    typedef CmdSceneDeviceSettings inherited;

  public:

    VZugHomeDeviceSettings(Device &aDevice) : inherited(aDevice) {};

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);


  };
  typedef boost::intrusive_ptr<VZugHomeDeviceSettings> VZugHomeDeviceSettingsPtr;


  /// A concrete class implementing the Scene object for a audio device, having a volume channel plus a index value (for specific song/sound effects)
  /// @note subclasses can implement more parameters
  class VZugHomeScene : public SimpleCmdScene
  {
    typedef SimpleCmdScene inherited;

  public:

    VZugHomeScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) : inherited(aSceneDeviceSettings, aSceneNo) {};

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

  };
  typedef boost::intrusive_ptr<VZugHomeScene> VZugHomeScenePtr;



  class VZugHomeDevice : public Device
  {
    typedef Device inherited;
    friend class VZugHomeDeviceContainer;

    VZugHomeComm vzugHomeComm;
    string modelId;
    string modelDesc;
    string serialNo;

    enum {
      model_unknown,
      model_MSLQ
    } deviceModel;

    uint64_t mostRecentPush; ///< "time" of most recent push we've already reported

    bool currentIsActive;
    string lastPushMessage;
    string currentStatus;
    string currentProgram;
    int programTemp;

    ButtonBehaviourPtr actionButton; ///< the pseudo-button required to send direct scene calls or button clicks

    SensorBehaviourPtr ovenTemp;
    SensorBehaviourPtr foodTemp;
    SensorBehaviourPtr remainingMinutes;

    StringStringMap pushMessageTriggers; ///< texts from push messages and corresponding direct actions

  public:

    VZugHomeDevice(VZugHomeDeviceContainer *aClassContainerP, const string aBaseURL);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "vzughome"; };

    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// prepare for calling a scene on the device level
    /// @param aScene the scene that is to be called
    /// @return true if scene preparation is ok and call can continue. If false, no further action will be taken
    /// @note this is called BEFORE scene values are recalled
    virtual bool prepareSceneCall(DsScenePtr aScene);

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @param aCompletedCB if not NULL, must be called when values are applied
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

    /// @}

    VZugHomeDeviceContainer &getVZugHomeDeviceContainer();

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// Get extra info (plan44 specific) to describe the addressable in more detail
    /// @return string, single line extra info describing aspects of the device not visible elsewhere
    virtual string getExtraInfo();

    /// disconnect device. For static device, this means removing the config from the container's DB. Note that command line
    /// static devices cannot be disconnected.
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// @name identification of the addressable entity
    /// @{

    /// @return model GUID in URN format to identify model of the connected hardware device as uniquely as possible
    virtual string hardwareModelGUID();

    /// @return human readable model name/short description
    virtual string modelName();

    /// @return Vendor name if known
    virtual string vendorName();

    /// @}

  protected:

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void queryDeviceInfos(StatusCB aCompletedCB);

    /// this will be called just before a device is added to the vdc, and thus needs to be fully constructed
    /// (settings, scenes, behaviours) and MUST have determined the henceforth invariable dSUID.
    /// After having received this call, the device must also be ready to load persistent settings.
    virtual void willBeAdded();

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(StatusCB aCompletedCB, bool aFactoryReset);

    /// load parameters from persistent DB
    /// @note implemented here to load pushMessageTriggers
    virtual ErrorPtr load();

  private:

    void gotModelId(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError);
    void gotModelDescription(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError);
    void gotSerialNumber(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError);
    void gotDeviceName(StatusCB aCompletedCB, JsonObjectPtr aResult, ErrorPtr aError);
    void getDeviceState();
    void gotCurrentStatus(JsonObjectPtr aResult, ErrorPtr aError);
    void gotCurrentProgram(JsonObjectPtr aResult, ErrorPtr aError);
    void gotCurrentProgramEnd(JsonObjectPtr aResult, ErrorPtr aError);
    void gotIsActive(JsonObjectPtr aResult, ErrorPtr aError);
    void gotLastPUSHNotifications(JsonObjectPtr aResult, ErrorPtr aError);
    void scheduleNextStatePoll(ErrorPtr aError);
    void sentTurnOff(SimpleCB aDoneCB, bool aForDimming, ErrorPtr aError);
    void sceneCmdSent(JsonObjectPtr aResult, ErrorPtr aError);

    void processPushMessage(const string aMessage);

    void deriveDsUid();

  };
  typedef boost::intrusive_ptr<VZugHomeDevice> VZugHomeDevicePtr;


} // namespace p44

#endif // ENABLE_VZUGHOME

#endif /* defined(__vdcd__vzughomedevice__) */
