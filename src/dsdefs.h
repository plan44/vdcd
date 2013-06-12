//
//  dsdefs.h
//  p44bridged
//
//  Created by Lukas Zeller on 11.06.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef p44bridged_dsdefs_h
#define p44bridged_dsdefs_h

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
} Scene;


#endif
