//
//  dalidevicecontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__dalidevicecontainer__
#define __p44bridged__dalidevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"

#include "dalicomm.hpp"

using namespace std;

namespace p44 {

  class DaliDeviceContainer;
  typedef boost::shared_ptr<DaliDeviceContainer> DaliDeviceContainerPtr;
  class DaliDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
  public:
    DaliDeviceContainer(int aInstanceNumber);

    // the DALI communication object
    DaliComm daliComm;

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

  };

} // namespace p44


#endif /* defined(__p44bridged__dalidevicecontainer__) */
