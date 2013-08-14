//
//  dsaddressable.h
//  vdcd
//
//  Created by Lukas Zeller on 14.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__dsaddressable__
#define __vdcd__dsaddressable__

#include "dsid.hpp"
#include "jsonrpccomm.hpp"

using namespace std;

namespace p44 {

  class DeviceContainer;

  /// base class representing a entity which is addressable with a dSID
  /// dS devices are most obvious addressables, but the vDC itself is also addressable and uses this base class
  class DsAddressable
  {
    friend class DeviceContainer;

  protected:
    DeviceContainer *deviceContainerP;
  public:
    DsAddressable(DeviceContainer *aDeviceContainerP);
    virtual ~DsAddressable();

    /// the digitalstrom ID of this addressable entity
    dSID dsid;

    /// get reference to device container
    DeviceContainer &getDeviceContainer() { return *deviceContainerP; };

    /// @name vDC API
    /// @{

    /// convenience method to check for existence of a string value and if it does, return its value in one call
    static ErrorPtr checkStringParam(JsonObjectPtr aParams, const char *aParamName, string &aParamValue);

    /// called by DeviceContainer to handle methods directed to a dSID
    /// @param aMethod the method
    /// @param aJsonRpcId the id parameter to be used in sendResult()
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSID parameter which has been
    ///   used already to route the method call to this DsAddressable.
    virtual ErrorPtr handleMethod(const string &aMethod, const char *aJsonRpcId, JsonObjectPtr aParams);

    /// called by DeviceContainer to handle notifications directed to a dSID
    /// @param aMethod the notification
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSID parameter which has been
    ///   used already to route the notification to this DsAddressable.
    virtual void handleNotification(const string &aMethod, JsonObjectPtr aParams);

    /// send a DsAddressable method or notification to vdSM
    /// @param aMethod the method or notification
    /// @param aParams the parameters object, or NULL if none
    /// @param aResponseHandler handler for response. If not set, request is sent as notification
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    /// @note the dSID will be automatically added to aParams (generating a params object if none was passed)
    bool sendRequest(const char *aMethod, JsonObjectPtr aParams, JsonRpcResponseCB aResponseHandler = JsonRpcResponseCB());

    /// send result from a method call back to the to vdSM
    /// @param aJsonRpcId the id parameter from the method call
    /// @param aParams the parameters object, or NULL if none
    /// @param aResponseHandler handler for response. If not set, request is sent as notification
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    bool sendResult(const char *aJsonRpcId, JsonObjectPtr aResult);

    /// @}


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// "pings" the DsAddressable. DsAddressable should respond by sending back a "pong" shortly after (using pong())
    /// base class just sends the pong, but derived classes which can actually ping their hardware should
    /// do so and send the pong only if the hardware actually responds.
    virtual void ping();

    /// sends a "pong" back to the vdSM. Devices should call this as a response to ping()
    void pong();

    /// @}

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  };
  typedef boost::shared_ptr<DsAddressable> DsAddressablePtr;

  
} // namespace p44


#endif /* defined(__vdcd__dsaddressable__) */
