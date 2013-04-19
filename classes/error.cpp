//
//  error.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "error.hpp"


Error::Error(ErrorCode aErrorCode)
{
  errorCode = aErrorCode;
  errorMessage.clear();
}


Error::Error(ErrorCode aErrorCode, std::string aErrorMessage)
{
  errorCode = aErrorCode;
  errorMessage = aErrorMessage;
}


ErrorCode Error::getErrorCode() const
{
  return errorCode;
}


const char *Error::getErrorDomain() const
{
  return "Error_baseClass";
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


