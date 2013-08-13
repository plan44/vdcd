//
//  error.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__dcerror__
#define __p44utils__dcerror__

#include "p44_common.hpp"

using namespace std;

namespace p44 {

  typedef long ErrorCode;

  typedef enum {
    ErrorOK,
  } CommonErrors;


  /// error base class
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
    Error(ErrorCode aErrorCode, const std::string &aErrorMessage);

    /// get error code
    /// @return the error code. Note that error codes are unique only within the same error domain.
    ///   error code 0 from any domain means OK.
    ErrorCode getErrorCode() const;

    /// get error domain
    /// @return the explicitly set error message, empty string if none is set.
    /// @note use description() to get a text at least howing the error domain and code if no message is set
    virtual const char *getErrorMessage() const;

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

    /// @return true if the error matches given domain
    bool isDomain(const char *aDomain) const;

    /// checks for OK condition, which means either no error object assigned at all to the smart ptr, or ErrorCode==0
    /// @param a ErrorPtr smart pointer
    /// @return true if OK
    static bool isOK(ErrorPtr aError);
  };


  /// C errno based system error
  class SysError : public Error
  {
  public:
    static const char *domain();
    virtual const char *getErrorDomain() const;

    /// create system error from current errno and set message to strerror() text
    SysError(const char *aContextMessage = NULL);

    /// create system error from passed errno and set message to strerror() text
    /// @param aErrNo a errno error number
    SysError(int aErrNo, const char *aContextMessage = NULL);

    /// factory function to create a ErrorPtr either containing NULL (if errno indicates OK)
    /// or a SysError (if errno indicates error)
    static ErrorPtr errNo(const char *aContextMessage = NULL);

    /// factory function to create a ErrorPtr either containing NULL (if aErrNo indicates OK)
    /// or a SysError (if aErrNo indicates error)
    static ErrorPtr err(int aErrNo, const char *aContextMessage = NULL);
  };


} // namespace p44


#endif /* defined(__p44utils__dcerror__) */
