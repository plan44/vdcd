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
  /// For each type of subsystem (enOcean, DALI, ...) this class is subclassed to implement
  /// the device class' specifics, in particular the interface with the hardware.
  class Device : public DsAddressable
  {
    typedef DsAddressable inherited;

    friend class DeviceContainer;
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
    SceneNo lastDimSceneNo; ///< most recently used dimming scene (used when T1234_CONT is received)

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

    /// @}



    /// @name interfaces for actual device hardware (or simulation)
    /// @{

    /// set basic device color
    /// @param aColorGroup color group number
    void setPrimaryGroup(DsGroup aColorGroup);

    /// get basic device color group
    /// @return color group number
    DsGroup getPrimaryGroup() { return primaryGroup; };

    /// report that device has vanished (disconnected without being told so via vDC API)
    /// This will call disconnect() on the device, and remove it from all vDC container lists
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @note this method should be called when bus scanning or other HW-side events detect disconnection
    ///   of a device, such that it can be reported to the dS system.
    /// @note calling hasVanished() might delete the object, so don't rely on 'this' after calling it unless you
    ///   still hold a DevicePtr to it
    void hasVanished(bool aForgetParams);

    /// @}


    /// set user assignable name
    /// @param new name of the addressable entity
    virtual void setName(const string &aName);

    /// get reference to device container
    DeviceContainer &getDeviceContainer() { return classContainerP->getDeviceContainer(); };


    /// load parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected)
    virtual ErrorPtr load();

    /// save unsaved parameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any parameters stored in persistent DB
    virtual ErrorPtr forget();



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

    /// start or stop dimming channel of this device
    /// @param aChannel the channel to start or stop dimming for
    /// @param aDimMode 1=start dimming up, -1=start dimming down, 0=stop dimming
    void dimChannel(DsChannelType aChannel, int aDimMode, int aArea);

    /// identify the device to the user
    /// @note for lights, this is usually implemented as a blink operation, but depending on the device type,
    ///   this can be anything.
    virtual void identifyToUser();

    /// @}


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset) { aCompletedCB(ErrorPtr()); /* NOP in base class */ };

    /// apply all pending channel value updates to the device's hardware
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    virtual void applyChannelValues() { /* NOP in base class */ };

    /// Process a named control value. The type, color and settings of the device determine if at all, and if, how
    /// the value affects physical outputs of the device
    /// @note this method must not directly update the hardware, but just prepare channel values such that these can
    ///   be applied using applyChannelValues().
    /// @param aName the name of the control value, which describes the purpose
    /// @param aValue the control value to process
    /// @note base class by default forwards the control value to all of its output behaviours.
    virtual void processControlValue(const string &aName, double aValue);


    typedef boost::function<void (bool aDisconnected)> DisconnectCB;

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

    /// @}


    /// add a behaviour and set its index
    /// @param aBehaviour a newly created behaviour, will get added to the correct button/binaryInput/sensor/output
    ///   array and given the correct index value. Primary output must be added first as it needs to have index 0.
    void addBehaviour(DsBehaviourPtr aBehaviour);


    /// @name channels
    /// @{

    /// @return number of output channels in this device
    int numChannels();

    /// get channel by index
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByIndex(size_t aChannelIndex);

    /// get output index by channelType
    /// @param aChannelType the channel type, can be channeltype_default to get primary/default channel
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByType(DsChannelType aChannelType);

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

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

    void outputSceneValueSaved(DsScenePtr aScene);
    void outputUndoStateSaved(DsBehaviourPtr aOutput, DsScenePtr aScene);

  };


} // namespace p44


#endif /* defined(__vdcd__device__) */
