//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __vdcd__enoceandevice__
#define __vdcd__enoceandevice__

#include "device.hpp"

#include "enoceancomm.hpp"


using namespace std;

namespace p44 {

  typedef uint64_t EnoceanDeviceID;

  class EnoceanDeviceContainer;
  class EnoceanChannelHandler;
  class EnoceanDevice;

  /// EnOcean subdevice
  typedef uint8_t EnoceanSubDevice;


  typedef boost::intrusive_ptr<EnoceanChannelHandler> EnoceanChannelHandlerPtr;

  /// single EnOcean device channel, abstract class
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
    int8_t dsChannelIndex; ///< for outputs, the dS channel index
    EnoceanChannel channel; ///< channel number

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr) = 0;

    /// collect data for outgoing message from this channel
    /// @param aEsp3PacketPtr must be set to a suitable packet if it is empty, or packet data must be augmented with
    ///   channel's data when packet already exists
    /// @note non-outputs will do nothing in this method
    virtual void collectOutgoingMessageData(Esp3PacketPtr &aEsp3PacketPtr) { /* NOP */ };

    /// check if channel is alive (for regularily sending sensors: has received life sign within timeout window)
    virtual bool isAlive() { return true; } // assume alive by default

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc() = 0;

  };



  typedef vector<EnoceanChannelHandlerPtr> EnoceanChannelHandlerVector;

  typedef boost::intrusive_ptr<EnoceanDevice> EnoceanDevicePtr;

  /// digitalstrom device representing one or multiple EnOcean device channels
  class EnoceanDevice : public Device
  {
    typedef Device inherited;

    friend class EnoceanChannelHandler;

    EnoceanAddress enoceanAddress; ///< the enocean device address
    EnoceanProfile eeProfile; ///< the EEP (RORG/FUNC/TYPE)
    EnoceanManufacturer eeManufacturer; ///< the manufacturer ID
    EnoceanSubDevice subDevice; ///< the subdevice number (relevant when one physical EnOcean device is represented as multiple vdSDs)

    string eeFunctionDesc; ///< short functional description (like: button, windowhandle, sensor...)
    const char *iconBaseName; ///< icon base name
    bool groupColoredIcon; ///< if set, use color suffix with icon base name

    EnoceanChannelHandlerVector channels; ///< the channel handlers for this device

    bool alwaysUpdateable; ///< if set, device updates are sent immediately, otherwise, updates are only sent as response to a device message
    bool updateAtEveryReceive; ///< if set, current values are sent to the device whenever a message is received, even if output state has not changed
    bool pendingDeviceUpdate; ///< set when update to the device is pending

    MLMicroSeconds lastPacketTime; ///< time when device received last packet (or device was created)
    int16_t lastRSSI; ///< RSSI of last packet received
    uint8_t lastRepeaterCount; ///< last packet's repeater count

  public:

    /// constructor, create device in container
    EnoceanDevice(EnoceanDeviceContainer *aClassContainerP);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean"; };

    /// check if device can be disconnected by software (i.e. Web-UI)
    /// @return true if device might be disconnectable by the user via software (i.e. web UI)
    /// @note EnOcean devices can be removed not only via unlearning, but also via Web-UI if needed
    virtual bool isSoftwareDisconnectable() { return true; };

    /// return time when last packet was received for this device
    /// @return time when last packet was received or Never
    MLMicroSeconds getLastPacketTime() { return lastPacketTime; };

    /// check presence of this addressable
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// get typed container reference
    EnoceanDeviceContainer &getEnoceanDeviceContainer();

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aAddress 32bit enocean device address/ID
    /// @param aSubDeviceIndex subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse if this is set, a teach-in response will be sent for profiles that need one
    ///   (This is set to false when re-creating logical devices from DB)
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress,
      EnoceanSubDevice aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aSendTeachInResponse
    );

    /// add channel handler and register behaviour
    /// @param aChannelHandler a handler for a channel (including a suitable behaviour)
    void addChannelHandler(EnoceanChannelHandlerPtr aChannelHandler);

    /// disconnect device. For EnOcean, this means breaking the pairing (learn-in) with the device
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @param aDoneCB if not NULL, must be called when values are applied
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    virtual void applyChannelValues(DoneCB aDoneCB, bool aForDimming);

    /// factory: create appropriate logical devices for a given EEP
    /// @param aClassContainerP the EnoceanDeviceContainer to create the devices in
    /// @param aAddress the EnOcean address
    /// @param aProfile the EPP
    /// @param aManufacturer the manufacturer code
    /// @return number of devices created
    static int createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, EnoceanAddress aAddress, EnoceanProfile aProfile, EnoceanManufacturer aManufacturer);
    

    /// set the enocean address identifying the device
    /// @param aAddress 32bit enocean device address/ID
    /// @param aChannel channel number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    virtual void setAddressingInfo(EnoceanAddress aAddress, EnoceanChannel aChannel);


    /// device and channel handler implementations can call this to enable immediate sending of output changes for the device
    /// (otherwise, output changes are sent only withing 1sec after receiving a message from the device)
    void setAlwaysUpdateable(bool aAlwaysUpdateable = true) { alwaysUpdateable = aAlwaysUpdateable; };

    /// device and channel handler implementations can call this to enable immediate sending of output changes for the device
    /// (otherwise, output changes are sent only withing 1sec after receiving a message from the device)
    void setUpdateAtEveryReceive(bool aUpdateAtEveryReceive = true) { updateAtEveryReceive = aUpdateAtEveryReceive; };


    /// set the icon info for the enocean device
    void setIconInfo(const char *aIconBaseName, bool aGroupColored) { iconBaseName = aIconBaseName; groupColoredIcon = aGroupColored; };

    /// get the enocean address identifying the hardware that contains this logical device
    /// @return EnOcean device ID/address
    EnoceanAddress getAddress();

    /// get the enocean subdevice number that identifies this logical device among other logical devices in the same
    ///   physical EnOcean device (having the same EnOcean deviceID/address)
    /// @return EnOcean device ID/address
    EnoceanSubDevice getSubDevice();

    /// set EEP information
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    virtual void setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer);


    /// set short functional description for this device (explaining the EEP in short, like "button", "sensor", "window handle")
    /// @param aString the description string
    void setFunctionDesc(string aString) { eeFunctionDesc = aString; };


    /// @return RORG/FUNC/TYPE EEP profile number 
    EnoceanProfile getEEProfile();

    /// @return manufacturer code
    EnoceanManufacturer getEEManufacturer();

    /// device specific radio packet handling
    /// @note base class implementation passes packet to all registered channels
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// signal that we need an outgoing packet at next possible occasion
    /// @note will cause output data from channel handlers to be collected
    /// @note can be called from channel handlers to trigger another update after the current one
    void needOutgoingUpdate();

    /// send outgoing packet updating outputs and device settings
    /// @note do not call this directly, use needOutgoingUpdate() instead to make
    ///   sure outgoing package is sent at appropriate time for device (e.g. just after receiving for battery powered devices)
    void sendOutgoingUpdate();

    /// device specific teach in response
    /// @note will be called from newDevice() when created device needs a teach-in response
    virtual void sendTeachInResponse() { /* NOP in base class */ };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// get profile variants this device can have
    /// @param aApiObjectValue must be an object typed API value, will receive profile variants as EEP/description key/values
    /// @return true if device has variants
    virtual bool getProfileVariants(ApiValuePtr aApiObjectValue) { return false; /* none in base class */ };

    /// @param aProfile must be an EEP profile code
    /// @return true if profile variant is valid and can be set
    virtual bool setProfileVariant(EnoceanProfile aProfile) { return false; /* profile not changeable in base class */ };


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    virtual string hardwareGUID();

    /// @return model GUID in URN format to identify model of hardware device as uniquely as possible
    virtual string hardwareModelGUID();

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    virtual string vendorId();

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// @}


  protected:

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    /// derive dSUID from hardware address
    void deriveDsUid();

    /// switch EEP profile
    void switchToProfile(EnoceanProfile aProfile);

  private:

    /// get handler associated with a behaviour
    EnoceanChannelHandlerPtr channelForBehaviour(const DsBehaviour *aBehaviourP);

  };
  
} // namespace p44

#endif /* defined(__vdcd__enoceandevice__) */
