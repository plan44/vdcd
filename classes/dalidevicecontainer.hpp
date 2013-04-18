//
//  dalidevicecontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__dalidevicecontainer__
#define __p44bridged__dalidevicecontainer__

#include "p44bridged_common.hpp"

#include "deviceclasscontainer.hpp"

#include "dalicomm.hpp"

class DaliDeviceContainer : public DeviceClassContainer
{
  typedef DeviceClassContainer inherited;

public:
  // the DALI communication object
  DaliComm daliComm;

  virtual const char *deviceClassIdentifier();

  virtual void collectDevices(CompletedCB aCompletedCB);

};


#endif /* defined(__p44bridged__dalidevicecontainer__) */
