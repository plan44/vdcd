//
//  binaryinputbehaviour.hpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__binaryinputbehaviour__
#define __vdcd__binaryinputbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {


  class BinaryInputBehaviour : public DsBehaviour
  {
    typedef DsBehaviour inherited;

  protected:

    /// @name behaviour description, constants or variables
    ///   set by device implementations when adding a Behaviour.
    /// @{
    DsBinaryInputType hardwareInputType; ///< the input type when device has hardwired functions
    bool reportsChanges; ///< set if the input detects changes without polling
    MLMicroSeconds updateInterval;
    /// @}

    /// @name persistent settings
    /// @{
    DsBinaryInputType configuredInputType; ///< the configurable input type (aka Sensor Function)
    MLMicroSeconds minPushInterval; ///< minimum time between two state pushes
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
    void setHardwareInputConfig(DsBinaryInputType aInputType, bool aReportsChanges, MLMicroSeconds aUpdateInterval);


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
    virtual const PropertyDescriptor *getDescDescriptor(int aPropIndex);
    virtual int numSettingsProps();
    virtual const PropertyDescriptor *getSettingsDescriptor(int aPropIndex);
    virtual int numStateProps();
    virtual const PropertyDescriptor *getStateDescriptor(int aPropIndex);
    // combined field access for all types of properties
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

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
