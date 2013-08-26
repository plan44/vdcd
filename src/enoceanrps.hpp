//
//  enoceanrps.hpp
//  vdcd
//
//  Created by Lukas Zeller on 26.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__enoceanrps__
#define __vdcd__enoceanrps__

#include "vdcd_common.hpp"

#include "enoceandevice.hpp"


using namespace std;

namespace p44 {

  /// single enOcean device channel
  class EnoceanRpsHandler : public EnoceanChannelHandler
  {
    typedef EnoceanChannelHandler inherited;

    /// private constructor, create new channels using factory static method
    EnoceanRpsHandler(EnoceanDevice &aDevice);

    bool pressed; ///< true if currently pressed, false if released, index: 0=on/down button, 1=off/up button
    int switchIndex; ///< which switch within the device (0..3)


  public:

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aClassContainerP the class container
    /// @param aSubDevice subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aNumSubdevicesP if not NULL, total number of subdevices is returned here
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress, EnoceanSubDevice aSubDevice,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      EnoceanSubDevice *aNumSubdevicesP
    );

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  private:
    void setButtonState(bool aPressed);

  };
  typedef boost::intrusive_ptr<EnoceanRpsHandler> EnoceanRpsHandlerPtr;

}

#endif /* defined(__vdcd__enoceanrps__) */
