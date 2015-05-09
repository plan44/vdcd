//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#include "error.hpp"

#include <string.h>
#include <errno.h>

#include "utils.hpp"

using namespace p44;

#pragma mark - error base class

Error::Error(ErrorCode aErrorCode)
{
  errorCode = aErrorCode;
  errorMessage.clear();
}


Error::Error(ErrorCode aErrorCode, const std::string &aErrorMessage)
{
  errorCode = aErrorCode;
  errorMessage = aErrorMessage;
}


ErrorCode Error::getErrorCode() const
{
  return errorCode;
}


const char *Error::domain()
{
  return "Error_baseClass";
}


const char *Error::getErrorDomain() const
{
  return Error::domain();
}


const char *Error::getErrorMessage() const
{
  return errorMessage.c_str();
}



string Error::description() const
{
  string errorText;
  if (errorMessage.size()>0)
    errorText = errorMessage;
  else
    errorText = "Error";
  // Append domain and code to message text
  string_format_append(errorText, " (%s:%ld)", getErrorDomain() , errorCode);
  return errorText;
}



bool Error::isError(const char *aDomain, ErrorCode aErrorCode) const
{
  return aErrorCode==errorCode && (aDomain==NULL || isDomain(aDomain));
}



bool Error::isDomain(const char *aDomain) const
{
  return strcmp(aDomain, getErrorDomain())==0;
}



bool Error::isOK(ErrorPtr aError)
{
  return (aError==NULL || aError->getErrorCode()==0);
}



bool Error::isError(ErrorPtr aError, const char *aDomain, ErrorCode aErrorCode)
{
  if (!aError) return false;
  return aError->isError(aDomain, aErrorCode);
}


#pragma mark - system error


const char *SysError::domain()
{
  return "System";
}



const char *SysError::getErrorDomain() const
{
  return SysError::domain();
}



SysError::SysError(const char *aContextMessage) :
  Error(errno, string(nonNullCStr(aContextMessage)).append(nonNullCStr(strerror(errno))))
{
}



SysError::SysError(int aErrNo, const char *aContextMessage) :
  Error(aErrNo, string(nonNullCStr(aContextMessage)).append(nonNullCStr(strerror(errno))))
{
}



ErrorPtr SysError::errNo(const char *aContextMessage)
{
  if (errno==0)
    return ErrorPtr(); // empty, no error
  return ErrorPtr(new SysError(aContextMessage));
}



ErrorPtr SysError::err(int aErrNo, const char *aContextMessage)
{
  if (aErrNo==0)
    return ErrorPtr(); // empty, no error
  return ErrorPtr(new SysError(aErrNo, aContextMessage));
}


#pragma mark - web error


ErrorPtr WebError::err(uint16_t aHTTPError, std::string aErrorMessage)
{
  if (aHTTPError==0)
    return ErrorPtr(); // empty, no error
  return ErrorPtr(new WebError(aHTTPError, aErrorMessage));
}


#pragma mark - text error


ErrorPtr TextError::err(const char *aFormat, ...)
{
  va_list args;

  va_start(args, aFormat);
  // now make the string
  string s;
  string_format_v(s, true, aFormat, args);
  va_end(args);
  return ErrorPtr(new TextError(s));
}




