//
//  enoceandevice.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__enoceandevice__
#define __p44bridged__enoceandevice__

#include "device.hpp"

#include "enoceancomm.hpp"

#include "enoceandevice.hpp"

using namespace std;

namespace p44 {

  typedef uint64_t EnoceanDeviceID;

  class EnoceanDeviceContainer;
  class EnoceanDevice;
  typedef boost::shared_ptr<EnoceanDevice> EnoceanDevicePtr;
  class EnoceanDevice : public Device
  {
    typedef Device inherited;

    EnoceanAddress enoceanAddress;
    int subDeviceIndex;

  public:
    EnoceanDevice(EnoceanDeviceContainer *aClassContainerP);

    /// set the enocean address and subdevice index (identifying)
    /// @param aAddress 32bit enocean device address/ID
    void setEnoceanAddress(EnoceanAddress aAddress);

    /// get number which identifies this enocean (sub)device.
    /// @return identifier number
    EnoceanAddress getEnoceanAddress();

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    void deriveDSID();
  };
  
} // namespace p44

#endif /* defined(__p44bridged__enoceandevice__) */
