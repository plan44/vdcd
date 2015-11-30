//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__buttonbehaviour__
#define __vdcd__buttonbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {

  /// Implements the behaviour of a digitalSTROM button, in particular the
  /// state machine which generates the different click types for the dS upstream
  /// from button press + button release events.
  /// This class should be used as-is for any virtual device which represents
  /// a user button or rocker switch.
  class ButtonBehaviour : public DsBehaviour
  {
    typedef DsBehaviour inherited;

    friend class Device;
    friend class DeviceContainer; // for local mode

  protected:

    /// @name hardware derived parameters (constant during operation)
    ///   fixed constants or values set ONCE by device implementations when adding a ButtonBehaviour.
    ///   These need to remain constant after device initialisation is complete!
    /// @{
    bool supportsLocalKeyMode; ///< set if this button can act as local button
    int buttonID; ///< the ID grouping all inputs of a hardware button (which can have multiple elements)
    DsButtonType buttonType; ///< type of button
    DsButtonElement buttonElementID; ///< identifies element of a multi-input button hardware-button
    DsButtonMode fixedButtonMode; ///< if not buttonMode_inactive, then this is the only mode that can be set
    /// @}

    /// @name persistent settings
    /// @{
    DsGroup buttonGroup; ///< the group this button belongs to
    DsButtonFunc buttonFunc; ///< the button function (LTNUM)
    DsButtonMode buttonMode; ///< the button mode (LTMODE)
    DsChannelType buttonChannel; ///< the channel the button is supposed to control
    bool setsLocalPriority; ///< button should set local priority
    bool callsPresent; ///< button should call "present" scene
    /// @}


    /// @name internal volatile state
    /// @{
    bool buttonPressed; ///< set if button is currently pressed
    MLMicroSeconds lastAction; ///< time of last clickType or actionMode/actionScene update

    DsClickType clickType; ///< set to last click type of button

    DsButtonActionMode actionMode; ///< action mode
    uint8_t actionId; ///< action Id (aka scene number)
    /// @}

  public:
  
    /// constructor
    ButtonBehaviour(Device &aDevice);

    /// initialisation of hardware-specific constants for this button input
    /// @param aButtonID the ID of the physical button (all inputs belonging to a single physical button
    ///   like a 2-way rocker or a 4-way navigation button must have the same buttonID. Multiple physical buttons must have distinct IDs)
    /// @param aType the physical button's type.
    /// @param aElement the element of the physical button this input represents (like: up or down for a 2-way rocker)
    /// @param aSupportsLocalKeyMode true if this button can be local key
    /// @param aCounterPartIndex for 2-way buttons, this identifies the index of the counterpart input (needed for dS 1.0 LTMODE compatibility only)
    /// @param aButtonModeFixed if set, buttonMode can only be switched between hardware-derived default mode and disabled
    /// @note this must be called once before the device gets added to the device container. Implementation might
    ///   also derive default values for settings from this information.
    void setHardwareButtonConfig(int aButtonID, DsButtonType aType, DsButtonElement aElement, bool aSupportsLocalKeyMode, int aCounterPartIndex, bool aButtonModeFixed);

    /// set group
    virtual void setGroup(DsGroup aGroup) { buttonGroup = aGroup; };


    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// button action occurred
    /// @param aPressed true if action is button pressed, false if action is button released
    void buttonAction(bool aPressed);

    /// send direct action
    /// @param aActionMode the mode how to send the action
    /// @param aSceneNo the scene number to send
    void sendAction(DsButtonActionMode aActionMode, uint8_t aActionId);

    /// @}

    
    /// @return button element that defines the function of this button in local operation modes
    DsButtonElement localFunctionElement();

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    /// the behaviour type
    virtual BehaviourType getType() { return behaviour_button; };

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
    enum {
      buttonflag_firstflag = 0x0001,
      buttonflag_setsLocalPriority = buttonflag_firstflag<<0,
      buttonflag_callsPresent = buttonflag_firstflag<<1,
      buttonflag_nextflag = buttonflag_firstflag<<2
    };
        virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  private:

    /// @name button state machine v2.01
    /// @{

    /// button states
    typedef enum {
      S0_idle,
      S1_initialpress,
      S2_holdOrTip,
      S3_hold,
      S4_nextTipWait,
      S5_nextPauseWait,
      S6_2ClickWait,
      S7_progModeWait,
      S8_awaitrelease,
      S9_2pauseWait,
      // S10 missing
      S11_localdim,
      S12_3clickWait,
      S13_3pauseWait,
      S14_awaitrelease, // duplicate of S8
    } ButtonState;

    // state machine vars
    ButtonState state;
    int clickCounter;
    int holdRepeats;
    bool outputOn;
    bool localButtonEnabled;
    bool dimmingUp;
    MLMicroSeconds timerRef;
    long buttonStateMachineTicket;

    // state machine params
    static const int t_long_function_delay = 500*MilliSecond;
    static const int t_dim_repeat_time = 1000*MilliSecond;
    static const int t_click_length = 140*MilliSecond;
    static const int t_click_pause = 140*MilliSecond;
    static const int t_tip_timeout = 800*MilliSecond;
    static const int t_local_dim_timeout = 160*MilliSecond;
    static const int max_hold_repeats = 30;

    // methods
    void resetStateMachine();
    void checkStateMachine(bool aButtonChange, MLMicroSeconds aNow);
    void localSwitchOutput();
    void localDim();
    void sendClick(DsClickType aClickType);
    
    /// @}

  };

  typedef boost::intrusive_ptr<ButtonBehaviour> ButtonBehaviourPtr;

}


#endif /* defined(__vdcd__buttonbehaviour__) */
