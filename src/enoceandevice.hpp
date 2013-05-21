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
    EnoceanProfile eeProfile;
    EnoceanManufacturer eeManufacturer;
    

  public:
    EnoceanDevice(EnoceanDeviceContainer *aClassContainerP);

    /// set the enocean address identifying the device
    /// @param aAddress 32bit enocean device address/ID
    void setEnoceanAddress(EnoceanAddress aAddress);

    /// set the enocean address identifying the device
    /// @return enOcean device ID/address
    EnoceanAddress getEnoceanAddress();

    /// set EEP information
    /// @param aEEPProfileNumber RORG/FUNC/TYPE EEP profile number
    /// @param aManNumber manufacturer number (or manufacturer_unknown)
    void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer);

    /// @return RORG/FUNC/TYPE EEP profile number 
    EnoceanProfile getEEProfile();

    /// @return eanufacturer code
    EnoceanManufacturer getEEManufacturer();


    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    void deriveDSID();
  };
  
} // namespace p44

#endif /* defined(__p44bridged__enoceandevice__) */
