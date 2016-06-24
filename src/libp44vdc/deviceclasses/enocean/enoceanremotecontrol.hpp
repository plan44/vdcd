//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#if ENABLE_ENOCEAN

#include "enoceandevice.hpp"


using namespace std;

namespace p44 {

  // pseudo-RORG used in this implementation to identify "remote control" devices, i.e. those that use local baseID to send out actions
  #define PSEUDO_RORG_REMOTECONTROL 0xFF
  #define PSEUDO_FUNC_SWITCHCONTROL 0xF6
  #define PSEUDO_TYPE_SIMPLEBLIND 0xFF // simplistic Fully-Up/Fully-Down blind controller
  #define PSEUDO_TYPE_BLIND 0xFE // time controlled blind with angle support
  #define PSEUDO_TYPE_ON_OFF 0xFD // simple relay switched on by key up and switched off by key down
  #define PSEUDO_TYPE_SWITCHED_LIGHT 0xFC // switched light (with full light behaviour)


  class EnoceanRemoteControlDevice : public EnoceanDevice
  {
    typedef EnoceanDevice inherited;

  public:

    /// constructor
    /// @param aDsuidIndexStep step between dSUID subdevice indices (default is 1, historically 2 for dual 2-way rocker switches)
    EnoceanRemoteControlDevice(EnoceanVdc *aVdcP, uint8_t aDsuidIndexStep = 1);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_remotecontrol"; };

    /// @param aVariant -1 to just get number of available teach-in variants. 0..n to send teach-in signal;
    ///   some devices may have different teach-in signals (like: one for ON, one for OFF).
    /// @return number of teach-in signal variants the device can send
    /// @note will be called via UI for devices that need to be learned into remote actors
    virtual uint8_t teachInSignal(int8_t aVariant);

    /// mark base offsets in use by this device
    /// @param aUsedOffsetsMap must be passed a string with 128 chars of '0' or '1'.
    virtual void markUsedBaseOffsets(string &aUsedOffsetsMap);

    /// factory: (re-)create logical device from address|channel|profile|manufacturer tuple
    /// @param aVdcP the class container
    /// @param aSubDeviceIndex subdevice number (multiple logical EnoceanDevices might exists for the same EnoceanAddress)
    ///   upon exit, this will be incremented by the number of subdevice indices the device occupies in the index space
    ///   (usually 1, but some profiles might reserve extra space, such as up/down buttons)
    /// @param aEEProfile VARIANT/RORG/FUNC/TYPE EEP profile number
    /// @param aEEManufacturer manufacturer number (or manufacturer_unknown)
    /// @param aSendTeachInResponse enable sending teach-in response for this device
    /// @return returns NULL if no device can be created for the given aSubDeviceIndex, new device otherwise
    static EnoceanDevicePtr newDevice(
      EnoceanVdc *aVdcP,
      EnoceanAddress aAddress,
      EnoceanSubDevice &aSubDeviceIndex,
      EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
      bool aNeedsTeachInResponse
    );

  protected:

    /// utility function to send button action telegrams
    /// @param aRight: right (B) button instead of left (A) button
    /// @param aUp: up instead of down button
    /// @param aPress: pressing button instead of releasing it
    void buttonAction(bool aRight, bool aUp, bool aPress);

  private:

    void sendSwitchBeaconRelease(bool aRight, bool aUp);

  };


  class EnoceanRelayControlDevice : public EnoceanRemoteControlDevice
  {
    typedef EnoceanRemoteControlDevice inherited;

    int movingDirection; ///< currently moving direction 0=stopped, -1=moving down, +1=moving up
    long commandTicket;
    bool missedUpdate;

  public:

    /// constructor
    /// @param aDsuidIndexStep step between dSUID subdevice indices (default is 1, historically 2 for dual 2-way rocker switches)
    EnoceanRelayControlDevice(EnoceanVdc *aVdcP, uint8_t aDsuidIndexStep = 1);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_relay"; };

    /// apply channel values
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

  private:

    void sendReleaseTelegram(SimpleCB aDoneCB, bool aUp);

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
    EnoceanBlindControlDevice(EnoceanVdc *aVdcP, uint8_t aDsuidIndexStep = 1);

    /// device type identifier
    /// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_blind"; };

    /// sync channel values (with time-derived estimates of current blind position
    virtual void syncChannelValues(SimpleCB aDoneCB);

    /// apply channel values
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming);

    /// start or stop dimming (optimized blind controller version)
    virtual void dimChannel(DsChannelType aChannelType, DsDimMode aDimMode);

  private:

    void changeMovement(SimpleCB aDoneCB, int aNewDirection);
    void sendReleaseTelegram(SimpleCB aDoneCB);

  };

}

#endif // ENABLE_ENOCEAN
#endif // __vdcd__enoceanremotecontrol__

