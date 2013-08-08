//
//  error.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
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


std::string Error::description() const
{
  std::string errorText;
  if (errorMessage.size()>0)
    errorText = errorMessage;
  else
    errorText = "Error";
  // create error message from code
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
  Error(aErrNo, nonNullCStr(strerror(aErrNo)))
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

