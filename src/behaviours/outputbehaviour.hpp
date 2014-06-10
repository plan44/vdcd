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

#ifndef __vdcd__outputbehaviour__
#define __vdcd__outputbehaviour__

#include "device.hpp"
#include "dsbehaviour.hpp"

using namespace std;

namespace p44 {


  class OutputBehaviour;

  /// represents a single channel of the output
  /// @note this class is not meant to be derived. Device specific channel functionality should
  ///   be implemented in derived Device classes' methods which are passed channels to process.
  ///   The ChannelBehaviour objects only represent the dS interface to channels, not the
  ///   device specific interface from dS channels to actual device hardware.
  class ChannelBehaviour : public PropertyContainer
  {
    typedef PropertyContainer inherited;
    friend class OutputBehaviour;

    OutputBehaviour &output;

  protected:

    /// @name hardware derived parameters (constant during operation)
    /// @{
    uint8_t channelIndex; ///< the index of the channel within the device
    DsChannelType channel; ///< the channel type (ID) of this channel
    string hardwareName; ///< name that identifies this channel among others for the human user (terminal label text etc.)
    /// @}

    /// @name persistent settings
    /// @{

    /// @}

    /// @name internal volatile state
    /// @{
    bool channelUpdatePending; ///< set if cachedOutputValue represents a value to be transmitted to the hardware
    int32_t cachedChannelValue; ///< the cached channel value
    MLMicroSeconds channelLastSent; ///< Never if the cachedChannelValue is not yet applied to the hardware, otherwise when it was sent
    MLMicroSeconds nextTransitionTime; ///< the transition time to use for the next channel value change
    /// @}

  public:

    ChannelBehaviour(OutputBehaviour &aOutput);

    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// set the fixed channel identification (defined by the device's hardware)
    /// @param aChannelType the digitalSTROM channel type
    /// @param aName a descriptive name for the channel, relating to function and/or device harware terminal labels
    void setChannelIdentification(DsChannelType aChannelType, const char *aName);

    /// get currently applied output value from device hardware
    virtual int32_t getChannelValue();

    /// set new output value on device
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setChannelValue(int32_t aNewValue, MLMicroSeconds aTransitionTime=0);

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// get the channel type
    /// @return the channel type
    DsChannelType getChannelType() { return channel; };

    /// get the channel index
    /// @return the channel index (0..N, 0=primary)
    size_t getChannelIndex() { return channelIndex; };

    /// check if this is the primary channel
    /// @return true if this is the primary (default) channel of a device
    bool isPrimary();

    /// set actual current output value as read from the device on startup, to update local cache value
    /// @param aActualChannelValue the value as read from the device
    /// @note only used at startup to get the inital value FROM the hardware.
    ///   NOT to be used to change the hardware output value!
    void initChannelValue(uint32_t aActualChannelValue);

    /// the value to be set in the hardware
    /// @return value to be set in actual hardware
    int32_t valueForHardware() { return cachedChannelValue; };

    /// the transition time to use to change value in the hardware
    /// @return transition time
    MLMicroSeconds transitionTimeForHardware() { return nextTransitionTime; };

    /// to be called when channel value has been successfully applied to hardware
    void channelValueApplied();

    /// call to make update pending
    void setChannelUpdatePending() { channelUpdatePending = true; }

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();


  protected:

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

  };

  typedef boost::intrusive_ptr<ChannelBehaviour> ChannelBehaviourPtr;

  typedef vector<ChannelBehaviourPtr> ChannelBehaviourVector;



  /// Implements the basic behaviour of an output with one or multiple output channels
  class OutputBehaviour : public DsBehaviour
  {
    typedef DsBehaviour inherited;
    friend class ChannelBehaviour;

    // channels
    ChannelBehaviourVector channels;

  protected:

    /// @name hardware derived parameters (constant during operation)
    /// @{
    DsOutputFunction outputFunction; ///< the function of the output
    DsUsageHint outputUsage; ///< the input type when device has hardwired functions
    DsChannelType defaultChannel; ///< the default (hardware-predefined) channel type of the output
    bool variableRamp; ///< output has variable ramp times
    double maxPower; ///< max power in Watts the output can control
    /// @}


    /// @name persistent settings
    /// @{
    DsOutputMode outputMode; ///< the mode of the output
    bool pushChanges; ///< if set, local changes to output will be pushed upstreams
    uint64_t outputGroups; ///< mask for group memberships (0..63)
    /// @}


    /// @name internal volatile state
    /// @{
    bool localPriority; ///< if set device is in local priority mode
    /// @}

    /// add a channel to the output
    /// @param aChannel the channel to add
    /// @note this is usually called by initialisation code of classes derived from OutputBehaviour to
    ///   add the behaviour specific channels.
    void addChannel(ChannelBehaviourPtr aChannel);

  public:

    OutputBehaviour(Device &aDevice);

    /// @name Access to channels
    /// @{

    /// get number of channels
    /// @return number of channels
    size_t numChannels();

    /// get channel by index
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByIndex(size_t aChannelIndex);

    /// get output index by channelType
    /// @param aChannelType the channel type, can be channeltype_default to get primary/default channel
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByType(DsChannelType aChannelType);

    /// @}


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// Configure hardware parameters of the output
    void setHardwareOutputConfig(DsOutputFunction aOutputFunction, DsUsageHint aUsage, bool aVariableRamp, double aMaxPower);

    /// @return true if device is in local priority mode
    void setLocalPriority(bool aLocalPriority) { localPriority = aLocalPriority; };

    /// @return true if device is in local priority mode
    bool hasLocalPriority() { return localPriority; };

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// check group membership
    /// @param aGroup color number to check
    /// @return true if device is member of this group
    bool isMember(DsGroup aGroup);

    /// set group membership
    /// @param aGroup group number to set or remove
    /// @param aIsMember true to make device member of this group
    void setGroupMembership(DsGroup aGroup, bool aIsMember);

    /// apply scene to output
    /// @param aScene the scene to apply to the output
    /// @note this method must handle dimming, but will *always* be called with basic INC_S/DEC_S/STOP_S scenes for that,
    ///   never with area dimming scenes. Area dimming scenes are converted to INC_S/DEC_S/STOP_S (normalized)
    ///   and filtered by actual area at the Device::callScene level. This is to keep the highly dS 1.0 specific
    ///   area withing the Device class and simplify implementations of output behaviours.
    virtual void applyScene(DsScenePtr aScene) { /* NOP in base class */ };

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    virtual void performSceneActions(DsScenePtr aScene) { /* NOP in base class, only relevant for lights */ };


    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @param aDoneCB will be called when capture is complete
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene, DoneCB aDoneCB) { if (aDoneCB) aDoneCB(); /* NOP in base class */ };

    /// switch on at minimum brightness if not already on (needed for callSceneMin), only relevant for lights
    virtual void onAtMinBrightness() { /* NOP in base class, only relevant for lights */ };

    /// Process a named control value. The type, color and settings of the output determine if at all,
    /// and if, how the value affects the output
    /// @param aName the name of the control value, which describes the purpose
    /// @param aValue the control value to process
    virtual void processControlValue(const string &aName, double aValue) { /* NOP in base class */ };


    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    // the behaviour type
    virtual BehaviourType getType() { return behaviour_output; };

    // for groups property
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain);

    // property access implementation for descriptor/settings/states
    virtual int numDescProps();
    virtual const PropertyDescriptorPtr getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    virtual int numSettingsProps();
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    virtual int numStateProps();
    virtual const PropertyDescriptorPtr getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    // combined field access for all types of properties
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);
    
  };
  
  typedef boost::intrusive_ptr<OutputBehaviour> OutputBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__outputbehaviour__) */
