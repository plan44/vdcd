//
//  utils.cpp
//  p44utils
//
//  Created by Lukas Zeller on 16.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "utils.hpp"

#include <string.h>
#include <stdio.h>

using namespace p44;

// old-style C-formatted output into string object
void p44::string_format_v(std::string &aStringObj, bool aAppend, const char *aFormat, va_list aArgs)
{
  const size_t bufsiz=128;
  ssize_t actualsize;
  char buf[bufsiz];

  buf[0]='\0';
  char *bufP = NULL;
  if (!aAppend) aStringObj.erase();
  // using aArgs in vsnprintf() is destructive, need a copy in
  // case we call the function a second time
  va_list args;
  va_copy(args, aArgs);
  actualsize = vsnprintf(buf, bufsiz, aFormat, aArgs);
  if (actualsize>=(ssize_t)bufsiz) {
    // default buffer was too small, create bigger dynamic buffer
    bufP = new char[actualsize+1];
    actualsize = vsnprintf(bufP, actualsize+1, aFormat, args);
    if (actualsize>0) {
      aStringObj += bufP;
    }
    delete [] bufP;
  }
  else {
    // small default buffer was big enough, add it
    if (actualsize<0) return; // abort, error
    aStringObj += buf;
  }
  va_end(args);
} // vStringObjPrintf


// old-style C-formatted output as std::string
std::string p44::string_format(const char *aFormat, ...)
{
  va_list args;
  va_start(args, aFormat);
  std::string s;
  // now make the string
  string_format_v(s, false, aFormat, args);
  va_end(args);
  return s;
} // string_format


// old-style C-formatted output appending to std::string
void p44::string_format_append(std::string &aStringToAppendTo, const char *aFormat, ...)
{
  va_list args;

  va_start(args, aFormat);
  // now make the string
  string_format_v(aStringToAppendTo, true, aFormat, args);
  va_end(args);
} // string_format_append


const char *p44::nonNullCStr(const char *aNULLOrCStr)
{
	if (aNULLOrCStr==NULL) return "";
	return aNULLOrCStr;
}


string p44::lowerCase(const char *aString)
{
  string s;
  while (char c=*aString++) {
    s += tolower(c);
  }
  return s;
}


string p44::lowerCase(const string &aString)
{
  return lowerCase(aString.c_str());
}


string p44::shellQuote(const char *aString)
{
  string s = "\"";
  while (char c=*aString++) {
    if (c=='"' || c=='\\') s += '\\'; // escape double quotes and backslashes
    s += c;
  }
  s += '"';
  return s;
}


string p44::shellQuote(const string &aString)
{
  return shellQuote(aString.c_str());
}







