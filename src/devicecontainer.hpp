//
//  devicecontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__devicecontainer__
#define __p44bridged__devicecontainer__

#include "p44bridged_common.hpp"

#include "jsoncomm.hpp"

using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    vdSMErrorOK,
    vdSMErrorMissingOperation,
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

    bool collecting;

    DsDeviceMap dSDevices; ///< available devices by dSID

    BusAddressMap busDevices; ///< registered devices by BusAddress.
    // TODO: %%% we'll probably get rid of this with the new vDSM API


  private:

    /// vdSM API message handler
    void vdsmMessageHandler(ErrorPtr aError, JsonObjectPtr aJsonObject);

  public:

    DeviceContainer();

    /// the list of containers
    ContainerList deviceClassContainers;

    /// JSON communication with vdsm
    JsonComm vdsmJsonComm;

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

    /// collect devices from all device classes
    /// @param aCompletedCB will be called when all device scans have completed
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.  
    void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

    /// register all unregistered devices
    void registerDevices(MLMicroSeconds aLastRegBefore = Never);


    /// called by device class containers to add devices to the container-wide devices list
    /// @param aDevice a device object which has a valid dsid
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    void addDevice(DevicePtr aDevice);

    /// called by device class containers to remove devices from the container-wide list
    /// @param aDevice a device object which has a valid dsid
    void removeDevice(DevicePtr aDevice);

    /// periodic task
    void periodicTask(MLMicroSeconds aCycleStartTime);

    /// @}

    /// send message to vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendMessage(const char *aOperation, JsonObjectPtr aParams);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  };

} // namespace p44

#endif /* defined(__p44bridged__devicecontainer__) */
