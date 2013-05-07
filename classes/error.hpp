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

using namespace std;

namespace p44 {

  typedef long ErrorCode;

  class Error;
  typedef boost::shared_ptr<Error> ErrorPtr;
  class Error {
    ErrorCode errorCode;
    std::string errorMessage;
  public:
    static const char *domain();
    /// create error with error code
    /// @param aErrorCode error code. aErrorCode==0 from any domain means OK.
    Error(ErrorCode aErrorCode);
    /// create error with error code and message
    /// @param aErrorCode error code. aErrorCode==0 from any domain means OK.
    /// @param aErrorMessage error message
    Error(ErrorCode aErrorCode, std::string aErrorMessage);
    /// get error code
    /// @return the error code. Note that error codes are unique only within the same error domain.
    ///   error code 0 from any domain means OK.
    ErrorCode getErrorCode() const;
    /// get error domain
    /// @return the error domain constant string
    virtual const char *getErrorDomain() const;
    /// get the description of the error
    /// @return a description string. If an error message was not set, a standard string with the error domain and number will be shown
    virtual std::string description() const;
    /// check for a specific error
    /// @param aDomain the domain or NULL to match any domain
    /// @param aErrorCode the error code to match
    /// @return true if the error matches domain and code
    bool isError(const char *aDomain, ErrorCode aErrorCode) const;
    static bool isError(ErrorPtr aError, const char *aDomain, ErrorCode aErrorCode);
    /// checks for OK condition, which means either no error object assigned at all to the smart ptr, or ErrorCode==0
    /// @param a ErrorPtr smart pointer
    /// @return true if OK
    static bool isOK(ErrorPtr aError);
  };

} // namespace p44


#endif /* defined(__p44bridged__dcerror__) */
