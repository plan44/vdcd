//
//  buttonbehaviour.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__buttonbehaviour__
#define __p44bridged__buttonbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {

  class ButtonBehaviour : public DSBehaviour
  {
    typedef DSBehaviour inherited;

    // button state machine v2.01

    // - states
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

    // - click types
    typedef enum {
      ct_tip_1x,
      ct_tip_2x,
      ct_tip_3x,
      ct_tip_4x,
      ct_hold_start,
      ct_hold_repeat,
      ct_hold_end,
      ct_click_1x,
      ct_click_2x,
      ct_click_3x,
      ct_short_long,
      ct_short_short_long
    } ClickType;

    // - vars
    bool buttonPressed;
    ButtonState state;
    int clickCounter;
    int holdRepeats;
    bool outputOn;
    bool localButtonEnabled;
    bool dimmingUp;
    MLMicroSeconds timerRef;
    bool timerPending;
    // - params
    const int t_long_function_delay = 500*MilliSecond;
    const int t_dim_repeat_time = 1000*MilliSecond;
    const int t_click_length = 140*MilliSecond;
    const int t_click_pause = 140*MilliSecond;
    const int t_tip_timeout = 800*MilliSecond;
    const int t_local_dim_timeout = 160*MilliSecond;
    const int max_hold_repeats = 30*MilliSecond;

    // - methods
    void resetStateMachine();
    void checkStateMachine(bool aButtonChange, MLMicroSeconds aNow);
    void checkTimer(MLMicroSeconds aCycleStartTime);
    void localToggle();
    void localDim();
    void sendClick(ClickType aClickType);

  public:
    // constructor
    ButtonBehaviour(Device *aDeviceP);

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId();
    virtual uint16_t productId();
    virtual uint16_t groupMemberShip();
    virtual uint8_t ltMode();
    virtual uint8_t outputMode();
    virtual uint8_t buttonIdGroup();

    /// @}

    /// @name interface towards actual device hardware (or simulation)

    /// button action occurred
    /// @param aPressed true if action is button pressed, false if action is button released
    void buttonAction(bool aPressed);
    
    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  };
  
}


#endif /* defined(__p44bridged__buttonbehaviour__) */
