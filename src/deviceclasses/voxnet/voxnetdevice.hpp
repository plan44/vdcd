//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__voxnetdevice__
#define __vdcd__voxnetdevice__

#include "device.hpp"

#if ENABLE_VOXNET

#include "audiobehaviour.hpp"
#include "voxnetdevicecontainer.hpp"

using namespace std;

namespace p44 {

  class VoxnetDeviceContainer;
  class VoxnetDevice;


  class VoxnetDeviceSettings : public AudioDeviceSettings
  {
    typedef AudioDeviceSettings inherited;
    friend class VoxnetDevice;

    string messageSourceID; ///< ID/alias of the source that provides messages
    string messageStream; ///< stream in the message source that provides messages
    int messageTitleNo; ///< the title number to play for messages, 0 if none
    string messageShellCommand; ///< the shell command to execute to start message playing, empty if none
    int messageDuration; ///< duration of message in seconds (0 if actual length is reported back by shell command)

  protected:

    VoxnetDeviceSettings(Device &aDevice);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  };
  typedef boost::intrusive_ptr<VoxnetDeviceSettings> VoxnetDeviceSettingsPtr;



  class VoxnetDevice : public Device
  {
    typedef Device inherited;
    friend class VoxnetDeviceContainer;

    string voxnetRoomID;

    string currentUser; ///< alias or ID of current user
    string currentSource; ///< alias or ID of current source
    string currentStream; ///< name of the source's substream
    bool knownMuted; ///< set if we know output is currently muted

    double preMessageVolume; ///< volume that was present when last message started playing, will be restored at end of message
    string preMessageSource; ///< alias or ID of source before message started playing
    string preMessageStream; ///< name of the source's substream before message started playing
    bool preMessagePower; ///< power state before message started playing

    long messageTimerTicket; ///< set while message is playing

    VoxnetDeviceSettingsPtr voxnetSettings() { return boost::dynamic_pointer_cast<VoxnetDeviceSettings>(deviceSettings); };

  public:

    VoxnetDevice(VoxnetDeviceContainer *aClassContainerP, const string aVoxnetRoomID);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "voxnet"; };

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

    /// process voxnet status
    void processVoxnetStatus(const string aVoxnetID, const string aVoxnetStatus);

    /// @}

    VoxnetDeviceContainer &getVoxnetDeviceContainer();

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

    /// @return human readable model name/short description
    virtual string modelName();

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID();

    /// @return model GUID in URN format to identify model of the connected hardware device as uniquely as possible
    virtual string hardwareModelGUID();

    /// @return Vendor name if known
    virtual string vendorName();

    /// @}

  protected:

    void deriveDsUid();

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

  private:

    void playMessage(AudioScenePtr aAudioScene);
    void playingStarted(const string &aPlayCommandOutput);
    void endOfMessage();

  };
  typedef boost::intrusive_ptr<VoxnetDevice> VoxnetDevicePtr;


} // namespace p44

#endif // ENABLE_VOXNET

#endif /* defined(__vdcd__voxnetdevice__) */
