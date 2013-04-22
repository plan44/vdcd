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

#include "dsid.hpp"

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

  /// collect devices from this device classes
  /// @param aCompletedCB will be called when device scan for this device class has been completed
  virtual void collectDevices(CompletedCB aCompletedCB) = 0;

  /// @}


  /// Add device collected from hardware side (bus scan, etc.)
  /// @param aDevice a device object which has a valid dsid
  virtual void addCollectedDevice(DevicePtr aDevice);

  /// description of object, mainly for debug and logging
  /// @return textual description of object
  virtual string description();

};



#endif /* defined(__p44bridged__deviceclasscontainer__) */
