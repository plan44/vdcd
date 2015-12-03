//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__enocean4bs__
#define __vdcd__enocean4bs__

#include "vdcd_common.hpp"

#include "enoceandevice.hpp"

using namespace std;

namespace p44 {


  /// single EnOcean device channel
  class Enocean4bsHandler : public EnoceanChannelHandler
  {
    typedef EnoceanChannelHandler inherited;

  protected:

    /// protected constructor
    /// @note create new channels using factory static methods of specialized subclasses
    Enocean4bsHandler(EnoceanDevice &aDevice) : inherited(aDevice) {};

  public:

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aClassContainerP the class container
    /// @param aSubDeviceIndex subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    ///   upon exit, this will be incremented by the number of subdevice indices the device occupies in the index space
    ///   (usually 1, but some profiles might reserve extra space, such as up/down buttons)
    /// @param aEEProfile RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse enable sending teach-in response for this device
    /// @return returns NULL if no device can be created for the given aSubDeviceIndex, new device otherwise
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress,
      EnoceanSubDevice &aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aSendTeachInResponse
    );

    /// prepare aOutgoingPacket for sending 4BS data.
    /// creates new packet if none passed in, and returns already collected data
    /// @param aOutgoingPacket existing packet will be used, if NULL, new packet will be created
    /// @param a4BSdata will be set to already collected 4BS data (from already consulted channels or device global bits like LRN)
    void prepare4BSpacket(Esp3PacketPtr &aOutgoingPacket, uint32_t &a4BSdata);

  };
  typedef boost::intrusive_ptr<Enocean4bsHandler> Enocean4bsHandlerPtr;



  class Enocean4BSDevice : public EnoceanDevice
  {
    typedef EnoceanDevice inherited;

  public:

    /// constructor
    Enocean4BSDevice(EnoceanDeviceContainer *aClassContainerP);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_4bs"; };

    /// device specific teach in response
    /// @note will be called from newDevice() when created device needs a teach-in response
    virtual void sendTeachInResponse();

    /// get table of profile variants
    /// @return NULL or pointer to a list of profile variants
    virtual const ProfileVariantEntry *profileVariantsTable();

  };


  #pragma mark - handler implementations


  /// generic, table driven sensor channel handler
  struct Enocean4BSSensorDescriptor;
  class Enocean4bsSensorHandler : public Enocean4bsHandler
  {
    typedef Enocean4bsHandler inherited;

    /// private constructor, friend class' Enocean4bsHandler::newDevice is the place to call it from
    Enocean4bsSensorHandler(EnoceanDevice &aDevice);

  public:

    /// the sensor channel descriptor
    const Enocean4BSSensorDescriptor *sensorChannelDescriptorP;

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aClassContainerP the class container
    /// @param aSubDeviceIndex subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    ///   upon exit, this will be incremented by the number of subdevice indices the device occupies in the index space
    ///   (usually 1, but some profiles might reserve extra space, such as up/down buttons)
    /// @param aEEProfile VARIANT/RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse enable sending teach-in response for this device
    /// @return returns NULL if no device can be created for the given aSubDeviceIndex, new device otherwise
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress,
      EnoceanSubDevice &aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aNeedsTeachInResponse
    );


    /// factory: add sensor/binary input channel to device by descriptor
    /// @param aDevice the device to add the channel to
    /// @param aSensorDescriptor a sensor or binary input descriptor
    /// @param aSetDeviceDescription if set, this sensor channel is the "main" channel and will set description on the device itself
    static void addSensorChannel(
      EnoceanDevicePtr aDevice,
      const Enocean4BSSensorDescriptor &aSensorDescriptor,
      bool aSetDeviceDescription
    );

    /// factory: create behaviour (sensor/binary input) by descriptor
    /// @param aDevice the device to add the behaviour to
    /// @param aSensorDescriptor a sensor or binary input descriptor
    /// @return the behaviour
    static DsBehaviourPtr newSensorBehaviour(const Enocean4BSSensorDescriptor &aSensorDescriptor, DevicePtr aDevice);

    /// utility: get description string from sensor descriptor info
    static string sensorDesc(const Enocean4BSSensorDescriptor &aSensorDescriptor);

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// check if channel is alive = has received life sign within timeout window
    virtual bool isAlive();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  };
  typedef boost::intrusive_ptr<Enocean4bsSensorHandler> Enocean4bsSensorHandlerPtr;


  /// heating valve handler
  class EnoceanA52001Handler : public Enocean4bsHandler
  {
    typedef Enocean4bsHandler inherited;
    friend class Enocean4bsHandler;

    enum {
      service_idle,
      service_openvalve,
      service_closevalve,
      service_finish
    } serviceState;

    int8_t lastValvePos; ///< last calculated valve position (used to calculate output for binary valves like MD10-FTL)

    /// private constructor, friend class' Enocean4bsHandler::newDevice is the place to call it from
    EnoceanA52001Handler(EnoceanDevice &aDevice);

  public:

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aClassContainerP the class container
    /// @param aSubDeviceIndex subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    ///   upon exit, this will be incremented by the number of subdevice indices the device occupies in the index space
    ///   (usually 1, but some profiles might reserve extra space, such as up/down buttons)
    /// @param aEEProfile VARIANT/RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse enable sending teach-in response for this device
    /// @return returns NULL if no device can be created for the given aSubDeviceIndex, new device otherwise
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress,
      EnoceanSubDevice &aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
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



  /// heating valve handler
  class EnoceanA5130XHandler : public Enocean4bsHandler
  {
    typedef Enocean4bsHandler inherited;
    friend class Enocean4bsHandler;

    // behaviours for extra sensors
    // Note: using base class' behaviour pointer for first sensor = dawn sensor
    DsBehaviourPtr outdoorTemp;
    DsBehaviourPtr windSpeed;
    DsBehaviourPtr dayIndicator;
    DsBehaviourPtr rainIndicator;
    DsBehaviourPtr sunWest;
    DsBehaviourPtr sunSouth;
    DsBehaviourPtr sunEast;

    /// private constructor, friend class' Enocean4bsHandler::newDevice is the place to call it from
    EnoceanA5130XHandler(EnoceanDevice &aDevice);

  public:

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aClassContainerP the class container
    /// @param aSubDeviceIndex subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    ///   upon exit, this will be incremented by the number of subdevice indices the device occupies in the index space
    ///   (usually 1, but some profiles might reserve extra space, such as up/down buttons)
    /// @param aEEProfile VARIANT/RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse enable sending teach-in response for this device
    /// @return returns NULL if no device can be created for the given aSubDeviceIndex, new device otherwise
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress,
      EnoceanSubDevice &aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aSendTeachInResponse
    );

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();
  };
  typedef boost::intrusive_ptr<EnoceanA5130XHandler> EnoceanA5130XHandlerPtr;



} // namespace p44

#endif /* defined(__vdcd__enocean4bs__) */
