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
#include "propertycontainer.hpp"

using namespace std;

namespace p44 {

  #define VDC_API_DOMAIN 0x0000
  #define VDC_CFG_DOMAIN 0x1000

  #define VDC_API_BHVR_DESC (VDC_API_DOMAIN+1)
  #define VDC_API_BHVR_SETTINGS (VDC_API_DOMAIN+2)
  #define VDC_API_BHVR_STATES (VDC_API_DOMAIN+3)


  class DeviceContainer;

  /// base class representing a entity which is addressable with a dSID
  /// dS devices are most obvious addressables, but the vDC itself is also addressable and uses this base class
  class DsAddressable : public PropertyContainer
  {
    typedef PropertyContainer inherited;

    friend class DeviceContainer;

    /// the user-assignable name
    string name;

  protected:
    DeviceContainer *deviceContainerP;


  public:
    DsAddressable(DeviceContainer *aDeviceContainerP);
    virtual ~DsAddressable();

    /// the digitalstrom ID of this addressable entity
    dSID dsid;

    /// get reference to device container
    DeviceContainer &getDeviceContainer() { return *deviceContainerP; };

    /// get user assigned name of the addressable
    /// @return name string
    string getName() { return name; };

    /// set user assignable name
    /// @param new name of the addressable entity
    /// @note might prevent truncating names (just shortening an otherwise unchanged name)
    /// @note subclasses might propagate the name into actual device hardware (e.g. hue)
    virtual void setName(const string &aName);

    /// initialize user assignable name with a default name or a name obtained from hardware
    /// @note use setName to change a name from the API or UI, as initializeName() does not
    ///   propagate to hardware
    void initializeName(const string &aName);

    /// @name vDC API
    /// @{

    /// convenience method to check for existence of a parameter and return appropriate error if not
    static ErrorPtr checkParam(JsonObjectPtr aParams, const char *aParamName, JsonObjectPtr &aParam);

    /// convenience method to check for existence of a string value and if it does, return its value in one call
    static ErrorPtr checkStringParam(JsonObjectPtr aParams, const char *aParamName, string &aParamValue);


    /// called by DeviceContainer to handle methods directed to a dSID
    /// @param aMethod the method
    /// @param aJsonRpcId the id parameter to be used in sendResult()
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSID parameter which has been
    ///   used already to route the method call to this DsAddressable.
    virtual ErrorPtr handleMethod(const string &aMethod, const string &aJsonRpcId, JsonObjectPtr aParams);

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
    bool sendResult(const string &aJsonRpcId, JsonObjectPtr aResult);

    /// send error from a method call back to the vdSM
    /// @param aJsonRpcId this must be the aJsonRpcId as received in the JsonRpcRequestCB handler.
    /// @param aErrorToSend From this error object, getErrorCode() and description() will be used as "code" and "message" members
    ///   of the JSON-RPC 2.0 error object.
    /// @return true if error could be sent, false otherwise (e.g. no vdSM connection)
    bool sendError(const string &aJsonRpcId, ErrorPtr aErrorToSend);

    /// push property value
    /// @param aName name of the property to return. "*" can be passed to return an object listing all properties in this container,
    ///   "^" to return the default property value (internally used for ptype_proxy).
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @param aIndex in case of array, the array element to push. Pass negative value for non-array properties
    /// @return true if push could be sent, false otherwise (e.g. no vdSM connection)
    bool pushProperty(const string &aName, int aDomain, int aIndex = -1);

    /// @}


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    typedef boost::function<void (bool aPresent)> PresenceCB;

    /// check presence of this addressable
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName() { return "DsAddressable"; }

    /// @return the entity type (one of dSD|vdSD|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "*"; }

    /// @return hardware version string or NULL if none
    virtual string hardwareVersion() { return ""; }

    /// @return number of vdSDs (virtual devices represented by a separate dSID)
    ///   that are contained in the same hardware device. -1 means "not available or ambiguous"
    virtual ssize_t numDevicesInHW() { return -1; }

    /// @return index of this vdSDs (0..numDevicesInHW()-1) among all vdSDs in the same hardware device
    ///   -1 means undefined
    virtual ssize_t deviceIndexInHW() { return -1; }

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    /// @note when grouping vdSDs which belong to the same hardware device using numDevicesInHW() and deviceIndexInHW()
    ///   hardwareGUID() must return the same unique ID for the containing hardware device for all contained dSDs
    virtual string hardwareGUID() { return ""; }

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID() { return ""; }

    /// @}



    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);


  private:

    void presenceResultHandler(bool aIsPresent);

  };
  typedef boost::intrusive_ptr<DsAddressable> DsAddressablePtr;

  
} // namespace p44


#endif /* defined(__vdcd__dsaddressable__) */
