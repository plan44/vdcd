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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set DEBUGFOCUS to 1 to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define DEBUGFOCUS 0

#include "buttonbehaviour.hpp"


using namespace p44;



ButtonBehaviour::ButtonBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  buttonGroup(group_yellow_light),
  buttonMode(buttonMode_inactive), // none by default, hardware should set a default matching the actual HW capabilities
  buttonChannel(channeltype_default), // by default, buttons act on default channel
  buttonFunc(buttonFunc_room_preset0x), // act as room button by default
  setsLocalPriority(false),
  clickType(ct_none),
  buttonPressed(false),
  lastClick(Never),
  buttonStateMachineTicket(0),
  callsPresent(false)
{
  // set default hrdware configuration
  setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
  // reset the button state machine
  resetStateMachine();
}


void ButtonBehaviour::setHardwareButtonConfig(int aButtonID, DsButtonType aType, DsButtonElement aElement, bool aSupportsLocalKeyMode, int aCounterPartIndex)
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
}



void ButtonBehaviour::buttonAction(bool aPressed)
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Button was %s\n", aPressed ? "pressed" : "released");
  buttonPressed = aPressed; // remember state
  checkStateMachine(true, MainLoop::now());
}


void ButtonBehaviour::resetStateMachine()
{
  buttonPressed = false;
  state = S0_idle;
  clickCounter = 0;
  holdRepeats = 0;
  outputOn = false;
  localButtonEnabled = false;
  dimmingUp = false;
  timerRef = Never;
  MainLoop::currentMainLoop().cancelExecutionTicket(buttonStateMachineTicket);
}



#if DEBUGFOCUSLOGGING
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



void ButtonBehaviour::checkStateMachine(bool aButtonChange, MLMicroSeconds aNow)
{
  MainLoop::currentMainLoop().cancelExecutionTicket(buttonStateMachineTicket);
  MLMicroSeconds timeSinceRef = aNow-timerRef;

  DBGFLOG(LOG_NOTICE, "button state machine entered in state %s at reference time %d and clickCounter=%d\n", stateNames[state], (int)(timeSinceRef/MilliSecond), clickCounter);
  switch (state) {

    case S0_idle :
      timerRef = 0; // no timer running
      if (aButtonChange && buttonPressed) {
        clickCounter = localButtonEnabled ? 0 : 1;
        timerRef = aNow;
        state = S1_initialpress;
      }
      break;

    case S1_initialpress :
      if (aButtonChange && !buttonPressed) {
        timerRef = aNow;
        state = S5_nextPauseWait;
      }
      else if (timeSinceRef>=t_click_length) {
        state = S2_holdOrTip;
      }
      break;

    case S2_holdOrTip:
      if (aButtonChange && !buttonPressed && clickCounter==0) {
        localSwitchOutput();
        timerRef = aNow;
        clickCounter = 1;
        state = S4_nextTipWait;
      }
      else if (aButtonChange && !buttonPressed && clickCounter>0) {
        sendClick((DsClickType)(ct_tip_1x+clickCounter-1));
        timerRef = aNow;
        state = S4_nextTipWait;
      }
      else if (timeSinceRef>=t_long_function_delay) {
        // long function
        if (!localButtonEnabled || !outputOn) {
          // hold
          holdRepeats = 0;
          timerRef = aNow;
          sendClick(ct_hold_start);
          state = S3_hold;
        }
        else if (localButtonEnabled && outputOn) {
          // local dimming
          dimmingUp = !dimmingUp; // change direction
          timerRef = aNow+t_local_dim_timeout; // force first timeout right away
          state = S11_localdim;
        }
      }
      break;

    case S3_hold:
      if (aButtonChange && !buttonPressed) {
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
      if (aButtonChange && buttonPressed) {
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
      if (aButtonChange && buttonPressed) {
        timerRef = aNow;
        clickCounter = 2;
        state = S6_2ClickWait;
      }
      else if (timeSinceRef>=t_click_pause) {
        if (localButtonEnabled)
          localSwitchOutput();
        else
          sendClick(ct_click_1x);
        state = S4_nextTipWait;
      }
      break;

    case S6_2ClickWait:
      if (aButtonChange && !buttonPressed) {
        timerRef = aNow;
        state = S9_2pauseWait;
      }
      else if (timeSinceRef>t_click_length) {
        state = S7_progModeWait;
      }
      break;

    case S7_progModeWait:
      if (aButtonChange && !buttonPressed) {
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
      if (aButtonChange && buttonPressed) {
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
      if (aButtonChange && !buttonPressed) {
        timerRef = aNow;
        sendClick(ct_click_3x);
        state = S4_nextTipWait;
      }
      else if (timeSinceRef>=t_click_length) {
        state = S13_3pauseWait;
      }
      break;

    case S13_3pauseWait:
      if (aButtonChange && !buttonPressed) {
        timerRef = aNow;
        sendClick(ct_tip_3x);
      }
      else if (timeSinceRef>=t_long_function_delay) {
        sendClick(ct_short_short_long);
        state = S8_awaitrelease;
      }
      break;

    case S11_localdim:
      if (aButtonChange && !buttonPressed) {
        state = S0_idle;
      }
      else if (timeSinceRef>=t_dim_repeat_time) {
        localDim();
        timerRef = aNow;
      }
      break;

    case S8_awaitrelease:
    case S14_awaitrelease:
      if (aButtonChange && !buttonPressed) {
        state = S0_idle;
      }
      break;
  }
  DBGFLOG(LOG_NOTICE, " -->                       exit state %s with %sfurther timing needed\n", stateNames[state], timerRef!=Never ? "" : "NO ");
  if (timerRef!=Never) {
    // need timing, schedule calling again
    buttonStateMachineTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&ButtonBehaviour::checkStateMachine, this, false, _1), aNow+10*MilliSecond);
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




void ButtonBehaviour::localSwitchOutput()
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Local switch\n");
//  if (isTwoWay()) {
//    // on or off depending on which side of the two-way switch was clicked
//    outputOn = secondKey;
//  }
//  else {
//    // one-way: toggle output
//    outputOn = !outputOn;
//  }
//  // TODO: actually switch output
//  // send status
//  sendClick(outputOn ? ct_local_on : ct_local_off);
  // pass on local toggle to device container
  #warning // TODO: tbd
}


void ButtonBehaviour::localDim()
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Local dim\n");
  // TODO: actually dim output in direction as indicated by dimmingUp
}



void ButtonBehaviour::sendClick(DsClickType aClickType)
{
  // update button state
  lastClick = MainLoop::now();
  clickType = aClickType;
  // button press is considered a (regular!) user action, have it checked globally first
  if (!device.getDeviceContainer().signalDeviceUserAction(device, true)) {
    // button press not consumed on global level, forward to upstream dS
    LOG(LOG_NOTICE,"ButtonBehaviour: Pushing value = %d, clickType %d\n", buttonPressed, aClickType);
    // issue a state property push
    pushBehaviourState();
    // also let device container know for local click handling
    #warning "%%% TODO: more elegant solution for this"
    device.getDeviceContainer().checkForLocalClickHandling(*this, aClickType);
  }
}



#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *ButtonBehaviour::tableName()
{
  return "ButtonSettings";
}



// data field definitions

static const size_t numFields = 5;

size_t ButtonBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *ButtonBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "dsGroup", SQLITE_INTEGER }, // Note: don't call a SQL field "group"!
    { "buttonFunc", SQLITE_INTEGER },
    { "buttonGroup", SQLITE_INTEGER },
    { "buttonFlags", SQLITE_INTEGER },
    { "buttonChannel", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


enum {
  buttonflag_setsLocalPriority = 0x0001,
  buttonflag_callsPresent = 0x0002
};

/// load values from passed row
void ButtonBehaviour::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  buttonGroup = (DsGroup)aRow->get<int>(aIndex++);
  buttonMode = (DsButtonMode)aRow->get<int>(aIndex++);
  buttonFunc = (DsButtonFunc)aRow->get<int>(aIndex++);
  int flags = aRow->get<int>(aIndex++);
  buttonChannel = (DsChannelType)aRow->get<int>(aIndex++);
  // decode the flags
  setsLocalPriority = flags & buttonflag_setsLocalPriority;
  callsPresent = flags & buttonflag_callsPresent;
}


// bind values to passed statement
void ButtonBehaviour::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
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
  };
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}

// state properties

enum {
  value_key,
  clickType_key,
  age_key,
  numStateProperties
};


int ButtonBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptorPtr ButtonBehaviour::getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numStateProperties] = {
    { "value", apivalue_uint64, value_key+states_key_offset, OKEY(button_key) },
    { "clickType", apivalue_uint64, clickType_key+states_key_offset, OKEY(button_key) },
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
        // States properties
        case value_key+states_key_offset:
          if (lastClick==Never)
            aPropValue->setNull();
          else
            aPropValue->setBoolValue(buttonPressed);
          return true;
        case clickType_key+states_key_offset:
          aPropValue->setUint64Value(clickType);
          return true;
        case age_key+states_key_offset:
          // age
          if (lastClick==Never)
            aPropValue->setNull();
          else
            aPropValue->setDoubleValue(((double)MainLoop::now()-lastClick)/Second);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        // Settings properties
        case group_key+settings_key_offset:
          buttonGroup = (DsGroup)aPropValue->int32Value();
          markDirty();
          return true;
        case mode_key+settings_key_offset:
          buttonMode = (DsButtonMode)aPropValue->int32Value();
          markDirty();
          return true;
        case function_key+settings_key_offset:
          buttonFunc = (DsButtonFunc)aPropValue->int32Value();
          markDirty();
          return true;
        case channel_key+settings_key_offset:
          buttonChannel = (DsChannelType)aPropValue->int32Value();
          markDirty();
          return true;
        case setsLocalPriority_key+settings_key_offset:
          setsLocalPriority = (DsButtonMode)aPropValue->boolValue();
          markDirty();
          return true;
        case callsPresent_key+settings_key_offset:
          callsPresent = (DsButtonMode)aPropValue->boolValue();
          markDirty();
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
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- buttonID: %d, buttonType: %d, buttonElementID: %d\n", buttonID, buttonType, buttonElementID);
  string_format_append(s, "- buttonChannel: %d, buttonFunc: %d, buttonmode/LTMODE: %d\n", buttonChannel, buttonFunc, buttonMode);
  s.append(inherited::description());
  return s;
}



