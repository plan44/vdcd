//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


  class OutputBehaviour : public DsBehaviour
  {
    typedef DsBehaviour inherited;


  protected:

    /// @name hardware derived parameters (constant during operation)
    /// @{
    DsOutputFunction outputFunction; ///< the function of the output
    DsUsageHint outputUsage; ///< the input type when device has hardwired functions
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
    void setHardwareOutputConfig(DsOutputFunction aOutputFunction, DsUsageHint aUsage, bool aVariableRamp, double aMaxPower);

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

    /// get currently set output value from device hardware
    /// @param aOutputBehaviour the output behaviour which wants to know the output value as set in the hardware
    virtual int32_t getOutputValue();

    /// set new output value on device
    /// @param aValue the new output value
    /// @param aTransitionTime time in microseconds to be spent on transition from current to new logical brightness (if possible in hardware)
    virtual void setOutputValue(int32_t aNewValue, MLMicroSeconds aTransitionTime=0);

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

    // property access implementation for descriptor/settings/states
    virtual int numDescProps();
    virtual const PropertyDescriptor *getDescDescriptor(int aPropIndex);
    virtual int numSettingsProps();
    virtual const PropertyDescriptor *getSettingsDescriptor(int aPropIndex);
    virtual int numStateProps();
    virtual const PropertyDescriptor *getStateDescriptor(int aPropIndex);
    // combined field access for all types of properties
    virtual bool accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

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
