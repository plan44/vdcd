//
//  devicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "devicecontainer.hpp"





void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers.push_back(aDeviceClassContainerPtr);
}
