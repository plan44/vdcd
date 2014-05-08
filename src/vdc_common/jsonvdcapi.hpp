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

#ifndef __vdcd__jsonvdcapi__
#define __vdcd__jsonvdcapi__

#include "p44_common.hpp"

#include "vdcapi.hpp"

#include "jsonrpccomm.hpp"

using namespace std;

namespace p44 {

  class VdcJsonApiConnection;
  class VdcJsonApiServer;
  class VdcJsonApiRequest;

  typedef boost::intrusive_ptr<VdcJsonApiConnection> VdcJsonApiConnectionPtr;
  typedef boost::intrusive_ptr<VdcJsonApiServer> VdcJsonApiServerPtr;
  typedef boost::intrusive_ptr<VdcJsonApiRequest> VdcJsonApiRequestPtr;



  class JsonApiValue;

  typedef boost::intrusive_ptr<JsonApiValue> JsonApiValuePtr;

  class JsonApiValue : public ApiValue
  {
    typedef ApiValue inherited;

    // using an embedded Json Object
    JsonObjectPtr jsonObj;

    // set value from a JsonObject
    void setJsonObject(JsonObjectPtr aJsonObject);

  public:

    JsonApiValue();

    virtual ApiValuePtr newValue(ApiValueType aObjectType);

    static ApiValuePtr newValueFromJson(JsonObjectPtr aJsonObject);

    virtual void clear();
    virtual void operator=(ApiValue &aApiValue);

    virtual void add(const string &aKey, ApiValuePtr aObj) { JsonApiValuePtr o = boost::dynamic_pointer_cast<JsonApiValue>(aObj); if (jsonObj && o) jsonObj->add(aKey.c_str(), o->jsonObject()); };
    virtual ApiValuePtr get(const string &aKey)  { JsonObjectPtr o; if (jsonObj && jsonObj->get(aKey.c_str(), o)) return newValueFromJson(o); else return ApiValuePtr(); };
    virtual void del(const string &aKey) { if (jsonObj) jsonObj->del(aKey.c_str()); };
    virtual int arrayLength() { return jsonObj ? jsonObj->arrayLength() : 0; };
    virtual void arrayAppend(ApiValuePtr aObj) { JsonApiValuePtr o = boost::dynamic_pointer_cast<JsonApiValue>(aObj); if (jsonObj && o) jsonObj->arrayAppend(o->jsonObject()); };
    virtual ApiValuePtr arrayGet(int aAtIndex) { if (jsonObj) { JsonObjectPtr o = jsonObj->arrayGet(aAtIndex); return newValueFromJson(o); } else return ApiValuePtr(); };
    virtual void arrayPut(int aAtIndex, ApiValuePtr aObj) { JsonApiValuePtr o = boost::dynamic_pointer_cast<JsonApiValue>(aObj); if (jsonObj && o) jsonObj->arrayPut(aAtIndex, o->jsonObject()); };
    virtual bool resetKeyIteration() { if (jsonObj) return jsonObj->resetKeyIteration(); else return false; };
    virtual bool nextKeyValue(string &aKey, ApiValuePtr &aValue) { if (jsonObj) { JsonObjectPtr o; bool gotone = jsonObj->nextKeyValue(aKey, o); aValue = newValueFromJson(o); return gotone; } else return false; };

    virtual uint64_t uint64Value() { return jsonObj ? (uint64_t)jsonObj->int64Value() : 0; };
    virtual int64_t int64Value() { return jsonObj ? jsonObj->int64Value() : 0; };
    virtual double doubleValue() { return jsonObj ? jsonObj->doubleValue() : 0; };
    virtual bool boolValue() { return jsonObj ? jsonObj->boolValue() : false; };
    virtual string stringValue() { if (getType()==apivalue_string) { return jsonObj ? jsonObj->stringValue() : ""; } else return inherited::stringValue(); };

    virtual void setUint64Value(uint64_t aUint64) { jsonObj = JsonObject::newInt64(aUint64); }
    virtual void setInt64Value(int64_t aInt64) { jsonObj = JsonObject::newInt64(aInt64); };
    virtual void setDoubleValue(double aDouble) { jsonObj = JsonObject::newDouble(aDouble); };
    virtual void setBoolValue(bool aBool) { jsonObj = JsonObject::newBool(aBool); };
    virtual bool setStringValue(const string &aString);
    virtual void setNull() { jsonObj.reset(); }

    JsonObjectPtr jsonObject() { return jsonObj; };

  protected:
    
    
  };


  /// a JSON API server
  class VdcJsonApiServer : public VdcApiServer
  {
    typedef VdcApiServer inherited;

  protected:

    /// create API connection of correct type for this API server
    /// @return API connection
    virtual VdcApiConnectionPtr newConnection();

  };



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
    virtual VdcApiConnectionPtr connection();

    /// send a vDC API result (answer for successful method call)
    /// @param aResult the result as a ApiValue. Can be NULL for procedure calls without return value
    /// @result empty or Error object in case of error sending result response
    virtual ErrorPtr sendResult(ApiValuePtr aResult);

    /// send a vDC API error (answer for unsuccesful method call)
    /// @param aErrorCode the error code
    /// @param aErrorMessage the error message or NULL to generate a standard text
    /// @param aErrorData the optional "data" member for the vDC API error object
    /// @result empty or Error object in case of error sending error response
    virtual ErrorPtr sendError(uint32_t aErrorCode, string aErrorMessage = "", ApiValuePtr aErrorData = ApiValuePtr());

  };



  class VdcJsonApiConnection : public VdcApiConnection
  {
    typedef VdcApiConnection inherited;

    friend class VdcJsonApiRequest;

    JsonRpcCommPtr jsonRpcComm;

  public:

    VdcJsonApiConnection();

    /// The underlying socket connection
    /// @return socket connection
    virtual SocketCommPtr socketConnection() { return jsonRpcComm; };

    /// request closing connection after last message has been sent
    virtual void closeAfterSend();

    /// get a new API value suitable for this connection
    /// @return new API value of suitable internal implementation to be used on this API connection
    virtual ApiValuePtr newApiValue();

    /// send a API request
    /// @param aMethod the vDC API method or notification name to be sent
    /// @param aParams the parameters for the method or notification request. Can be NULL.
    /// @param aResponseHandler if the request is a method call, this handler will be called when the method result arrives
    ///   Note that the aResponseHandler might not be called at all in case of lost messages etc. So do not rely on
    ///   this callback for chaining a execution thread.
    /// @return empty or Error object in case of error
    virtual ErrorPtr sendRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler = VdcApiResponseCB());

  private:

    void jsonRequestHandler(JsonRpcComm *aJsonRpcComm, const char *aMethod, const char *aJsonRpcId, JsonObjectPtr aParams);
    void jsonResponseHandler(VdcApiResponseCB aResponseHandler, int32_t aResponseId, ErrorPtr &aError, JsonObjectPtr aResultOrErrorData);

  };


}




#endif /* defined(__vdcd__jsonvdcapi__) */
