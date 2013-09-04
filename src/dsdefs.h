//
//  dsdefs.h
//  vdcd
//
//  Created by Lukas Zeller on 11.06.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef vdcd_dsdefs_h
#define vdcd_dsdefs_h

/// scene numbers
typedef enum {
  START_ZONE_SCENES = 0,  /**< first zone scene */
  T0_S0 = 0,              /**< main off scene */
  T1_S0 = 1,              /**< area 1 off scene */
  T2_S0 = 2,              /**< area 2 off scene */
  T3_S0 = 3,              /**< area 3 off scene */
  T4_S0 = 4,              /**< area 4 off scene */
  T0_S1 = 5,              /**< main on scene */
  T1_S1 = 6,              /**< area 1 on scene */
  T2_S1 = 7,              /**< area 2 on scene */
  T3_S1 = 8,              /**< area 3 on scene */
  T4_S1 = 9,              /**< area 4 on scene */
  T1234_CONT = 10,        /**< area 1-4 increment/decrement continue */
  DEC_S = 11,             /**< decrement value */
  INC_S = 12,             /**< increment value */
  MIN_S = 13,             /**< minimum value */
  MAX_S = 14,             /**< maximum value */
  STOP_S = 15,            /**< stop */
  /**< empty */
  T0_S2 = 17,             /**< main scene 2 */
  T0_S3 = 18,             /**< main scene 3 */
  T0_S4 = 19,             /**< main scene 4 */
  T1_S2 = 20,             /**< area 1 scene 2 */
  T1_S3 = 21,             /**< area 1 scene 3 */
  T1_S4 = 22,             /**< area 1 scene 4 */
  T2_S2 = 23,             /**< area 2 scene 2 */
  T2_S3 = 24,             /**< area 2 scene 3 */
  T2_S4 = 25,             /**< area 2 scene 4 */
  T3_S2 = 26,             /**< area 3 scene 2 */
  T3_S3 = 27,             /**< area 3 scene 3 */
  T3_S4 = 28,             /**< area 3 scene 4 */
  T4_S2 = 29,             /**< area 4 scene 2 */
  T4_S3 = 30,             /**< area 4 scene 3 */
  T4_S4 = 31,             /**< area 4 scene 4 */
  T1E_S0 = 32,            /**< area 1 extended off scene */
  T1E_S1 = 33,            /**< area 1 extended on scene */
  T2E_S0 = 34,            /**< area 2 extended off scene */
  T2E_S1 = 35,            /**< area 2 extended on scene */
  T3E_S0 = 36,            /**< area 3 extended off scene */
  T3E_S1 = 37,            /**< area 3 extended on scene */
  T4E_S0 = 38,            /**< area 4 extended off scene */
  T4E_S1 = 39,            /**< area 4 extended on scene */
  /**< empty */
  /**< empty */
  T1_DEC = 42,            /**< area 1 decrement value */
  T1_INC = 43,            /**< area 1 increment value */
  T2_DEC = 44,            /**< area 2 decrement value */
  T2_INC = 45,            /**< area 2 increment value */
  T3_DEC = 46,            /**< area 3 decrement value */
  T3_INC = 47,            /**< area 3 increment value */
  T4_DEC = 48,            /**< area 4 decrement value */
  T4_INC = 49,            /**< area 4 increment value */
  LOCAL_OFF = 50,         /**< local button off scene */
  LOCAL_ON = 51,          /**< local button on scene */
  T1_STOP_S = 52,         /**< area 1 stop */
  T2_STOP_S = 53,         /**< area 2 stop */
  T3_STOP_S = 54,         /**< area 3 stop */
  T4_STOP_S = 55,         /**< area 4 stop */

  START_APARTMENT_SCENES = 64,                    /**< first apartment scene */
  AUTO_STANDBY = (START_APARTMENT_SCENES + 0),    /**< auto-standby scene */
  SIG_PANIC = (START_APARTMENT_SCENES + 1),       /**< panic */
  ENERGY_OL = (START_APARTMENT_SCENES + 2),       /**< overload energy consumption dSM */
  STANDBY = (START_APARTMENT_SCENES + 3),         /**< standby scene */
  DEEP_OFF = (START_APARTMENT_SCENES + 4),        /**< deep off scene */
  SLEEPING = (START_APARTMENT_SCENES + 5),        /**< sleeping */
  WAKE_UP = (START_APARTMENT_SCENES + 6),         /**< awake */
  PRESENT = (START_APARTMENT_SCENES + 7),         /**< at home */
  ABSENT = (START_APARTMENT_SCENES + 8),          /**< not at home */
  SIG_BELL = (START_APARTMENT_SCENES + 9),        /**< bell */
  SIG_ALARM = (START_APARTMENT_SCENES + 10),      /**< alarm/fire */
} DsSceneNumber;

/// group/color (upper 4 bits in LTNUMGRP0)
typedef enum {
  group_variable = 0,
  group_yellow_light = 1,
  group_grey_shadow = 2,
  group_blue_climate = 3,
  group_cyan_audio = 4,
  group_magenta_video = 5,
  group_red_security = 6,
  group_green_access = 7,
  group_black_joker = 8,
  group_white = 9,
  group_displays = 10
} DsGroup;

typedef uint64_t DsGroupMask; ///< 64 bit mask, Bit0 = group 0, Bit63 = group 63


/// button click types
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
  ct_none = 255 ///< no click (for state)
} DsClickType;


/// button mode aka "LTMODE"
typedef enum {
  buttonMode_standard = 0,
  buttonMode_turbo = 1,
  buttonMode_presence = 2,
  buttonMode_switch = 3,
  buttonMode_reserved1 = 4,
  buttonMode_rockerDown1 = 5,
  buttonMode_rockerDown2 = 6,
  buttonMode_rockerDown3 = 7,
  buttonMode_rockerDown4 = 8,
  buttonMode_rockerUp1 = 9,
  buttonMode_rockerUp2 = 10,
  buttonMode_rockerUp3 = 11,
  buttonMode_rockerUp4 = 12,
  buttonMode_rockerUpDown = 13,
  buttonMode_standard_multi = 14,
  buttonMode_reserved2 = 15,
  buttonMode_akm_rising1_falling0 = 16,
  buttonMode_akm_rising0_falling1 = 17,
  buttonMode_akm_rising1 = 18,
  buttonMode_akm_falling1 = 19,
  buttonMode_akm_rising0 = 20,
  buttonMode_akm_falling0 = 21,
  buttonMode_akm_risingToggle = 22,
  buttonMode_akm_fallingToggle = 23,
  buttonMode_inactive = 255
} DsButtonMode;


/// button types (for buttonDescriptions[].buttonType)
typedef enum {
  buttonType_undefined, ///< kind of button not defined by device hardware
  buttonType_single, ///< single pushbutton
  buttonType_2way, ///< two-way pushbutton or rocker
  buttonType_4way, ///< 4-way navigation button
  buttonType_4wayWithCenter, ///< 4-way navigation with center button
  buttonType_8wayWithCenter, ///< 8-way navigation with center button
  buttonType_onOffSwitch, ///< On-Off switch
} DsButtonType;


/// button element IDs (for buttonDescriptions[].buttonElementID)
typedef enum {
  buttonElement_center, ///< center element / single button
  buttonElement_down, ///< down, for 2,4,8-way
  buttonElement_up, ///< up, for 2,4,8-way
  buttonElement_left, ///< left, for 2,4,8-way
  buttonElement_right, ///< right, for 2,4,8-way
  buttonElement_upperLeft, ///< upper left, for 8-way
  buttonElement_lowerLeft, ///< lower left, for 8-way
  buttonElement_upperRight, ///< upper right, for 8-way
  buttonElement_lowerRight, ///< lower right, for 8-way
} DsButtonElement;


/// button function aka "LTNUM" (lower 4 bits in LTNUMGRP0)
typedef enum {
  // all colored buttons
  buttonFunc_device = 0, ///< device button (and preset 2-4)
  buttonFunc_area1_preset0x = 1, ///< area1 button (and preset 2-4)
  buttonFunc_area2_preset0x = 2, ///< area2 button (and preset 2-4)
  buttonFunc_area3_preset0x = 3, ///< area3 button (and preset 2-4)
  buttonFunc_area4_preset0x = 4, ///< area4 button (and preset 2-4)
  buttonFunc_room_preset0x = 5, ///< room button (and preset 1-4)
  buttonFunc_room_preset1x = 6, ///< room button (and preset 10-14)
  buttonFunc_room_preset2x = 7, ///< room button (and preset 20-24)
  buttonFunc_room_preset3x = 8, ///< room button (and preset 30-34)
  buttonFunc_room_preset4x = 9, ///< room button (and preset 40-44)
  buttonFunc_area1_preset1x = 10, ///< area1 button (and preset 12-14)
  buttonFunc_area2_preset2x = 11, ///< area2 button (and preset 22-24)
  buttonFunc_area3_preset3x = 12, ///< area3 button (and preset 32-34)
  buttonFunc_area4_preset4x = 13, ///< area4 button (and preset 42-44)
  // black buttons
  buttonFunc_alarm = 1, ///< alarm
  buttonFunc_panic = 2, ///< panic
  buttonFunc_leave = 3, ///< leaving home
  buttonFunc_doorbell = 5, ///< door bell
  buttonFunc_apartment = 14, ///< appartment button
  buttonFunc_app = 15, ///< application specific button
} DsButtonFunc;


/// output functions
typedef enum {
  outputFunction_switch, ///< switch output
  outputFunction_dimmer, ///< effective value dimmer
  outputFunction_positional, ///< positional (servo, valve, blinds)
} DsOutputFunction;

/// output modes
typedef enum {
  outputmode_disabled, ///< disabled
  outputmode_binary, ///< binary ON/OFF mode
  outputmode_gradual, ///< gradual output value (dimmer, positional etc.)
} DsOutputMode;

/// hardware error status
typedef enum {
  hardwareError_none, ///< hardware is ok
  hardwareError_openCircuit, ///< input or output open circuit  (eg. bulb burnt)
  hardwareError_shortCircuit, ///< input or output short circuit
  hardwareError_overload, ///< output overload
  hardwareError_busConnection, ///< third party device bus problem (such as DALI short-circuit)
  hardwareError_lowBattery, ///< third party device has low battery
  hardwareError_deviceError, ///< other device error
} DsHardwareError;


/// sensor types
typedef enum {
  sensorType_none,
  sensorType_temperature, ///< temperature in degrees celsius
  sensorType_humitity, ///< relative humitity in %
  sensorType_illumination, ///< illumination in lux
  sensorType_supplyVoltage, ///< supply voltage level in Volts
  sensorType_gas_CO, ///< CO concentration in ppm
  sensorType_gas_radon, ///< Radon activity in Bq/m3
  sensorType_gas_type, ///< gas type sensor
  sensorType_dust_PM10, ///< particles <10µm in μg/m3
  sensorType_dust_PM2_5, ///< particles <2.5µm in μg/m3
  sensorType_dust_PM1, ///< particles <1µm in μg/m3
  sensorType_set_point, ///< room operating panel set point, 0..1
  sensorType_fan_speed, ///< fan speed, 0..1 (0=off, <0=auto)
  sensorType_wind_speed, ///< wind speed in m/s
} DsSensorType;


/// binary input types (sensor functions)
typedef enum {
  binInpType_none, ///< no system function
  binInpType_presence, ///< Presence
  binInpType_light, ///< Light
  binInpType_presenceInDarkness, ///< Presence in darkness
  binInpType_twilight, ///< twilight
  binInpType_motion, ///< motion
  binInpType_motionInDarkness, ///< motion in darkness
  binInpType_smoke, ///< smoke
  binInpType_wind, ///< wind
  binInpType_rain, ///< rain
  binInpType_sun, ///< solar radiation
  binInpType_thermostat, ///< thermostat
} DsBinaryInputType;


#endif
