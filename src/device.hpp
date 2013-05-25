//
//  device.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__device__
#define __p44bridged__device__

#include "deviceclasscontainer.hpp"

#include "dsid.hpp"

using namespace std;

namespace p44 {

  class Device;
  class DSBehaviour;


  /// a DSBehaviour represents and implements a device behaviour according to dS specs
  /// (for example: the dS Light state machine). The interface of a DSBehaviour is generic
  /// such that it can be used by different physical implementations (e.g. both DALI devices
  /// and hue devices will make use of the dS light state machine behaviour.
  class DSBehaviour
  {
  protected:
    Device *deviceP;
  public:
    DSBehaviour(Device *aDeviceP);
    virtual ~DSBehaviour();

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId() = 0;
    virtual uint16_t productId() = 0;
    virtual uint16_t groupMemberShip() = 0;
    virtual uint8_t ltMode() = 0;
    virtual uint8_t outputMode() = 0;
    virtual uint8_t buttonIdGroup() = 0;

    virtual uint16_t version() { return 0xFFFF; }

    /// @}

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc() = 0;

  };


  typedef boost::shared_ptr<Device> DevicePtr;
  /// base class representing a virtual digitalSTROM device
  /// for each type of subsystem (enOcean, DALI, ...) this class is subclassed to implement
  /// the device class' specifics.
  class Device
  {
    friend class DeviceContainer;
    
    MLMicroSeconds registered; ///< set when registered by dS system
    MLMicroSeconds registering; ///< set when registration has been started (but not yet confirmed)
    /// TODO: %%% old vDSM interface, hope we get rid of the bus address later
    uint32_t busAddress;

    DSBehaviour *behaviourP; ///< private owned instance of the behaviour, set right after creation
  protected:
    DeviceClassContainer *classContainerP;
  public:
    Device(DeviceClassContainer *aClassContainerP);
    virtual ~Device();

    /// the digitalstrom ID
    dSID dsid;

    /// get pointer to the behaviour
    /// @return the behaviour. If NULL, the device ist not yet set up and cannot be operated
    DSBehaviour *getDSBehaviour() { return behaviourP; };

    /// set the device behaviour
    /// @param aBehaviour the behaviour. Ownership is passed to the Device.
    void setDSBehaviour(DSBehaviour *aBehaviour);

    /// number of inputs
    /// @return returns total number of inputs the associated physical device has.
    /// @note for each input, a separate device exists with increasing serialNo part in the dsid
    virtual int getNumInputs() { return 0; }

    /// input index of this device
    /// @return index of input (0..getNumInputs()-1) of this sub-device within its physical device
    virtual int getInputIndex() { return 0; }


    /// Get the parameters for registering this device with the vdSM
    /// @return JSON object containing the parameters
    JsonObjectPtr registrationParams();

    /// Confirm registration
    void confirmRegistration(JsonObjectPtr aParams);

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  };


} // namespace p44


#endif /* defined(__p44bridged__device__) */
