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

#ifndef __vdcd__dalidevice__
#define __vdcd__dalidevice__

#include "device.hpp"

#if ENABLE_DALI

#include "dalicomm.hpp"
#include "lightbehaviour.hpp"

using namespace std;

namespace p44 {

  class DaliDeviceContainer;
  class DaliBusDevice;
  class DaliBusDeviceGroup;
  class DaliDevice;
  class DaliDimmerDevice;
  class DaliRGBWDevice;

  typedef boost::intrusive_ptr<DaliBusDevice> DaliBusDevicePtr;
  typedef boost::intrusive_ptr<DaliBusDeviceGroup> DaliBusDeviceGroupPtr;
  typedef boost::intrusive_ptr<DaliDevice> DaliDevicePtr;
  typedef boost::intrusive_ptr<DaliDimmerDevice> DaliDimmerDevicePtr;
  typedef boost::intrusive_ptr<DaliRGBWDevice> DaliRGBWDevicePtr;

  class DaliBusDevice : public P44Obj
  {
    typedef P44Obj inherited;
    friend class DaliBusDeviceGroup;
    friend class DaliDevice;
    friend class DaliRGBWDevice;
    friend class DaliDimmerDevice;
    friend class DaliDeviceContainer;

    DaliDeviceInfo deviceInfo; ///< the device info of the bus device (ballast)

    DsUid dSUID; ///< the dSUID of the bus device (if single device, this will become the dS device's dSUID)

    DaliDeviceContainer &daliDeviceContainer;

    long dimRepeaterTicket; ///< DALI dimming repeater ticket

    /// cached status (call syncStatus() to update these)
    bool isDummy; ///< set if dummy (not found on bus, but known to be part of a composite device)
    bool isPresent; ///< set if present
    bool lampFailure; ///< set if lamp has failure

    /// cached parameters (call syncParams() to update these)
    Brightness currentBrightness; ///< current brightness
    Brightness minBrightness; ///< currently set minimal brightness
    MLMicroSeconds currentTransitionTime; ///< currently set transition time
    uint8_t currentFadeTime; ///< currently set DALI fade time
    double currentDimPerMS; ///< current dim steps per second
    uint8_t currentFadeRate; ///< currently set DALI fade rate

  public:

    DaliBusDevice(DaliDeviceContainer &aDaliDeviceContainer);

    /// use passed device info and derive dSUID from it
    void setDeviceInfo(DaliDeviceInfo aDeviceInfo);

    /// clear all device info except short address and revert to short address derived dSUID
    void clearDeviceInfo();

    /// derive the dSUID from collected device info
    virtual void deriveDsUid();

    /// check if bus device represents a DALI group
    /// @return true if group
    virtual bool isGrouped() { return false; }

  protected:

    /// initialize device for first use
    /// @param aCompletedCB will be called when initialisation is complete
    /// @param aUsedGroupsMask groups that are in use. Single devices should not be in any of these groups
    virtual void initialize(StatusCB aCompletedCB, uint16_t aUsedGroupsMask);

    /// update parameters from device to local vars
    void updateParams(StatusCB aCompletedCB);

    /// update status information from device
    void updateStatus(StatusCB aCompletedCB);


    /// convert dS brightness value to DALI arc power
    /// @param aBrightness 0..100%
    /// @return arcpower 0..254
    uint8_t brightnessToArcpower(Brightness aBrightness);

    /// convert DALI arc power to dS brightness value
    /// @param aArcpower 0..254
    /// @return brightness 0..100%
    Brightness arcpowerToBrightness(int aArcpower);

    /// set transition time for subsequent brightness changes
    /// @param aTransitionTime time for transition
    void setTransitionTime(MLMicroSeconds aTransitionTime);

    /// set transition time for subsequent brightness changes
    /// @param aBrightness new brightness to set
    void setBrightness(Brightness aBrightness);

    /// start or stop optimized DALI dimming
    /// @param aDimMode according to DsDimMode: 1=start dimming up, -1=start dimming down, 0=stop dimming
    /// @param aDimPerMS dim speed in brightness value per millsecond
    void dim(DsDimMode aDimMode, double aDimPerMS);

    /// DALI address to use for querying brightness etc.
    /// @return DALI address
    /// @note this will be overridden in DaliBusDeviceGroup to read info from single master dimmer, not group
    virtual uint8_t addressForQuery() { return deviceInfo.shortAddress; };


    typedef boost::function<void (uint16_t aGroupBitMask, ErrorPtr aError)> DaliGroupsCB;
    /// Utility: Retrieve group membership mask for a given short address
    /// @param aDaliGroupsCB delivers the result
    /// @param aShortAddress which device to query (note: not necessarily myself!)
    void getGroupMemberShip(DaliGroupsCB aDaliGroupsCB, DaliAddress aShortAddress);



  private:

    void queryGroup0to7Response(DaliGroupsCB aDaliGroupsCB, DaliAddress aShortAddress, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);
    void queryGroup8to15Response(DaliGroupsCB aDaliGroupsCB, DaliAddress aShortAddress, uint16_t aGroupBitMask, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);
    void groupMembershipResponse(StatusCB aCompletedCB, uint16_t aUsedGroupsMask, DaliAddress aShortAddress, uint16_t aGroups, ErrorPtr aError);

    void queryActualLevelResponse(StatusCB aCompletedCB, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);
    void queryMinLevelResponse(StatusCB aCompletedCB, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

    void dimRepeater(DaliAddress aDaliAddress, uint8_t aCommand, MLMicroSeconds aCycleStartTime);

    void queryStatusResponse(StatusCB aCompletedCB, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

  };


  class DaliBusDeviceGroup : public DaliBusDevice
  {
    typedef DaliBusDevice inherited;
    friend class DaliDevice;
    friend class DaliRGBWDevice;
    friend class DaliDeviceContainer;

    string mixID; ///< dSUIDs of all members, XOR mixed
    DaliAddress groupMaster; ///< the DALI short address of the master dimmer (i.e. the one that is read from)
    DaliComm::ShortAddressList groupMembers; ///< short addresses of members of the group

  public:

    /// initialize device for first use
    /// @param aCompletedCB will be called when initialisation is complete
    /// @param aUsedGroupsMask groups that are in use. grouped devices should not be in any of these groups except their own group
    virtual void initialize(StatusCB aCompletedCB, uint16_t aUsedGroupsMask);

    /// creates a dimmer group, addressed via a group address rather than single bus address
    /// @note initially, the group is empty. addDaliBusDevice must be used to add devices
    /// @param aGroupNo the group number for this group
    DaliBusDeviceGroup(DaliDeviceContainer &aDaliDeviceContainer, uint8_t aGroupNo);

    /// add a DALI bus device to the group. This will check if the device in question already is configured
    /// correctly for the group, and if not, device will be made member of the group
    /// @param aDaliBusDevice a completely scanned DALI bus device having a valid dSUID
    /// @note the device passed will be checked to be a member of the specified group, and will be reprogrammed if it isn't yet.
    void addDaliBusDevice(DaliBusDevicePtr aDaliBusDevice);

    /// derive the dSUID from mix of dSUIDs of single bus devices
    virtual void deriveDsUid();

    /// check if bus device represents a DALI group
    /// @return true if group
    virtual bool isGrouped() { return true; }

  protected:

    /// DALI address to use for querying brightness etc.
    /// @return DALI address
    /// @note reading info from single master dimmer, not group
    virtual uint8_t addressForQuery() { return groupMaster; };


  private:

    void initNextGroupMember(StatusCB aCompletedCB, DaliComm::ShortAddressList::iterator aNextMember);
    void groupMembershipResponse(StatusCB aCompletedCB, DaliComm::ShortAddressList::iterator aNextMember, uint16_t aGroups, ErrorPtr aError);


  };


  /// types of DALI dS devices
  typedef enum {
    dalidevice_single, ///< single DALI dimmer, single channel
    dalidevice_group, ///< group of DALI dimmers, single channel
    dalidevice_composite ///< multichannel color/tunable white device consisting of multiple dimmers or groups
  } DaliDeviceTypes;


  /// base class for all DALI devices
  class DaliDevice : public Device
  {
    typedef Device inherited;
    friend class DaliDeviceCollector;

  public:

    DaliDevice(DaliDeviceContainer *aClassContainerP);

    /// @return type of DALI device
    virtual DaliDeviceTypes daliTechnicalType() = 0;

    /// get typed container reference
    DaliDeviceContainer &daliDeviceContainer();

    /// device level API methods (p44 specific, JSON only, for configuring grouped devices)
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

  };



  class DaliDimmerDevice : public DaliDevice
  {
    typedef DaliDevice inherited;
    friend class DaliDeviceCollector;

  public:

    DaliBusDevicePtr brightnessDimmer; ///< the actual DALI device controlling brightness

    DaliDimmerDevice(DaliDeviceContainer *aClassContainerP);

    /// @return technical type of DALI device
    virtual DaliDeviceTypes daliTechnicalType() { return brightnessDimmer && brightnessDimmer->isGrouped() ? dalidevice_group : dalidevice_single; }

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return daliTechnicalType()==dalidevice_group ? "dali_group" : "dali_single"; };

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

    /// disconnect device. For DALI, we'll check if the device is still present on the bus, and only if not
    /// we allow disconnection
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @param aCompletedCB if not NULL, must be called when values are applied
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

    /// start or stop dimming (optimized DALI version)
    /// @param aChannel the channelType to start or stop dimming for
    /// @param aDimMode according to DsDimMode: 1=start dimming up, -1=start dimming down, 0=stop dimming
    /// @note this method can rely on a clean start-stop sequence in all cases, which means it will be called once to
    ///   start a dimming process, and once again to stop it. There are no repeated start commands or missing stops - Device
    ///   class makes sure these cases (which may occur at the vDC API level) are not passed on to dimChannel()
    virtual void dimChannel(DsChannelType aChannelType, DsDimMode aDimMode);

    /// @}

    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return daliTechnicalType()==dalidevice_group ? "DALI dimmer group" : "DALI dimmer"; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID();

    /// @return model GUID in URN format to identify hardware model of device as uniquely as possible
    virtual string hardwareModelGUID();

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID();

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

    /// @}

    /// this will be called just before a device is added to the vdc, and thus needs to be fully constructed
    /// (settings, scenes, behaviours) and MUST have determined the henceforth invariable dSUID.
    /// After having received this call, the device must also be ready to load persistent settings.
    virtual void willBeAdded();

    /// derive the dSUID from collected device info
    void deriveDsUid();

  private:

    void brightnessDimmerSynced(StatusCB aCompletedCB, bool aFactoryReset, ErrorPtr aError);
    void checkPresenceResponse(PresenceCB aPresenceResultHandler);
    void disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent);

  };


  class DaliRGBWDevice : public DaliDevice
  {
    typedef DaliDevice inherited;
    friend class DaliDeviceCollector;
    friend class DaliDeviceContainer;

    uint32_t collectionID; ///< the ID of the collection that created this composite device

  public:


    enum {
      dimmer_red,
      dimmer_green,
      dimmer_blue,
      dimmer_white,
      numDimmers
    };
    typedef uint8_t DimmerIndex;

    DaliBusDevicePtr dimmers[numDimmers];

    DaliRGBWDevice(DaliDeviceContainer *aClassContainerP);

    /// @return type of DALI device
    virtual DaliDeviceTypes daliTechnicalType() { return dalidevice_composite; }

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "dali_rgbw"; };

    /// get typed container reference
    DaliDeviceContainer &daliDeviceContainer();

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

    /// add a dimmer
    /// @param aDimmerBusDevice the DALI dimmer to add
    /// @param aDimmerType the type of dimmer (which channel: R,G,B,W)
    /// @return true if dimmer of that type could be added
    bool addDimmer(DaliBusDevicePtr aDimmerBusDevice, string aDimmerType);


    /// check presence of this addressable
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// disconnect device. For DALI, we'll check if the device is still present on the bus, and only if not
    /// we allow disconnection
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

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
    virtual string modelName() { return "DALI composite color light"; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID();

    /// @return model GUID in URN format to identify model of device as uniquely as possible
    virtual string hardwareModelGUID();

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID();

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

    /// @}

    /// this will be called just before a device is added to the vdc, and thus needs to be fully constructed
    /// (settings, scenes, behaviours) and MUST have determined the henceforth invariable dSUID.
    /// After having received this call, the device must also be ready to load persistent settings.
    virtual void willBeAdded();

    /// derive the dSUID from collected device info
    void deriveDsUid();

  private:

    void updateNextDimmer(StatusCB aCompletedCB, bool aFactoryReset, DimmerIndex aDimmerIndex, ErrorPtr aError);
    DaliBusDevicePtr firstBusDevice();

    void checkPresenceResponse(PresenceCB aPresenceResultHandler, DaliBusDevicePtr aDimmer);
    void disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent);
    
  };



} // namespace p44

#endif // ENABLE_DALI
#endif // __vdcd__dalidevice__
