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

using namespace std;

namespace p44 {

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

    DaliDeviceContainer *daliDeviceContainerP();

    void setDeviceInfo(DaliDeviceInfo aDeviceInfo);

    /// "pings" the device. Device should respond by sending back a "pong" shortly after
    virtual void ping();

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    void deriveDSID();

  private:

    void pingResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__p44bridged__dalidevice__) */
