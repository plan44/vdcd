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


  /// the persistent parameters of a device with light behaviour
  class ButtonSettings : public PersistentParams
  {
    typedef PersistentParams inherited;

  public:
    DsButtonMode buttonMode; ///< the button mode (LTMODE)
    DsGroup buttonGroup; ///< the group/color of the button
    DsButtonFunc buttonFunction; ///< the function of the button

    ButtonSettings(ParamStore &aParamStore);

    /// @return true if this is a two-way button
    bool isTwoWay();

    /// @name PersistentParams methods which implement actual storage
    /// @{

    /// SQLIte3 table name to store these parameters to
    virtual const char *tableName();
    /// data field definitions
    virtual const FieldDefinition *getFieldDefs();
    /// load values from passed row
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);
    /// bind values to passed statement
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);
    
    /// @}
  };



  class ButtonBehaviour : public DSBehaviour
  {
    typedef DSBehaviour inherited;

  private:

    // dS enums

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
      key_2way_A = 1, ///< two way rocker switch, lower
      key_2way_B = 2, ///< two way rocker switch, upper
      key_local_1way = 4, ///< local one way push button
      key_local_2way_A = 5, ///< two way rocker switch, lower
      key_local_2way_B = 6, ///< two way rocker switch, upper
    } KeyId;

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

    // - state machine vars
    bool buttonPressed;
    bool secondKey;
    ButtonState state;
    int clickCounter;
    int holdRepeats;
    bool outputOn;
    bool localButtonEnabled;
    bool dimmingUp;
    MLMicroSeconds timerRef;
    bool timerPending;

    // - state machine params
    static const int t_long_function_delay = 500*MilliSecond;
    static const int t_dim_repeat_time = 1000*MilliSecond;
    static const int t_click_length = 140*MilliSecond;
    static const int t_click_pause = 140*MilliSecond;
    static const int t_tip_timeout = 800*MilliSecond;
    static const int t_local_dim_timeout = 160*MilliSecond;
    static const int max_hold_repeats = 30;

    // - hardware params
    DsHardwareButtonType hardwareButtonType; ///< the hardware button type
    bool hasLocalButton; ///< set if first button is local
    ButtonSettings buttonSettings; ///< the persistent params of this button device


    // - methods
    void resetStateMachine();
    void checkStateMachine(bool aButtonChange, MLMicroSeconds aNow);
    void checkTimer(MLMicroSeconds aCycleStartTime);
    void localSwitchOutput();
    void localDim();
    void sendClick(ClickType aClickType);


  public:
    // constructor
    ButtonBehaviour(Device *aDeviceP);

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId();
    virtual uint16_t productId();
    virtual uint8_t ltMode();
    virtual uint8_t outputMode();
    virtual uint8_t buttonIdGroup();
    virtual uint16_t version();

    /// @}

    /// @name interface towards actual device hardware (or simulation)

    /// set hardware button characteristics
    void setHardwareButtonType(DsHardwareButtonType aButtonType, bool aFirstButtonLocal);

    /// query type/mode of button
    /// @return button mode (LTMODE)
    DsButtonMode getButtonMode();

    /// set type of button (usually set when collecting actual devices)
    /// @param aButtonMode button mode (LTMODE)
    void setButtonMode(DsButtonMode aButtonMode);

    /// enable disable "local" button functionality
    void setLocalButtonEnabled(bool aEnabled);

    /// button action occurred
    /// @param aPressed true if action is button pressed, false if action is button released
    /// @param aPressed true if action was detected on second key (for 2-way rocker switches)
    void buttonAction(bool aPressed, bool aSecondKey);

    /// @}


    /// @name interaction with digitalSTROM system
    /// @{

    /// handle message from vdSM
    /// @param aOperation the operation keyword
    /// @param aParams the parameters object, or NULL if none
    /// @return Error object if message generated an error
    // TODO: delete if button does not need to handle any message
    //virtual ErrorPtr handleMessage(string &aOperation, JsonObjectPtr aParams);

    /// get behaviour-specific parameter
    /// @param aParamName name of the parameter
    /// @param aArrayIndex index of the parameter if the parameter is an array
    /// @param aValue will receive the current value
    virtual ErrorPtr getBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t &aValue);

    /// set behaviour-specific parameter
    /// @param aParamName name of the parameter
    /// @param aArrayIndex index of the parameter if the parameter is an array
    /// @param aValue the new value to set
    virtual ErrorPtr setBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t aValue);

    /// load behaviour parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected)
    virtual ErrorPtr load();

    /// save unsaved behaviourparameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any behaviour parameters stored in persistent DB
    virtual ErrorPtr forget();
    
    /// @}
    

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    uint8_t getLTNUMGRP0();

    void setLTNUMGRP0(uint8_t aLTNUMGRP0);


  };

}


#endif /* defined(__p44bridged__buttonbehaviour__) */
