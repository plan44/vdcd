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

#include "deviceclasscontainer.hpp"

using namespace std;


/// container for all devices hosted by this application
/// - is the connection point to a vDSM
/// - contains one or multiple device class containers
///   (each representing a specific class of devices, e.g. different bus types etc.)
class DeviceContainer;
typedef boost::shared_ptr<DeviceContainer> DeviceContainerPtr;
class DeviceContainer
{
  list<DeviceClassContainerPtr> deviceClassContainers;
public:
  /// add a device class container
  /// @param aDeviceClassContainerPtr a shared_ptr to a device class container
  void addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr);
};

#endif /* defined(__p44bridged__devicecontainer__) */
