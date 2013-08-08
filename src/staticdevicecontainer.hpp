//
//  staticdevicecontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__staticdevicecontainer__
#define __vdcd__staticdevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {

	typedef std::multimap<string, string> DeviceConfigMap;
	
  class StaticDeviceContainer;
  typedef boost::shared_ptr<StaticDeviceContainer> StaticDeviceContainerPtr;
  class StaticDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;
		DeviceConfigMap deviceConfigs;
  public:
    StaticDeviceContainer(int aInstanceNumber, DeviceConfigMap aDeviceConfigs);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

  };

} // namespace p44


#endif /* defined(__vdcd__staticdevicecontainer__) */
