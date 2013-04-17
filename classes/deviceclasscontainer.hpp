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

class DeviceContainer;

class DeviceClassContainer;
typedef boost::shared_ptr<DeviceClassContainer> DeviceClassContainerPtr;
class DeviceClassContainer
{
  DeviceContainer *deviceContainerP;
public:

};



#endif /* defined(__p44bridged__deviceclasscontainer__) */
