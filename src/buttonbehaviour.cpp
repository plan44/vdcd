//
//  buttonbehaviour.cpp
//  p44bridged
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
  resetStateMachine();
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
  #warning // TODO: generically implement this one
  deviceP->getDeviceContainer().localSwitchOutput(deviceP->dsid, outputOn);
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
  sendMessage("DeviceButtonClick", params);
}





#pragma mark - functional identification for digitalSTROM system

#warning // TODO: for now, we just emulate a SW-TKM2xx in a more or less hard-coded way

// from DeviceConfig.py:
// #  productName (functionId, productId, groupMemberShip, ltMode, outputMode, buttonIdGroup)
// %%% luz: was apparently wrong names, TKM200 is the 4-input version TKM210 the 2-input, I corrected the functionID below:
// deviceDefaults["SW-TKM200"] = ( 0x8103, 1224, 257, 0, 0, 0 ) # 4 inputs
// deviceDefaults["SW-TKM210"] = ( 0x8102, 1234, 257, 0, 0, 0 ) # 2 inputs

// Die Function-ID enthält (für alle dSIDs identisch) in den ersten 4 Bit die "Farbe" (Gruppe) des devices
//  1 Light (yellow)
//  2 Blinds (grey)
//  3 Climate (blue)
//  4 Audio (cyan)
//  5 Video (magenta)
//  6 Security (red)
//  7 Access (green)
//  8 Joker (black)


// Die Function-ID enthält (für alle dSIDs identisch) in den letzten 2 Bit eine Kennung,
// wieviele Eingänge das Gerät besitzt: 0=keine, 1=1 Eingang, 2=2 Eingänge, 3=4 Eingänge.
// Die dSID muss für den ersten Eingang mit einer durch 4 teilbaren dSID beginnen, die weiteren dSIDs sind fortlaufend.


//  Each device has a function ID programmed into its chip. The function ID has the capabilities of the chip coded into it.
//
//  1111 11
//  5432 1098 76 543210
//  xxxx.xxxx xx.xxxxxx
//
//  Bits 15..12 (dS-Class), Bits 11..6 (dS-Subclass), Bits 5..0 (Functionmodule)

//  dS-Class = group/color

//  dS Subclass Light
//  - 0: dS-Standard

//  Function Module Light/dS-Standard
//  - Bits 5..4 : 0 = No output available, 1 = Output is a switch, 2 = Output is a dimmer, 3 = undefined
//  - Bit 3     : if set, The first button is a local-button
//  - Bits 2..0 : 0 = 1-Way, 1 = 2-Way, 2 = 2x1-Way, 3 = 4-Way, 4 = 4x1-Way, 5 = 2x2-Way, 6 = reserved, 7 = no buttons


uint16_t ButtonBehaviour::functionId()
{
  return
    (group_black_joker<<12) +
    (0x04 << 6) + // ??
    (0 << 4) + // no output
    ((hasLocalButton ? 1 : 0) << 3) +
    hardwareButtonType; // reflect the hardware button type
}



uint16_t ButtonBehaviour::productId()
{
  return 1234;
}


uint16_t ButtonBehaviour::groupMemberShip()
{
  return 257; // all groups
}


uint16_t ButtonBehaviour::version()
{
#warning // TODO: just faking a >=3.5.0 version because only those have KEYSTATE
  return 0x0350;
}


uint8_t ButtonBehaviour::ltMode()
{
  return buttonSettings.buttonMode;
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



