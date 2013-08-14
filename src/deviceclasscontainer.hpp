//
//  deviceclasscontainer.hpp
//  vdcd
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__deviceclasscontainer__
#define __vdcd__deviceclasscontainer__

#include "vdcd_common.hpp"

#include "devicecontainer.hpp"

#include "dsid.hpp"

using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    DeviceClassErrorOK,
    DeviceClassErrorInitialize,
  } DeviceClassErrors;
	
  class DeviceClassError : public Error
  {
  public:
    static const char *domain() { return "DeviceClass"; }
    virtual const char *getErrorDomain() const { return DeviceClassError::domain(); };
    DeviceClassError(DeviceClassErrors aError) : Error(ErrorCode(aError)) {};
    DeviceClassError(DeviceClassErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };
	
	
	
  class Device;

  typedef boost::shared_ptr<Device> DevicePtr;


  class DeviceClassContainer;
  typedef boost::shared_ptr<DeviceClassContainer> DeviceClassContainerPtr;
  typedef boost::weak_ptr<DeviceClassContainer> DeviceClassContainerWeakPtr;
  typedef std::list<DevicePtr> DeviceList;
  class DeviceClassContainer
  {
    DeviceClassContainerWeakPtr mySelf; ///< weak pointer to myself
    DeviceContainer *deviceContainerP; ///< link to the deviceContainer
    DeviceList devices; ///< the devices of this class
    int instanceNumber; ///< the instance number identifying this instance among other instances of this class
  public:
    /// @param aInstanceNumber index which uniquely (and as stable as possible) identifies a particular instance
    ///   of this class container. This is used when generating dsids for devices that don't have their own
    ///   unique ID, by using a hashOf(DeviceContainer's id, deviceClassIdentifier(), aInstanceNumber)
    DeviceClassContainer(int aInstanceNumber);

    /// associate with container
    /// @param aDeviceContainerP device container this device class is contained in
    void setDeviceContainer(DeviceContainer *aDeviceContainerP);
    /// get associated container
    /// @return associated device container
    DeviceContainer &getDeviceContainer() const;

		/// initialize
		/// @param aCompletedCB will be called when initialisation is complete
		///  callback will return an error if initialisation has failed and the device class is not functional
    virtual void initialize(CompletedCB aCompletedCB, bool aFactoryReset);
		
    /// @name persistence
    /// @{

		/// get the persistent data dir path
		/// @return full path to directory to save persistent data
		const char *getPersistentDataDir();
		
    /// @}
		
		
    /// @name identification
    /// @{

    /// deviceclass identifier
		/// @return constant identifier for this container class (no spaces, filename-safe)
    virtual const char *deviceClassIdentifier() const = 0;
		
    /// Instance number (to differentiate multiple device class containers of the same class)
		/// @return instance index number
		int getInstanceNumber();
		

    /// get a sufficiently unique identifier for this class container
    /// @return ID that identifies this container running on a specific hardware
    ///   the ID should not be dependent on the software version
    ///   the ID must differ for each of multiple device class containers run on the same hardware
    ///   the ID MUST change when same software runs on different hardware
    ///   Usually, MAC address is used as base ID.
    string deviceClassContainerInstanceIdentifier() const;

    /// @}


    /// @name device detection and registration
    /// @{

    /// collect devices from this device classes
    /// @param aCompletedCB will be called when device scan for this device class has been completed
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.
    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive) = 0;

    /// Add device collected from hardware side (bus scan, etc.)
    /// @param aDevice a device object which has a valid dsid
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    virtual void addDevice(DevicePtr aDevice);


    /// Remove device known no longer connected to the system (for example: explicitly unlearned enOcean switch)
    /// @param aDevice a device object which has a valid dsid
    /// @param aForget if set, all parameters stored for the device will be deleted
    virtual void removeDevice(DevicePtr aDevice, bool aForget = false);


    /// Forget all previously collected devices
    virtual void forgetDevices();

		/// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  };

} // namespace p44

#endif /* defined(__vdcd__deviceclasscontainer__) */
