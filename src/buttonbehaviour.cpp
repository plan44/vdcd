//
//  buttonbehaviour.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "buttonbehaviour.hpp"

using namespace p44;


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
  "ct_short_short_long"
};


#endif



ButtonBehaviour::ButtonBehaviour(Device *aDeviceP) :
  inherited(aDeviceP)
{
  resetStateMachine();
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
        localToggle();
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
          localToggle();
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



void ButtonBehaviour::localToggle()
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Local toggle\n");
}


void ButtonBehaviour::localDim()
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Local dim\n");
}



void ButtonBehaviour::sendClick(ClickType aClickType)
{
  LOG(LOG_NOTICE,"ButtonBehaviour: Sending Click Type %d = %s\n", aClickType, ClickTypeNames[aClickType]);
}





#pragma mark - functional identification for digitalSTROM system

#warning // TODO: for now, we just emulate a SW-TKM2xx in a more or less hard-coded way

// from DeviceConfig.py:
// #  productName (functionId, productId, groupMemberShip, ltMode, outputMode, buttonIdGroup)
// %%% luz: was apparently wrong names, TKM200 is the 4-input version TKM210 the 2-input, I corrected the functionID below:
// deviceDefaults["SW-TKM200"] = ( 0x8103, 1224, 257, 0, 0, 0 ) # 4 inputs
// deviceDefaults["SW-TKM210"] = ( 0x8102, 1234, 257, 0, 0, 0 ) # 2 inputs

// Die Function-ID enthält (für alle dSIDs identisch) in den letzten 2 Bit eine Kennung,
// wieviele Eingänge das Gerät besitzt: 0=keine, 1=1 Eingang, 2=2 Eingänge, 3=4 Eingänge.
// Die dSID muss für den ersten Eingang mit einer durch 4 teilbaren dSID beginnen, die weiteren dSIDs sind fortlaufend.


uint16_t ButtonBehaviour::functionId()
{
  int i = deviceP->getNumInputs();
  return 0x8100 + i>3 ? 3 : i; // 0 = no inputs, 1..2 = 1..2 inputs, 3 = 4 inputs
}



uint16_t ButtonBehaviour::productId()
{
  return 1234;
}


uint16_t ButtonBehaviour::groupMemberShip()
{
  return 257; // all groups
}


uint8_t ButtonBehaviour::ltMode()
{
  return 0;
}


uint8_t ButtonBehaviour::outputMode()
{
  return 0;
}



uint8_t ButtonBehaviour::buttonIdGroup()
{
  return 0;
}



string ButtonBehaviour::shortDesc()
{
  return string("Button");
}
