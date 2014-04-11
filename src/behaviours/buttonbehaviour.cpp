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

#include "buttonbehaviour.hpp"

using namespace p44;



ButtonBehaviour::ButtonBehaviour(Device &aDevice) :
  inherited(aDevice),
  // persistent settings
  buttonMode(buttonMode_inactive), // none by default, hardware should set a default matching the actual HW capabilities
  buttonChannel(channeltype_default), // by default, buttons act on default channel
  buttonFunc(buttonFunc_room_preset0x), // act as room button by default
  setsLocalPriority(false),
  clickType(ct_none),
  buttonPressed(false),
  lastClick(Never),
  callsPresent(false)
{
  // set default hrdware configuration
  setHardwareButtonConfig(0, buttonType_single, buttonElement_center, false, 0);
  // default group
  setGroup(group_yellow_light);
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
  // button press is considered a (regular!) user action
  if (!device.getDeviceContainer().signalDeviceUserAction(device, true)) {
    // not suppressed
    buttonPressed = aPressed; // remember state
    checkStateMachine(true, MainLoop::now());
  }
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
  timerPending = false;
}



void ButtonBehaviour::checkTimer(MLMicroSeconds aCycleStartTime)
{
  checkStateMachine(false, aCycleStartTime);
  timerPending = false;
}



void ButtonBehaviour::checkStateMachine(bool aButtonChange, MLMicroSeconds aNow)
{
  if (timerPending) {
    MainLoop::currentMainLoop().cancelExecutionsFrom(this);
    timerPending = false;
  }
  MLMicroSeconds timeSinceRef = aNow-timerRef;

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
  if (timerRef!=Never) {
    // need timing, schedule calling again
    timerPending = true;
    MainLoop::currentMainLoop().executeOnceAt(boost::bind(&ButtonBehaviour::checkTimer, this, _2), aNow+10*MilliSecond, this);
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
  LOG(LOG_NOTICE,"ButtonBehaviour: Pushing value = %d, clickType %d\n", buttonPressed, aClickType);
  // issue a state porperty push
  device.pushProperty("buttonInputStates", VDC_API_DOMAIN, (int)index);
  // also let device container know for local click handling
  #warning "%%% TODO: more elegant solution for this"
  device.getDeviceContainer().checkForLocalClickHandling(*this, aClickType);
//  sendMessage("DeviceButtonClick", params);
}



#pragma mark - persistence implementation


// SQLIte3 table name to store these parameters to
const char *ButtonBehaviour::tableName()
{
  return "ButtonSettings";
}



// data field definitions

static const size_t numFields = 4;

size_t ButtonBehaviour::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *ButtonBehaviour::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
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
const PropertyDescriptor *ButtonBehaviour::getDescDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numDescProperties] = {
    { "supportsLocalKeyMode", apivalue_bool, false, supportsLocalKeyMode_key+descriptions_key_offset, &button_key },
    { "buttonID", apivalue_uint64, false, buttonID_key+descriptions_key_offset, &button_key },
    { "buttonType", apivalue_uint64, false, buttonType_key+descriptions_key_offset, &button_key },
    { "buttonElementID", apivalue_uint64, false, buttonElementID_key+descriptions_key_offset, &button_key },
  };
  return &properties[aPropIndex];
}


// settings properties

enum {
  mode_key,
  function_key,
  channel_key,
  setsLocalPriority_key,
  callsPresent_key,
  numSettingsProperties
};


int ButtonBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *ButtonBehaviour::getSettingsDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numSettingsProperties] = {
    { "mode", apivalue_uint64, false, mode_key+settings_key_offset, &button_key },
    { "function", apivalue_uint64, false, function_key+settings_key_offset, &button_key },
    { "channel", apivalue_uint64, false, channel_key+settings_key_offset, &button_key },
    { "setsLocalPriority", apivalue_bool, false, setsLocalPriority_key+settings_key_offset, &button_key },
    { "callsPresent", apivalue_bool, false, callsPresent_key+settings_key_offset, &button_key },
  };
  return &properties[aPropIndex];
}

// state properties

enum {
  value_key,
  clickType_key,
  age_key,
  numStateProperties
};


int ButtonBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptor *ButtonBehaviour::getStateDescriptor(int aPropIndex)
{
  static const PropertyDescriptor properties[numStateProperties] = {
    { "value", apivalue_uint64, false, value_key+states_key_offset, &button_key },
    { "clickType", apivalue_uint64, false, clickType_key+states_key_offset, &button_key },
    { "age", apivalue_double, false, age_key+states_key_offset, &button_key },
  };
  return &properties[aPropIndex];
}


// access to all fields

bool ButtonBehaviour::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&button_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
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
      switch (aPropertyDescriptor.accessKey) {
        // Settings properties
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
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
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



