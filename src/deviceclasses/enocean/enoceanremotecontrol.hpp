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

#ifndef __vdcd__enoceanremotecontrol__
#define __vdcd__enoceanremotecontrol__

#include "vdcd_common.hpp"

#include "enoceandevice.hpp"


using namespace std;

namespace p44 {

  // pseudo-RORG used in this implementation to identify "remote control" devices, i.e. those that use local baseID to send out actions
  #define PSEUDO_RORG_REMOTECONTROL 0xFF
  #define PSEUDO_FUNC_SWITCHCONTROL 0xF6
  #define PSEUDO_TYPE_SIMPLEBLIND 0xFF // simplistic Fully-Up/Fully-Down blind controller
  #define PSEUDO_TYPE_BLIND 0xFE // time controlled blind with angle support

  /// remote control type channel (using base id to communicate with actor)
  class EnoceanRemoteControlHandler : public EnoceanChannelHandler
  {
    typedef EnoceanChannelHandler inherited;

  protected:
  
    /// private constructor, create new channels using factory static method
    EnoceanRemoteControlHandler(EnoceanDevice &aDevice);

  public:

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aClassContainerP the class container
    /// @param aSubDeviceIndex subdevice number to create (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    /// @param aEEProfile VARIANT/RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse enable sending teach-in response for this device
    /// @return returns NULL if no device can be created for the given aSubDeviceIndex, new device otherwise
    static EnoceanDevicePtr newDevice(
      EnoceanDeviceContainer *aClassContainerP,
      EnoceanAddress aAddress,
      EnoceanSubDevice aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aNeedsTeachInResponse
    );

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr) { /* no responses by default */ };

  };
  typedef boost::intrusive_ptr<EnoceanRemoteControlHandler> EnoceanRemoteControlHandlerPtr;



  class EnoceanRemoteControlDevice : public EnoceanDevice
  {
    typedef EnoceanDevice inherited;

  public:

    /// constructor
    /// @param aDsuidIndexStep step between dSUID subdevice indices (default is 1, historically 2 for dual 2-way rocker switches)
    EnoceanRemoteControlDevice(EnoceanDeviceContainer *aClassContainerP, uint8_t aDsuidIndexStep = 1);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_remotecontrol"; };

    /// device specific teach in signal
    /// @note will be called via UI for devices that need to be learned into remote actors
    virtual bool sendTeachInSignal();

    /// mark base offsets in use by this device
    /// @param aUsedOffsetsMap must be passed a string with 128 chars of '0' or '1'.
    virtual void markUsedBaseOffsets(string &aUsedOffsetsMap);

  private:

    void sendSwitchBeaconRelease();

  };


  class EnoceanBlindControlDevice : public EnoceanRemoteControlDevice
  {
    typedef EnoceanRemoteControlDevice inherited;

    int movingDirection; ///< currently moving direction 0=stopped, -1=moving down, +1=moving up
    long commandTicket;
    bool missedUpdate;

  public:

    /// constructor
    /// @param aDsuidIndexStep step between dSUID subdevice indices (default is 1, historically 2 for dual 2-way rocker switches)
    EnoceanBlindControlDevice(EnoceanDeviceContainer *aClassContainerP, uint8_t aDsuidIndexStep = 1);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_blind"; };

    /// sync channel values (with time-derived estimates of current blind position
    virtual void syncChannelValues(SimpleCB aDoneCB);

    /// apply channel values
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

  private:

    void changeMovement(SimpleCB aDoneCB, int aNewDirection);
    void sendReleaseTelegram(SimpleCB aDoneCB);
    void buttonAction(bool aBlindUp, bool aPress);

  };




}

#endif /* defined(__vdcd__enoceanremotecontrol__) */
