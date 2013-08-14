//
//  device.hpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__device__
#define __vdcd__device__

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

    /// @}


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// set basic device color to represent in identification for digitalSTROM system
    virtual void setDeviceColor(DsGroup aColorGroup);

    /// @}


    /// @name interaction with digitalSTROM system, to be implemented/used in concrete classes
    /// @{

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
  class Device : public DsAddressable
  {
    typedef DsAddressable inherited;

    friend class DeviceContainer;
    
    MLMicroSeconds announced; ///< set when last announced to the vdSM
    MLMicroSeconds announcing; ///< set when announcement has been started (but not yet confirmed)

    DSBehaviour *behaviourP; ///< private owned instance of the behaviour, set right after creation
  protected:
    DeviceClassContainer *classContainerP;
  public:
    Device(DeviceClassContainer *aClassContainerP);
    virtual ~Device();

    /// get pointer to the behaviour
    /// @return the behaviour. If NULL, the device ist not yet set up and cannot be operated
    DSBehaviour *getDSBehaviour() { return behaviourP; };

    /// get reference to device container
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

    /// load parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected)
    virtual ErrorPtr load();

    /// save unsaved parameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any parameters stored in persistent DB
    virtual ErrorPtr forget();


    typedef boost::function<void (DevicePtr aDevice, bool aDisconnected)> DisconnectCB;

    /// disconnect device. If presence is represented by data stored in the vDC rather than
    /// detection of real physical presence on a bus, this call must clear the data that marks
    /// the device as connected to this vDC (such as a learned-in enOcean button).
    /// For devices where the vDC can be *absolutely certain* that they are still connected
    /// to the vDC AND cannot possibly be connected to another vDC as well, this call should
    /// return false.
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    /// @note at the time aDisconnectResultHandler is called, the only owner left for the device object might be the
    ///   aDevice argument to the DisconnectCB handler.
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);


    /// report that device has vanished (disconnected without being told so via vDC API)
    /// This will call disconnect() on the device, and remove it from all vDC container lists
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @note this method should be called when bus scanning or other HW-side events detect disconnection
    ///   of a device, such that it can be reported to the dS system.
    /// @note calling hasVanished() might delete the object, so don't rely on 'this' after calling it unless you
    ///   still hold a DevicePtr to it
    void hasVanished(bool aForgetParams);

    /// @name DsAddressable API implementation

    /// @{

    /// called to let device handle device-level methods
    /// @param aMethod the method
    /// @param aJsonRpcId the id parameter to be used in sendResult()
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSID parameter which has been
    ///   used already to route the method call to this device.
    virtual ErrorPtr handleMethod(const string &aMethod, const string &aJsonRpcId, JsonObjectPtr aParams);

    /// called to let device handle device-level notification
    /// @param aMethod the notification
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSID parameter which has been
    ///   used already to route the notification to this device.
    virtual void handleNotification(const string &aMethod, JsonObjectPtr aParams);

    /// @}


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset) { aCompletedCB(ErrorPtr()); /* NOP in base class */ };

    /// get currently set output value from device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    virtual int16_t getOutputValue(int aChannel) { return 0; };

    /// set new output value on device
    /// @param aChannel the output channel. Traditional dS devices have one single output only, but future devices might have many
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int aChannel, int16_t aValue, MLMicroSeconds aTransitionTime=0) { /* NOP */ };

    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();
    
  protected:

    virtual ErrorPtr getDeviceParam(const string &aParamName, int aArrayIndex, uint32_t &aValue);
    virtual ErrorPtr setDeviceParam(const string &aParamName, int aArrayIndex, uint32_t aValue);

  private:

    // method handlers

  };


} // namespace p44


#endif /* defined(__vdcd__device__) */
