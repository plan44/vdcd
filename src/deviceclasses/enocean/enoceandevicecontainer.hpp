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

#ifndef __vdcd__enoceandevicecontainer__
#define __vdcd__enoceandevicecontainer__

#include "vdcd_common.hpp"

#include "deviceclasscontainer.hpp"

#include "enoceancomm.hpp"

#include "enoceandevice.hpp"

#include "sqlite3persistence.hpp"


using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    EnoceanErrorOK,
    EnoceanDeviceLearned,
    EnoceanDeviceUnlearned,
    EnoceanNoKnownProfile,
    EnoceanLearnTimeout,
    EnoceanLearnAborted,
  } EnoceanErrors;

  class EnoceanError : public Error
  {
  public:
    static const char *domain() { return "Enocean"; }
    virtual const char *getErrorDomain() const { return EnoceanError::domain(); };
    EnoceanError(EnoceanErrors aError) : Error(ErrorCode(aError)) {};
    EnoceanError(EnoceanErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  typedef std::multimap<EnoceanAddress, EnoceanDevicePtr> EnoceanDeviceMap;

  /// @param aEnoceanDevicePtr the EnOcean device the key event originates from
  /// @param aSubDeviceIndex subdevice, can be -1 if subdevice cannot be determined (multiple rockers released)
  /// @return true if locally handled such that no further operation is needed, false otherwise
  typedef boost::function<bool (EnoceanDevicePtr aEnoceanDevicePtr, int aSubDeviceIndex, uint8_t aAction)> KeyEventHandlerCB;


  /// persistence for enocean device container
  class EnoceanPersistence : public SQLite3Persistence
  {
    typedef SQLite3Persistence inherited;
  protected:
    /// Get DB Schema creation/upgrade SQL statements
    virtual string dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion);
  };


  class EnoceanDeviceContainer;
  typedef boost::intrusive_ptr<EnoceanDeviceContainer> EnoceanDeviceContainerPtr;
  class EnoceanDeviceContainer : public DeviceClassContainer
  {
    friend class EnoceanDevice;
    typedef DeviceClassContainer inherited;

    bool learningMode;
    bool disableProximityCheck;
    bool selfTesting;

    KeyEventHandlerCB keyEventHandler;

    EnoceanDeviceMap enoceanDevices; ///< local map linking EnoceanDeviceID to devices

		EnoceanPersistence db;

  public:

    EnoceanDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);
		
		void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    // the Enocean communication object
    EnoceanComm enoceanComm;

    virtual const char *deviceClassIdentifier() const;

    /// perform self test
    /// @param aCompletedCB will be called when self test is done, returning ok or error
    virtual void selfTest(CompletedCB aCompletedCB);

    /// collect and add devices to the container
    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// @param aForget if set, all parameters stored for the device (if any) will be deleted. Note however that
    ///   the devices are not disconnected (=unlearned) by this.
    virtual void removeDevices(bool aForget);

    /// @return human readable model name/short description
    virtual string modelName() { return "enOcean vDC"; }

  protected:

    /// add device to container (already known device, already stored in DB)
    /// @return false if aEnoceanDevice dSUID is already known and thus was *not* added
    virtual bool addKnownDevice(EnoceanDevicePtr aEnoceanDevice);

    /// add newly learned device to enOcean container (and remember it in DB)
    /// @return false if aEnoceanDevice dSUID is already known and thus was *not* added
    virtual bool addAndRemeberDevice(EnoceanDevicePtr aEnoceanDevice);

    /// un-pair devices by physical device address
    /// @param aEnoceanAddress address for which to disconnect and forget all physical devices
    /// @param aForgetParams if set, associated dS level configuration will be cleared such that
    ///   after reconnect the device will appear with default config
    void unpairDevicesByAddress(EnoceanAddress aEnoceanAddress, bool aForgetParams);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @param aDisableProximityCheck true to disable proximity check (e.g. minimal RSSI requirement for some enOcean devices)
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    virtual void setLearnMode(bool aEnableLearning, bool aDisableProximityCheck);

  protected:

    /// remove device
    /// @param aDevice device to remove (possibly only part of a multi-function physical device)
    virtual void removeDevice(DevicePtr aDevice, bool aForget);

  private:

    void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);
    void handleTestRadioPacket(CompletedCB aCompletedCB, Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__vdcd__enoceandevicecontainer__) */
