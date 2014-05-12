//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__p44_vdcd_host__
#define __vdcd__p44_vdcd_host__

#include "devicecontainer.hpp"

#include "jsoncomm.hpp"

using namespace std;

namespace p44 {

  class P44VdcError : public Error
  {
  public:
    static const char *domain() { return "p44vdc"; }
    virtual const char *getErrorDomain() const { return P44VdcError::domain(); };
    P44VdcError(ErrorCode aError) : Error(aError) {};
    P44VdcError(ErrorCode aError, const std::string &aErrorMessage) : Error(aError, aErrorMessage) {};
  };



  /// plan44 specific config API JSON request
  class P44JsonApiRequest : public VdcApiRequest
  {
    typedef VdcApiRequest inherited;
    JsonCommPtr jsonComm;

  public:

    /// constructor
    P44JsonApiRequest(JsonCommPtr aJsonComm);

    /// return the request ID as a string
    /// @return request ID as string
    virtual string requestId() { return ""; }

    /// get the API connection this request originates from
    /// @return API connection
    virtual VdcApiConnectionPtr connection() { return VdcApiConnectionPtr(); } // is not really a regular VDC API call, so there's no connection

    /// get a new API value suitable for answering this request connection
    /// @return new API value of suitable internal implementation to be used on this API connection
    virtual ApiValuePtr newApiValue();

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
  typedef boost::intrusive_ptr<P44JsonApiRequest> P44JsonApiRequestPtr;



  /// plan44 specific implementation of a vdc host, with a separate API used by WebUI components.
  class P44VdcHost : public DeviceContainer
  {
    typedef DeviceContainer inherited;
    friend class P44JsonApiRequest;

    long learnIdentifyTicket;

  public:

    P44VdcHost();

    /// JSON API for web interface
    SocketCommPtr configApiServer;

    void startConfigApi();

		/// perform self testing
    /// @param aCompletedCB will be called when the entire self test is done
    /// @param aButton button for interacting with tests
    /// @param aRedLED red LED output
    /// @param aRedLED green LED output
    void selfTest(CompletedCB aCompletedCB, ButtonInputPtr aButton, IndicatorOutputPtr aRedLED, IndicatorOutputPtr aGreenLED);

  private:

    SocketCommPtr configApiConnectionHandler(SocketCommPtr aServerSocketComm);
    void configApiRequestHandler(JsonCommPtr aJsonComm, ErrorPtr aError, JsonObjectPtr aJsonObject);
    void learnHandler(JsonCommPtr aJsonComm, bool aLearnIn, ErrorPtr aError);
    void identifyHandler(JsonCommPtr aJsonComm, DevicePtr aDevice);
    void endIdentify();

    ErrorPtr processVdcRequest(JsonCommPtr aJsonComm, JsonObjectPtr aRequest);
    ErrorPtr processP44Request(JsonCommPtr aJsonComm, JsonObjectPtr aRequest);

    static void sendCfgApiResponse(JsonCommPtr aJsonComm, JsonObjectPtr aResult, ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<P44VdcHost> P44VdcHostPtr;



}

#endif /* defined(__vdcd__p44_vdcd_host__) */
