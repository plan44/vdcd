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

ButtonSettings::ButtonSettings(DsBehaviour &aBehaviour) :
  inherited(aBehaviour),
  buttonGroup(group_yellow_light), // default to light
  buttonMode(buttonmode_inactive), // none by default, hardware should set a default matching the actual HW capabilities
  buttonFunc(buttonfunc_room_preset0x), // act as room button by default
  setsLocalPriority(false),
  callsPresent(false)
{
}


// SQLIte3 table name to store these parameters to
const char *ButtonSettings::tableName()
{
  return "ButtonSettings";
}



// data field definitions

static const size_t numFields = 4;

size_t ButtonSettings::numFieldDefs()
{
  return inherited::numFieldDefs()+numFields;
}


const FieldDefinition *ButtonSettings::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "buttonMode", SQLITE_INTEGER },
    { "buttonFunc", SQLITE_INTEGER },
    { "buttonGroup", SQLITE_INTEGER },
    { "buttonFlags", SQLITE_INTEGER }
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
void ButtonSettings::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  buttonMode = (DsButtonMode)aRow->get<int>(aIndex++);
  buttonFunc = (DsButtonFunc)aRow->get<int>(aIndex++);
  buttonGroup  = (DsGroup)aRow->get<int>(aIndex++);
  int flags = aRow->get<int>(aIndex++);
  // decode the flags
  setsLocalPriority = flags & buttonflag_setsLocalPriority;
  callsPresent = flags & buttonflag_callsPresent;
}


// bind values to passed statement
void ButtonSettings::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // encode the flags
  int flags = 0;
  if (setsLocalPriority) flags |= buttonflag_setsLocalPriority;
  if (callsPresent) flags |= buttonflag_callsPresent;
  // bind the fields
  aStatement.bind(aIndex++, buttonMode);
  aStatement.bind(aIndex++, buttonFunc);
  aStatement.bind(aIndex++, buttonGroup);
  aStatement.bind(aIndex++, flags);
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



ButtonBehaviour::ButtonBehaviour(Device &aDevice, size_t aIndex) :
  inherited(aDevice, aIndex),
  buttonSettings(*this)
{
  // set default hrdware configuration
  setHardwareButtonType(0, buttonType_single, buttonElement_center, false);
  // reset the button state machine
  resetStateMachine();
}


void ButtonBehaviour::setHardwareButtonType(int aButtonID, DsButtonType aType, DsButtonElement aElement, bool aSupportsLocalKeyMode)
{
  buttonID = aButtonID;
  buttonType = aType;
  buttonElementID = aElement;
  supportsLocalKeyMode = aSupportsLocalKeyMode;
  // now derive default settings from hardware
  // - default to standard mode
  buttonSettings.buttonMode = buttonmode_standard;
  // - modify for 2-way
  if (buttonType==buttonType_2way) {
    // part of a 2-way button
    if (buttonElementID==buttonElement_up) {
      buttonSettings.buttonMode = (DsButtonMode)((int)buttonmode_rockerDown1+aButtonID);
    }
    else if (buttonElementID==buttonElement_down) {
      buttonSettings.buttonMode = (DsButtonMode)((int)buttonmode_rockerUp1+aButtonID);
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
    MainLoop::currentMainLoop()->executeOnceAt(boost::bind(&ButtonBehaviour::checkTimer, this, _2), aNow+10*MilliSecond, this);
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
//  if (buttonSettings.isTwoWay()) {
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
  clickType = aClickType;
  #ifdef DEBUG
  LOG(LOG_NOTICE,"ButtonBehaviour: Pushing value = %s, clickType %d/%s\n", buttonPressed ? "pressed" : "released", aClickType, ClickTypeNames[aClickType]);
  #else
  LOG(LOG_NOTICE,"ButtonBehaviour: Pushing value = %d, clickType %d\n", buttonPressed, aClickType);
  #endif
  // issue a state porperty push
  device.pushProperty("buttonInputStates", VDC_API_DOMAIN, (int)index);
  // also let device container know for local click handling
  #warning "%%% TODO: more elegant solution for this"
  device.getDeviceContainer().checkForLocalClickHandling(*this, aClickType);
//  sendMessage("DeviceButtonClick", params);
}



#pragma mark - persistent settings management


ErrorPtr ButtonBehaviour::load()
{
  // load button settings
  return buttonSettings.load();
}


ErrorPtr ButtonBehaviour::save()
{
  // save button settings
  return buttonSettings.save();
}


ErrorPtr ButtonBehaviour::forget()
{
  // delete button settings
  return buttonSettings.deleteFromStore();
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

static const PropertyDescriptor descriptionProperties[numDescProperties] = {
  { "supportsLocalKeyMode", ptype_int32, false, supportsLocalKeyMode_key+descriptions_key_offset, &button_key },
  { "buttonID", ptype_int32, false, buttonID_key+descriptions_key_offset, &button_key },
  { "buttonType", ptype_int32, false, buttonType_key+descriptions_key_offset, &button_key },
  { "buttonElementID", ptype_int32, false, buttonElementID_key+descriptions_key_offset, &button_key },
};

int ButtonBehaviour::numDescProps() { return numDescProperties; }
const PropertyDescriptor *ButtonBehaviour::getDescDescriptor(int aPropIndex) { return &descriptionProperties[aPropIndex]; }


// settings properties

enum {
  group_key,
  mode_key,
  setsLocalPriority_key,
  callsPresent_key,
  numSettingsProperties
};

static const PropertyDescriptor settingsProperties[numSettingsProperties] = {
  { "group", ptype_int32, false, group_key+settings_key_offset, &button_key },
  { "mode", ptype_int32, false, mode_key+settings_key_offset, &button_key },
  { "setsLocalPriority", ptype_int32, false, setsLocalPriority_key+settings_key_offset, &button_key },
  { "callsPresent", ptype_int32, false, callsPresent_key+settings_key_offset, &button_key },
};

int ButtonBehaviour::numSettingsProps() { return numSettingsProperties; }
const PropertyDescriptor *ButtonBehaviour::getSettingsDescriptor(int aPropIndex) { return &settingsProperties[aPropIndex]; }

// state properties

enum {
  value_key,
  clickType_key,
  numStateProperties
};

static const PropertyDescriptor stateProperties[numStateProperties] = {
  { "value", ptype_int32, false, value_key+states_key_offset, &button_key },
  { "clickType", ptype_int32, false, clickType_key+states_key_offset, &button_key },
};

int ButtonBehaviour::numStateProps() { return numStateProperties; }
const PropertyDescriptor *ButtonBehaviour::getStateDescriptor(int aPropIndex) { return &stateProperties[aPropIndex]; }


// access to all fields

bool ButtonBehaviour::accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&button_key) {
    if (!aForWrite) {
      // read properties
      switch (aPropertyDescriptor.accessKey) {
        // Description properties
        case supportsLocalKeyMode_key+descriptions_key_offset:
          aPropValue = JsonObject::newBool(supportsLocalKeyMode);
          return true;
        case buttonID_key+descriptions_key_offset:
          aPropValue = JsonObject::newInt32(buttonID);
          return true;
        case buttonType_key+descriptions_key_offset:
          aPropValue = JsonObject::newInt32(buttonType);
          return true;
        case buttonElementID_key+descriptions_key_offset:
          aPropValue = JsonObject::newInt32(buttonElementID);
          return true;
        // Settings properties
        case group_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(buttonSettings.buttonGroup);
          return true;
        case mode_key+settings_key_offset:
          aPropValue = JsonObject::newInt32(buttonSettings.buttonMode);
          return true;
        case setsLocalPriority_key+settings_key_offset:
          aPropValue = JsonObject::newBool(buttonSettings.setsLocalPriority);
          return true;
        case callsPresent_key+settings_key_offset:
          aPropValue = JsonObject::newBool(buttonSettings.callsPresent);
          return true;
        // States properties
        case value_key+states_key_offset:
          aPropValue = JsonObject::newInt32(buttonPressed ? 1 : 0);
          return true;
        case clickType_key+states_key_offset:
          aPropValue = JsonObject::newInt32(clickType);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor.accessKey) {
        // Settings properties
        case group_key+settings_key_offset:
          buttonSettings.buttonGroup = (DsGroup)aPropValue->int32Value();
          buttonSettings.markDirty();
          return true;
        case mode_key+settings_key_offset:
          buttonSettings.buttonMode = (DsButtonMode)aPropValue->int32Value();
          buttonSettings.markDirty();
          return true;
        case setsLocalPriority_key+settings_key_offset:
          buttonSettings.setsLocalPriority = (DsButtonMode)aPropValue->boolValue();
          buttonSettings.markDirty();
          return true;
        case callsPresent_key+settings_key_offset:
          buttonSettings.callsPresent = (DsButtonMode)aPropValue->boolValue();
          buttonSettings.markDirty();
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}


#pragma mark - ButtonBehaviour description/shortDesc


string ButtonBehaviour::description()
{
  string s = string_format("%s behaviour\n", shortDesc().c_str());
  string_format_append(s, "- buttonID: %d, buttonType: %d, buttonElementID: %d\n", buttonID, buttonType, buttonElementID);
  string_format_append(s, "- group: %d, fbuttonmode/LTMODE: %d\n", buttonSettings.buttonGroup, buttonSettings.buttonMode);
  s.append(inherited::description());
  return s;
}



