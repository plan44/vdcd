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

#ifndef __vdcd__vdchost__
#define __vdcd__vdchost__

#include "vdcd_common.hpp"

#include "dsdefs.h"

#include "persistentparams.hpp"
#include "dsaddressable.hpp"
#include "valuesource.hpp"
#include "digitalio.hpp"

#include "vdcapi.hpp"


using namespace std;

namespace p44 {

  class Vdc;
  class Device;
  class ButtonBehaviour;
  class DsUid;

  typedef boost::intrusive_ptr<Vdc> VdcPtr;
  typedef boost::intrusive_ptr<Device> DevicePtr;

  /// Callback for learn events
  /// @param aLearnIn true if new device learned in, false if device learned out
  /// @param aError error occurred during learn-in
  typedef boost::function<void (bool aLearnIn, ErrorPtr aError)> LearnCB;

  /// Callback for device identification (user action) events
  /// @param aDevice device that was activated
  typedef boost::function<void (DevicePtr aDevice, bool aRegular)> DeviceUserActionCB;

  /// Callback for other global device activity
  typedef enum {
    vdchost_activitysignal, ///< user-relevant activity, can be used to trigger flashing an activity LED.
    vdchost_descriptionchanged ///< user-visible description of the device (such as vdchost name) has changed.
  } VdchostEvent;
  typedef boost::function<void (VdchostEvent aActivity)> VdchostEventCB;



  /// persistence for digitalSTROM paramters
  class DsParamStore : public ParamStore
  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };



  class VdcHost;
  typedef boost::intrusive_ptr<VdcHost> VdcHostPtr;
  typedef map<DsUid, VdcPtr> VdcMap;
  typedef map<DsUid, DevicePtr> DsDeviceMap;


  /// container for all devices hosted by this application
  /// In dS terminology, this object represents the vDC host (a program/daemon hosting one or multiple virtual device connectors).
  /// - is the connection point to a vDSM
  /// - contains one or multiple vDC containers
  ///   (each representing a specific class of devices, e.g. different bus types etc.)
  class VdcHost : public DsAddressable, public PersistentParams
  {
    typedef DsAddressable inherited;
    typedef PersistentParams inheritedParams;

    friend class VdcCollector;
    friend class VdcInitializer;
    friend class Vdc;
    friend class DsAddressable;

    bool externalDsuid; ///< set when dSUID is set to a external value (usually UUIDv1 based)
    bool storedDsuid; ///< set when using stored (DB persisted) dSUID that is not equal to default dSUID 
    uint64_t mac; ///< MAC address as found at startup

    DsDeviceMap dSDevices; ///< available devices by API-exposed ID (dSUID or derived dsid)
    DsParamStore dsParamStore; ///< the database for storing dS device parameters

    string iconDir; ///< the directory where to load icons from
    string persistentDataDir; ///< the directory for the vdcd to store SQLite DBs and possibly other persistent data

    string productName; ///< the name of the vdcd product (model name) as a a whole
    string productVersion; ///< the version string of the vdcd product as a a whole
    string deviceHardwareId; ///< the device hardware id (such as a serial number) of the vdcd product as a a whole
    string descriptionTemplate; ///< how to describe the vdcd (e.g. in service announcements)

    bool collecting;
    long announcementTicket;
    long periodicTaskTicket;
    MLMicroSeconds lastActivity;
    MLMicroSeconds lastPeriodicRun;

    int8_t localDimDirection;

    // learning
    bool learningMode;
    LearnCB learnHandler;

    // user action monitor (learn)buttons
    DeviceUserActionCB deviceUserActionHandler;

    // global event monitor
    VdchostEventCB eventMonitorHandler;

    // mainloop statistics
    int mainloopStatsInterval; ///< 0=none, N=every PERIODIC_TASK_INTERVAL*N seconds
    int mainLoopStatsCounter;

    // active vDC API session
    DsUid connectedVdsm;
    long sessionActivityTicket;
    VdcApiConnectionPtr activeSessionConnection;

  public:

    VdcHost();

    /// the list of containers by API-exposed ID (dSUID or derived dsid)
    VdcMap vdcs;

    /// API for vdSM
    VdcApiServerPtr vdcApiServer;

    /// active session
    VdcApiConnectionPtr getSessionConnection() { return activeSessionConnection; };

    /// set user assignable name
    /// @param new name of this instance of the vdc host
    virtual void setName(const string &aName);

    /// set the human readable name of the vdcd product as a a whole
    /// @param aProductName product (model) name
    void setProductName(const string &aProductName) { productName = aProductName; }

    /// set the the human readable version string of the vdcd product as a a whole
    /// @param aProductVersion product version string
    void setProductVersion(const string &aProductVersion) { productVersion = aProductVersion; }

    /// set the the human readable hardware id (such as a serial number) of the vdcd product as a a whole
    /// @param aDeviceHardwareId device serial number or similar id
    void setDeviceHardwareId(const string &aDeviceHardwareId) { deviceHardwareId = aDeviceHardwareId; }

    /// set the the human readable hardware id (such as a serial number) of the vdcd product as a a whole
    /// @param aDescriptionTemplate template for external device description
    /// @note the following sequences will be substituted
    /// - %V : vendor name
    /// - %V : product model (name)
    /// - %N : user-assigned name, if any, preceeded by a space and in double quotes
    /// - %S : device hardware id (if set specifically, otherwise the dSUID is used)
    void setDescriptionTemplate(const string &aDescriptionTemplate) { descriptionTemplate = aDescriptionTemplate; }

    /// Set how dSUIDs are generated
    /// @param aExternalDsUid if specified, this is used directly as dSUID for the device container
    /// @note Must be set before any other activity in the device container, in particular before
    ///   any class containers are added to the device container
    void setIdMode(DsUidPtr aExternalDsUid);

    /// Set directory for loading device icons
    /// @param aIconDir  full path to directory to load device icons from. Empty string or NULL means "no icons"
    /// @note the path needs to contain subdirectories for each size, named iconX with X=side lengt in pixels
    ///   At this time, only 16x16 icons are defined, so only one subdirectory named icon16 needs to exist
    ///   within icondir
    void setIconDir(const char *aIconDir);

    /// Get directory for loading device icons from
    /// @return the path to the icon dir, always with a trailing path separator, ready to append subpaths and filenames
    const char *getIconDir();

    /// Set how often mainloop statistics are printed out log (LOG_INFO)
    /// @param aInterval 0=none, N=every PERIODIC_TASK_INTERVAL*N seconds
    void setMainloopStatsInterval(int aInterval) { mainloopStatsInterval = aInterval; };

    /// @return MAC address as 12 char hex string (6 bytes)
    string macAddressString();

    /// @return IPv4 address as string
    string ipv4AddressString();

    /// @return URL for Web-UI (for access from local LAN)
    virtual string webuiURLString() { return ""; /* none by default */ }

    /// prepare device container internals for creating and adding vDCs
    /// In particular, this triggers creating/loading the vdc host dSUID, which serves as a base ID
    /// for most class containers and many devices.
    /// @param aFactoryReset if set, database will be reset
    void prepareForVdcs(bool aFactoryReset);

		/// initialize
    /// @param aCompletedCB will be called when the entire container is initialized or has been aborted with a fatal error
    /// @param aFactoryReset if set, database will be reset
    void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    /// start running normally
    void startRunning();

    /// activity monitor
    /// @param aActivityCB will be called for globally relevant events.
    void setEventMonitor(VdchostEventCB aEventCB);

    /// activity monitor
    /// @param aUserActionCB will be called when the user has performed an action (usually: button press) in a device
    void setUserActionMonitor(DeviceUserActionCB aUserActionCB);

    /// find a value source
    /// @param aValueSourceID internal, persistent ID of the value source
    ValueSource *getValueSourceById(string aValueSourceID);



		/// @name device detection and registration
    /// @{

    /// collect devices from all vDCs, and have each of them initialized
    /// @param aCompletedCB will be called when all device scans have completed
    /// @param aIncremental if set, search is only made for additional new devices. Disappeared devices
    ///   might not get detected this way
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.
    /// @param aClearSettings if set, persistent settings of currently known devices will be deleted before
    ///   re-scanning for devices, which means devices will have default settings after collecting.
    ///   Note that this is mutually exclusive with aIncremental (incremental scan does not remove any devices,
    ///   and thus cannot remove any settings, either)
    void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    /// Put vDC controllers into learn-in mode
    /// @param aCompletedCB handler to call when a learn-in action occurs
    /// @param aDisableProximityCheck true to disable proximity check (e.g. minimal RSSI requirement for some EnOcean devices)
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

    /// called by vDC containers to report learn event
    /// @param aLearnIn true if new device learned in, false if device learned out
    /// @param aError error occurred during learn-in
    void reportLearnEvent(bool aLearnIn, ErrorPtr aError);

    /// called to signal a user-generated action from a device, which may be used to detect a device
    /// @param aDevice device where user action was detected
    /// @param aRegular if true, the user action is a regular action, such as a button press
    /// @return true if normal user action processing should be suppressed
    bool signalDeviceUserAction(Device &aDevice, bool aRegular);

    /// called by vDC containers to add devices to the container-wide devices list
    /// @param aDevice a device object which has a valid dSUID
    /// @return false if aDevice's dSUID is already known.
    /// @note aDevice is added *only if no device is already known with this dSUID*
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    bool addDevice(DevicePtr aDevice);

    /// called by vDC containers to remove devices from the container-wide list
    /// @param aDevice a device object which has a valid dSUID
    /// @param aForget if set, parameters stored for the device will be deleted
    void removeDevice(DevicePtr aDevice, bool aForget);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return productName.size()>0 ? productName : "vDC host"; }

    /// @return human readable product version string
    virtual string modelVersion() { return productVersion; }

    /// @return unique ID for the functional model of this entity
    virtual string modelUID() { return DSUID_P44VDC_MODELUID_UUID; /* using the p44 modelUID namespace UUID itself */ }

    /// @return the entity type (one of dSD|vdSD|vDChost|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "vDChost"; }

    /// @return hardware version string or NULL if none
    virtual string hardwareVersion() { return ""; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID() { return ""; }

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID() { return ""; }

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    /// @note class containers and devices will inherit this (vdc host's) vendor name if not overridden
    virtual string vendorName() { return "plan44.ch"; };

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    string getDeviceHardwareId() { return deviceHardwareId; };

    /// @return text describing the vdc host device, suitable for publishing via Avahi etc.
    string publishedDescription();


    /// @}


    /// @name vdc host level property persistence
    /// @{

    /// load parameters from persistent DB and determine root (vdc host) dSUID
    ErrorPtr loadAndFixDsUID();

    /// save unsaved parameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    ErrorPtr save();

    /// forget any parameters stored in persistent DB
    ErrorPtr forget();

    // load additional settings from file
    void loadSettingsFromFiles();

    /// @}


  protected:

    /// add a vDC container
    /// @param aVdcPtr a intrusive_ptr to a vDC container
    /// @note this is a one-time initialisation. Device class containers are not meant to be removed at runtime
    void addVdc(VdcPtr aVdcPtr);


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

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

    // method and notification dispatching
    ErrorPtr handleMethodForDsUid(const string &aMethod, VdcApiRequestPtr aRequest, const DsUid &aDsUid, ApiValuePtr aParams);
    void handleNotificationForDsUid(const string &aMethod, const DsUid &aDsUid, ApiValuePtr aParams);
    DsAddressablePtr addressableForParams(const DsUid &aDsUid, ApiValuePtr aParams);

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

    // vDC level method and notification handlers
    ErrorPtr helloHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams);
    ErrorPtr byeHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams);
    ErrorPtr removeHandler(VdcApiRequestPtr aForRequest, DevicePtr aDevice);
    void removeResultHandler(DevicePtr aDevice, VdcApiRequestPtr aForRequest, bool aDisconnected);
    void deviceInitialized(DevicePtr aDevice);

    // announcing dSUID addressable entities within the device container (vdc host)
    void resetAnnouncing();
    void startAnnouncing();
    void announceNext();
    void announceResultHandler(DsAddressablePtr aAddressable, VdcApiRequestPtr aRequest, ErrorPtr &aError, ApiValuePtr aResultOrErrorData);

    // activity monitor
    void signalActivity();

    // periodic task
    void periodicTask(MLMicroSeconds aCycleStartTime);

    // getting MAC
    void getMyMac(StatusCB aCompletedCB, bool aFactoryReset);

    /// get all value sources in this vdcd
    /// @param aApiObjectValue must be an object typed API value, will receive available value sources as valueSourceID/description key/values
    void createValueSourcesList(ApiValuePtr aApiObjectValue);

  };

} // namespace p44

#endif /* defined(__vdcd__vdchost__) */
