//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0


#include "dsaddressable.hpp"

#include "devicecontainer.hpp"

using namespace p44;



DsAddressable::DsAddressable(DeviceContainer *aDeviceContainerP) :
  deviceContainerP(aDeviceContainerP),
  announced(Never),
  announcing(Never)
{
}


DsAddressable::~DsAddressable()
{
}


string DsAddressable::modelVersion()
{
  return getDeviceContainer().modelVersion();
}


void DsAddressable::setName(const string &aName)
{
  // TODO: for now dsm API truncates names to 20 bytes. Therefore,
  //   we prevent replacing a long name with a truncated version
  if (name!=aName && (name.length()<20 || name.substr(0,20)!=aName)) {
    name = aName;
  }
}


void DsAddressable::initializeName(const string &aName)
{
  // just assign
  name = aName;
}


void DsAddressable::reportVanished()
{
  if (announced!=Never) {
    // report to vDC API client that the device is now offline
    sendRequest("vanish", ApiValuePtr());
  }
}


#pragma mark - vDC API



ErrorPtr DsAddressable::checkParam(ApiValuePtr aParams, const char *aParamName, ApiValuePtr &aParam)
{
  ErrorPtr err;
  if (aParams)
    aParam = aParams->get(aParamName);
  else
    aParam.reset();
  if (!aParam)
    err = ErrorPtr(new VdcApiError(400, string_format("Invalid Parameters - missing '%s'",aParamName)));
  return err;
}


ErrorPtr DsAddressable::checkStringParam(ApiValuePtr aParams, const char *aParamName, string &aString)
{
  ErrorPtr err;
  ApiValuePtr o;
  err = checkParam(aParams, aParamName, o);
  if (Error::isOK(err)) {
    aString = o->stringValue();
  }
  return err;
}


ErrorPtr DsAddressable::checkBoolParam(ApiValuePtr aParams, const char *aParamName, bool &aBool)
{
  ErrorPtr err;
  ApiValuePtr o;
  err = checkParam(aParams, aParamName, o);
  if (Error::isOK(err)) {
    aBool = o->boolValue();
  }
  return err;
}



ErrorPtr DsAddressable::checkDsuidParam(ApiValuePtr aParams, const char *aParamName, DsUid &aDsUid)
{
  ErrorPtr err;
  ApiValuePtr o;
  err = checkParam(aParams, aParamName, o);
  if (Error::isOK(err)) {
    aDsUid.setAsBinary(o->binaryValue());
  }
  return err;
}




ErrorPtr DsAddressable::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="getProperty") {
    // query must be present
    ApiValuePtr query;
    if (Error::isOK(respErr = checkParam(aParams, "query", query))) {
      // now read
      ApiValuePtr result = aRequest->newApiValue();
      respErr = accessProperty(access_read, query, result, VDC_API_DOMAIN, PropertyDescriptorPtr());
      if (Error::isOK(respErr)) {
        // send back property result
        aRequest->sendResult(result);
      }
    }
  }
  else if (aMethod=="setProperty") {
    // properties must be present
    ApiValuePtr value;
    if (Error::isOK(respErr = checkParam(aParams, "properties", value))) {
      // check preload flag
      bool preload = false;
      ApiValuePtr o = aParams->get("preload");
      if (o) {
        preload = o->boolValue();
      }
      respErr = accessProperty(preload ? access_write_preload : access_write, value, ApiValuePtr(), VDC_API_DOMAIN, PropertyDescriptorPtr());
      if (Error::isOK(respErr)) {
        // send back OK if write was successful
        aRequest->sendResult(ApiValuePtr());
      }
    }
  }
  else {
    respErr = ErrorPtr(new VdcApiError(405, "unknown method"));
  }
  return respErr;
}


bool DsAddressable::pushProperty(ApiValuePtr aQuery, int aDomain)
{
  if (announced!=Never) {
    // device is announced: push value changes
    // - get the value
    ApiValuePtr value = aQuery->newValue(apivalue_object);
    ErrorPtr err = accessProperty(access_read, aQuery, value, aDomain, PropertyDescriptorPtr());
    if (Error::isOK(err)) {
      // - send pushProperty
      ApiValuePtr pushParams = aQuery->newValue(apivalue_object);
      pushParams->add("properties", value);
      return sendRequest("pushProperty", pushParams);
    }
  }
  else {
    // not announced, suppress pushProperty
    ALOG(LOG_WARNING, "pushProperty suppressed - is not yet announced");
  }
  return false;
}



void DsAddressable::handleNotification(const string &aMethod, ApiValuePtr aParams)
{
  if (aMethod=="ping") {
    // issue device ping (which will issue a pong when device is reachable)
    ALOG(LOG_INFO, "ping -> checking presence...");
    checkPresence(boost::bind(&DsAddressable::presenceResultHandler, this, _1));
  }
  else {
    // unknown notification
    ALOG(LOG_WARNING, "unknown notification '%s'", aMethod.c_str());
  }
}


bool DsAddressable::sendRequest(const char *aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  VdcApiConnectionPtr api = getDeviceContainer().getSessionConnection();
  if (api) {
    if (!aParams) {
      // create params object because we need it for the dSUID
      aParams = api->newApiValue();
      aParams->setType(apivalue_object);
    }
    aParams->add("dSUID", aParams->newBinary(getDsUid().getBinary()));
    return getDeviceContainer().sendApiRequest(aMethod, aParams, aResponseHandler);
  }
  return false; // no connection
}



void DsAddressable::presenceResultHandler(bool aIsPresent)
{
  if (aIsPresent) {
    // send back Pong notification
    ALOG(LOG_INFO, "is present -> sending pong");
    sendRequest("pong", ApiValuePtr());
  }
  else {
    ALOG(LOG_NOTICE, "is NOT present -> no Pong sent");
  }
}



#pragma mark - interaction with subclasses


void DsAddressable::checkPresence(PresenceCB aPresenceResultHandler)
{
  // base class just assumes being present
  aPresenceResultHandler(true);
}


#pragma mark - property access

enum {
  type_key,
  dSUID_key,
  model_key,
  modelUID_key,
  modelVersion_key,
  hardwareVersion_key,
  hardwareGUID_key,
  hardwareModelGUID_key,
  oemGUID_key,
  vendorId_key,
  extraInfo_key,
  objectDescription_key,
  deviceIcon16_key,
  iconName_key,
  name_key,
  numDsAddressableProperties
};


static char dsAddressable_key;

int DsAddressable::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  return inherited::numProps(aDomain, aParentDescriptor)+numDsAddressableProperties;
}


PropertyDescriptorPtr DsAddressable::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numDsAddressableProperties] = {
    { "type", apivalue_string, type_key, OKEY(dsAddressable_key) },
    { "dSUID", apivalue_binary, dSUID_key, OKEY(dsAddressable_key) },
    { "model", apivalue_string, model_key, OKEY(dsAddressable_key) },
    { "modelUID", apivalue_string, modelUID_key, OKEY(dsAddressable_key) },
    { "modelVersion", apivalue_string, modelVersion_key, OKEY(dsAddressable_key) },
    { "hardwareVersion", apivalue_string, hardwareVersion_key, OKEY(dsAddressable_key) },
    { "hardwareGuid", apivalue_string, hardwareGUID_key, OKEY(dsAddressable_key) },
    { "hardwareModelGuid", apivalue_string, hardwareModelGUID_key, OKEY(dsAddressable_key) },
    { "oemGuid", apivalue_string, oemGUID_key, OKEY(dsAddressable_key) },
    { "vendorId", apivalue_string, vendorId_key, OKEY(dsAddressable_key) },
    { "x-p44-extraInfo", apivalue_string, extraInfo_key, OKEY(dsAddressable_key) },
    { "x-p44-description", apivalue_string, objectDescription_key, OKEY(dsAddressable_key) },
    { "deviceIcon16", apivalue_binary, deviceIcon16_key, OKEY(dsAddressable_key) },
    { "deviceIconName", apivalue_string, iconName_key, OKEY(dsAddressable_key) },
    { "name", apivalue_string, name_key, OKEY(dsAddressable_key) }
  };
  int n = inherited::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


bool DsAddressable::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(dsAddressable_key)) {
    if (aMode!=access_read) {
      switch (aPropertyDescriptor->fieldKey()) {
        case name_key: setName(aPropValue->stringValue()); return true;
      }
    }
    else {
      switch (aPropertyDescriptor->fieldKey()) {
        case type_key: aPropValue->setStringValue(entityType()); return true; // the entity type
        case dSUID_key: aPropValue->setStringValue(dSUID.getString()); return true; // always the real dSUID
        case model_key: aPropValue->setStringValue(modelName()); return true; // human readable model identification
        case modelUID_key: aPropValue->setStringValue(modelUID()); return true; // unique model identification, same features = same model
        case modelVersion_key: if (modelVersion().size()>0) { aPropValue->setStringValue(modelVersion()); return true; } else return false;
        case hardwareVersion_key: if (hardwareVersion().size()>0) { aPropValue->setStringValue(hardwareVersion()); return true; } else return false;
        case hardwareGUID_key: if (hardwareGUID().size()>0) { aPropValue->setStringValue(hardwareGUID()); return true; } else return false;
        case hardwareModelGUID_key: if (hardwareModelGUID().size()>0) { aPropValue->setStringValue(hardwareModelGUID()); return true; } else return false;
        case oemGUID_key: if (oemGUID().size()>0) { aPropValue->setStringValue(oemGUID()); return true; } else return false;
        case vendorId_key: if (vendorId().size()>0) { aPropValue->setStringValue(vendorId()); return true; } else return false;
        case extraInfo_key: if (getExtraInfo().size()>0) { aPropValue->setStringValue(getExtraInfo()); return true; } else return false;
        case objectDescription_key: aPropValue->setStringValue(description()); return true;
        case deviceIcon16_key: { string icon; if (getDeviceIcon(icon, true, "icon16")) { aPropValue->setBinaryValue(icon); return true; } else return false; }
        case iconName_key: { string iconName; if (getDeviceIcon(iconName, false, "icon16")) { aPropValue->setStringValue(iconName); return true; } else return false; }
        case name_key: aPropValue->setStringValue(getName()); return true;
      }
      return true;
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor); // let base class handle it
}


#pragma mark - icon loading

bool DsAddressable::getIcon(const char *aIconName, string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  DBGLOG(LOG_DEBUG, "Trying to load icon named '%s/%s' for dSUID %s", aResolutionPrefix, aIconName, dSUID.getString().c_str());
  const char *iconDir = getDeviceContainer().getIconDir();
  if (iconDir && *iconDir) {
    string iconPath = string_format("%s%s/%s.png", iconDir, aResolutionPrefix, aIconName);
    // TODO: maybe add cache lookup here
    // try to access this file
    int fildes = open(iconPath.c_str(), O_RDONLY);
    if (fildes<0) {
      return false; // can't load from this location
    }
    // file seems to exist
    if (aWithData) {
      // load it
      ssize_t bytes = 0;
      const size_t bufsize = 4096; // usually a 16x16 png is 3.4kB
      char buffer[bufsize];
      aIcon.clear();
      while (true) {
        bytes = read(fildes, buffer, bufsize);
        if (bytes<=0)
          break; // done
        aIcon.append(buffer, bytes);
      }
      close(fildes);
      // done
      if (bytes<0) {
        // read error, do not return half-read icon
        aIcon.clear();
        return false;
      }
      DBGLOG(LOG_DEBUG, "- successfully loaded icon named '%s'", aIconName);
    }
    else {
      // just name
      close(fildes);
      aIcon = aIconName; // this is a name for which the file exists
    }
    return true;
  }
  else {
    // no icon dir
    if (aWithData) {
      // data requested but no icon dir -> cannot return data
      return false;
    }
    else {
      // name requested but no directory to check for file -> always just return name
      aIcon = aIconName;
      return true;
    }
  }
}


static const char *groupColors[] = {
  "white", // variable/undefined are shown as white
  "yellow", // Light
  "grey", // shadow
  "blue", // heating = 3, ///< heating - formerly "climate"
  "cyan", // audio
  "magenta", // video
  "red", // security
  "green", // green
  "black", // joker
  "white", // cooling
  "blue", // ventilation
  "blue" // windows
};
const int numGroupColors = sizeof(groupColors)/sizeof(const char *);


bool DsAddressable::getGroupColoredIcon(const char *aIconName, DsGroup aGroup, string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  string  iconName;
  bool found = false;
  if (aGroup<numGroupColors) {
    // try first with color name
    found = getIcon(string_format("%s_%s", aIconName, groupColors[aGroup]).c_str(), aIcon, aWithData, aResolutionPrefix);
  }
  else {
    // for groups without a basic color, use _n suffix (n=group no)
    found = getIcon(string_format("%s_%d", aIconName, (int)aGroup).c_str(), aIcon, aWithData, aResolutionPrefix);
  }
  if (!found) {
    // no group-specific icon found, try with "other"
    found = getIcon(string_format("%s_other", aIconName).c_str(), aIcon, aWithData, aResolutionPrefix);
  }
  return found;
}


bool DsAddressable::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("unknown", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return false;
}


string DsAddressable::vendorId()
{
  // by default, use vendorname: URN schema
  if (!vendorName().empty())
    return string_format("vendorname:%s", vendorName().c_str());
  else
    return "";
}




#pragma mark - description/shortDesc/logging


void DsAddressable::logAddressable(int aErrLevel, const char *aFmt, ... )
{
  va_list args;
  va_start(args, aFmt);
  // format the message
  string message = string_format("%s %s: ", entityType(), shortDesc().c_str());
  string_format_v(message, true, aFmt, args);
  va_end(args);
  globalLogger.logStr(aErrLevel, message);
}


string DsAddressable::shortDesc()
{
  // short description is dSUID...
  string s = dSUID.getString();
  // ...and user-set name, if any
  if (!name.empty())
    string_format_append(s, " (%s)", name.c_str());
  return s;
}



string DsAddressable::description()
{
  string s = string_format("%s %s - %sannounced", entityType(), shortDesc().c_str(), announced==Never ? "NOT YET " : "");
  return s;
}
