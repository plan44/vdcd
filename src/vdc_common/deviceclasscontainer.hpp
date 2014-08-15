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


  /// This is the base class for a "class" (usually: type of hardware) of virtual devices.
  /// In dS terminology, this object represents a vDC (virtual device connector).
  class DeviceClassContainer : public DsAddressable
  {
    typedef DsAddressable inherited;

    DeviceVector devices; ///< the devices of this class
    int instanceNumber; ///< the instance number identifying this instance among other instances of this class
    int tag; ///< tag used to in self test failures for showing on LEDs

  public:

    #if VDCS_PSEUDO_CLASSIC_DSID
    DsUid pseudoClassicId;
    /// the dSUID exposed in the VDC API (might be pseudoclassic during beta)
    virtual const DsUid &getApiDsUid();
    #endif


    /// @param aInstanceNumber index which uniquely (and as stable as possible) identifies a particular instance
    ///   of this class container. This is used when generating dsuids for devices that don't have their own
    ///   unique ID, by using a hashOf(DeviceContainer's id, deviceClassIdentifier(), aInstanceNumber)
    /// @param aDeviceContainerP device container this device class is contained in
    /// @param numeric tag for this device container (e.g. for blinking self test error messages)
    DeviceClassContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    /// add device class to device container.
    void addClassToDeviceContainer();

		/// initialize
		/// @param aCompletedCB will be called when initialisation is complete
		///   callback will return an error if initialisation has failed and the device class is not functional
		/// @param aFactoryReset if set, also perform factory reset for data persisted for this device class
    virtual void initialize(CompletedCB aCompletedCB, bool aFactoryReset);
		
    /// @name persistence
    /// @{

		/// get the persistent data dir path
		/// @return full path to directory to save persistent data
		const char *getPersistentDataDir();

    /// get the tag
    int getTag() { return tag; };
		
    /// @}
		
		
    /// @name identification
    /// @{

    /// deviceclass identifier
		/// @return constant identifier for this container class (no spaces, filename-safe)
    virtual const char *deviceClassIdentifier() const = 0;
		
    /// Instance number (to differentiate multiple device class containers of the same class)
		/// @return instance index number
		int getInstanceNumber() const;

    /// @return URL for Web-UI (for access from local LAN)
    virtual string webuiURLString() { return deviceContainerP->webuiURLString(); /* by default, return vDC host's config URL */ };

    /// get a sufficiently unique identifier for this class container
    /// @return ID that identifies this container running on a specific hardware
    ///   the ID should not be dependent on the software version
    ///   the ID must differ for each of multiple device class containers run on the same hardware
    ///   the ID MUST change when same software runs on different hardware
    /// @note Current implementation derives this from the devicecontainer's dSUID,
    ///   the deviceClassIdentitfier and the instance number in the form "class:instanceIndex@devicecontainerDsUid"
    string deviceClassContainerInstanceIdentifier() const;

    /// @}


    /// @name device collection, learning/pairing, self test
    /// @{

    /// collect devices from this device classes for normal operation
    /// @param aCompletedCB will be called when device scan for this device class has been completed
    /// @param aIncremental if set, search is only made for additional new devices. Disappeared devices
    ///   might not get detected this way
    /// @param aExhaustive if set, device search is made exhaustive (may include longer lasting procedures to
    ///   recollect lost devices, assign bus addresses etc.). Without this flag set, device search should
    ///   still be complete under normal conditions, but might sacrifice corner case detection for speed.
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive) = 0;

    /// perform self test
    /// @param aCompletedCB will be called when self test is done, returning ok or error
    /// @note self will be called *instead* of collectDevices() but might need to do some form of
    ///   collecting devices to perform the test. It might do that by calling collectDevices(), but
    ///   must make sure NOT to modify or generate any persistent data for the class.
    virtual void selfTest(CompletedCB aCompletedCB);

    /// Forget all previously collected devices
    /// @param aForget if set, all parameters stored for the device (if any) will be deleted. Note however that
    ///   the device is not disconnected (=unlearned) by this.
    virtual void removeDevices(bool aForget);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @param aDisableProximityCheck true to disable proximity check (e.g. minimal RSSI requirement for some EnOcean devices)
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    virtual void setLearnMode(bool aEnableLearning, bool aDisableProximityCheck) { /* NOP in base class */ }

    /// @}


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

    /// Remove device known no longer connected to the system (for example: explicitly unlearned EnOcean switch)
    /// @param aDevice a device object which has a valid dSUID
    /// @param aForget if set, all parameters stored for the device will be deleted. Note however that
    ///   the device is not disconnected (=unlearned) by this.
    virtual void removeDevice(DevicePtr aDevice, bool aForget = false);

		/// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "vDC virtual device controller"; }

    /// @return the entity type (one of dSD|vdSD|vDChost|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "vDC"; }

    /// @return hardware version string or NULL if none
    virtual string hardwareVersion() { return ""; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID() { return ""; }

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID() { return ""; }
    
    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // derive dSUID
    void deriveDsUid();

  };

} // namespace p44

#endif /* defined(__vdcd__deviceclasscontainer__) */
