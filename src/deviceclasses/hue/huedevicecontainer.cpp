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

#include "huedevicecontainer.hpp"

#include "huedevice.hpp"

using namespace p44;


HueDeviceContainer::HueDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  inherited(aInstanceNumber, aDeviceContainerP, aTag),
  hueComm()
{
}



const char *HueDeviceContainer::deviceClassIdentifier() const
{
  return "hue_Lights_Container";
}


bool HueDeviceContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vdc_hue", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string HueDeviceContainer::getExtraInfo()
{
  return string_format("hue api: %s", hueComm.baseURL.c_str());
}



#pragma mark - DB and initialisation

// Version history
//  1 : first version
#define HUE_SCHEMA_MIN_VERSION 1 // minimally supported version, anything older will be deleted
#define HUE_SCHEMA_VERSION 1 // current version

string HuePersistence::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string sql;
  if (aFromVersion==0) {
    // create DB from scratch
		// - use standard globs table for schema version
    sql = inherited::dbSchemaUpgradeSQL(aFromVersion, aToVersion);
		// - add fields to globs table
    sql.append(
      "ALTER TABLE globs ADD hueBridgeUUID TEXT;"
      "ALTER TABLE globs ADD hueBridgeUser TEXT;"
    );
    // reached final version in one step
    aToVersion = HUE_SCHEMA_VERSION;
  }
  return sql;
}


void HueDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), HUE_SCHEMA_VERSION, HUE_SCHEMA_MIN_VERSION, aFactoryReset);
	aCompletedCB(error); // return status of DB init
}



#pragma mark - collect devices


int HueDeviceContainer::getRescanModes() const
{
  // normal and incremental make sense, no exhaustive mode
  return rescanmode_incremental+rescanmode_normal;
}


void HueDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  collectedHandler = aCompletedCB;
  if (!aIncremental) {
    // full collect, remove all devices
    removeDevices(aClearSettings);
  }
  // load hue bridge uuid and token
  sqlite3pp::query qry(db);
  if (qry.prepare("SELECT hueBridgeUUID, hueBridgeUser FROM globs")==SQLITE_OK) {
    sqlite3pp::query::iterator i = qry.begin();
    if (i!=qry.end()) {
      bridgeUuid = nonNullCStr(i->get<const char *>(0));
      bridgeUserName = nonNullCStr(i->get<const char *>(1));
    }
  }
  if (bridgeUuid.length()>0) {
    // we know a bridge by UUID, try to refind it
    hueComm.uuid = bridgeUuid;
    hueComm.userName = bridgeUserName;
    hueComm.refindBridge(boost::bind(&HueDeviceContainer::refindResultHandler, this, _1));
  }
  else {
    // no bridge known, can't collect anything at this time
    collectedHandler(ErrorPtr());
  }
}




void HueDeviceContainer::refindResultHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // found already registered bridge again
    LOG(LOG_NOTICE,
      "Hue bridge %s found again:\n"
      "- userName = %s\n"
      "- API base URL = %s",
      hueComm.uuid.c_str(),
      hueComm.userName.c_str(),
      hueComm.baseURL.c_str()
    );
    // collect existing lights
    // Note: for now we don't search for new lights, this is left to the Hue App, so users have control
    //   if they want new lights added or not
    collectLights();
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "Error refinding hue bridge with uuid %s, error = %s", hueComm.uuid.c_str(), aError->description().c_str());
    collectedHandler(ErrorPtr()); // no hue bridge to collect lights from (but this is not a collect error)
  }
}


void HueDeviceContainer::setLearnMode(bool aEnableLearning, bool aDisableProximityCheck)
{
  if (aEnableLearning) {
    hueComm.findNewBridge(
      getDeviceContainer().getDsUid().getString().c_str(), // dSUID is suitable as hue login name
      getDeviceContainer().modelName().c_str(),
      15*Second, // try to login for 15 secs
      boost::bind(&HueDeviceContainer::searchResultHandler, this, _1)
    );
  }
  else {
    // stop learning
    hueComm.stopFind();
  }
}


void HueDeviceContainer::searchResultHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // found and authenticated bridge
    LOG(LOG_NOTICE,
      "Hue bridge found and logged in:\n"
      "- uuid = %s\n"
      "- userName = %s\n"
      "- API base URL = %s",
      hueComm.uuid.c_str(),
      hueComm.userName.c_str(),
      hueComm.baseURL.c_str()
    );
    // learning in or out requires all devices to be removed first
    // (on learn-in, the bridge's devices will be added afterwards)
    removeDevices(false);
    // check if we found the already learned-in bridge
    bool learnIn = false;
    if (hueComm.uuid==bridgeUuid) {
      // this is the bridge that was learned in previously. Learn it out
      // - delete it from the whitelist
      string url = "/config/whitelist/" + hueComm.userName;
      hueComm.apiAction(httpMethodDELETE, url.c_str(), JsonObjectPtr(), NULL);
      // - forget uuid + user name
      bridgeUuid.clear();
      bridgeUserName.clear();
    }
    else {
      // new bridge found
      learnIn = true;
      bridgeUuid = hueComm.uuid;
      bridgeUserName = hueComm.userName;
    }
    // save the bridge parameters
    db.executef(
      "UPDATE globs SET hueBridgeUUID='%s', hueBridgeUser='%s'",
      bridgeUuid.c_str(),
      bridgeUserName.c_str()
    );
    // now process the learn in/out
    if (learnIn) {
      // TODO: now get lights
      collectedHandler = NULL; // we are not collecting, this is adding new lights while in operation already
      collectLights();
    }
    // report successful learn event
    getDeviceContainer().reportLearnEvent(learnIn, ErrorPtr());
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "No hue bridge found to register, error = %s", aError->description().c_str());
  }
}


void HueDeviceContainer::collectLights()
{
  // Note: can be used to incrementally search additional lights
  // issue lights query
  LOG(LOG_INFO, "Querying hue bridge for available lights...");
  hueComm.apiQuery("/lights", boost::bind(&HueDeviceContainer::collectedLightsHandler, this, _1, _2));
}


void HueDeviceContainer::collectedLightsHandler(JsonObjectPtr aResult, ErrorPtr aError)
{
  LOG(LOG_INFO, "hue bridge reports lights = \n%s", aResult ? aResult->c_strValue() : "<none>");
  if (aResult) {
    // pre-v1.3 bridges: { "1": { "name": "Bedroom" }, "2": .... }
    // v1.3 and later bridges: { "1": { "name": "Bedroom", "state": {...}, "modelid":"LCT001", ... }, "2": .... }
    // v1.4 and later bridges: { "1": { "state": {...}, "type": "Dimmable light", "name": "lux demoboard", "modelid": "LWB004","uniqueid":"00:17:88:01:00:e5:a0:87-0b", "swversion": "66012040" }
    aResult->resetKeyIteration();
    string lightID;
    JsonObjectPtr lightInfo;
    while (aResult->nextKeyValue(lightID, lightInfo)) {
      // create hue device
      if (lightInfo) {
        // pre 1.3 bridges, which do not know yet hue Lux, don't have the "state" -> no state == all lights have color (hue or living color)
        // 1.3 and later bridges do have "state", and if "state" contains "colormode", it's a color light
        bool hasColor = true; // assume color (default if no "state" delivered in answer)
        JsonObjectPtr o = lightInfo->get("state");
        if (o) {
          JsonObjectPtr cm = o->get("colormode");
          if (!cm) hasColor = false; // lamp without color mode -> just brightness (hue lux)
        }
        // 1.4 and later FINALLY have a "uniqueid"!
        string uniqueID;
        o = lightInfo->get("uniqueid");
        if (o) uniqueID = o->stringValue();
        // create device now
        HueDevicePtr newDev = HueDevicePtr(new HueDevice(this, lightID, hasColor, uniqueID));
        if (addDevice(newDev)) {
          // actually added, no duplicate, set the name
          // (otherwise, this is an incremental collect and we knew this light already)
          JsonObjectPtr n = lightInfo->get("name");
          if (n) newDev->initializeName(n->stringValue());
        }
      }
    }
  }
  // collect phase done
  if (collectedHandler)
    collectedHandler(ErrorPtr());
  collectedHandler = NULL; // done
}









