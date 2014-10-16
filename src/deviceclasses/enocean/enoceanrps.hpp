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

#ifndef __vdcd__enoceanrps__
#define __vdcd__enoceanrps__

#include "vdcd_common.hpp"

#include "enoceandevice.hpp"


using namespace std;

namespace p44 {

  /// single EnOcean device channel
  class EnoceanRpsHandler : public EnoceanChannelHandler
  {
    typedef EnoceanChannelHandler inherited;

  protected:
  
    /// private constructor, create new channels using factory static method
    EnoceanRpsHandler(EnoceanDevice &aDevice);

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
      EnoceanSubDevice &aNumSubdevices,
      bool aNeedsTeachInResponse
    );

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr) = 0;

  };
  typedef boost::intrusive_ptr<EnoceanRpsHandler> EnoceanRpsHandlerPtr;


  /// single EnOcean button channel
  class EnoceanRpsButtonHandler : public EnoceanRpsHandler
  {
    typedef EnoceanRpsHandler inherited;
    friend class EnoceanRpsHandler;

    /// private constructor, create new channels using factory static method
    EnoceanRpsButtonHandler(EnoceanDevice &aDevice);

    bool pressed; ///< true if currently pressed, false if released, index: 0=on/down button, 1=off/up button
    int switchIndex; ///< which switch within the device (A..D)
    bool isRockerUp; ///< set if rocker up side of switch


    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();
    
  private:
    void setButtonState(bool aPressed);
    
  };
  typedef boost::intrusive_ptr<EnoceanRpsButtonHandler> EnoceanRpsButtonHandlerPtr;


  /// single EnOcean window handle channel
  class EnoceanRpsWindowHandleHandler : public EnoceanRpsHandler
  {
    typedef EnoceanRpsHandler inherited;
    friend class EnoceanRpsHandler;

    /// private constructor, create new channels using factory static method
    EnoceanRpsWindowHandleHandler(EnoceanDevice &aDevice);


    bool isTiltedStatus; ///< set if this represents the tilted status (otherwise, it's the open status)


    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  private:

  };
  typedef boost::intrusive_ptr<EnoceanRpsWindowHandleHandler> EnoceanRpsWindowHandleHandlerPtr;



  /// single EnOcean key card switch handler
  class EnoceanRpsCardKeyHandler : public EnoceanRpsHandler
  {
    typedef EnoceanRpsHandler inherited;
    friend class EnoceanRpsHandler;

    /// private constructor, create new channels using factory static method
    EnoceanRpsCardKeyHandler(EnoceanDevice &aDevice);

    /// handle radio packet related to this channel
    /// @param aEsp3PacketPtr the radio packet to analyze and extract channel related information
    virtual void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr);

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  private:

  };
  typedef boost::intrusive_ptr<EnoceanRpsCardKeyHandler> EnoceanRpsCardKeyHandlerPtr;





  class EnoceanRPSDevice : public EnoceanDevice
  {
    typedef EnoceanDevice inherited;

  public:

    /// constructor
    EnoceanRPSDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanSubDevice aTotalSubdevices);

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual const char *deviceTypeIdentifier() { return "enocean_rps"; };

    /// get profile variants this device can have
    /// @param aApiObjectValue must be an object typed API value, will receive profile variants as EEP/description key/values
    /// @return true if device has variants
    virtual bool getProfileVariants(ApiValuePtr aApiObjectValue);

    /// @param aProfile must be an EEP profile code
    /// @return true if profile variant is valid and can be set
    virtual bool setProfileVariant(EnoceanProfile aProfile);

    /// @name dSUID space and hardware grouping properties needed for dS 1.0 backwards compatibility
    /// @{

    /// @return size of dSUID block (number of consecutive dSUIDs) that is guaranteed reserved for this device
    /// @note devices can
    virtual int idBlockSize();

    /// @return number of vdSDs (virtual devices represented by a separate dSUID)
    ///   that are contained in the same hardware device. -1 means "not available or ambiguous"
    virtual ssize_t numDevicesInHW();

    /// @return index of this vdSDs (0..numDevicesInHW()-1) among all vdSDs in the same hardware device
    ///   -1 means undefined
    virtual ssize_t deviceIndexInHW();

    /// @}

  };



}

#endif /* defined(__vdcd__enoceanrps__) */
