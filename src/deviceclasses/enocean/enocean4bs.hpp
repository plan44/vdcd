//
//  enocean4bs.hpp
//  vdcd
//
//  Created by Lukas Zeller on 26.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__enocean4bs__
#define __vdcd__enocean4bs__

#include "vdcd_common.hpp"

#include "enoceandevice.hpp"


using namespace std;

namespace p44 {


  struct Enocean4BSDescriptor;

  /// single enOcean device channel
  class Enocean4bsHandler : public EnoceanChannelHandler
  {
    typedef EnoceanChannelHandler inherited;

    /// private constructor, create new channels using factory static method
    Enocean4bsHandler(EnoceanDevice &aDevice);

  public:

    /// the channel descriptor
    const Enocean4BSDescriptor *channelDescriptorP;

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
      EnoceanSubDevice &aNumSubdevices,
      bool aSendTeachInResponse
    );

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// collect data for outgoing message from this channel
    /// @param aEsp3PacketPtr must be set to a suitable packet if it is empty, or packet data must be augmented with
    ///   channel's data when packet already exists
    virtual void collectOutgoingMessageData(Esp3PacketPtr &aEsp3PacketPtr);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  };
  typedef boost::intrusive_ptr<Enocean4bsHandler> Enocean4bsHandlerPtr;



  class Enocean4BSDevice : public EnoceanDevice
  {
    typedef EnoceanDevice inherited;

  public:

    /// constructor
    Enocean4BSDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanSubDevice aTotalSubdevices) : inherited(aClassContainerP, aTotalSubdevices) {};

    /// device specific teach in response
    /// @note will be called from newDevice() when created device needs a teach-in response
    virtual void sendTeachInResponse();

  };


} // namespace p44

#endif /* defined(__vdcd__enocean4bs__) */
