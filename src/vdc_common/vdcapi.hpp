//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __vdcd__apiconnection__
#define __vdcd__apiconnection__

#include "p44_common.hpp"

#include "apivalue.hpp"
#include "socketcomm.hpp"


using namespace std;

namespace p44 {


  class VdcApiError : public Error
  {
  public:
    static const char *domain() { return "VdcApi"; }
    virtual const char *getErrorDomain() const { return VdcApiError::domain(); };
    VdcApiError(ErrorCode aError) : Error(aError) {};
    VdcApiError(ErrorCode aError, const std::string &aErrorMessage) : Error(aError, aErrorMessage) {};
  };


  class VdcApiConnection;
  class VdcApiServer;
  class VdcApiRequest;

  typedef boost::intrusive_ptr<VdcApiConnection> VdcApiConnectionPtr;
  typedef boost::intrusive_ptr<VdcApiServer> VdcApiServerPtr;
  typedef boost::intrusive_ptr<VdcApiRequest> VdcApiRequestPtr;

  /// callback for delivering a API request (needs answer) or notification (does not need answer)
  /// @param aApiConnection the VdcApiConnection calling this handler
  /// @param aRequest the request. The handler must pass this object when calling sendResult(). If this is a notification, aRequest is NULL.
  /// @param aMethod If this is a method call, this is the JSON-RPC (2.0) method or notification requested by the peer.
  /// @param aParams the parameters of the request as ApiValue
  typedef boost::function<void (VdcApiConnectionPtr aApiConnection, VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)> VdcApiRequestCB;

  /// callback for delivering the result for a previously sent request
  /// @param aApiConnection the VdcApiConnection calling this handler
  /// @param aRequest the request that caused this answer
  /// @param aError the referenced ErrorPtr will be set when an error occurred.
  ///   If the error returned is an VdcApiError, aError.getErrorCode() will return the "code" member from the API error object,
  ///   and aError.description() will return the "message" member from the API error object.
  ///   aResultOrErrorData will contain the "data" member from the JSON-RPC error object, if any.
  /// @param aResultOrErrorData the result object in case of success, or the "data" member from the JSON-RPC error object
  ///   in case of an error returned via JSON-RPC from the remote peer.
  typedef boost::function<void (VdcApiConnectionPtr aApiConnection, VdcApiRequestPtr aRequest, ErrorPtr &aError, ApiValuePtr aResultOrErrorData)> VdcApiResponseCB;


  /// callback for announcing new API connection (which may or may not lead to a session) or termination of a connection
  /// @param aApiConnection the VdcApiConnection calling this handler
  /// @param aError set if an error occurred on the connection (including remote having closed the connection)
  /// @param aResultOrErrorData the result object in case of success, or the "data" member from the JSON-RPC error object
  ///   in case of an error returned via JSON-RPC from the remote peer.
  typedef boost::function<void (VdcApiConnectionPtr aApiConnection, ErrorPtr &aError)> VdcApiConnectionCB;




  /// a single API connection
  class VdcApiConnection : public P44Obj
  {
    typedef P44Obj inherited;

  protected:

    VdcApiRequestCB apiRequestHandler;

  public:

    /// install callback for received API requests
    /// @param aApiRequestHandler will be called when a API request has been received
    void setRequestHandler(VdcApiRequestCB aApiRequestHandler);

    /// end connection
    void closeConnection();

    /// get a new API value suitable for this connection
    /// @return new API value of suitable internal implementation to be used on this API connection
    virtual ApiValuePtr newApiValue() = 0;

    /// The underlying socket connection
    /// @return socket connection
    virtual SocketCommPtr socketConnection() = 0;


    /// send a API request
    /// @param aMethod the vDC API method or notification name to be sent
    /// @param aParams the parameters for the method or notification request. Can be NULL.
    /// @param aResponseHandler if the request is a method call, this handler will be called when the method result arrives
    ///   Note that the aResponseHandler might not be called at all in case of lost messages etc. So do not rely on
    ///   this callback for chaining a execution thread.
    /// @return empty or Error object in case of error
    virtual ErrorPtr sendRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler = VdcApiResponseCB()) = 0;

    /// request closing connection after last message has been sent
    virtual void closeAfterSend() = 0;
  };



  /// a API server
  class VdcApiServer : public SocketComm
  {
    typedef SocketComm inherited;

    VdcApiConnectionCB apiConnectionStatusHandler; ///< connection status handler

  public:

    VdcApiServer();

    /// set connection status handler
    /// @param aConnectionCB will be called when connections opens, ends or has error
    void setConnectionStatusHandler(VdcApiConnectionCB aConnectionCB);

    /// start API server
    void start();

    /// stop API server, close all connections
    void stop();

  protected:

    /// create API connection of correct type for this API server
    /// @return API connection
    virtual VdcApiConnectionPtr newConnection() = 0;

  private:

    SocketCommPtr serverConnectionHandler(SocketCommPtr aServerSocketComm);
    void connectionStatusHandler(SocketCommPtr aSocketComm, ErrorPtr aError);

  };



  /// a single request which needs to be answered
  class VdcApiRequest : public P44Obj
  {
    typedef P44Obj inherited;

  public:

    /// return the request ID as a string
    /// @return request ID as string
    virtual string requestId() = 0;

    /// get the API connection this request originates from
    /// @return API connection
    virtual VdcApiConnectionPtr connection() = 0;

    /// get a new API value suitable for answering this request connection
    /// @return new API value of suitable internal implementation to be used on this API connection
    virtual ApiValuePtr newApiValue() { return connection()->newApiValue(); }; // default is asking connection

    /// send a vDC API result (answer for successful method call)
    /// @param aResult the result as a ApiValue. Can be NULL for procedure calls without return value
    /// @result empty or Error object in case of error sending result response
    virtual ErrorPtr sendResult(ApiValuePtr aResult) = 0;

    /// send a vDC API error (answer for unsuccesful method call)
    /// @param aErrorCode the error code
    /// @param aErrorMessage the error message or NULL to generate a standard text
    /// @param aErrorData the optional "data" member for the vDC API error object
    /// @result empty or Error object in case of error sending error response
    virtual ErrorPtr sendError(uint32_t aErrorCode, string aErrorMessage = "", ApiValuePtr aErrorData = ApiValuePtr()) = 0;

    /// send p44utils::Error object as vDC API error
    /// @param aForRequest this must be the VdcApiRequestPtr received in the VdcApiRequestCB handler.
    /// @param aErrorToSend From this error object, getErrorCode() and description() will be used as "code" and "message" members
    ///   of the vDC API error object.
    /// @result empty or Error object in case of error sending error response
    ErrorPtr sendError(ErrorPtr aErrorToSend);

  };

}


#endif /* defined(__vdcd__apiconnection__) */
