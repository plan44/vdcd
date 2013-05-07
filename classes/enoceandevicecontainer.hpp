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




  class EnoceanDeviceContainer;
  typedef boost::shared_ptr<EnoceanDeviceContainer> EnoceanDeviceContainerPtr;
  class EnoceanDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;

    CompletedCB learningCompleteHandler;

  public:
    EnoceanDeviceContainer(int aInstanceNumber);

    // the Enocean communication object
    EnoceanComm enoceanComm;

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aExhaustive);

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

  private:

    void handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError);

  };

} // namespace p44

#endif /* defined(__p44bridged__enoceandevicecontainer__) */
