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

  public:

    // - click types
    typedef enum {
      ct_tip_1x = 0, ///< first tip
      ct_tip_2x = 1, ///< second tip
      ct_tip_3x = 2, ///< third tip
      ct_tip_4x = 3, ///< fourth tip
      ct_hold_start = 4, ///< hold start
      ct_hold_repeat = 5, ///< hold repeat
      ct_hold_end = 6, ///< hold end
      ct_click_1x = 7, ///< short click
      ct_click_2x = 8, ///< double click
      ct_click_3x = 9, ///< triple click
      ct_short_long = 10, ///< short/long = programming mode
      ct_local_off = 11, ///< local button has turned device off
      ct_local_on = 12, ///< local button has turned device on
      ct_short_short_long = 13, ///< short/short/long = local programming mode
      ct_local_stop = 14, ///< local stop
      numClickTypes
    } ClickType;

    typedef enum {
      key_1way = 0, ///< one way push button
      key_2way_A = 1, ///< two way rocker switch, upper
      key_2way_B = 2, ///< two way rocker switch, lower
      key_local = 4, ///< local button
    } KeyId;

  private:

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
    const int max_hold_repeats = 30;
    // - type of button
    KeyId keyId;

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

    /// set type of button
    void setKeyId(KeyId aKeyId);

    /// button action occurred
    /// @param aPressed true if action is button pressed, false if action is button released
    void buttonAction(bool aPressed);
    
    /// @}

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  };
  
}


#endif /* defined(__p44bridged__buttonbehaviour__) */
