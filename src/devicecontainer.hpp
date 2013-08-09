//
//  devicecontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__devicecontainer__
#define __vdcd__devicecontainer__

#include "vdcd_common.hpp"

#include "jsonrpccomm.hpp"
#include "persistentparams.hpp"
#include "dsid.hpp"

using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    vdSMErrorOK,
    vdSMErrorMissingOperation,
    vdSMErrorMissingParameter,
    vdSMErrorInvalidParameter,
    vdSMErrorDeviceNotFound,
    vdSMErrorUnknownDeviceOperation,
    vdSMErrorUnknownContainerOperation,
  } vdSMErrors;

  class vdSMError : public Error
  {
  public:
    static const char *domain() { return "vdSMAPI"; }
    virtual const char *getErrorDomain() const { return vdSMError::domain(); };
    vdSMError(vdSMErrors aError) : Error(ErrorCode(aError)) {};
    vdSMError(vdSMErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };



  class DeviceClassContainer;
  class Device;
  class dSID;
  typedef boost::shared_ptr<DeviceClassContainer> DeviceClassContainerPtr;
  typedef boost::shared_ptr<Device> DevicePtr;

  /// generic callback for signalling completion (with success/error reporting)
  typedef boost::function<void (ErrorPtr aError)> CompletedCB;

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
  typedef boost::shared_ptr<DeviceContainer> DeviceContainerPtr;
  typedef list<DeviceClassContainerPtr> ContainerList;
  typedef std::map<dSID, DevicePtr> DsDeviceMap;

  typedef std::map<uint32_t, DevicePtr> BusAddressMap;


  class DeviceContainer
  {
    friend class DeviceClassCollector;

    DsDeviceMap dSDevices; ///< available devices by dSID
    DsParamStore dsParamStore; ///< the database for storing dS device parameters

    string persistentDataDir;

    bool collecting;
    long announcementTicket;
    long periodicTaskTicket;

    long localDimTicket;
    bool localDimDown;

  private:

    /// vdSM API message handler
    void vdsmMessageHandler(ErrorPtr aError, JsonObjectPtr aJsonObject);

  public:

    DeviceContainer();

    /// the digitalstrom ID
    dSID containerDsid;

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

    /// start vDC session (say Hello to the vdSM)
    void startContainerSession();

    /// end vDC session
    void endContainerSession();

    /// announce all not-yet announced devices to the vdSM
    void announceDevices();

    /// called by device class containers to add devices to the container-wide devices list
    /// @param aDevice a device object which has a valid dsid
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    void addDevice(DevicePtr aDevice);

    /// called by device class containers to remove devices from the container-wide list
    /// @param aDevice a device object which has a valid dsid
    /// @param aForget if set, parameters stored for the device will be deleted
    void removeDevice(DevicePtr aDevice, bool aForget);

    /// periodic task
    void periodicTask(MLMicroSeconds aCycleStartTime);

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

    /// save

    /// @}

		
    /// send message to vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendMessage(const char *aOperation, JsonObjectPtr aParams);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  private:

    void handleClickLocally(int aClickType, int aKeyID);
    void localDimHandler();

    void deviceAnnounced();

    SocketCommPtr vdcApiConnectionHandler(SocketComm *aServerSocketCommP);
    void vdcApiRequestHandler(JsonRpcComm *aJsonRpcComm, const char *aMethod, const char *aJsonRpcId, JsonObjectPtr aParams);

  };

} // namespace p44

#endif /* defined(__vdcd__devicecontainer__) */
