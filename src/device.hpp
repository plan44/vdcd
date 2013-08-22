//
//  device.hpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__device__
#define __vdcd__device__

#include "dsbehaviour.hpp"

#include "dsscene.hpp"

using namespace std;

namespace p44 {

  class Device;

  typedef vector<DsBehaviourPtr> BehaviourVector;

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

  protected:

    /// the class container
    DeviceClassContainer *classContainerP;

    /// @name behaviours
    /// @{
    BehaviourVector buttons; ///< buttons and switches (user interaction)
    BehaviourVector binaryInputs; ///< binary inputs (not for user interaction)
    BehaviourVector outputs; ///< outputs (on/off as well as continuous ones like dimmer, positionals etc.)
    BehaviourVector sensors; ///< sensors (measurements)
    /// @}

    /// device global parameters (for all behaviours), in particular the scene table
    /// @note devices assign this with a derived class which is specialized
    ///   for the device type and, if needed, proper type of scenes (light, blinds, RGB light etc. have different scene tables)
    DeviceSettingsPtr deviceSettings;

    // r/w properties
    bool progMode;

    // variables set by concrete devices (=hardware dependent)
    DsGroup primaryGroup; ///< basic color of the device (can be black)
    DsGroupMask groupMembership; ///< mask for groups the device is member of ("GRP" property)

  public:
    Device(DeviceClassContainer *aClassContainerP);
    virtual ~Device();


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// set basic device color
    /// @param aColorGroup color group number
    void setPrimaryGroup(DsGroup aColorGroup);

    /// check group membership
    /// @param aColorGroup color group number to check
    /// @return true if device is member of this group
    bool isMember(DsGroup aColorGroup);

    /// set group membership
    /// @param aColorGroup color group number to check
    /// @param aIsMember true to make device member of this group
    void setGroupMembership(DsGroup aColorGroup, bool aIsMember);

    /// @}


    /// get reference to device container
    DeviceContainer &getDeviceContainer() { return classContainerP->getDeviceContainer(); };

    /// check if device is public dS device (which should be registered with vdSM)
    /// @return true if device is registerable with vdSM
    virtual bool isPublicDS();

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


    /// call scene on this device
    /// @param aSceneNo the scene to call.
    void callScene(SceneNo aSceneNo, bool aForce);

    /// save scene on this device
    /// @param aSceneNo the scene to save current state into
    void saveScene(SceneNo aSceneNo);


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

    /// get currently set output value from device hardware
    /// @param aOutputBehaviour the output behaviour which wants to know the output value as set in the hardware
    virtual int16_t getOutputValue(OutputBehaviour &aOutputBehaviour) { return 0; };

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which wants to set the hardware output value
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(OutputBehaviour &aOutputBehaviour, int16_t aValue, MLMicroSeconds aTransitionTime=0) { /* NOP */ };

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "vdSD - virtual device"; }

    /// @return the entity type (one of dSD|vdSD|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "vdSD"; }
    
    /// @}


    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual PropertyContainerPtr getContainer(const PropertyDescriptor &aPropertyDescriptor, int &aDomain, int aIndex = 0);
    virtual ErrorPtr writtenProperty(const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex, PropertyContainerPtr aContainer);
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);


  private:

    // method handlers

  };


} // namespace p44


#endif /* defined(__vdcd__device__) */
