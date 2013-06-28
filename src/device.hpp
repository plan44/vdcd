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
#include "dsdefs.h"

using namespace std;

namespace p44 {

  typedef uint8_t Brightness;
  typedef uint8_t SceneNo;


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
    DsGroup deviceColorGroup; ///< basic color of the device, as represented in the function ID
    DsGroupMask groupMembership; ///< mask for groups the device is member of ("GRP" property)

  public:
    DSBehaviour(Device *aDeviceP);
    virtual ~DSBehaviour();

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId() = 0;
    virtual uint16_t productId() = 0;
    virtual uint8_t ltMode() = 0;
    virtual uint8_t outputMode() = 0;
    virtual uint8_t buttonIdGroup() = 0;

    virtual uint16_t version() { return 0xFFFF; }

    /// Confirm registration
    virtual void confirmRegistration(JsonObjectPtr aParams);

    /// @}


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// set basic device color to represent in identification for digitalSTROM system
    void setDeviceColor(DsGroup aColorGroup);

    /// @}


    /// @name interaction with digitalSTROM system, to be implemented/used in concrete classes
    /// @{

    /// handle message from vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return Error object if message generated an error
    virtual ErrorPtr handleMessage(string &aOperation, JsonObjectPtr aParams);

    /// get behaviour-specific parameter
    /// @param aParamName name of the parameter
    /// @param aArrayIndex index of the parameter if the parameter is an array
    /// @param aValue will receive the current value
    virtual ErrorPtr getBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t &aValue);

    /// set behaviour-specific parameter
    /// @param aParamName name of the parameter
    /// @param aArrayIndex index of the parameter if the parameter is an array
    /// @param aValue the new value to set
    virtual ErrorPtr setBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t aValue);

    /// load behaviour parameters from persistent DB
    virtual ErrorPtr load() { return ErrorPtr(); /* NOP in base class */ };

    /// save unsaved behaviour parameters to persistent DB
    virtual ErrorPtr save() { return ErrorPtr(); /* NOP in base class */ };

    /// forget any parameters stored in persistent DB
    virtual ErrorPtr forget() { return ErrorPtr(); /* NOP in base class */ };

    /// send message to vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendMessage(const char *aOperation, JsonObjectPtr aParams);

    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description() { return ""; /* empty string, to allow chaining descriptions for behaviour hierarchies */ };

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

    /// get pointer to device container
    DeviceContainer &getDeviceContainer() { return classContainerP->getDeviceContainer(); };

    /// check if device is public dS device (which should be registered with vdSM)
    /// @return true if device is registerable with vdSM
    virtual bool isPublicDS();

    /// set the device behaviour
    /// @param aBehaviour the behaviour. Ownership is passed to the Device.
    void setDSBehaviour(DSBehaviour *aBehaviour);

    /// number of buttons
    /// @return returns total number of buttons the associated physical device has.
    /// @note for each button, a separate logical device may exist with increasing serialNo part in the dsid
    virtual int getNumButtons() { return 0; }

    /// button index of this device
    /// @return index of button (0..getNumButtons()-1) of this sub-device within its physical device
    virtual int getButtonIndex() { return 0; }

    /// Get the parameters for registering this device with the vdSM
    /// @return JSON object containing the parameters
    JsonObjectPtr registrationParams();

    /// Confirm registration
    void confirmRegistration(JsonObjectPtr aParams);


    /// handle message from vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return Error object if message generated an error
    ErrorPtr handleMessage(string &aOperation, JsonObjectPtr aParams);

    /// send message to vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendMessage(const char *aOperation, JsonObjectPtr aParams);


    /// load parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected)
    virtual ErrorPtr load();

    /// save unsaved parameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any parameters stored in persistent DB
    virtual ErrorPtr forget();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset) { aCompletedCB(ErrorPtr()); /* NOP in base class */ };

    /// "pings" the device. Device should respond by sending back a "pong" shortly after (using pong())
    /// base class just sends the pong, but derived classes which can actually ping their hardware should
    /// do so and send the pong only if the hardware actually responds.
    virtual void ping();

    /// sends a "pong" back to the vdSM. Devices should call this as a response to ping()
    void pong();
    
    /// get currently set output value from device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    virtual int16_t getOutputValue(int aChannel) { return 0; };

    /// set new output value on device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int aChannel, int16_t aValue, MLMicroSeconds aTransitionTime=0) { /* NOP */ };

    /// @}


    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();
    
  protected:

    virtual ErrorPtr getDeviceParam(const string &aParamName, int aArrayIndex, uint32_t &aValue);
    virtual ErrorPtr setDeviceParam(const string &aParamName, int aArrayIndex, uint32_t aValue);

  };


} // namespace p44


#endif /* defined(__p44bridged__device__) */
