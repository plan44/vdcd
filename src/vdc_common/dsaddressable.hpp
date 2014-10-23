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

#ifndef __vdcd__dsaddressable__
#define __vdcd__dsaddressable__

#include "dsuid.hpp"
#include "propertycontainer.hpp"
#include "dsdefs.h"

#include "vdcapi.hpp"

using namespace std;

namespace p44 {

  #define VDC_API_DOMAIN 0x0000
  #define VDC_CFG_DOMAIN 0x1000

  class DeviceContainer;

  /// base class representing a entity which is addressable with a dSUID
  /// dS devices are most obvious addressables, but vDCs and the vDC host itself is also addressable and uses this base class
  class DsAddressable : public PropertyContainer
  {
    typedef PropertyContainer inherited;

    friend class DeviceContainer;

    /// the user-assignable name
    string name;

    /// announcement status
    MLMicroSeconds announced; ///< set when last announced to the vdSM
    MLMicroSeconds announcing; ///< set when announcement has been started (but not yet confirmed)

  protected:
    DeviceContainer *deviceContainerP;

    /// the actual (modern) dSUID
    DsUid dSUID;

  public:
    DsAddressable(DeviceContainer *aDeviceContainerP);
    virtual ~DsAddressable();

    /// the dSUID exposed in the VDC API (might be pseudoclassic during beta)
    virtual const DsUid &getApiDsUid();

    /// the real (always modern, 34 hex) dSUID
    const DsUid &getDsUid() { return dSUID; };

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
    static ErrorPtr checkParam(ApiValuePtr aParams, const char *aParamName, ApiValuePtr &aParam);

    /// convenience method to check if a string value exists and if yes, return its value in one call
    static ErrorPtr checkStringParam(ApiValuePtr aParams, const char *aParamName, string &aParamValue);

    /// convenience method to check if a dSUID value exists and if it does, return its value in one call
    static ErrorPtr checkDsuidParam(ApiValuePtr aParams, const char *aParamName, DsUid &aDsUid);

    /// called by DeviceContainer to handle methods directed to a dSUID
    /// @param aRequest this is the request to respond to
    /// @param aMethod the method
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSUID parameter which has been
    ///   used already to route the method call to this DsAddressable.
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams);

    /// called by DeviceContainer to handle notifications directed to a dSUID
    /// @param aMethod the notification
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSUID parameter which has been
    ///   used already to route the notification to this DsAddressable.
    virtual void handleNotification(const string &aMethod, ApiValuePtr aParams);

    /// send a DsAddressable method or notification to vdSM
    /// @param aMethod the method or notification
    /// @param aParams the parameters object, or NULL if none
    /// @param aResponseHandler handler for response. If not set, request is sent as notification
    /// @return true if message could be sent, false otherwise (e.g. no vdSM connection)
    /// @note the dSUID will be automatically added to aParams (generating a params object if none was passed)
    bool sendRequest(const char *aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler = VdcApiResponseCB());

    /// push property value
    /// @param aQuery description of what should be pushed (same syntax as in getProperty API)
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @return true if push could be sent, false otherwise (e.g. no vdSM connection)
    bool pushProperty(ApiValuePtr aQuery, int aDomain);

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

    /// @return human readable, language independent model name/short description
    virtual string modelName() = 0;

    /// @return human readable version string
    /// @note base class implementation returns version string of vdc host by default
    virtual string modelVersion();


    /// @return unique ID for the functional model of this entity
    /// @note modelUID must be equal between all devices of the same model/class/kind, where "same" means
    ///   the functionality relevant for the dS system. If different connected hardware devices
    ///   (different hardwareModelGuid) provide EXACTLY the same dS functionality, these devices MAY
    ///   have the same modelUID. Vice versa, two identical hardware devices (two digital input for example)
    ///   might have the same hardwareModelGuid, but different modelUID if for example one input is mapped
    ///   as a button, and the other as a binaryInput.
    virtual string modelUID() = 0;

    /// @return the entity type (one of dSD|vdSD|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() { return "*"; };

    /// @return hardware version string or NULL if none
    virtual string hardwareVersion() { return ""; };

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    /// @note Already defined schemas for hardwareGUID are
    /// - enoceanaddress:XXXXXXXX = 8 hex digits enOcean device address
    /// - gs1:(01)ggggg = GS1 formatted GTIN
    /// - uuid:UUUUUUU = UUID
    /// - macaddress:MMMMM = MAC Address
    /// - sparkcoreid:ssssss = spark core ID
    virtual string hardwareGUID() { return ""; };

    /// @return model GUID in URN format to identify model of the connected hardware device as uniquely as possible
    /// @note model GUID must be equal between all devices of the same model/class/kind, where "same" should be
    ///   focused to the context of functionality relevant for the dS system, if possible. On the other hand,
    ///   identifiers allowing global lookup (such as GTIN) are preferred if available over less generic
    ///   model identification.
    /// Already defined schemas for modelGUID are
    /// - enoceaneep:RRFFTT = 6 hex digits enOcean EEP
    /// - gs1:(01)ggggg = GS1 formatted GTIN
    /// - uuid:UUUUUUU = UUID
    virtual string hardwareModelGUID() { return ""; };

    /// @return OEM GUID in URN format to identify hardware as uniquely as possible
    virtual string oemGUID() { return ""; };

    /// @return Vendor ID in URN format to identify vendor as uniquely as possible
    /// @note vendor ID can be simply the name of the vendor in clear text, or possibly a platform-specific, numeric identifier
    ///   that can be used to look up the vendor in the specific platform context (such as EnOcean)
    /// Already defined schemas for vendorId are
    /// - enoceanvendor:VVV[:nnn] = 3 hex digits enOcean vendor ID, optionally followed by vendor name (if known)
    /// - vendorname:nnnnn = vendor name in plain text
    virtual string vendorId() { return ""; };

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// Get extra info (plan44 specific) to describe the addressable in more detail
    /// @return string, single line extra info describing aspects of the device not visible elsewhere
    virtual string getExtraInfo() { return ""; };

    /// @}



    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description() = 0;

  protected:

    /// @name icon loading mechanism
    /// @{

    /// get icon data or name
    /// @param aIconName basic name of the icon to load (no filename extension!)
    /// @param aIcon string to get icon name or data assigned
    /// @param aWithData if set, aIcon will be set to the icon's PNG data, otherwise aIcon will be set to the icon name
    /// @param aResolutionPrefix subfolder within icondir to look for files. Usually "icon16".
    /// @return true if icon data or name could be obtained, false otherwise
    /// @note when icondir (in devicecontainer) is set, the method will check in this dir if a file with aIconName exists
    ///   in the subdirectory specified by aResolutionPrefix - even if only querying for icon name. This allows for
    ///   implementations of getDeviceIcon to start with the most specific icon name, and falling back to more
    ///   generic icons defined by superclass when specific icons don't exist.
    ///   When icondir is NOT set, asking for icon data will always fail, and asking for name will always succeed
    ///   and return aIconName in aIcon.
    bool getIcon(const char *aIconName, string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// get icon colored according to aGroup
    /// @param aIconName basic name of the icon to load (no color suffix nor filename extension!)
    /// @param aGroup the dS group/color. The color will be appended as suffix to aIconName to build the
    ///   final icon name. If no specific icon exists for the group, the suffix "_other" will be tried before
    ///   returning false.
    /// @param aWithData if set, aIcon will be set to the icon's PNG data, otherwise aIcon will be set to the icon name
    /// @param aResolutionPrefix subfolder within icondir to look for files. Usually "icon16".
    /// @return true if icon data or name could be obtained, false otherwise
    bool getGroupColoredIcon(const char *aIconName, DsGroup aGroup, string &aIcon, bool aWithData, const char *aResolutionPrefix);

    /// @}

    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

  private:

    void presenceResultHandler(bool aIsPresent);

  };
  typedef boost::intrusive_ptr<DsAddressable> DsAddressablePtr;


} // namespace p44


#endif /* defined(__vdcd__dsaddressable__) */
