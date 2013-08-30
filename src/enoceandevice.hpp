//
//  enoceandevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__enoceandevice__
#define __vdcd__enoceandevice__

#include "device.hpp"

#include "enoceancomm.hpp"

#include "enoceandevice.hpp"

using namespace std;

namespace p44 {

  typedef uint64_t EnoceanDeviceID;

  class EnoceanDeviceContainer;
  class EnoceanChannelHandler;
  class EnoceanDevice;

  /// EnOcean subdevice
  typedef uint8_t EnoceanSubDevice;


  typedef boost::intrusive_ptr<EnoceanChannelHandler> EnoceanChannelHandlerPtr;

  /// single enOcean device channel, abstract class
  class EnoceanChannelHandler : public P44Obj
  {
    typedef P44Obj inherited;

    friend class EnoceanDevice;

  protected:

    EnoceanDevice &device; ///< the associated enocean device

    /// private constructor
    /// @note create new channels using factory static methods of specialized subclasses
    EnoceanChannelHandler(EnoceanDevice &aDevice);

  public:

    DsBehaviourPtr behaviour; ///< the associated behaviour
    EnoceanChannel channel; ///< channel number

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr) = 0;

    /// collect data for outgoing message from this channel
    /// @param aEsp3PacketPtr must be set to a suitable packet if it is empty, or packet data must be augmented with
    ///   channel's data when packet already exists
    /// @note non-outputs will do nothing in this method
    virtual void collectOutgoingMessageData(Esp3PacketPtr &aEsp3PacketPtr) { /* NOP */ };

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc() = 0;

  };



  typedef vector<EnoceanChannelHandlerPtr> EnoceanChannelHandlerVector;

  typedef boost::intrusive_ptr<EnoceanDevice> EnoceanDevicePtr;

  /// digitalstrom device representing one or multiple enOcean device channels
  class EnoceanDevice : public Device
  {
    typedef Device inherited;

    friend class EnoceanChannelHandler;

    EnoceanAddress enoceanAddress; ///< the enocean device address
    EnoceanProfile eeProfile; ///< the EEP (RORG/FUNC/TYPE)
    EnoceanManufacturer eeManufacturer; ///< the manufacturer ID
    EnoceanSubDevice subDevice; ///< the subdevice number (relevant when one physical enOcean device is represented as multiple vdSDs)
		EnoceanSubDevice totalSubdevices; ///< number of subdevices in the physical device (of which this logical device represents one, which can have one or multiple channels)

    EnoceanChannelHandlerVector channels; ///< the channel handlers for this device

    bool alwaysUpdateable; ///< if set, device updates are sent immediately, otherwise, updates are only sent as response to a device message
    bool pendingDeviceUpdate; ///< set when update to the device is pending

  public:
    /// constructor, create device in container
    EnoceanDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanSubDevice aTotalSubdevices);

    /// get typed container reference
    EnoceanDeviceContainer &getEnoceanDeviceContainer();

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aAddress 32bit enocean device address/ID
    /// @param aSubDevice subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aNumSubdevices total number of subdevices is returned here
    /// @param aSendTeachInResponse if this is set, a teach-in response will be sent for profiles that need one
    ///   (This is set to false when re-creating logical devices from DB)
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress, EnoceanSubDevice aSubDevice,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      EnoceanSubDevice &aNumSubdevices,
      bool aSendTeachInResponse
    );

    /// add channel handler and register behaviour
    /// @param aChannelHandler a handler for a channel (including a suitable behaviour)
    void addChannelHandler(EnoceanChannelHandlerPtr aChannelHandler);

    /// disconnect device. For enOcean, this means breaking the pairing (learn-in) with the device
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   output behaviours to collect data for an outgoing message.
    virtual void updateOutputValue(OutputBehaviour &aOutputBehaviour);

    /// factory: create appropriate logical devices for a given EEP
    /// @param aClassContainerP the EnoceanDeviceContainer to create the devices in
    /// @param aLearnInPacket the packet containing the EPP and possibly other learn-in relevant information
    /// @param aNeedsTeachInResponse will be set if any of the devices created needs a teach-in response
    /// @return number of devices created
    static int createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, Esp3PacketPtr aLearnInPacket, bool &aNeedsTeachInResponse);
    

    /// set the enocean address identifying the device
    /// @param aAddress 32bit enocean device address/ID
    /// @param aChannel channel number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    virtual void setAddressingInfo(EnoceanAddress aAddress, EnoceanChannel aChannel);

    /// device and channel handler implementations can call this to enable immediate sending of output changes for the device
    /// (otherwise, output changes are sent only withing 1sec after receiving a message from the device)
    void setAlwaysUpdateable() { alwaysUpdateable = true; };

    /// get the enocean address identifying the hardware that contains this logical device
    /// @return enOcean device ID/address
    EnoceanAddress getAddress();

    /// get the enocean subdevice number that identifies this logical device among other logical devices in the same
    ///   physical enOcean device (having the same enOcean deviceID/address)
    /// @return enOcean device ID/address
    EnoceanSubDevice getSubDevice();

		/// get number of subdevices in the physical device (of which this logical device represents one subdevice)
		/// @return number of subdevices
		EnoceanChannel getTotalSubDevices();
		

    /// set EEP information
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    virtual void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer);

    /// @return RORG/FUNC/TYPE EEP profile number 
    EnoceanProfile getEEProfile();

    /// @return manufacturer code
    EnoceanManufacturer getEEManufacturer();

    /// device specific radio packet handling
    /// @note base class implementation passes packet to all registered channels
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// send outgoing packet updating outputs and device settings
    /// @note this will be called shortly after an incoming packet was received
    ///   when device updates are pending
    void sendOutgoingUpdate();

    /// device specific teach in response
    /// @note will be called from newDevice() when created device needs a teach-in response
    virtual void sendTeachInResponse() { /* NOP in base class */ };


    /// description of object, mainly for debug and logging
    /// @return manufacturer name according to EPP
    string manufacturerName();

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID();

    /// @}


  protected:

    /// derive dSID from hardware address
    void deriveDSID();

  private:

    /// get handler associated with a behaviour
    EnoceanChannelHandlerPtr channelForBehaviour(const DsBehaviour *aBehaviourP);

  };
  
} // namespace p44

#endif /* defined(__vdcd__enoceandevice__) */
