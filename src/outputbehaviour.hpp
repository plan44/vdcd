//
//  outputbehaviour.hpp
//  vdcd
//
//  Created by Lukas Zeller on 23.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__outputbehaviour__
#define __vdcd__outputbehaviour__

#include "device.hpp"
#include "dsbehaviour.hpp"

using namespace std;

namespace p44 {


  class OutputBehaviour : public DsBehaviour
  {
    typedef DsBehaviour inherited;


  protected:

    /// @name hardware derived parameters (constant during operation)
    /// @{
    DsOutputFunction outputFunction; ///< the function of the output
    bool variableRamp; ///< output has variable ramp times
    double maxPower; ///< max power in Watts the output can control
    /// @}


    /// @name persistent settings
    /// @{
    DsOutputMode outputMode; ///< the mode of the output
    bool pushChanges; ///< if set, local changes to output will be pushed upstreams
    /// @}


    /// @name internal volatile state
    /// @{
    bool outputUpdatePending; ///< set if cachedOutputValue represents a value to be transmitted to the hardware
    int32_t cachedOutputValue; ///< the cached output value
    MLMicroSeconds outputLastSent; ///< Never if the cachedOutputValue is not yet applied to the hardware, otherwise when it was sent
    MLMicroSeconds nextTransitionTime; ///< the transition time to use for the next output value change
    /// @}

  public:

    OutputBehaviour(Device &aDevice);

    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// Configure hardware parameters of the output
    void setHardwareOutputConfig(DsOutputFunction aOutputFunction, bool aVariableRamp, double aMaxPower);

    /// set actual current output value as read from the device on startup, to update local cache value
    /// @param aActualOutputValue the value as read from the device
    /// @note only used at startup to get the inital value FROM the hardware.
    ///   NOT to be used to change the hardware output value!
    void initOutputValue(uint32_t aActualOutputValue);

    /// the value to be set in the hardware
    /// @return value to be set in actual hardware
    int32_t valueForHardware() { return cachedOutputValue; };

    /// the transition time to use to change value in the hardware
    /// @return transition time
    MLMicroSeconds transitionTimeForHardware() { return nextTransitionTime; };

    /// to be called when output value has been successfully applied to hardware
    void outputValueApplied();

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// apply scene to output
    /// @param aScene the scene to apply to the output
    virtual void applyScene(DsScenePtr aScene) { /* NOP in base class */ };

    /// capture current state into passed scene object
    /// @param aScene the scene object to update
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void captureScene(DsScenePtr aScene) { /* NOP in base class */ };

    /// get currently set output value from device hardware
    /// @param aOutputBehaviour the output behaviour which wants to know the output value as set in the hardware
    virtual int32_t getOutputValue();

    /// set new output value on device
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int32_t aNewValue, MLMicroSeconds aTransitionTime=0);

    /// switch on at minimum brightness if not already on (needed for callSceneMin), only relevant for lights
    virtual void onAtMinBrightness() { /* NOP in base class, only relevant for lights */ };


    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    // the behaviour type
    virtual BehaviourType getType() { return behaviour_output; };

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
  
  typedef boost::intrusive_ptr<OutputBehaviour> OutputBehaviourPtr;

} // namespace p44

#endif /* defined(__vdcd__outputbehaviour__) */
