//
//  demodevicecontainer.hpp
//  vdcd
//
//  Created by Lukas Zeller on 2013-11-11
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__demodevicecontainer__
#define __vdcd__demodevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {

  class DemoDeviceContainer;
  typedef boost::intrusive_ptr<DemoDeviceContainer> DemoDeviceContainerPtr;
  class DemoDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;

  public:
    DemoDeviceContainer(int aInstanceNumber);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

  };

} // namespace p44


#endif /* defined(__vdcd__demodevicecontainer__) */
