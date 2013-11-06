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

#include "dsuid.hpp"

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

  typedef boost::intrusive_ptr<Device> DevicePtr;


  class DeviceClassContainer;
  typedef boost::intrusive_ptr<DeviceClassContainer> DeviceClassContainerPtr;
  typedef std::vector<DevicePtr> DeviceVector;
  class DeviceClassContainer : public PropertyContainer
  {
    typedef PropertyContainer inherited;

    DeviceContainer *deviceContainerP; ///< link to the deviceContainer
    DeviceVector devices; ///< the devices of this class
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
		int getInstanceNumber() const;
		

    /// get a sufficiently unique identifier for this class container
    /// @return ID that identifies this container running on a specific hardware
    ///   the ID should not be dependent on the software version
    ///   the ID must differ for each of multiple device class containers run on the same hardware
    ///   the ID MUST change when same software runs on different hardware
    /// @note Current implementation derives this from the devicecontainer's dSUID (modern) or mac address (classic),
    ///   the deviceClassIdentitfier and the instance number in the form "class:instanceIndex@devicecontainerDsidOrMAC"
    string deviceClassContainerInstanceIdentifier() const;

    /// @}


    /// @name device collection and learning/pairing
    /// @{

    /// collect devices from this device classes
    /// @param aCompletedCB will be called when device scan for this device class has been completed
    /// @param aIncremental if set, search is only made for additional new devices. Disappeared devices
    ///   might not get detected this way
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive) = 0;

    /// Forget all previously collected devices
    /// @param aForget if set, all parameters stored for the device (if any) will be deleted. Note however that
    ///   the device is not disconnected (=unlearned) by this.
    virtual void removeDevices(bool aForget);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    virtual void setLearnMode(bool aEnableLearning) { /* NOP in base class */ }

    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name services for actual device class controller implementations
    /// @{

    /// Add device collected from hardware side (bus scan, etc.)
    /// @param aDevice a device object which has a valid dSUID
    /// @return false if aDevice's dSUID is already known.
    /// @note if aDevice's dSUID is already known, it will *not* be added again. This facilitates
    ///   implementation of incremental collection of newly appeared devices (scanning entire bus,
    ///   known ones will just be ignored when encountered again)
    /// @note this can be called as part of a collectDevices scan, or when a new device is detected
    ///   by other means than a scan/collect operation
    virtual bool addDevice(DevicePtr aDevice);

    /// Remove device known no longer connected to the system (for example: explicitly unlearned enOcean switch)
    /// @param aDevice a device object which has a valid dSUID
    /// @param aForget if set, all parameters stored for the device will be deleted. Note however that
    ///   the device is not disconnected (=unlearned) by this.
    virtual void removeDevice(DevicePtr aDevice, bool aForget = false);

    /// get device smart pointer by instance pointer
    DevicePtr getDevicePtrForInstance(Device *aDeviceP);

		/// @}

  protected:

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

  };

} // namespace p44

#endif /* defined(__vdcd__deviceclasscontainer__) */
