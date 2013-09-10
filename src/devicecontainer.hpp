//
//  devicecontainer.hpp
//  vdcd
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__devicecontainer__
#define __vdcd__devicecontainer__

#include "vdcd_common.hpp"

#include "dsdefs.h"

#include "persistentparams.hpp"
#include "dsaddressable.hpp"

using namespace std;

namespace p44 {

  class DeviceClassContainer;
  class Device;
  class ButtonBehaviour;
  class dSID;

  typedef boost::intrusive_ptr<DeviceClassContainer> DeviceClassContainerPtr;
  typedef boost::intrusive_ptr<Device> DevicePtr;

  /// generic callback for signalling completion (with success/error reporting)
  typedef boost::function<void (ErrorPtr aError)> CompletedCB;

  /// Callback for learn events
  /// @param aLearnIn true if new device learned in, false if device learned out
  /// @param aError error occurred during learn-in
  typedef boost::function<void (bool aLearnIn, ErrorPtr aError)> LearnCB;


  /// persistence for digitalSTROM paramters
  class DsParamStore : public ParamStore
  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };



  /// container for all devices hosted by this application
  /// - is the connection point to a vDSM
  /// - contains one or multiple device class containers
  ///   (each representing a specific class of devices, e.g. different bus types etc.)
  class DeviceContainer;
  typedef boost::intrusive_ptr<DeviceContainer> DeviceContainerPtr;
  typedef list<DeviceClassContainerPtr> ContainerList;
  typedef map<dSID, DevicePtr> DsDeviceMap;

  typedef list<JsonRpcCommPtr> ApiConnectionList;

  class DeviceContainer : public DsAddressable
  {
    typedef DsAddressable inherited;

    friend class DeviceClassCollector;
    friend class DeviceClassInitializer;
    friend class DeviceClassContainer;
    friend class DsAddressable;

    DsDeviceMap dSDevices; ///< available devices by dSID
    DsParamStore dsParamStore; ///< the database for storing dS device parameters

    string persistentDataDir;

    bool collecting;
    long announcementTicket;
    long periodicTaskTicket;

    long localDimTicket;
    bool localDimDown;

    // vDC session
    bool sessionActive;
    dSID connectedVdsm;
    long sessionActivityTicket;
    ApiConnectionList apiConnections;
    JsonRpcCommPtr sessionComm;

    // learning
    bool learningMode;
    LearnCB learnHandler;

  public:

    DeviceContainer();

    /// the list of containers
    ContainerList deviceClassContainers;

    /// API for vdSM
    SocketComm vdcApiServer;

    /// add a device class container
    /// @param aDeviceClassContainerPtr a shared_ptr to a device class container
    /// @note this is a one-time initialisation. Device class containers are not meant to be removed at runtime
    void addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr);

    /// get a sufficiently unique identifier for this device container
    /// @return ID that identifies this container running on a specific hardware
    ///   the ID should not be dependent on the software version
    ///   the ID MUST change when same software runs on different hardware
    ///   Usually, a hardware-ID such as the MAC address is used
    string deviceContainerInstanceIdentifier() const;

		/// initialize
    /// @param aCompletedCB will be called when the entire container is initialized or has been aborted with a fatal error
    void initialize(CompletedCB aCompletedCB, bool aFactoryReset);


		/// @name device detection and registration
    /// @{

    /// collect devices from all device classes, and have each of them initialized
    /// @param aCompletedCB will be called when all device scans have completed
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.  
    void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

    /// Put device class controllers into learn-in mode
    /// @param aCompletedCB handler to call when a learn-in action occurs
    void startLearning(LearnCB aLearnHandler);

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

    virtual ErrorPtr handleMethod(const string &aMethod, const string &aJsonRpcId, JsonObjectPtr aParams);
    virtual void handleNotification(const string &aMethod, JsonObjectPtr aParams);

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

    /// called by device class containers to add devices to the container-wide devices list
    /// @param aDevice a device object which has a valid dsid
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    void addDevice(DevicePtr aDevice);

    /// called by device class containers to remove devices from the container-wide list
    /// @param aDevice a device object which has a valid dsid
    /// @param aForget if set, parameters stored for the device will be deleted
    void removeDevice(DevicePtr aDevice, bool aForget);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "vDC virtual device controller"; }

    /// @return the entity type (one of dSD|vdSD|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "vDC"; }

    /// @return 0xMMmmrrrr digitalstrom Profile version (MM=major, mm=minor, rrrr=revision)
    virtual uint32_t dsProfileVersion() { return 0; }

    /// @return hardware version string or NULL if none
    virtual string hardwareVersion() { return ""; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID() { return ""; }

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID() { return ""; }
    
    /// @}


  protected:

    /// @name methods for friend classes to send API messages
    /// @{

    /// send a raw JSON-RPC method or notification to vdSM
    /// @param aMethod the method or notification
    /// @param aParams the parameters object, or NULL if none
    /// @param aResponseHandler handler for response. If not set, request is sent as notification
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendApiRequest(const char *aMethod, JsonObjectPtr aParams, JsonRpcResponseCB aResponseHandler = JsonRpcResponseCB());

    /// send a raw JSON-RPC result from a method call back to the to vdSM
    /// @param aJsonRpcId the id parameter from the method call
    /// @param aParams the parameters object, or NULL if none
    /// @param aResponseHandler handler for response. If not set, request is sent as notification
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendApiResult(const string &aJsonRpcId, JsonObjectPtr aResult);

    /// send error from a method call back to the vdSM
    /// @param aJsonRpcId this must be the aJsonRpcId as received in the JsonRpcRequestCB handler.
    /// @param aErrorToSend From this error object, getErrorCode() and description() will be used as "code" and "message" members
    ///   of the JSON-RPC 2.0 error object.
    /// @result empty or Error object in case of error sending error response
    bool sendApiError(const string &aJsonRpcId, ErrorPtr aErrorToSend);

    /// @}


  protected:

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual PropertyContainerPtr getContainer(const PropertyDescriptor &aPropertyDescriptor, int &aDomain, int aIndex = 0);
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

  private:

    // local operation mode
    void handleClickLocally(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType);
    void localDimHandler();

    // vDC API connection and session handling
    SocketCommPtr vdcApiConnectionHandler(SocketComm *aServerSocketCommP);
    void vdcApiConnectionStatusHandler(SocketComm *aJsonRpcComm, ErrorPtr aError);
    void endApiConnection(JsonRpcComm *aJsonRpcComm);
    void vdcApiRequestHandler(JsonRpcComm *aJsonRpcComm, const char *aMethod, const char *aJsonRpcId, JsonObjectPtr aParams);
    void sessionTimeoutHandler();
    void startContainerSession();
    void endContainerSession();

    // method and notification dispatching
    ErrorPtr handleMethodForDsid(const string &aMethod, const string &aJsonRpcId, const dSID &aDsid, JsonObjectPtr aParams);
    void handleNotificationForDsid(const string &aMethod, const dSID &aDsid, JsonObjectPtr aParams);

    // vDC level method and notification handlers
    ErrorPtr helloHandler(JsonRpcComm *aJsonRpcComm, const string &aJsonRpcId, JsonObjectPtr aParams);
    ErrorPtr byeHandler(JsonRpcComm *aJsonRpcComm, const string &aJsonRpcId, JsonObjectPtr aParams);
    ErrorPtr removeHandler(DevicePtr aDevice, const string &aJsonRpcId);
    void removeResultHandler(const string &aJsonRpcId, DevicePtr aDevice, bool aDisconnected);

    // announcing devices
    void announceDevices();
    void announceResultHandler(DevicePtr aDevice, JsonRpcComm *aJsonRpcComm, int32_t aResponseId, ErrorPtr &aError, JsonObjectPtr aResultOrErrorData);

  public:
    // public for C++ limitation reasons only, semantically private

    // periodic task
    void periodicTask(MLMicroSeconds aCycleStartTime);

  };

} // namespace p44

#endif /* defined(__vdcd__devicecontainer__) */
