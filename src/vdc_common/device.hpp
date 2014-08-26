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

#ifndef __vdcd__device__
#define __vdcd__device__

#include "dsbehaviour.hpp"

#include "dsscene.hpp"

using namespace std;

namespace p44 {

  class Device;
  typedef boost::intrusive_ptr<Device> DevicePtr;

  class ChannelBehaviour;
  typedef boost::intrusive_ptr<ChannelBehaviour> ChannelBehaviourPtr;

  typedef vector<DsBehaviourPtr> BehaviourVector;

  typedef boost::intrusive_ptr<OutputBehaviour> OutputBehaviourPtr;

  /// base class representing a virtual digitalSTROM device.
  /// For each type of subsystem (EnOcean, DALI, ...) this class is subclassed to implement
  /// the device class' specifics, in particular the interface with the hardware.
  class Device : public DsAddressable
  {
    typedef DsAddressable inherited;

    friend class DeviceContainer;
    friend class DeviceClassCollector;
    friend class DsBehaviour;
    friend class DsScene;
    friend class SceneChannels;

  protected:

    /// the class container
    DeviceClassContainer *classContainerP;

    /// @name behaviours
    /// @{
    BehaviourVector buttons; ///< buttons and switches (user interaction)
    BehaviourVector binaryInputs; ///< binary inputs (not for user interaction)
    BehaviourVector sensors; ///< sensors (measurements)
    OutputBehaviourPtr output; ///< the output (if any)
    /// @}

    /// device global parameters (for all behaviours), in particular the scene table
    /// @note devices assign this with a derived class which is specialized
    ///   for the device type and, if needed, proper type of scenes (light, blinds, RGB light etc. have different scene tables)
    DeviceSettingsPtr deviceSettings;

    // volatile r/w properties
    bool progMode; ///< if set, device is in programming mode
    DsScenePtr previousState; ///< a pseudo scene which holds the device state before the last applyScene() call, used to do undoScene()

    // variables set by concrete devices (=hardware dependent)
    DsGroup primaryGroup; ///< basic color of the device (can be black)

    // volatile internal state
    long dimTimeoutTicket; ///< for timing out dimming operations (autostop when no INC/DEC is received)
    DsDimMode currentDimMode; ///< current dimming in progress
    DsChannelType currentDimChannel; ///< currently dimmed channel (if dimming in progress)
    long dimHandlerTicket; ///< for standard dimming
    bool isDimming; ///< if set, dimming is in progress

    // hardware access serializer/pacer
    DoneCB appliedOrSupersededCB; ///< will be called when values are either applied or ignored because a subsequent change is already pending
    bool applyInProgress; ///< set when applying values is in progress
    int missedApplyAttempts; ///< number of apply attempts that could not be executed. If>0, completing next apply will trigger a re-apply to finalize values
    DoneCB updatedOrCachedCB; ///< will be called when current values are either read from hardware, or new values have been requested for applying
    bool updateInProgress; ///< set when updating channel values from hardware is in progress
    long serializerWatchdogTicket; ///< watchdog terminating non-responding hardware requests

  public:
    Device(DeviceClassContainer *aClassContainerP);
    virtual ~Device();

    /// @name identification and invariable properties of the device (can be overriden in subclasses)
    /// @{

    /// check if device is public dS device (which should be registered with vdSM)
    /// @return true if device is registerable with vdSM
    virtual bool isPublicDS() { return true; }; // base class assumes that all devices are public

    /// @return size of dSUID block (number of consecutive dSUIDs) that is guaranteed reserved for this device
    /// @note devices can
    virtual int idBlockSize() { return 1; }; // normal devices only reserve one single ID (their own)

    /// @return human readable model name/short description
    virtual string modelName() { return "vdSD - virtual device"; }

    /// @return the entity type (one of dSD|vdSD|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "vdSD"; }


    /// @return true if there is an icon, false if not
    /// @param aIcon string to put to binary PNG icon data for 16x16 icon into (when result is true)
    virtual bool getDeviceIcon16(string &aIcon);

    /// @}



    /// @name general device level methods
    /// @{

    /// set basic device color
    /// @param aColorGroup color group number
    void setPrimaryGroup(DsGroup aColorGroup);

    /// get basic device color group
    /// @return color group number
    DsGroup getPrimaryGroup() { return primaryGroup; };

    /// get dominant group (i.e. the group that should color the icon)
    DsGroup getDominantGroup();

    /// report that device has vanished (disconnected without being told so via vDC API)
    /// This will call disconnect() on the device, and remove it from all vDC container lists
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @note this method should be called when bus scanning or other HW-side events detect disconnection
    ///   of a device, such that it can be reported to the dS system.
    /// @note calling hasVanished() might delete the object, so don't rely on 'this' after calling it unless you
    ///   still hold a DevicePtr to it
    void hasVanished(bool aForgetParams);

    /// set user assignable name
    /// @param new name of the addressable entity
    virtual void setName(const string &aName);

    /// get reference to device container
    DeviceContainer &getDeviceContainer() { return classContainerP->getDeviceContainer(); };

    /// add a behaviour and set its index
    /// @param aBehaviour a newly created behaviour, will get added to the correct button/binaryInput/sensor/output
    ///   array and given the correct index value.
    void addBehaviour(DsBehaviourPtr aBehaviour);

    /// get scenes
    /// @return NULL if device has no scenes, scene device settings otherwise 
    SceneDeviceSettingsPtr getScenes() { return boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings); };

    /// load parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected)
    virtual ErrorPtr load();

    /// save unsaved parameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any parameters stored in persistent DB
    virtual ErrorPtr forget();

    /// @}


    /// @name API implementation
    /// @{

    /// called to let device handle device-level methods
    /// @param aMethod the method
    /// @param aRequest the request to be passed to answering methods
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSUID parameter which has been
    ///   used already to route the method call to this device.
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    /// called to let device handle device-level notification
    /// @param aMethod the notification
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSUID parameter which has been
    ///   used already to route the notification to this device.
    virtual void handleNotification(const string &aMethod, ApiValuePtr aParams);

    /// call scene on this device
    /// @param aSceneNo the scene to call.
    void callScene(SceneNo aSceneNo, bool aForce);

    /// undo scene call on this device (i.e. revert outputs to values present immediately before calling that scene)
    /// @param aSceneNo the scene call to undo (needs to be specified to prevent undoing the wrong scene)
    void undoScene(SceneNo aSceneNo);

    /// save scene on this device
    /// @param aSceneNo the scene to save current state into
    void saveScene(SceneNo aSceneNo);

    /// store updated version of a scene for this device
    /// @param aScene the updated scene object that should be stored
    /// @note only updates the scene if aScene is marked dirty
    void updateScene(DsScenePtr aScene);

    /// Process a named control value. The type, color and settings of the device determine if at all, and if, how
    /// the value affects physical outputs of the device
    /// @note this method must not directly update the hardware, but just prepare channel values such that these can
    ///   be applied using requestApplyingChannels().
    /// @param aName the name of the control value, which describes the purpose
    /// @param aValue the control value to process
    /// @note base class by default forwards the control value to all of its output behaviours.
    virtual void processControlValue(const string &aName, double aValue);

    /// @}


    /// @name high level hardware access
    /// @note these methods provide a level of abstraction for accessing hardware (especially output functionality)
    ///   by providing a generic base implementation for functionality. Only in very specialized cases, subclasses may
    ///   still want to derive these methods to provide device hardware specific optimization.
    ///   However, for normal devices it is recommended NOT to derive these methods, but only the low level access
    ///   methods.
    /// @{

    /// request applying channels changes now, but actual execution might get postponed if hardware is laggy
    /// @param aAppliedOrSupersededCB will called when values are either applied, or applying has been superseded by
    ///   even newer values set requested to be applied.
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    /// @note this internally calls applyChannelValues() to perform actual work, but serializes the behaviour towards the caller
    ///   such that aAppliedOrSupersededCB of the previous request is always called BEFORE initiating subsequent
    ///   channel updates in the hardware. It also may discard requests (but still calling aAppliedOrSupersededCB) to
    ///   avoid stacking up delayed requests.
    void requestApplyingChannels(DoneCB aAppliedOrSupersededCB, bool aForDimming);

    /// request that channel values are updated by reading them back from the device's hardware
    /// @param aUpdatedOrCachedCB will be called when values are updated with actual hardware values
    ///   or pending values are in process to be applied to the hardware and thus these cached values can be considered current.
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    void requestUpdatingChannels(DoneCB aUpdatedOrCachedCB);

    /// start or stop dimming channel of this device. Usually implemented in device specific manner in subclasses.
    /// @param aChannel the channelType to start or stop dimming for
    /// @param aDimMode according to DsDimMode: 1=start dimming up, -1=start dimming down, 0=stop dimming
    /// @note unlike the vDC API "dimChannel" command, which must be repeated for dimming operations >5sec, this
    ///   method MUST NOT terminate dimming automatically except when reaching the minimum or maximum level
    ///   available for the device. The 5 second timeout is implemented at the device level and causes calling
    ///   dimChannel() to be called with aDimMode=0 when timeout happens.
    /// @note this method can rely on a clean start-stop sequence in all cases, which means it will be called once to
    ///   start a dimming process, and once again to stop it. There are no repeated start commands or missing stops - Device
    ///   class makes sure these cases (which may occur at the vDC API level) are not passed on to dimChannel()
    virtual void dimChannel(DsChannelType aChannelType, DsDimMode aDimMode);

    /// identify the device to the user
    /// @note for lights, this is usually implemented as a blink operation, but depending on the device type,
    ///   this can be anything.
    /// @note base class delegates this to the output behaviour (if any)
    virtual void identifyToUser();
    

    typedef boost::function<void (bool aDisconnected)> DisconnectCB;

    /// disconnect device. If presence is represented by data stored in the vDC rather than
    /// detection of real physical presence on a bus, this call must clear the data that marks
    /// the device as connected to this vDC (such as a learned-in EnOcean button).
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

    /// @}


    /// @name channels
    /// @{

    /// @return number of output channels in this device
    int numChannels();

    /// get channel by index
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @param aPendingApplyOnly if true, only channels with pending values to be applied are returned
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByIndex(size_t aChannelIndex, bool aPendingApplyOnly = false);

    /// get output index by channelType
    /// @param aChannelType the channel type, can be channeltype_default to get primary/default channel
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByType(DsChannelType aChannelType, bool aPendingApplyOnly = false);

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:


    /// @name low level hardware access
    /// @note actual hardware specific implementation is in derived methods in subclasses.
    ///   Base class uses these methods to access the hardware in a generic way.
    ///   These methods should never be called directly!
    /// @note base class implementations are NOP
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset) { aCompletedCB(ErrorPtr()); /* NOP in base class */ };

    /// apply all pending channel value updates to the device's hardware
    /// @param aDoneCB will called when values are actually applied, or hardware reports an error/timeout
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    /// @note this method will NOT be called again until aCompletedCB is called, even if that takes a long time.
    ///   Device::requestApplyingChannels() provides an implementation that prevent calling applyChannelValues too early,
    virtual void applyChannelValues(DoneCB aDoneCB, bool aForDimming) { if (aDoneCB) aDoneCB(); /* just call completed in base class */ };

    /// synchronize channel values by reading them back from the device's hardware (if possible)
    /// @param aDoneCB will be called when values are updated with actual hardware values
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    /// @note implementation must use channel's syncChannelValue() method
    virtual void syncChannelValues(DoneCB aDoneCB) { if (aDoneCB) aDoneCB(); /* assume caches up-to-date */ };
    
    /// @}


    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);
    virtual ErrorPtr writtenProperty(PropertyAccessMode aMode, PropertyDescriptorPtr aPropertyDescriptor, int aDomain, PropertyContainerPtr aContainer);

    /// set local priority of the device if specified scene does not have dontCare set.
    /// @param aSceneNo the scene to check don't care for
    void setLocalPriority(SceneNo aSceneNo);

    /// switch outputs on that are off, and set minmum (logical) output value
    /// @param aSceneNo the scene to check don't care for
    void callSceneMin(SceneNo aSceneNo);


  private:

    DsGroupMask behaviourGroups();

    void dimChannelForArea(DsChannelType aChannel, DsDimMode aDimMode, int aArea, MLMicroSeconds aAutoStopAfter);
    void dimAutostopHandler(DsChannelType aChannel);
    void dimHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNow);
    void dimDoneHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNextDimAt);
    void outputSceneValueSaved(DsScenePtr aScene);
    void outputUndoStateSaved(DsBehaviourPtr aOutput, DsScenePtr aScene);
    void sceneValuesApplied(DsScenePtr aScene);
    void sceneActionsComplete(DsScenePtr aScene);

    void applyingChannelsComplete();
    void updatingChannelsComplete();
    void serializerWatchdog();
    bool checkForReapply();

  };


} // namespace p44


#endif /* defined(__vdcd__device__) */
