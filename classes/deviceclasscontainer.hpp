//
//  deviceclasscontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__deviceclasscontainer__
#define __p44bridged__deviceclasscontainer__

#include "p44bridged_common.hpp"

#include "devicecontainer.hpp"

class Device;
class dSID;

typedef boost::shared_ptr<Device> DevicePtr;
typedef std::map<dSID, DevicePtr> DeviceMap;


class DeviceClassContainer;
typedef boost::shared_ptr<DeviceClassContainer> DeviceClassContainerPtr;
typedef boost::weak_ptr<DeviceClassContainer> DeviceClassContainerWeakPtr;
class DeviceClassContainer
{
  DeviceContainer *deviceContainerP; ///< link to the deviceContainer
  DeviceMap devices; ///< the devices of this class, mapped by dsid
public:
  DeviceClassContainer();

  /// associate with container
  /// @param aDeviceContainerP device container this device class is contained in
  void setDeviceContainer(DeviceContainer *aDeviceContainerP);
  /// get associated container
  /// @return associated device container
  DeviceContainer *getDeviceContainerP() const;


  /// @name identification
  /// @{

  /// deviceclass identifier
  virtual const char *deviceClassIdentifier() const = 0;

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

  /// collect devices from all device classes
  /// @param aCompletedCB will be called when device scan for this device class has been completed
  virtual void collectDevices(CompletedCB aCompletedCB) = 0;

  /// @}

protected:

  /// Add collected device
  /// @param aDevice a device object which has a valid dsid
  virtual void addCollectedDevice(DevicePtr aDevice);


};



#endif /* defined(__p44bridged__deviceclasscontainer__) */
