//
//  buttonbehaviour.cpp
//  vdcd
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "buttonbehaviour.hpp"

using namespace p44;


#pragma mark - ButtonSettings

ButtonSettings::ButtonSettings(ParamStore &aParamStore) :
  inherited(aParamStore),
  buttonGroup(group_yellow_light), // default to light
  buttonFunction(buttonfunc_room_preset0x), // default to regular room button
  buttonMode(buttonmode_inactive) // none by default, hardware should set a default matching the actual HW capabilities
{

}


// SQLIte3 table name to store these parameters to
const char *ButtonSettings::tableName()
{
  return "dsButton";
}

/// data field definitions
const FieldDefinition *ButtonSettings::getFieldDefs()
{
  static const FieldDefinition dataDefs[] = {
    { "buttonMode", SQLITE_INTEGER },
    { "buttonGroup", SQLITE_INTEGER },
    { "buttonFunction", SQLITE_INTEGER },
    { NULL, 0 },
  };
  return dataDefs;
}


/// load values from passed row
void ButtonSettings::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  buttonMode = (DsButtonMode)aRow->get<int>(aIndex++);
  buttonGroup  = (DsGroup)aRow->get<int>(aIndex++);
  buttonFunction = (DsButtonFunc)aRow->get<int>(aIndex++);
}


// bind values to passed statement
void ButtonSettings::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, buttonMode);
  aStatement.bind(aIndex++, buttonGroup);
  aStatement.bind(aIndex++, buttonFunction);
}


bool ButtonSettings::isTwoWay()
{
  return buttonMode>=buttonmode_rockerDown1 && buttonMode<=buttonmode_rockerUpDown;
}




#pragma mark - ButtonBehaviour


#ifdef DEBUG
static const char *ClickTypeNames[] {
  "ct_tip_1x",
  "ct_tip_2x",
  "ct_tip_3x",
  "ct_tip_4x",
  "ct_hold_start",
  "ct_hold_repeat",
  "ct_hold_end",
  "ct_click_1x",
  "ct_click_2x",
  "ct_click_3x",
  "ct_short_long",
  "ct_local_off",
  "ct_local_on",
  "ct_short_short_long",
  "ct_local_stop"
};
#endif



ButtonBehaviour::ButtonBehaviour(Device *aDeviceP) :
  inherited(aDeviceP),
  buttonSettings(aDeviceP->getDeviceContainer().getDsParamStore()),
  hardwareButtonType(hwbuttontype_none), // undefined, actual device should set it
  hasLocalButton(false)
{
  buttonSettings.buttonGroup = deviceColorGroup; // default to overall color
  resetStateMachine();
}



void ButtonBehaviour::setDeviceColor(DsGroup aColorGroup)
{
  // have base class set color
  inherited::setDeviceColor(aColorGroup);
  // adjust default
  if (deviceColorGroup==group_black_joker)
    buttonSettings.buttonFunction = buttonfunc_app; // black devices have an App button by default
  else
    buttonSettings.buttonFunction = buttonfunc_room_preset0x; // all others are room button
}



void ButtonBehaviour::setHardwareButtonType(DsHardwareButtonType aButtonType, bool aFirstButtonLocal)
{
  hardwareButtonType = aButtonType;
  hasLocalButton = aFirstButtonLocal;
  // now set default button mode
  if (hardwareButtonType==hwbuttontype_2way || hardwareButtonType==hwbuttontype_2x2way) {
    // this is a 2-way hardware button (with single dsid)
    buttonSettings.buttonMode = buttonmode_rockerUpDown;
  }
  else if (hardwareButtonType==hwbuttontype_1way || hardwareButtonType==hwbuttontype_2x1way || hardwareButtonType==hwbuttontype_4x1way) {
    // this is a 1-way hardware button
    buttonSettings.buttonMode = buttonmode_standard;
  }
  else {
    // no known button type, inactive until set via LTMODE param
    buttonSettings.buttonMode = buttonmode_inactive;
  }
  buttonSettings.markDirty();
}




DsButtonMode ButtonBehaviour::getButtonMode()
{
  return buttonSettings.buttonMode;
}


void ButtonBehaviour::setButtonMode(DsButtonMode aButtonMode)
{
  // set mode
  buttonSettings.buttonMode = aButtonMode;
  buttonSettings.markDirty();
}


uint8_t ButtonBehaviour::getLTNUMGRP0()
{
  return (buttonSettings.buttonGroup<<4)+(buttonSettings.buttonFunction & 0xF);
}


void ButtonBehaviour::setLTNUMGRP0(uint8_t aLTNUMGRP0)
{
  buttonSettings.buttonGroup = (DsGroup)((aLTNUMGRP0>>4) & 0xF); // upper 4 bits
  buttonSettings.buttonFunction = (DsButtonFunc)(aLTNUMGRP0 & 0xF); // lower 4 bits
  buttonSettings.markDirty();
}






void ButtonBehaviour::buttonAction(bool aPressed, bool aSecondKey)
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Button was %s\n", aPressed ? "pressed" : "released");
  buttonPressed = aPressed; // remember state
  if (state!=S0_idle && secondKey!=aSecondKey) {
    // pressing the other key within a state machine run
    // aborts the current operation and begins a new run
    resetStateMachine();
  }
  secondKey = aSecondKey;
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
    MainLoop::currentMainLoop()->cancelExecutionsFrom(this);
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
        sendClick((ClickType)(ct_tip_1x+clickCounter-1));
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
    MainLoop::currentMainLoop()->executeOnceAt(boost::bind(&ButtonBehaviour::checkTimer, this, _2), aNow+10*MilliSecond, this);
  }
}



void ButtonBehaviour::localSwitchOutput()
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Local switch\n");
  if (buttonSettings.isTwoWay()) {
    // on or off depending on which side of the two-way switch was clicked
    outputOn = secondKey;
  }
  else {
    // one-way: toggle output
    outputOn = !outputOn;
  }
  // TODO: actually switch output
  // send status
  sendClick(outputOn ? ct_local_on : ct_local_off);
  // pass on local toggle to device container
  #warning // TODO: tbd
}


void ButtonBehaviour::localDim()
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Local dim\n");
  // TODO: actually dim output in direction as indicated by dimmingUp
}


void ButtonBehaviour::setLocalButtonEnabled(bool aEnabled)
{
  localButtonEnabled = aEnabled;
}





void ButtonBehaviour::sendClick(ClickType aClickType)
{
  KeyId keyId = key_1way;
  if (buttonSettings.isTwoWay()) {
    keyId = secondKey ? key_2way_B : key_2way_A;
  }
  #ifdef DEBUG
  LOG(LOG_NOTICE,"ButtonBehaviour: Sending KeyId %d, Click Type %d = %s\n", keyId, aClickType, ClickTypeNames[aClickType]);
  #else
  LOG(LOG_NOTICE,"ButtonBehaviour: Sending KeyId %d, Click Type %d\n", keyId, aClickType);
  #endif
  JsonObjectPtr params = JsonObject::newObj();
  params->add("key", JsonObject::newInt32(keyId));
  params->add("click", JsonObject::newInt32(aClickType));
  #warning "%%% TODO: replace by property push"

  #warning "%%% TODO: more elegant solution for this"
  deviceP->getDeviceContainer().checkForLocalClickHandling(*deviceP, aClickType, keyId);
//  sendMessage("DeviceButtonClick", params);
}





#pragma mark - functional identification for digitalSTROM system

// Standard group
//  0 variable (all)
//  1 Light (yellow)
//  2 Blinds (grey)
//  3 Climate (blue)
//  4 Audio (cyan)
//  5 Video (magenta)
//  6 Security (red)
//  7 Access (green)
//  8 Joker (black)

// Function ID:
//
//  1111 11
//  5432 1098 76 543210
//  gggg.cccc cc.xxxxxx
//
//  - gggg   : device group (color, class), 0..15
//  - cccccc : device subclass
//             - 000100 : dS-Standard R105 (current dS standard)
//  - xxxxxx : class specific config
//
//  Light:
//  - Xxxxxx : Bit 5 : if set, ramp time is variable and can be set in RAMPTIMEMAX
//  - xXxxxx : Bit 4 : if set, device has a power output
//  - xxXxxx : Bit 3 : if set, device has extra hardware features like extra binary inputs, sensors etc.
//  - xxxXxx : Bit 2 : reserved
//  - xxxxXX : Bit 0..1 : 0 = no button, 1 = one button, 2 = two buttons, 3 = four buttons

//  Name,          FunctionId  ProductId,  ltMode, outputMode,   buttonIdGroup
//  "GE-KM200",    0x1111,     200,        0,      16,           0x10
//  "GE-TKM210",   0x1111,     1234,       0,      16,           0x15
//  "GE-TKM220",   0x1101,     1244,       0,      0,            0x15
//  "GE-TKM230",   0x1102,     1254,       0,      0,            0x15
//  "GE-KL200",    0x1111,     3272,       0,      35,           0x10
//  "GE-KL210",    0x1111,     5320,       0,      35,           0x10
//  "GE-SDM200",   0x1111,     2248,       0,      16,           0x10
//  "GE-SDS200",   0x1119,     6344,       0,      16,           0x10
//  "GR-KL200",    0x2131,     3272,       0,      33,           0x20
//  "GR-KL210",    0x2131,     3282,       0,      33,           0x20
//  "GR-KL220",    0x2131,     3292,       0,      42,           0x20
//  "GR-TKM200",   0x2101,     1224,       0,      0,            0x25
//  "GR-TKM210",   0x2101,     1234,       0,      0,            0x25
//  "RT-TKM200",   0x6001,     1224,       0,      16,           0
//  "RT-SDM200",   0x6001,     2248,       0,      16,           0
//  "GN-TKM200",   0x7050,     1224,       0,      16,           0
//  "GN-TKM210",   0x6001,     1234,       0,      16,           0
//  "GN-KM200",    0x6001,     200,        0,      16,           70
//  "SW-KL200",    0x8111,     5320,       0,      41,           0
//  "SW-KL210",    0x8111,     3273,       0,      40,           0
//  "SW-TKM210",   0x8102,     1234,       0,      0,            0
//  "SW-TKM200",   0x8103,     1224,       0,      0,            0


uint16_t ButtonBehaviour::functionId()
{
  int i = deviceP->getNumButtons();
  return
    (deviceColorGroup<<12) +
    (0x04 << 6) + // DS Standard R105
    (0 << 4) + // no variable ramp time (B5), no output (B4)
    (i>3 ? 3 : i); // 0 = no inputs, 1..2 = 1..2 inputs, 3 = 4 inputs
}



uint16_t ButtonBehaviour::productId()
{
#warning // TODO: just faking a SW-TKM210
  return 1234; // SW-TKM210
}


uint16_t ButtonBehaviour::version()
{
#warning // TODO: just faking a 3.5.2 version because our real SW-TKM210 has that version
  return 0x0352;
}


uint8_t ButtonBehaviour::ltMode()
{
  return buttonSettings.buttonMode; // standard button is 0, see DsButtonMode
}


uint8_t ButtonBehaviour::outputMode()
{
  return outputmode_none;
}



uint8_t ButtonBehaviour::buttonIdGroup()
{
  // LTNUMGRP0
  return getLTNUMGRP0();
}


#pragma mark - interaction with digitalSTROM system


/* %%% old API

// handle message from vdSM
ErrorPtr ButtonBehaviour::handleMessage(string &aOperation, JsonObjectPtr aParams)
{
  ErrorPtr err;
  if (aOperation=="callscene") {
    // NOP for button
  }
  else {
    err = inherited::handleMessage(aOperation, aParams);
  }
  return err;
}

*/


// LTNUMGRP0
//  Bits 4..7: Button group/color
//  Bits 0..3: Button function/id/number


// get behaviour-specific parameter
ErrorPtr ButtonBehaviour::getBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t &aValue)
{
  if (aParamName=="LTMODE")
    aValue = getButtonMode();
  else if (aParamName=="LTNUMGRP0")
    aValue = getLTNUMGRP0();
  else if (aParamName=="KEYSTATE")
    aValue = buttonPressed ? (secondKey ? 0x02 : 0x01) : 0;
  else
    return inherited::getBehaviourParam(aParamName, aArrayIndex, aValue); // none of my params, let parent handle it
  // done
  return ErrorPtr();
}


// set behaviour-specific parameter
ErrorPtr ButtonBehaviour::setBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t aValue)
{
  if (aParamName=="LTMODE")
    setButtonMode((DsButtonMode)aValue);
  else if (aParamName=="LTNUMGRP0")
    setLTNUMGRP0(aValue);
  else
    return inherited::setBehaviourParam(aParamName, aArrayIndex, aValue); // none of my params, let parent handle it
  // set a local param, mark dirty
  buttonSettings.markDirty();
  return ErrorPtr();
}


// this is usually called from the device container when device is added (detected)
ErrorPtr ButtonBehaviour::load()
{
  // load light settings (and scenes along with it)
  return buttonSettings.loadFromStore(deviceP->dsid.getString().c_str());
}


// this is usually called from the device container in regular intervals
ErrorPtr ButtonBehaviour::save()
{
  // save light settings (and scenes along with it)
  return buttonSettings.saveToStore(deviceP->dsid.getString().c_str());
}


ErrorPtr ButtonBehaviour::forget()
{
  // delete light settings (and scenes along with it)
  return buttonSettings.deleteFromStore();
}


#pragma mark - ButtonBehaviour description/shortDesc


string ButtonBehaviour::shortDesc()
{
  return string("Button");
}


string ButtonBehaviour::description()
{
  string s = string_format("dS behaviour %s\n", shortDesc().c_str());
  string_format_append(s, "- hardware button type: %d, %s local button\n", hardwareButtonType, hasLocalButton ? "has" : "no");
  string_format_append(s, "- group: %d, function/number: %d, buttonmode/LTMODE: %d\n", buttonSettings.buttonGroup, buttonSettings.buttonFunction, buttonSettings.buttonMode);
  s.append(inherited::description());
  return s;
}



