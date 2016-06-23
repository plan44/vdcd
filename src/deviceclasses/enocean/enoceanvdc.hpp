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

#ifndef __vdcd__enoceanvdc__
#define __vdcd__enoceanvdc__

#include "vdcd_common.hpp"

#if ENABLE_ENOCEAN

#include "vdc.hpp"
#include "sqlite3persistence.hpp"

#include "enoceancomm.hpp"
#include "enoceandevice.hpp"


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


  class EnoceanVdc;
  typedef boost::intrusive_ptr<EnoceanVdc> EnoceanVdcPtr;
  class EnoceanVdc : public Vdc
  {
    friend class EnoceanDevice;
    typedef Vdc inherited;

    bool learningMode;
    bool disableProximityCheck;
    bool selfTesting;

    KeyEventHandlerCB keyEventHandler;

    EnoceanDeviceMap enoceanDevices; ///< local map linking EnoceanDeviceID to devices

		EnoceanPersistence db;

  public:

    EnoceanVdc(int aInstanceNumber, VdcHost *aVdcHostP, int aTag);
		
		void initialize(StatusCB aCompletedCB, bool aFactoryReset);

    // the Enocean communication object
    EnoceanComm enoceanComm;

    virtual const char *vdcClassIdentifier() const;

    /// perform self test
    /// @param aCompletedCB will be called when self test is done, returning ok or error
    virtual void selfTest(StatusCB aCompletedCB);

    /// collect and add devices to the container
    virtual void collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings);

    /// vdc level methods (p44 specific, JSON only, for configuring multichannel RGB(W) devices)
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    /// @param aForget if set, all parameters stored for the device (if any) will be deleted. Note however that
    ///   the devices are not disconnected (=unlearned) by this.
    virtual void removeDevices(bool aForget);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "EnOcean"; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    /// - enoceanaddress:XXXXXXXX = 8 hex digits enOcean device address
    virtual string hardwareGUID() { return string_format("enoceanaddress:%08X", enoceanComm.modemAddress()); };

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

  protected:

    /// add device to container (already known device, already stored in DB)
    /// @return false if aEnoceanDevice dSUID is already known and thus was *not* added
    virtual bool addKnownDevice(EnoceanDevicePtr aEnoceanDevice);

    /// add newly learned device to EnOcean container (and remember it in DB)
    /// @return false if aEnoceanDevice dSUID is already known and thus was *not* added
    virtual bool addAndRememberDevice(EnoceanDevicePtr aEnoceanDevice);

    /// un-pair devices by physical device address
    /// @param aEnoceanAddress address for which to disconnect and forget all physical devices
    /// @param aForgetParams if set, associated dS level configuration will be cleared such that
    ///   after reconnect the device will appear with default config
    /// @param aFromIndex starting subdevice index, defaults to 0
    /// @param aNumIndices how many subdevice index positions (0 = all)
    void unpairDevicesByAddress(EnoceanAddress aEnoceanAddress, bool aForgetParams, EnoceanSubDevice aFromIndex=0, EnoceanSubDevice aNumIndices=0);

    /// set container learn mode
    /// @param aEnableLearning true to enable learning mode
    /// @param aDisableProximityCheck true to disable proximity check (e.g. minimal RSSI requirement for some EnOcean devices)
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on VdcHost.
    virtual void setLearnMode(bool aEnableLearning, bool aDisableProximityCheck);

  protected:

    /// remove device
    /// @param aDevice device to remove (possibly only part of a multi-function physical device)
    virtual void removeDevice(DevicePtr aDevice, bool aForget);

  private:

    void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);
    void handleEventPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);
    void handleTestRadioPacket(StatusCB aCompletedCB, Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);

    Tristate processLearn(EnoceanAddress aDeviceAddress, EnoceanProfile aEEProfile, EnoceanManufacturer aManufacturer);

    ErrorPtr addProfile(VdcApiRequestPtr aRequest, ApiValuePtr aParams);
    ErrorPtr simulatePacket(VdcApiRequestPtr aRequest, ApiValuePtr aParams);
  };

} // namespace p44

#endif // ENABLE_ENOCEAN
#endif // __vdcd__enoceanvdc__
