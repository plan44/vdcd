//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__enoceansensorhandler__
#define __vdcd__enoceansensorhandler__

#include "vdcd_common.hpp"

#if ENABLE_ENOCEAN

#include "enoceandevice.hpp"

/// enocean bit specification to bit number macro
#define DB(byte,bit) (byte*8+bit)
/// enocean bit specification to bit mask macro
#define DBMASK(byte,bit) ((uint32_t)1<<DB(byte,bit))

using namespace std;

namespace p44 {

  struct EnoceanSensorDescriptor;

  /// decoder function
  /// @param aDescriptor descriptor for data to extract
  /// @param aBehaviour the beehaviour that will receive the extracted value
  /// @param aDataP pointer to data, MSB comes first, LSB comes last (for 4BS data: MSB=enocean DB_3, LSB=enocean DB_0)
  /// @param aDataSize number of data bytes
  typedef void (*BitFieldHandlerFunc)(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize);

  /// enocean sensor value descriptor
  typedef struct EnoceanSensorDescriptor {
    uint8_t variant; ///< the variant from the EEP signature
    uint8_t func; ///< the function code from the EPP signature
    uint8_t type; ///< the type code from the EPP signature
    uint8_t subDevice; ///< subdevice index, in case EnOcean device needs to be split into multiple logical vdSDs
    DsGroup primaryGroup; ///< the dS group for the entire device
    DsGroup channelGroup; ///< the dS group for this channel
    BehaviourType behaviourType; ///< the behaviour type
    uint8_t behaviourParam; ///< DsSensorType, DsBinaryInputType or DsOutputFunction resp., depending on behaviourType
    DsUsageHint usage; ///< usage hint
    float min; ///< min value
    float max; ///< max value
    uint8_t msBit; ///< most significant bit of sensor value field in data (for 4BS: 31=Bit7 of DB_3, 0=Bit0 of DB_0)
    uint8_t lsBit; ///< least significant bit of sensor value field in data (for 4BS: 31=Bit7 of DB_3, 0=Bit0 of DB_0)
    double updateInterval; ///< normal update interval (average time resolution) in seconds
    double aliveSignInterval; ///< maximum interval between two reports of a sensor. If sensor does not push a value for longer than that, it should be considered out-of-order
    BitFieldHandlerFunc bitFieldHandler; ///< function used to convert between bit field in telegram and engineering value for the behaviour
    const char *typeText;
    const char *unitText;
  } EnoceanSensorDescriptor;


  /// @name functions and texts for use in EnoceanSensorDescriptor table entries
  /// @{

  namespace EnoceanSensors {

    void handleBitField(const EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize);
    uint64_t bitsExtractor(const struct EnoceanSensorDescriptor &aSensorDescriptor, uint8_t *aDataP, int aDataSize);

    void stdSensorHandler(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize);
    void invSensorHandler(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize);

    void stdInputHandler(const struct EnoceanSensorDescriptor &aSensorDescriptor, DsBehaviourPtr aBehaviour, uint8_t *aDataP, int aDataSize);

    // texts
    extern const char *tempText;
    extern const char *tempSetPt;
    extern const char *tempUnit;
    extern const char *humText;
    extern const char *humUnit;
    extern const char *illumText;
    extern const char *illumUnit;
    extern const char *occupText;
    extern const char *motionText;
    extern const char *unityUnit;
    extern const char *binaryUnit;
    extern const char *setPointText;
    extern const char *fanSpeedText;
    extern const char *dayNightText;
    extern const char *contactText;

  }

  /// @}


  /// generic, table driven sensor channel handler
  class EnoceanSensorHandler : public EnoceanChannelHandler
  {
    typedef EnoceanChannelHandler inherited;

  protected:

    /// protected constructor, static factory function newDevice of derived class is the place to call it from
    EnoceanSensorHandler(EnoceanDevice &aDevice);

  public:

    /// device creator function
    typedef EnoceanDevicePtr (*CreateDeviceFunc)(EnoceanDeviceContainer *aClassContainerP);

    /// the sensor channel descriptor
    const EnoceanSensorDescriptor *sensorChannelDescriptorP;

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
      CreateDeviceFunc aCreateDeviceFunc,
      const EnoceanSensorDescriptor *aDescriptorTable,
      EnoceanAddress aAddress,
      EnoceanSubDevice &aSubDeviceIndex, // current subdeviceindex, factory returns NULL when no device can be created for this subdevice index
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aSendTeachInResponse
    );


    /// factory: add sensor/binary input channel to device by descriptor
    /// @param aDevice the device to add the channel to
    /// @param aSensorDescriptor a sensor or binary input descriptor
    /// @param aSetDeviceDescription if set, this sensor channel is the "main" channel and will set description on the device itself
    static void addSensorChannel(
      EnoceanDevicePtr aDevice,
      const EnoceanSensorDescriptor &aSensorDescriptor,
      bool aSetDeviceDescription
    );

    /// factory: create behaviour (sensor/binary input) by descriptor
    /// @param aDevice the device to add the behaviour to
    /// @param aSensorDescriptor a sensor or binary input descriptor
    /// @return the behaviour
    static DsBehaviourPtr newSensorBehaviour(const EnoceanSensorDescriptor &aSensorDescriptor, DevicePtr aDevice);

    /// utility: get description string from sensor descriptor info
    static string sensorDesc(const EnoceanSensorDescriptor &aSensorDescriptor);

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// check if channel is alive = has received life sign within timeout window
    virtual bool isAlive();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  };
  typedef boost::intrusive_ptr<EnoceanSensorHandler> EnoceanSensorHandlerPtr;


} // namespace p44

#endif // ENABLE_ENOCEAN
#endif // __vdcd__enoceansensorhandler__
