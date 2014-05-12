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

#ifndef __vdcd__binaryinputbehaviour__
#define __vdcd__binaryinputbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {


  /// Implements the behaviour of a digitalSTROM binary input
  /// This class should be used as-is in virtual devices representing binary inputs
  class BinaryInputBehaviour : public DsBehaviour
  {
    typedef DsBehaviour inherited;

  protected:

    /// @name behaviour description, constants or variables
    ///   set by device implementations when adding a Behaviour.
    /// @{
    DsBinaryInputType hardwareInputType; ///< the input type when device has hardwired functions
    DsUsageHint inputUsage; ///< the input type when device has hardwired functions
    bool reportsChanges; ///< set if the input detects changes without polling
    MLMicroSeconds updateInterval;
    /// @}

    /// @name persistent settings
    /// @{
    DsBinaryInputType configuredInputType; ///< the configurable input type (aka Sensor Function)
    MLMicroSeconds minPushInterval; ///< minimum time between two state pushes
    MLMicroSeconds changesOnlyInterval; ///< time span during which only actual value changes are reported. After this interval, next hardware sensor update, even without value change, will cause a push)
    /// @}


    /// @name internal volatile state
    /// @{
    bool currentState; ///< current input value
    MLMicroSeconds lastUpdate; ///< time of last update from hardware
    MLMicroSeconds lastPush; ///< time of last push
    /// @}


  public:

    /// constructor
    BinaryInputBehaviour(Device &aDevice);

    /// initialisation of hardware-specific constants for this binary input
    /// @note this must be called once before the device gets added to the device container. Implementation might
    ///   also derive default values for settings from this information.
    void setHardwareInputConfig(DsBinaryInputType aInputType, DsUsageHint aUsage, bool aReportsChanges, MLMicroSeconds aUpdateInterval);


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// button action occurred
    /// @param aNewState the new state of the input
    void updateInputState(bool aNewState);

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    /// the behaviour type
    virtual BehaviourType getType() { return behaviour_binaryinput; };

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
  typedef boost::intrusive_ptr<BinaryInputBehaviour> BinaryInputBehaviourPtr;
  
  
  
} // namespace p44

#endif /* defined(__vdcd__binaryinputbehaviour__) */
