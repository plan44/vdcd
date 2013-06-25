//
//  dsdefs.h
//  p44bridged
//
//  Created by Lukas Zeller on 11.06.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef p44bridged_dsdefs_h
#define p44bridged_dsdefs_h

/// scene numbers
typedef enum {
  START_ZONE_SCENES = 0,  /**< first zone scene */
  T0_S0 = 0,              /**< main off scene */
  T1_S0 = 1,              /**< group 1 off scene */
  T2_S0 = 2,              /**< group 2 off scene */
  T3_S0 = 3,              /**< group 3 off scene */
  T4_S0 = 4,              /**< group 4 off scene */
  T0_S1 = 5,              /**< main on scene */
  T1_S1 = 6,              /**< group 1 on scene */
  T2_S1 = 7,              /**< group 2 on scene */
  T3_S1 = 8,              /**< group 3 on scene */
  T4_S1 = 9,              /**< group 4 on scene */
  T1234_CONT = 10,        /**< group 1-4 increment/decrement continue */
  DEC_S = 11,             /**< decrement value */
  INC_S = 12,             /**< increment value */
  MIN_S = 13,             /**< minimum value */
  MAX_S = 14,             /**< maximum value */
  STOP_S = 15,            /**< stop */
  /**< empty */
  T0_S2 = 17,             /**< main scene 2 */
  T0_S3 = 18,             /**< main scene 3 */
  T0_S4 = 19,             /**< main scene 4 */
  T1_S2 = 20,             /**< group 1 scene 2 */
  T1_S3 = 21,             /**< group 1 scene 3 */
  T1_S4 = 22,             /**< group 1 scene 4 */
  T2_S2 = 23,             /**< group 2 scene 2 */
  T2_S3 = 24,             /**< group 2 scene 3 */
  T2_S4 = 25,             /**< group 2 scene 4 */
  T3_S2 = 26,             /**< group 3 scene 2 */
  T3_S3 = 27,             /**< group 3 scene 3 */
  T3_S4 = 28,             /**< group 3 scene 4 */
  T4_S2 = 29,             /**< group 4 scene 2 */
  T4_S3 = 30,             /**< group 4 scene 3 */
  T4_S4 = 31,             /**< group 4 scene 4 */
  T1E_S0 = 32,            /**< group 1 extended off scene */
  T1E_S1 = 33,            /**< group 1 extended on scene */
  T2E_S0 = 34,            /**< group 2 extended off scene */
  T2E_S1 = 35,            /**< group 2 extended on scene */
  T3E_S0 = 36,            /**< group 3 extended off scene */
  T3E_S1 = 37,            /**< group 3 extended on scene */
  T4E_S0 = 38,            /**< group 4 extended off scene */
  T4E_S1 = 39,            /**< group 4 extended on scene */
  /**< empty */
  /**< empty */
  T1_DEC = 42,            /**< group 1 decrement value */
  T1_INC = 43,            /**< group 1 increment value */
  T2_DEC = 44,            /**< group 2 decrement value */
  T2_INC = 45,            /**< group 2 increment value */
  T3_DEC = 46,            /**< group 3 decrement value */
  T3_INC = 47,            /**< group 3 increment value */
  T4_DEC = 48,            /**< group 4 decrement value */
  T4_INC = 49,            /**< group 4 increment value */
  LOCAL_OFF = 50,         /**< local button off scene */
  LOCAL_ON = 51,          /**< local button on scene */
  T1_STOP_S = 52,         /**< group 1 stop */
  T2_STOP_S = 53,         /**< group 2 stop */
  T3_STOP_S = 54,         /**< group 3 stop */
  T4_STOP_S = 55,         /**< group 4 stop */

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
} DsScene;

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

/// button mode aka "LTMODE"
typedef enum {
  buttonmode_standard = 0,
  buttonmode_turbo = 1,
  buttonmode_presence = 2,
  buttonmode_switch = 3,
  buttonmode_reserved1 = 4,
  buttonmode_rockerDown1 = 5,
  buttonmode_rockerDown2 = 6,
  buttonmode_rockerDown3 = 7,
  buttonmode_rockerDown4 = 8,
  buttonmode_rockerUp1 = 9,
  buttonmode_rockerUp2 = 10,
  buttonmode_rockerUp3 = 11,
  buttonmode_rockerUp4 = 12,
  buttonmode_rockerUpDown = 13,
  buttonmode_standard_multi = 14,
  buttonmode_reserved2 = 15,
  buttonmode_akm_rising1_falling0 = 16,
  buttonmode_akm_rising0_falling1 = 17,
  buttonmode_akm_rising1 = 18,
  buttonmode_akm_falling1 = 19,
  buttonmode_akm_rising0 = 20,
  buttonmode_akm_falling0 = 21,
  buttonmode_akm_risingToggle = 22,
  buttonmode_akm_fallingToggle = 23,
  buttonmode_inactive = 255
} DsButtonMode;


/// hardware button type aka "button number"
typedef enum {
  hwbuttontype_1way = 0,
  hwbuttontype_2way = 1,
  hwbuttontype_2x1way = 2,
  hwbuttontype_4way = 3,
  hwbuttontype_4x1way = 4,
  hwbuttontype_2x2way = 5,
  hwbuttontype_reserved = 6,
  hwbuttontype_none = 7
} DsHardwareButtonType;


/// button function aka "LTNUM" (lower 4 bits in LTNUMGRP0)
typedef enum {
  // all colored buttons
  buttonfunc_device = 0, ///< device button (and preset 2-4)
  buttonfunc_area1_preset0x = 1, ///< area1 button (and preset 2-4)
  buttonfunc_area2_preset0x = 2, ///< area2 button (and preset 2-4)
  buttonfunc_area3_preset0x = 3, ///< area3 button (and preset 2-4)
  buttonfunc_area4_preset0x = 4, ///< area4 button (and preset 2-4)
  buttonfunc_room_preset0x = 5, ///< room button (and preset 1-4)
  buttonfunc_room_preset1x = 6, ///< room button (and preset 10-14)
  buttonfunc_room_preset2x = 7, ///< room button (and preset 20-24)
  buttonfunc_room_preset3x = 8, ///< room button (and preset 30-34)
  buttonfunc_room_preset4x = 9, ///< room button (and preset 40-44)
  buttonfunc_area1_preset1x = 10, ///< area1 button (and preset 12-14)
  buttonfunc_area2_preset2x = 11, ///< area2 button (and preset 22-24)
  buttonfunc_area3_preset3x = 12, ///< area3 button (and preset 32-34)
  buttonfunc_area4_preset4x = 13, ///< area4 button (and preset 42-44)
  // black buttons
  buttonfunc_alarm = 1, ///< alarm
  buttonfunc_panic = 2, ///< panic
  buttonfunc_leave = 3, ///< leaving home
  buttonfunc_doorbell = 5, ///< door bell
  buttonfunc_apartment = 14, ///< appartment button
  buttonfunc_app = 15, ///< application specific button
} DsButtonFunc;


/// output modes
typedef enum {
  outputmode_none = 0, ///< no output
  outputmode_diagnostic = 1, ///< diagnostic module
  outputmode_switch = 16, ///< switch output
  outputmode_dim_eff = 17, ///< effective value dimmer
  outputmode_dim_eff_char = 18, ///< effective value dimmer with characteristic curve
  outputmode_dim_leading = 19, ///< leading edge phase dimmer
  outputmode_dim_leading_char = 20, ///< leading edge phase dimmer with characteristic curve
  outputmode_dim_phase_trailing = 21, ///< trailing edge phase dimmer
  outputmode_dim_phase_trailing_char = 22, ///< trailing edge phase dimmer with characteristic curve
  outputmode_dim_pwm = 23, ///< PWM
  outputmode_dim_pwm_char = 24, ///< PWM with characteristic curve
  outputmode_transient_off = 25, ///< transient off switch
  outputmode_transient_on = 26, ///< transient on switch
  outputmode_trp_trn = 27, ///< Schalter mit fixem Ansteuerwinkel (TRP/TRN)
  outputmode_dim_wavepacket = 28, ///< wave packet dimmer

  //  Leistungsausgangsbetriebsart für XX-KL 200 und XX-ZWS 200
  //  32 Umsteuerrelais
  //  33 Positionssteuerung (Rolladen, Markise, Jalousie)
  //  34 Schalterzweistufige
  //  35 Schalterzweipolige
  //  36 Ausschaltwischer zweipolig
  //  37 Einschaltwischer zweipolig
  //  38 Schalter dreistufig
  //  39 Schalter (mit automatischer Szenenumkonfiguration)
  //  40 Wischer (mit automatischer Szenenumkonfiguration)
  //  41 Sparen (mit automatischer Szenenumkonfiguration)
  //  42 Positionssteuerung mit unkalibrierter Lamellenverstellung (Jalousie)

} DsOutputModes;



#endif
