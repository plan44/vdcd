//
//  enoceandevicecontainer.hpp
//  vdcd
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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

    KeyEventHandlerCB keyEventHandler;

    EnoceanDeviceMap enoceanDevices; ///< local map linking EnoceanDeviceID to devices

		EnoceanPersistence db;

  public:

    EnoceanDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP);
		
		void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    // the Enocean communication object
    EnoceanComm enoceanComm;

    virtual const char *deviceClassIdentifier() const;

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
    /// @note learn events (new devices found or devices removed) must be reported by calling reportLearnEvent() on DeviceContainer.
    virtual void setLearnMode(bool aEnableLearning);

  protected:

    /// remove device
    /// @param aDevice device to remove (possibly only part of a multi-function physical device)
    virtual void removeDevice(DevicePtr aDevice, bool aForget);

  private:

    void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__vdcd__enoceandevicecontainer__) */
