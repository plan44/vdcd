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

#ifndef __vdcd__devicecontainer__
#define __vdcd__devicecontainer__

#include "vdcd_common.hpp"

#include "dsdefs.h"

#include "persistentparams.hpp"
#include "dsaddressable.hpp"
#include "digitalio.hpp"

#include "vdcapi.hpp"


using namespace std;

namespace p44 {

  class DeviceClassContainer;
  class Device;
  class ButtonBehaviour;
  class DsUid;

  typedef boost::intrusive_ptr<DeviceClassContainer> DeviceClassContainerPtr;
  typedef boost::intrusive_ptr<Device> DevicePtr;

  /// generic callback for signalling something done
  typedef boost::function<void ()> DoneCB;

  /// generic callback for signalling completion (with success/error reporting)
  typedef boost::function<void (ErrorPtr aError)> CompletedCB;

  /// Callback for learn events
  /// @param aLearnIn true if new device learned in, false if device learned out
  /// @param aError error occurred during learn-in
  typedef boost::function<void (bool aLearnIn, ErrorPtr aError)> LearnCB;

  /// Callback for device identification (user action) events
  /// @param aDevice device that was activated
  typedef boost::function<void (DevicePtr aDevice, bool aRegular)> DeviceUserActionCB;


  /// persistence for digitalSTROM paramters
  class DsParamStore : public ParamStore
  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };



  class DeviceContainer;
  typedef boost::intrusive_ptr<DeviceContainer> DeviceContainerPtr;
  typedef map<DsUid, DeviceClassContainerPtr> ContainerMap;
  typedef map<DsUid, DevicePtr> DsDeviceMap;


  /// container for all devices hosted by this application
  /// In dS terminology, this object represents the vDC host (a program/daemon hosting one or multiple virtual device connectors).
  /// - is the connection point to a vDSM
  /// - contains one or multiple device class containers
  ///   (each representing a specific class of devices, e.g. different bus types etc.)
  class DeviceContainer : public DsAddressable
  {
    typedef DsAddressable inherited;

    friend class DeviceClassCollector;
    friend class DeviceClassInitializer;
    friend class DeviceClassContainer;
    friend class DsAddressable;

    bool dsUids; ///< set to use dsUIDs (GS1/UUID based, 34 hex chars)
    bool externalDsuid; ///< set when dSUID is set to a external value (usually UUIDv1 based)
    uint64_t mac; ///< MAC address as found at startup

    DsDeviceMap dSDevices; ///< available devices by API-exposed ID (dSUID or derived dsid)
    DsParamStore dsParamStore; ///< the database for storing dS device parameters

    string persistentDataDir;

    bool collecting;
    long announcementTicket;
    long periodicTaskTicket;

    int8_t localDimDirection;

    // learning
    bool learningMode;
    LearnCB learnHandler;

    // user action monitor (learn)buttons
    DeviceUserActionCB deviceUserActionHandler;

    // activity monitor
    DoneCB activityHandler;

    // active vDC API session
    DsUid connectedVdsm;
    long sessionActivityTicket;
    VdcApiConnectionPtr activeSessionConnection;

  public:

    DeviceContainer();

    /// the list of containers by API-exposed ID (dSUID or derived dsid)
    ContainerMap deviceClassContainers;

    /// API for vdSM
    VdcApiServerPtr vdcApiServer;

    /// active session
    VdcApiConnectionPtr getSessionConnection() { return activeSessionConnection; };


    /// Set how dSUIDs are generated
    /// @param aDsUid true to enable modern dSUIDs (GS1/UUID based)
    /// @param aExternalDsUid if specified, this is used directly as dSUID for the device container
    /// @note Must be set before any other activity in the device container, in particular before
    ///   any class containers are added to the device container
    void setIdMode(bool aDsUid, DsUidPtr aExternalDsUid = DsUidPtr());


    /// @return true if modern GS1/UUID based dSUIDs should be used
    bool usingDsUids() { return dsUids; };

    /// @return MAC address as 12 char hex string (6 bytes)
    string macAddressString();

    /// @return IPv4 address as string
    string ipv4AddressString();

    /// @return URL for Web-UI (for access from local LAN)
    virtual string webuiURLString() { return ""; /* none by default */ }

		/// initialize
    /// @param aCompletedCB will be called when the entire container is initialized or has been aborted with a fatal error
    void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    /// start running normally
    void startRunning();

    /// activity monitor
    /// @param aActivityCB will be called when there is user-relevant activity. Can be used to trigger flashing an activity LED.
    void setActivityMonitor(DoneCB aActivityCB);

    /// activity monitor
    /// @param aUserActionCB will be called when the user has performed an action (usually: button press) in a device
    void setUserActionMonitor(DeviceUserActionCB aUserActionCB);


		/// @name device detection and registration
    /// @{

    /// collect devices from all device classes, and have each of them initialized
    /// @param aCompletedCB will be called when all device scans have completed
    /// @param aIncremental if set, search is only made for additional new devices. Disappeared devices
    ///   might not get detected this way
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.  
    void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// Put device class controllers into learn-in mode
    /// @param aCompletedCB handler to call when a learn-in action occurs
    /// @param aDisableProximityCheck true to disable proximity check (e.g. minimal RSSI requirement for some enOcean devices)
    void startLearning(LearnCB aLearnHandler, bool aDisableProximityCheck = false);

    /// stop learning mode
    void stopLearning();

    /// @return true if currently in learn mode
    bool isLearning() { return learningMode; };

    /// @}


    /// @name persistence
    /// @{

    /// set the directory where to store persistent data (databases etc.)
    /// @param aPersistentDataDir full path to directory to save persistent data
    void setPersistentDataDir(const char *aPersistentDataDir);

		/// get the persistent data dir path
		/// @return full path to directory to save persistent data
		const char *getPersistentDataDir();

    /// get the dsParamStore
    DsParamStore &getDsParamStore() { return dsParamStore; }

    /// @}


    /// @name DsAddressable API implementation
    /// @{

    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);
    virtual void handleNotification(const string &aMethod, ApiValuePtr aParams);

    /// @}

    /// have button clicks checked for local handling
    void checkForLocalClickHandling(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name methods for DeviceClassContainers
    /// @{

    /// called by device class containers to report learn event
    /// @param aLearnIn true if new device learned in, false if device learned out
    /// @param aError error occurred during learn-in
    void reportLearnEvent(bool aLearnIn, ErrorPtr aError);

    /// called to signal a user-generated action from a device, which may be used to detect a device
    /// @param aDevice device where user action was detected
    /// @param aRegular if true, the user action is a regular action, such as a button press
    /// @return true if normal user action processing should be suppressed
    bool signalDeviceUserAction(Device &aDevice, bool aRegular);

    /// called by device class containers to add devices to the container-wide devices list
    /// @param aDevice a device object which has a valid dSUID
    /// @return false if aDevice's dSUID is already known.
    /// @note aDevice is added *only if no device is already known with this dSUID*
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    bool addDevice(DevicePtr aDevice);

    /// called by device class containers to remove devices from the container-wide list
    /// @param aDevice a device object which has a valid dSUID
    /// @param aForget if set, parameters stored for the device will be deleted
    void removeDevice(DevicePtr aDevice, bool aForget);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "vDC host"; }

    /// @return the entity type (one of dSD|vdSD|vDChost|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "vDChost"; }

    /// @return hardware version string or NULL if none
    virtual string hardwareVersion() { return ""; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID() { return ""; }

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID() { return ""; }
    
    /// @}


  protected:

    /// add a device class container
    /// @param aDeviceClassContainerPtr a intrusive_ptr to a device class container
    /// @note this is a one-time initialisation. Device class containers are not meant to be removed at runtime
    void addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr);


    /// @name method for friend classes to send API messages
    /// @note sending results/errors is done via the VcdApiRequest object
    /// @{

    /// send a API method or notification call to the vdSM
    /// @param aMethod the method or notification
    /// @param aParams the parameters object, or NULL if none
    /// @param aResponseHandler handler for response. If not set, request is sent as notification
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendApiRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler = VdcApiResponseCB());


    /// @}

  protected:

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // method and notification dispatching
    ErrorPtr handleMethodForDsUid(const string &aMethod, VdcApiRequestPtr aRequest, const DsUid &aDsUid, ApiValuePtr aParams);
    void handleNotificationForDsUid(const string &aMethod, const DsUid &aDsUid, ApiValuePtr aParams);

  private:

    // derive dSUID
    void deriveDsUid();

    // local operation mode
    void handleClickLocally(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType);
    void localDimHandler();

    // API connection status handling
    void vdcApiConnectionStatusHandler(VdcApiConnectionPtr aApiConnection, ErrorPtr &aError);

    // API request handling
    void vdcApiRequestHandler(VdcApiConnectionPtr aApiConnection, VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    // generic session handling
    void sessionTimeoutHandler();

    // vDC level method and notification handlers
    ErrorPtr helloHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams);
    ErrorPtr byeHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams);
    ErrorPtr removeHandler(VdcApiRequestPtr aForRequest, DevicePtr aDevice);
    void removeResultHandler(VdcApiRequestPtr aForRequest, bool aDisconnected);

    // announcing dSUID addressable entities within the device container (vdc host)
    void resetAnnouncing();
    void startAnnouncing();
    void announceNext();
    void announceResultHandler(DsAddressablePtr aAddressable, VdcApiRequestPtr aRequest, ErrorPtr &aError, ApiValuePtr aResultOrErrorData);

    // activity monitor
    void signalActivity();

    // getting MAC
    void getMyMac(CompletedCB aCompletedCB, bool aFactoryReset);

  public:
    // public for C++ limitation reasons only, semantically private

    // periodic task
    void periodicTask(MLMicroSeconds aCycleStartTime);

  };

} // namespace p44

#endif /* defined(__vdcd__devicecontainer__) */
