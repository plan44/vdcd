//
//  utils.h
//  p44bridged
//
//  Created by Lukas Zeller on 16.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__utils__
#define __p44bridged__utils__

#include <string>

/// printf-style format into std::string
/// @param aFormat printf-style format string
/// @return formatted string
std::string string_format(const char *aFormat, ...);

/// printf-style format appending to std::string
/// @param aStringToAppendTo std::string to append format to
/// @param aFormat printf-style format string
void string_format_append(std::string &aStringToAppendTo, const char *aFormat, ...);


#endif /* defined(__p44bridged__utils__) */
