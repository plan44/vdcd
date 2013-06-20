//
//  enoceandevicecontainer.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__enoceandevicecontainer__
#define __p44bridged__enoceandevicecontainer__

#include "p44bridged_common.hpp"

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
  typedef boost::shared_ptr<EnoceanDeviceContainer> EnoceanDeviceContainerPtr;
  class EnoceanDeviceContainer : public DeviceClassContainer
  {
    friend class EnoceanDevice;
    typedef DeviceClassContainer inherited;

    CompletedCB learningCompleteHandler;
    KeyEventHandlerCB keyEventHandler;

    EnoceanDeviceMap enoceanDevices; ///< local map linking EnoceanDeviceID to devices
		
		EnoceanPersistence db;

  public:
    EnoceanDeviceContainer(int aInstanceNumber);
		
		void initialize(CompletedCB aCompletedCB, bool aFactoryReset);

    // the Enocean communication object
    EnoceanComm enoceanComm;

    virtual const char *deviceClassIdentifier() const;

    /// collect and add devices to the container
    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

    /// forget all devices (but don't delete learned-in devices, so next collect will add them again)
    virtual void forgetDevices();

    /// add device to container (already known device, already stored in DB)
    virtual void addKnownDevice(EnoceanDevicePtr aEnoceanDevice);

    /// add newly learned device to enOcean container (and remember it in DB)
    virtual void addAndRemeberDevice(EnoceanDevicePtr aEnoceanDevice);

    /// remove device
    /// @param aDevice device to remove (possibly only part of a multi-function physical device)
    virtual void removeDevice(DevicePtr aDevice, bool aForget);

    /// remove devices by physical device address
    /// @param aEnoceanAddress address for which to remove all physical devices
    void removeDevicesByAddress(EnoceanAddress aEnoceanAddress);


    /// learn RPS device (repeated switch)
    /// Listen for switch device actions with high signal (i.e. which are physically nearby)
    /// and add them to the known switches (if already known)
    /// @param aCompletedCB handler to call when learn-in (EnoceanDeviceLearned) or learn-out (EnoceanDeviceUnlearned)
    ///   completes or learn mode times out (EnoceanLearnTimeout)
    /// @param aLearnTimeout how long to wait for a keypress to learn in or out
    void learnDevice(CompletedCB aCompletedCB, MLMicroSeconds aLearnTimeout);
    /// @return true if currently in learn mode
    bool isLearning();
    /// stop learning
    void stopLearning();


  protected:

//    /// get device by Address
//    /// @param aDeviceAddress enocean address
//    /// @return the device having the specified Address or empty pointer
//    EnoceanDevicePtr getDeviceByAddress(EnoceanAddress aDeviceAddress);

  private:

    void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);
    void endLearning(ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__p44bridged__enoceandevicecontainer__) */
