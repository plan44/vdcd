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
#include "channelbehaviour.hpp"

using namespace std;

namespace p44 {

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
    ChannelBehaviourPtr getChannelByIndex(size_t aChannelIndex, bool aPendingApplyOnly = false);

    /// get output index by channelType
    /// @param aChannelType the channel type, can be channeltype_default to get primary/default channel
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByType(DsChannelType aChannelType, bool aPendingApplyOnly = false);

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
    /// @return true if apply is complete, i.e. everything ready to apply to hardware outputs.
    ///   false if scene cannot yet be applied to hardware, and actual transfer to hardware will happen later
    /// @note This method must NOT call device level applyChannelValues() to actually apply values to hardware for
    ///   a one-step scene value change.
    ///   It MAY cause subsequent applyChannelValues() calls AFTER returning to perform special effects
    /// @note this method does not handle dimming, and MUST NOT be called with dimming specific scenes. For dimming,
    ///   only the device's dimChannel method must be used.
    virtual bool applyScene(DsScenePtr aScene) { return true; /* just return "apply complete" in base class */ };

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
