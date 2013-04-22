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


class DaliDeviceContainer;
class DaliDevice;
typedef boost::shared_ptr<DaliDevice> DaliDevicePtr;
class DaliDevice : public Device
{
  typedef Device inherited;

  // the device info
  DaliDeviceInfo deviceInfo;

public:
  DaliDevice(DaliDeviceContainer *aClassContainerP);
  
  void setDeviceInfo(DaliDeviceInfo aDeviceInfo);

  /// description of object, mainly for debug and logging
  /// @return textual description of object
  virtual string description();

protected:

  void deriveDSID();
};


#endif /* defined(__p44bridged__dalidevice__) */
