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
		EnoceanSubDevice totalSubdevices; ///< number of subdevices in the physical device (of which this logical device represents one, which can have one or multiple channels)

    string eeFunctionDesc; ///< short functional description (like: button, windowhandle, sensor...)
    const char *iconBaseName; ///< icon base name
    bool groupColoredIcon; ///< if set, use color suffix with icon base name

    EnoceanChannelHandlerVector channels; ///< the channel handlers for this device

    bool alwaysUpdateable; ///< if set, device updates are sent immediately, otherwise, updates are only sent as response to a device message
    bool pendingDeviceUpdate; ///< set when update to the device is pending

  public:

    /// constructor, create device in container
    EnoceanDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanSubDevice aTotalSubdevices);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean"; };

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
    /// @param aLearnInPacket the packet containing the EPP and possibly other learn-in relevant information
    /// @return number of devices created
    static int createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, Esp3PacketPtr aLearnInPacket);
    

    /// set the enocean address identifying the device
    /// @param aAddress 32bit enocean device address/ID
    /// @param aChannel channel number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    virtual void setAddressingInfo(EnoceanAddress aAddress, EnoceanChannel aChannel);


    /// device and channel handler implementations can call this to enable immediate sending of output changes for the device
    /// (otherwise, output changes are sent only withing 1sec after receiving a message from the device)
    void setAlwaysUpdateable() { alwaysUpdateable = true; };

    /// set the icon info for the enocean device
    void setIconInfo(const char *aIconBaseName, bool aGroupColored) { iconBaseName = aIconBaseName; groupColoredIcon = aGroupColored; };

    /// get the enocean address identifying the hardware that contains this logical device
    /// @return EnOcean device ID/address
    EnoceanAddress getAddress();

    /// get the enocean subdevice number that identifies this logical device among other logical devices in the same
    ///   physical EnOcean device (having the same EnOcean deviceID/address)
    /// @return EnOcean device ID/address
    EnoceanSubDevice getSubDevice();

		/// get number of subdevices in the physical device (of which this logical device represents one subdevice)
		/// @return number of subdevices
		EnoceanChannel getTotalSubDevices();
		

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

    /// @return model GUID in URN format to identify model of device as uniquely as possible
    virtual string modelGUID();

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

    /// derive dSUID from hardware address
    void deriveDsUid();

  private:

    /// get handler associated with a behaviour
    EnoceanChannelHandlerPtr channelForBehaviour(const DsBehaviour *aBehaviourP);

  };
  
} // namespace p44

#endif /* defined(__vdcd__enoceandevice__) */
