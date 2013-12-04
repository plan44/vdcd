//
//  vdcapi.hpp
//  vdcd
//
//  Created by Lukas Zeller on 04.12.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__apiconnection__
#define __vdcd__apiconnection__

#include "p44_common.hpp"

#include "apivalue.hpp"
#include "socketcomm.hpp"


#ifndef VDC_API_NO_JSON
#include "jsonrpccomm.hpp"
#endif

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
    virtual ApiValuePtr newApiValue() = 0;

    /// The underlying socket connection
    virtual SocketCommPtr socketConnection() = 0;


    /// send a API request
    /// @param aMethod the vDC API method or notification name to be sent
    /// @param aParams the parameters for the method or notification request. Can be NULL.
    /// @param aResponseHandler if the request is a method call, this handler will be called when the method result arrives
    ///   Note that the aResponseHandler might not be called at all in case of lost messages etc. So do not rely on
    ///   this callback for chaining a execution thread.
    /// @return empty or Error object in case of error
    virtual ErrorPtr sendRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler = VdcApiResponseCB()) = 0;

    /// send a vDC API result (answer for successful method call)
    /// @param aForRequest this must be the VdcApiRequestPtr received in the VdcApiRequestCB handler.
    /// @param aResult the result as a ApiValue. Can be NULL for procedure calls without return value
    /// @result empty or Error object in case of error sending result response
    virtual ErrorPtr sendResult(VdcApiRequestPtr aForRequest, ApiValuePtr aResult) = 0;

    /// send a vDC API error (answer for unsuccesful method call)
    /// @param aForRequest this must be the VdcApiRequestPtr received in the VdcApiRequestCB handler.
    /// @param aErrorCode the error code
    /// @param aErrorMessage the error message or NULL to generate a standard text
    /// @param aErrorData the optional "data" member for the vDC API error object
    /// @result empty or Error object in case of error sending error response
    virtual ErrorPtr sendError(VdcApiRequestPtr aForRequest, uint32_t aErrorCode, string aErrorMessage = "", ApiValuePtr aErrorData = ApiValuePtr()) = 0;

    /// send p44utils::Error object as vDC API error
    /// @param aForRequest this must be the VdcApiRequestPtr received in the VdcApiRequestCB handler.
    /// @param aErrorToSend From this error object, getErrorCode() and description() will be used as "code" and "message" members
    ///   of the vDC API error object.
    /// @result empty or Error object in case of error sending error response
    ErrorPtr sendError(VdcApiRequestPtr aForRequest, ErrorPtr aErrorToSend);

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

    /// set connection monitor
    /// @param aConnectionCB will be called when connections opens, ends or has error
    void setConnectionStatusHandler(VdcApiConnectionCB aConnectionCB);

    /// start API server
    void start();

    /// stop API server, close all connections
    void stop();

  protected:

    virtual VdcApiConnectionPtr newConnection() = 0;

  private:

    SocketCommPtr serverConnectionHandler(SocketComm *aServerSocketCommP);
    void connectionStatusHandler(SocketComm *aSocketComm, ErrorPtr aError);

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

  };


  /// TODO: move to separate file


  class VdcJsonApiConnection;
  class VdcJsonApiServer;
  class VdcJsonApiRequest;

  typedef boost::intrusive_ptr<VdcJsonApiConnection> VdcJsonApiConnectionPtr;
  typedef boost::intrusive_ptr<VdcJsonApiServer> VdcJsonApiServerPtr;
  typedef boost::intrusive_ptr<VdcJsonApiRequest> VdcJsonApiRequestPtr;


  /// a JSON API server
  class VdcJsonApiServer : public VdcApiServer
  {
    typedef VdcApiServer inherited;

  protected:

    virtual VdcApiConnectionPtr newConnection();

  };


  class VdcJsonApiConnection : public VdcApiConnection
  {
    typedef VdcApiConnection inherited;

    JsonRpcCommPtr jsonRpcComm;

  public:

    VdcJsonApiConnection();

    virtual SocketCommPtr socketConnection() { return jsonRpcComm; };

    virtual void closeAfterSend();

    virtual ApiValuePtr newApiValue();

    virtual ErrorPtr sendRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler = VdcApiResponseCB());
    virtual ErrorPtr sendResult(VdcApiRequestPtr aForRequest, ApiValuePtr aResult);
    virtual ErrorPtr sendError(VdcApiRequestPtr aForRequest, uint32_t aErrorCode, string aErrorMessage = "", ApiValuePtr aErrorData = ApiValuePtr());

  private:

    void jsonRequestHandler(JsonRpcComm *aJsonRpcComm, const char *aMethod, const char *aJsonRpcId, JsonObjectPtr aParams);
    void jsonResponseHandler(VdcApiResponseCB aResponseHandler, int32_t aResponseId, ErrorPtr &aError, JsonObjectPtr aResultOrErrorData);


  };




  /// a single request which needs to be answered
  class VdcJsonApiRequest : public VdcApiRequest
  {
    typedef VdcApiRequest inherited;

    string jsonRpcId;
    VdcJsonApiConnectionPtr jsonConnection;

  public:

    /// constructor
    VdcJsonApiRequest(VdcJsonApiConnectionPtr aConnection, const char *aJsonRpcId);


    /// return the request ID as a string
    /// @return request ID as string
    virtual string requestId() { return jsonRpcId; }

    /// get the API connection this request originates from
    /// @return API connection
    virtual VdcApiConnectionPtr connection() { return jsonConnection; }
    
  };



  /// TODO: move to separate file


  /// a protobuf API server
  class VdcPbufApiServer : public VdcApiServer
  {
    typedef VdcApiServer inherited;

  public:

    VdcPbufApiServer();

  protected:

    virtual VdcApiConnectionPtr newConnection();

  };


}


#endif /* defined(__vdcd__apiconnection__) */
