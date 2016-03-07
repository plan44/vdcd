//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0

#include "buttonbehaviour.hpp"
#include "outputbehaviour.hpp"


using namespace p44;



ButtonBehaviour::ButtonBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  buttonGroup(group_yellow_light),
  buttonMode(buttonMode_inactive), // none by default, hardware should set a default matching the actual HW capabilities
  fixedButtonMode(buttonMode_inactive), // by default, mode can be set. Hardware may fix the possible mode
  buttonChannel(channeltype_default), // by default, buttons act on default channel
  buttonFunc(buttonFunc_room_preset0x), // act as room button by default
  setsLocalPriority(false),
  clickType(ct_none),
  actionMode(buttonActionMode_none),
  actionId(0),
  buttonPressed(false),
  lastAction(Never),
  buttonStateMachineTicket(0),
  callsPresent(false),
  buttonActionMode(buttonActionMode_none),
  buttonActionId(0),
  stateMachineMode(statemachine_standard)
{
  // set default hrdware configuration
  setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0, false);
  // reset the button state machine
  resetStateMachine();
}


void ButtonBehaviour::setHardwareButtonConfig(int aButtonID, DsButtonType aType, DsButtonElement aElement, bool aSupportsLocalKeyMode, int aCounterPartIndex, bool aButtonModeFixed)
{
  buttonID = aButtonID;
  buttonType = aType;
  buttonElementID = aElement;
  supportsLocalKeyMode = aSupportsLocalKeyMode;
  // now derive default settings from hardware
  // - default to standard mode
  buttonMode = buttonMode_standard;
  // - modify for 2-way
  if (buttonType==buttonType_2way) {
    // part of a 2-way button.
    if (buttonElementID==buttonElement_up) {
      buttonMode = (DsButtonMode)((int)buttonMode_rockerUp_pairWith0+aCounterPartIndex);
    }
    else if (buttonElementID==buttonElement_down) {
      buttonMode = (DsButtonMode)((int)buttonMode_rockerDown_pairWith0+aCounterPartIndex);
    }
  }
  if (aButtonModeFixed) {
    // limit settings to this mode
    fixedButtonMode = buttonMode;
  }
}



void ButtonBehaviour::buttonAction(bool aPressed)
{
  BLOG(LOG_NOTICE, "Button[%zu] '%s' was %s", index, hardwareName.c_str(), aPressed ? "pressed" : "released");
  bool stateChanged = aPressed!=buttonPressed;
  buttonPressed = aPressed; // remember new state
  // check which statemachine to use
  if (buttonMode==buttonMode_turbo || stateMachineMode!=statemachine_standard) {
    // use custom state machine
    checkCustomStateMachine(stateChanged, MainLoop::now());
  }
  else {
    // use regular state machine
    checkStandardStateMachine(stateChanged, MainLoop::now());
  }
}


void ButtonBehaviour::resetStateMachine()
{
  buttonPressed = false;
  state = S0_idle;
  clickCounter = 0;
  holdRepeats = 0;
  dimmingUp = false;
  timerRef = Never;
  MainLoop::currentMainLoop().cancelExecutionTicket(buttonStateMachineTicket);
}



#if FOCUSLOGGING
static const char *stateNames[] = {
  "S0_idle",
  "S1_initialpress",
  "S2_holdOrTip",
  "S3_hold",
  "S4_nextTipWait",
  "S5_nextPauseWait",
  "S6_2ClickWait",
  "S7_progModeWait",
  "S8_awaitrelease",
  "S9_2pauseWait",
  // S10 missing
  "S11_localdim",
  "S12_3clickWait",
  "S13_3pauseWait",
  "S14_awaitrelease"
};
#endif




// plan44 "turbo" state machine which can tolerate missing a "press" or a "release" event
// Note: only to be called when button state changes
void ButtonBehaviour::checkCustomStateMachine(bool aStateChanged, MLMicroSeconds aNow)
{
  MLMicroSeconds timeSinceRef = aNow-timerRef;
  timerRef = aNow;

  if (buttonMode==buttonMode_turbo || stateMachineMode==statemachine_simple) {
    BFOCUSLOG("simple button state machine entered in state %s at reference time %d and clickCounter=%d", stateNames[state], (int)(timeSinceRef/MilliSecond), clickCounter);
    // reset click counter if tip timeout has passed since last event
    if (timeSinceRef>t_tip_timeout) {
      clickCounter = 0;
    }
    // use Idle and Awaitrelease states only to remember previous button state detected
    bool isTip = false;
    if (buttonPressed) {
      // the button was pressed right now
      // - always count button press as a tip
      isTip = true;
      // - state is now Awaitrelease
      state = S14_awaitrelease;
    }
    else {
      // the button was released right now
      // - if we haven't seen a press before, assume the press got lost and act on the release
      if (state==S0_idle) {
        isTip = true;
      }
      // - state is now idle again
      state = S0_idle;
    }
    if (isTip) {
      if (isLocalButtonEnabled() && clickCounter==0) {
        // first tip switches local output if local button is enabled
        localSwitchOutput();
      }
      else {
        // other tips are sent upstream
        sendClick((DsClickType)(ct_tip_1x+clickCounter));
        clickCounter++;
        if (clickCounter>=4) clickCounter = 0; // wrap around
      }
    }
  }
  else if (stateMachineMode==statemachine_dimmer) {
    BFOCUSLOG("dimmer button state machine entered");
    // just issue hold and stop events (e.g. for volume)
    if (aStateChanged) {
      if (isLocalButtonEnabled() && isOutputOn()) {
        // local dimming start/stop
        localDim(buttonPressed);
      }
      else {
        // not local button mode
        if (buttonPressed) {
          BFOCUSLOG("started dimming - sending ct_hold_start");
          // button just pressed
          sendClick(ct_hold_start);
          // schedule hold repeats
          holdRepeats = 0;
          MainLoop::currentMainLoop().executeTicketOnce(buttonStateMachineTicket, boost::bind(&ButtonBehaviour::dimRepeat, this), t_dim_repeat_time);
        }
        else {
          // button just released
          BFOCUSLOG("stopped dimming - sending ct_hold_end");
          sendClick(ct_hold_end);
          MainLoop::currentMainLoop().cancelExecutionTicket(buttonStateMachineTicket);
        }
      }
    }
  }
  else {
    BLOG(LOG_ERR, "invalid stateMachineMode");
  }
}


void ButtonBehaviour::dimRepeat()
{
  buttonStateMachineTicket = 0;
  // button still pressed
  BFOCUSLOG("dimming in progress - sending ct_hold_repeat (repeatcount = %d)", holdRepeats);
  sendClick(ct_hold_repeat);
  holdRepeats++;
  if (holdRepeats<max_hold_repeats) {
    // schedule next repeat
    buttonStateMachineTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&ButtonBehaviour::dimRepeat, this), t_dim_repeat_time);
  }
}



// standard button state machine
void ButtonBehaviour::checkStandardStateMachine(bool aStateChanged, MLMicroSeconds aNow)
{
  MainLoop::currentMainLoop().cancelExecutionTicket(buttonStateMachineTicket);
  MLMicroSeconds timeSinceRef = aNow-timerRef;

  BFOCUSLOG("button state machine entered in state %s at reference time %d and clickCounter=%d", stateNames[state], (int)(timeSinceRef/MilliSecond), clickCounter);
  switch (state) {

    case S0_idle :
      timerRef = Never; // no timer running
      if (aStateChanged && buttonPressed) {
        clickCounter = isLocalButtonEnabled() ? 0 : 1;
        timerRef = aNow;
        state = S1_initialpress;
      }
      break;

    case S1_initialpress :
      if (aStateChanged && !buttonPressed) {
        timerRef = aNow;
        state = S5_nextPauseWait;
      }
      else if (timeSinceRef>=t_click_length) {
        state = S2_holdOrTip;
      }
      break;

    case S2_holdOrTip:
      if (aStateChanged && !buttonPressed && clickCounter==0) {
        localSwitchOutput();
        timerRef = aNow;
        clickCounter = 1;
        state = S4_nextTipWait;
      }
      else if (aStateChanged && !buttonPressed && clickCounter>0) {
        sendClick((DsClickType)(ct_tip_1x+clickCounter-1));
        timerRef = aNow;
        state = S4_nextTipWait;
      }
      else if (timeSinceRef>=t_long_function_delay) {
        // long function
        if (!isLocalButtonEnabled() || !isOutputOn()) {
          // hold
          holdRepeats = 0;
          timerRef = aNow;
          sendClick(ct_hold_start);
          state = S3_hold;
        }
        else if (isLocalButtonEnabled() && isOutputOn()) {
          // local dimming
          localDim(true); // start dimming
          state = S11_localdim;
        }
      }
      break;

    case S3_hold:
      if (aStateChanged && !buttonPressed) {
        // no packet send time, skip S15
        sendClick(ct_hold_end);
        state = S0_idle;
      }
      else if (timeSinceRef>=t_dim_repeat_time) {
        if (holdRepeats<max_hold_repeats) {
          timerRef = aNow;
          sendClick(ct_hold_repeat);
          holdRepeats++;
        }
        else {
          sendClick(ct_hold_end);
          state = S14_awaitrelease;
        }
      }
      break;

    case S4_nextTipWait:
      if (aStateChanged && buttonPressed) {
        timerRef = aNow;
        if (clickCounter>=4)
          clickCounter = 2;
        else
          clickCounter++;
        state = S2_holdOrTip;
      }
      else if (timeSinceRef>=t_tip_timeout) {
        state = S0_idle;
      }
      break;

    case S5_nextPauseWait:
      if (aStateChanged && buttonPressed) {
        timerRef = aNow;
        clickCounter = 2;
        state = S6_2ClickWait;
      }
      else if (timeSinceRef>=t_click_pause) {
        if (isLocalButtonEnabled())
          localSwitchOutput();
        else
          sendClick(ct_click_1x);
        state = S4_nextTipWait;
      }
      break;

    case S6_2ClickWait:
      if (aStateChanged && !buttonPressed) {
        timerRef = aNow;
        state = S9_2pauseWait;
      }
      else if (timeSinceRef>t_click_length) {
        state = S7_progModeWait;
      }
      break;

    case S7_progModeWait:
      if (aStateChanged && !buttonPressed) {
        sendClick(ct_tip_2x);
        timerRef = aNow;
        state = S4_nextTipWait;
      }
      else if (timeSinceRef>t_long_function_delay) {
        sendClick(ct_short_long);
        state = S8_awaitrelease;
      }
      break;

    case S9_2pauseWait:
      if (aStateChanged && buttonPressed) {
        timerRef = aNow;
        clickCounter = 3;
        state = S12_3clickWait;
      }
      else if (timeSinceRef>=t_click_pause) {
        sendClick(ct_click_2x);
        state = S4_nextTipWait;
      }
      break;

    case S12_3clickWait:
      if (aStateChanged && !buttonPressed) {
        timerRef = aNow;
        sendClick(ct_click_3x);
        state = S4_nextTipWait;
      }
      else if (timeSinceRef>=t_click_length) {
        state = S13_3pauseWait;
      }
      break;

    case S13_3pauseWait:
      if (aStateChanged && !buttonPressed) {
        timerRef = aNow;
        sendClick(ct_tip_3x);
      }
      else if (timeSinceRef>=t_long_function_delay) {
        sendClick(ct_short_short_long);
        state = S8_awaitrelease;
      }
      break;

    case S11_localdim:
      if (aStateChanged && !buttonPressed) {
        state = S0_idle;
        localDim(dimmode_stop); // stop dimming
      }
      break;

    case S8_awaitrelease:
    case S14_awaitrelease:
      if (aStateChanged && !buttonPressed) {
        state = S0_idle;
      }
      break;
  }
  BFOCUSLOG(" -->                       exit state %s with %sfurther timing needed", stateNames[state], timerRef!=Never ? "" : "NO ");
  if (timerRef!=Never) {
    // need timing, schedule calling again
    buttonStateMachineTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&ButtonBehaviour::checkStandardStateMachine, this, false, _1), aNow+10*MilliSecond);
  }
}


DsButtonElement ButtonBehaviour::localFunctionElement()
{
  if (buttonType!=buttonType_undefined) {
    // hardware defines the button
    return buttonElementID;
  }
  // default to center
  return buttonElement_center;
}


bool ButtonBehaviour::isLocalButtonEnabled()
{
  return supportsLocalKeyMode && buttonFunc==buttonFunc_device;
}


bool ButtonBehaviour::isOutputOn()
{
  if (device.output) {
    ChannelBehaviourPtr ch = device.output->getChannelByType(channeltype_default);
    if (ch) {
      return ch->getChannelValue()>0; // on if channel is above zero
    }
  }
  return false; // no output or channel -> is not on
}


DsDimMode ButtonBehaviour::twoWayDirection()
{
  if (buttonMode<buttonMode_rockerDown_pairWith0 || buttonMode>buttonMode_rockerUp_pairWith3) return dimmode_stop; // single button -> no direction
  return buttonMode>=buttonMode_rockerDown_pairWith0 && buttonMode<=buttonMode_rockerDown_pairWith3 ? dimmode_down : dimmode_up; // down = -1, up = 1
}



void ButtonBehaviour::localSwitchOutput()
{
  BLOG(LOG_NOTICE, "Button[%zu] '%s': Local switch", index, hardwareName.c_str());
  int dir = twoWayDirection();
  if (dir==0) {
    // single button, toggle
    dir = isOutputOn() ? -1 : 1;
  }
  // actually switch output
  if (device.output) {
    ChannelBehaviourPtr ch = device.output->getChannelByType(channeltype_default);
    if (ch) {
      ch->setChannelValue(dir>0 ? ch->getMax() : ch->getMin());
      device.requestApplyingChannels(NULL, false);
    }
  }
  // send status
  sendClick(dir>0 ? ct_local_on : ct_local_off);
}


void ButtonBehaviour::localDim(bool aStart)
{
  BLOG(LOG_NOTICE, "Button[%zu] '%s': Local dim %s", index, hardwareName.c_str(), aStart ? "START" : "STOP");
  if (device.output) {
    if (aStart) {
      // start dimming, determine direction (directly from two-way buttons or via toggling direction for single buttons)
      DsDimMode dm = twoWayDirection();
      if (dm==dimmode_stop) {
        // not two-way, need to toggle direction
        dimmingUp = !dimmingUp; // change direction
        dm = dimmingUp ? dimmode_up : dimmode_down;
      }
      device.dimChannel(channeltype_default, dm);
    }
    else {
      // just stop
      device.dimChannel(channeltype_default, dimmode_stop);
    }
  }
}



void ButtonBehaviour::sendClick(DsClickType aClickType)
{
  // check for p44-level scene buttons
  if (buttonActionMode!=buttonActionMode_none && (aClickType==ct_tip_1x || aClickType==ct_click_1x)) {
    // trigger direct scene action for single clicks
    sendAction(buttonActionMode, buttonActionId);
    return;
  }
  // update button state
  lastAction = MainLoop::now();
  clickType = aClickType;
  actionMode = buttonActionMode_none;
  // button press is considered a (regular!) user action, have it checked globally first
  if (!device.getDeviceContainer().signalDeviceUserAction(device, true)) {
    // button press not consumed on global level, forward to upstream dS
    BLOG(LOG_NOTICE,
      "Button[%zu] '%s' pushes value = %d, clickType %d",
      index, hardwareName.c_str(), buttonPressed, aClickType
    );
    // issue a state property push
    pushBehaviourState();
    // also let device container know for local click handling
    // TODO: more elegant solution for this
    device.getDeviceContainer().checkForLocalClickHandling(*this, aClickType);
  }
}


void ButtonBehaviour::sendAction(DsButtonActionMode aActionMode, uint8_t aActionId)
{
  lastAction = MainLoop::now();
  actionMode = aActionMode;
  actionId = aActionId;
  BLOG(LOG_NOTICE,
    "Button[%zu] '%s' pushes actionMode = %d, actionId %d",
    index, hardwareName.c_str(), actionMode, actionId
  );
  // issue a state property push
  pushBehaviourState();
}



#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *ButtonBehaviour::tableName()
{
  return "ButtonSettings";
}



// data field definitions

static const size_t numFields = 8;

size_t ButtonBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *ButtonBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "dsGroup", SQLITE_INTEGER }, // Note: don't call a SQL field "group"!
    { "buttonFunc", SQLITE_INTEGER },  // ACTUALLY: buttonMode! (harmless old bug, but DB field names are misleading)
    { "buttonGroup", SQLITE_INTEGER }, // ACTUALLY: buttonFunc! (harmless old bug, but DB field names are misleading)
    { "buttonFlags", SQLITE_INTEGER },
    { "buttonChannel", SQLITE_INTEGER },
    { "buttonActionMode", SQLITE_INTEGER },
    { "buttonActionId", SQLITE_INTEGER },
    { "buttonSMMode", SQLITE_INTEGER },
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}

// Buggy (but functionally harmless) mapping as per 2016-01-11
//  DB                    actual property
//  --------------------- -----------------------
//  dsGroup               buttonGroup
//  buttonFunc            buttonMode    // WRONG
//  buttonGroup           buttonFunc    // WRONG
//  buttonFlags           flags
//  buttonChannel         buttonChannel
//  ...all ok from here


/// load values from passed row
void ButtonBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inherited::loadFromRow(aRow, aIndex, NULL); // no common flags in base class
  // get the fields
  aRow->getCastedIfNotNull<DsGroup, int>(aIndex++, buttonGroup);
  aRow->getCastedIfNotNull<DsButtonMode, int>(aIndex++, buttonMode);
  if (buttonMode!=buttonMode_inactive && fixedButtonMode!=buttonMode_inactive && buttonMode!=fixedButtonMode) {
    // force mode according to fixedButtonMode, even if settings (from older versions) say something different
    buttonMode = fixedButtonMode;
  }
  aRow->getCastedIfNotNull<DsButtonFunc, int>(aIndex++, buttonFunc);
  uint64_t flags = aRow->getWithDefault<int>(aIndex++, 0);
  aRow->getCastedIfNotNull<DsChannelType, int>(aIndex++, buttonChannel);
  aRow->getCastedIfNotNull<DsButtonActionMode, int>(aIndex++, buttonActionMode);
  aRow->getCastedIfNotNull<uint8_t, int>(aIndex++, buttonActionId);
  if (!aRow->getCastedIfNotNull<ButtonStateMachineMode, int>(aIndex++, stateMachineMode)) {
    // no value yet for stateMachineMode -> old simpleStateMachine flag is still valid
    if (flags & buttonflag_OBSOLETE_simpleStateMachine) stateMachineMode = statemachine_simple; // flag is set, use simple state machine mode
  }
  // decode the flags
  setsLocalPriority = flags & buttonflag_setsLocalPriority;
  callsPresent = flags & buttonflag_callsPresent;
  // pass the flags out to subclasses which call this superclass to get the flags (and decode themselves)
  if (aCommonFlagsP) *aCommonFlagsP = flags;
}


// bind values to passed statement
void ButtonBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // encode the flags
  int flags = 0;
  if (setsLocalPriority) flags |= buttonflag_setsLocalPriority;
  if (callsPresent) flags |= buttonflag_callsPresent;
  // bind the fields
  aStatement.bind(aIndex++, buttonGroup);
  aStatement.bind(aIndex++, buttonMode);
  aStatement.bind(aIndex++, buttonFunc);
  aStatement.bind(aIndex++, flags);
  aStatement.bind(aIndex++, buttonChannel);
  aStatement.bind(aIndex++, buttonActionMode);
  aStatement.bind(aIndex++, buttonActionId);
  aStatement.bind(aIndex++, stateMachineMode);
}



#pragma mark - property access

static char button_key;

// description properties

enum {
  supportsLocalKeyMode_key,
  buttonID_key,
  buttonType_key,
  buttonElementID_key,
  numDescProperties
};


int ButtonBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptorPtr ButtonBehaviour::getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numDescProperties] = {
    { "supportsLocalKeyMode", apivalue_bool, supportsLocalKeyMode_key+descriptions_key_offset, OKEY(button_key) },
    { "buttonID", apivalue_uint64, buttonID_key+descriptions_key_offset, OKEY(button_key) },
    { "buttonType", apivalue_uint64, buttonType_key+descriptions_key_offset, OKEY(button_key) },
    { "buttonElementID", apivalue_uint64, buttonElementID_key+descriptions_key_offset, OKEY(button_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// settings properties

enum {
  group_key,
  mode_key,
  function_key,
  channel_key,
  setsLocalPriority_key,
  callsPresent_key,
  buttonActionMode_key,
  buttonActionId_key,
  stateMachineMode_key,
  numSettingsProperties
};


int ButtonBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptorPtr ButtonBehaviour::getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSettingsProperties] = {
    { "group", apivalue_uint64, group_key+settings_key_offset, OKEY(button_key) },
    { "mode", apivalue_uint64, mode_key+settings_key_offset, OKEY(button_key) },
    { "function", apivalue_uint64, function_key+settings_key_offset, OKEY(button_key) },
    { "channel", apivalue_uint64, channel_key+settings_key_offset, OKEY(button_key) },
    { "setsLocalPriority", apivalue_bool, setsLocalPriority_key+settings_key_offset, OKEY(button_key) },
    { "callsPresent", apivalue_bool, callsPresent_key+settings_key_offset, OKEY(button_key) },
    { "x-p44-buttonActionMode", apivalue_uint64, buttonActionMode_key+settings_key_offset, OKEY(button_key) },
    { "x-p44-buttonActionId", apivalue_uint64, buttonActionId_key+settings_key_offset, OKEY(button_key) },
    { "x-p44-stateMachineMode", apivalue_uint64, stateMachineMode_key+settings_key_offset, OKEY(button_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}

// state properties

enum {
  value_key,
  clickType_key,
  actionMode_key,
  actionId_key,
  age_key,
  numStateProperties
};


int ButtonBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptorPtr ButtonBehaviour::getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numStateProperties] = {
    { "value", apivalue_bool, value_key+states_key_offset, OKEY(button_key) },
    { "clickType", apivalue_uint64, clickType_key+states_key_offset, OKEY(button_key) },
    { "actionMode", apivalue_uint64, actionMode_key+states_key_offset, OKEY(button_key) },
    { "actionId", apivalue_uint64, actionId_key+states_key_offset, OKEY(button_key) },
    { "age", apivalue_double, age_key+states_key_offset, OKEY(button_key) },
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


// access to all fields

bool ButtonBehaviour::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(button_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Description properties
        case supportsLocalKeyMode_key+descriptions_key_offset:
          aPropValue->setBoolValue(supportsLocalKeyMode);
          return true;
        case buttonID_key+descriptions_key_offset:
          aPropValue->setUint64Value(buttonID);
          return true;
        case buttonType_key+descriptions_key_offset:
          aPropValue->setUint64Value(buttonType);
          return true;
        case buttonElementID_key+descriptions_key_offset:
          aPropValue->setUint64Value(buttonElementID);
          return true;
        // Settings properties
        case group_key+settings_key_offset:
          aPropValue->setUint16Value(buttonGroup);
          return true;
        case mode_key+settings_key_offset:
          aPropValue->setUint64Value(buttonMode);
          return true;
        case function_key+settings_key_offset:
          aPropValue->setUint64Value(buttonFunc);
          return true;
        case channel_key+settings_key_offset:
          aPropValue->setUint64Value(buttonChannel);
          return true;
        case setsLocalPriority_key+settings_key_offset:
          aPropValue->setBoolValue(setsLocalPriority);
          return true;
        case callsPresent_key+settings_key_offset:
          aPropValue->setBoolValue(callsPresent);
          return true;
        case buttonActionMode_key+settings_key_offset:
          aPropValue->setUint8Value(buttonActionMode);
          return true;
        case buttonActionId_key+settings_key_offset:
          aPropValue->setUint8Value(buttonActionId);
          return true;
        case stateMachineMode_key+settings_key_offset:
          aPropValue->setUint8Value(stateMachineMode);
          return true;
        // States properties
        case value_key+states_key_offset:
          if (lastAction==Never)
            aPropValue->setNull();
          else
            aPropValue->setBoolValue(buttonPressed);
          return true;
        case clickType_key+states_key_offset:
          // click type is available only if last actions was a regular click
          if (actionMode!=buttonActionMode_none) return false;
          aPropValue->setUint64Value(clickType);
          return true;
        case actionMode_key+states_key_offset:
          // actionMode is available only if last actions was direct action
          if (actionMode==buttonActionMode_none) return false;
          aPropValue->setUint64Value(actionMode);
          return true;
        case actionId_key+states_key_offset:
          // actionId is available only if last actions was direct action
          if (actionMode==buttonActionMode_none) return false;
          aPropValue->setUint64Value(actionId);
          return true;
        case age_key+states_key_offset:
          // age
          if (lastAction==Never)
            aPropValue->setNull();
          else
            aPropValue->setDoubleValue((double)(MainLoop::now()-lastAction)/Second);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case group_key+settings_key_offset:
          setGroup((DsGroup)aPropValue->int32Value());
          return true;
        case mode_key+settings_key_offset: {
          DsButtonMode m = (DsButtonMode)aPropValue->int32Value();
          if (m!=buttonMode_inactive && fixedButtonMode!=buttonMode_inactive) {
            // only one particular mode (aside from inactive) is allowed.
            m = fixedButtonMode;
          }
          setPVar(buttonMode, m);
          return true;
        }
        case function_key+settings_key_offset:
          setFunction((DsButtonFunc)aPropValue->int32Value());
          return true;
        case channel_key+settings_key_offset:
          setPVar(buttonChannel, (DsChannelType)aPropValue->int32Value());
          return true;
        case setsLocalPriority_key+settings_key_offset:
          setPVar(setsLocalPriority, aPropValue->boolValue());
          return true;
        case callsPresent_key+settings_key_offset:
          setPVar(callsPresent, aPropValue->boolValue());
          return true;
        case buttonActionMode_key+settings_key_offset:
          setPVar(buttonActionMode, (DsButtonActionMode)aPropValue->uint8Value());
          return true;
        case buttonActionId_key+settings_key_offset:
          setPVar(buttonActionId, aPropValue->uint8Value());
          return true;
        case stateMachineMode_key+settings_key_offset:
          setPVar(stateMachineMode, (ButtonStateMachineMode)aPropValue->uint8Value());
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - description/shortDesc


string ButtonBehaviour::description()
{
  string s = string_format("%s behaviour", shortDesc().c_str());
  string_format_append(s, "\n- buttonID: %d, buttonType: %d, buttonElementID: %d", buttonID, buttonType, buttonElementID);
  string_format_append(s, "\n- buttonChannel: %d, buttonFunc: %d, buttonmode/LTMODE: %d", buttonChannel, buttonFunc, buttonMode);
  s.append(inherited::description());
  return s;
}



