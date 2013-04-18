//
//  device.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__device__
#define __p44bridged__device__

#include "deviceclasscontainer.hpp"

#include "dsid.hpp"

class Device
{
  bool registered; ///< set when registered by dS system
  bool connected; ///< set when physical device is connected (known to be present on the bus etc.)
protected:
  DeviceClassContainerWeakPtr classContainer;
public:
  Device();

  /// the digitalstrom ID
  dSID dsid;
};

#endif /* defined(__p44bridged__device__) */
