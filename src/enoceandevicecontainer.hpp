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
    EnoceanLearnTimeout,
    EnoceanLearnAborted,
  } EnoceanErrors;

  class EnoceanError : public Error
  {
  public:
    static const char *domain() { return "Enocean"; }
    EnoceanError(EnoceanErrors aError) : Error(ErrorCode(aError)) {};
    EnoceanError(EnoceanErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
    virtual const char *getErrorDomain() const { return EnoceanError::domain(); };
  };


  typedef std::map<EnoceanAddress, EnoceanDevicePtr> EnoceanDeviceMap;

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
    typedef DeviceClassContainer inherited;

    CompletedCB learningCompleteHandler;
    KeyEventHandlerCB keyEventHandler;

    EnoceanDeviceMap enoceanDevices; ///< local map linking EnoceanDeviceID to devices

  public:
    EnoceanDeviceContainer(int aInstanceNumber);

    /// set the directory where to store persistent data (databases etc.)
    /// @param aPersistentDataDir full path to directory to save 
    void setPersistentDataDir(const char *aPersistentDataDir);

    // the Enocean communication object
    EnoceanComm enoceanComm;

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

    virtual void forgetCollectedDevices();

    virtual void addCollectedDevice(DevicePtr aDevice);

//    virtual void removeDevice(DevicePtr aDevice);


    /// learn RPS device (repeated switch)
    /// Listen for switch device actions with high signal (i.e. which are physically nearby)
    /// and add them to the known switches (if already known)
    /// @param aCompletedCB handler to call when learn-in (EnoceanDeviceLearned) or learn-out (EnoceanDeviceUnlearned)
    ///   completes or learn mode times out (EnoceanLearnTimeout)
    /// @param aLearnTimeout how long to wait for a keypress to learn in or out
    void learnSwitchDevice(CompletedCB aCompletedCB, MLMicroSeconds aLearnTimeout);
    /// @return true if currently in learn mode
    bool isLearning();
    /// stop learning
    void stopLearning();


    /// set keypress handler (for local direct operation)
    void setKeyEventHandler(KeyEventHandlerCB aKeyEventHandler);

  protected:

    /// get device by Address
    /// @param aDeviceAddress enocean address
    /// @return the device having the specified Address or empty pointer
    EnoceanDevicePtr getDeviceByAddress(EnoceanAddress aDeviceAddress);

  private:

    void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);

    void endLearning(ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__p44bridged__enoceandevicecontainer__) */
