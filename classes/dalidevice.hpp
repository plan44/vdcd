//
//  dalidevice.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__dalidevice__
#define __p44bridged__dalidevice__

#include "device.hpp"

#include "dalicomm.hpp"


class DaliDevice : public Device
{
  typedef Device inherited;

  // the device info
  DaliDeviceInfo deviceInfo;

public:
  DaliDevice();
  
  void setDeviceInfo(DaliDeviceInfo aDeviceInfo);

  void deriveDSID();

};


#endif /* defined(__p44bridged__dalidevice__) */
