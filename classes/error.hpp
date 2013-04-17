//
//  error.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__dcerror__
#define __p44bridged__dcerror__

#include "p44bridged_common.hpp"

typedef long ErrorCode;

class Error;
typedef boost::shared_ptr<Error> ErrorPtr;
class Error {
  ErrorCode errorCode;
  std::string errorMessage;
public:
  Error(ErrorCode aErrorCode);
  Error(ErrorCode aErrorCode, std::string aErrorMessage);
  ErrorCode getErrorCode();
  virtual const char *getErrorDomain();
  virtual std::string description();
};



#endif /* defined(__p44bridged__dcerror__) */
