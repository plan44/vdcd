//
//  Copyright (c) 2019-2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//

#ifndef __p44utils__config__
#define __p44utils__config__

#include "p44vdc_config.hpp" // include vdcd specific settings that might also influence p44utils

#ifndef ENABLE_NAMED_ERRORS
  #define ENABLE_NAMED_ERRORS P44_CPP17_FEATURE // Enable if compiler can do C++17
#endif
#ifndef ENABLE_P44SCRIPT
  #define ENABLE_P44SCRIPT 1 // Scripting support in some of the p44utils components
#endif
#ifndef ENABLE_JSON_APPLICATION
  #define ENABLE_JSON_APPLICATION 1 // JSON resource file handling support in Application
#endif
#ifndef ENABLE_P44LRGRAPHICS
  #define ENABLE_P44LRGRAPHICS 1 // p44lrgraphics support in some of the p44utils components
#endif
#ifndef ENABLE_APPLICATION_SUPPORT
  #define ENABLE_APPLICATION_SUPPORT 1 // support for Application (e.g. domain specific commandline options) in other parts of P44 utils
#endif


#endif // __p44utils__config__
