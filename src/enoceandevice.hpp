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
    EnoceanChannel channel;

  public:
    /// constructor, create device in container
    EnoceanDevice(EnoceanDeviceContainer *aClassContainerP);

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aAddress 32bit enocean device address/ID
    /// @param aChannel channel number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aNumChannels if not NULL, total number of channels is returned here
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress, EnoceanChannel aChannel,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      int *aNumChannelsP = NULL
    );

    /// factory: create appropriate logical devices for a given EEP
    /// @param aClassContainerP the EnoceanDeviceContainer to create the devices in
    /// @param aLearnInPacket the packet containing the EPP and possibly other learn-in relevant information
    /// @return number of devices created
    static int createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, Esp3PacketPtr aLearnInPacket);


    /// set the enocean address identifying the device
    /// @param aAddress 32bit enocean device address/ID
    /// @param aChannel channel number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    void setAddressingInfo(EnoceanAddress aAddress, EnoceanChannel aChannel);

    /// get the enocean address identifying the hardware that contains this logical device
    /// @return enOcean device ID/address
    EnoceanAddress getAddress();

    /// get the enocean channel that identifies this logical device among other logical devices in the same
    ///   physical enOcean device (having the same enOcean deviceID/adress
    /// @return enOcean device ID/address
    EnoceanChannel getChannel();


    /// set EEP information
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    virtual void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer);

    /// @return RORG/FUNC/TYPE EEP profile number 
    EnoceanProfile getEEProfile();

    /// @return manufacturer code
    EnoceanManufacturer getEEManufacturer();

    /// device specific radio packet handling
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr) = 0;

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    void deriveDSID();
  };
  
} // namespace p44

#endif /* defined(__p44bridged__enoceandevice__) */
