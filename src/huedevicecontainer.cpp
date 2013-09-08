//
//  huedevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huedevicecontainer.hpp"

#include "huedevice.hpp"

using namespace p44;


HueDeviceContainer::HueDeviceContainer(int aInstanceNumber) :
  inherited(aInstanceNumber),
  hueComm()
{
}

const char *HueDeviceContainer::deviceClassIdentifier() const
{
  return "hue_Lights_Container";
}


#pragma mark - DB and initialisation


#define HUE_SCHEMA_VERSION 1

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


void HueDeviceContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), HUE_SCHEMA_VERSION, aFactoryReset);
	aCompletedCB(error); // return status of DB init
}



#pragma mark - collect devices


void HueDeviceContainer::forgetDevices()
{
  inherited::forgetDevices();
  // TODO: do additional hue specific stuff
  bridgeUuid.clear();
  bridgeUserName.clear();
}



void HueDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  collectedHandler = aCompletedCB;
  // forget all devices
  forgetDevices();
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
    hueComm.refindBridge(boost::bind(&HueDeviceContainer::refindResultHandler, this, _2));
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
      "- API base URL = %s\n",
      hueComm.uuid.c_str(),
      hueComm.userName.c_str(),
      hueComm.baseURL.c_str()
    );


    // TODO: %%% implement collecting lights

    // for now: just say ok
    collectedHandler(ErrorPtr()); // ok
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "Error refinding hue bridge with uuid %s, error = %s\n", hueComm.uuid.c_str(), aError->description().c_str());
    collectedHandler(ErrorPtr()); // no hue bridge to collect lights from (but this is not a collect error)
  }
}


void HueDeviceContainer::setLearnMode(bool aEnableLearning)
{
  if (aEnableLearning) {
    hueComm.findNewBridge(
      getDeviceContainer().dsid.getString().c_str(), // dsid is suitable as hue login name
      getDeviceContainer().modelName().c_str(),
      15*Second, // try to login for 15 secs
      boost::bind(&HueDeviceContainer::searchResultHandler, this, _2)
    );
  }
  else {
    // stop learning
    #warning "for now, extend search beyond learning period"
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
      "- API base URL = %s\n",
      hueComm.uuid.c_str(),
      hueComm.userName.c_str(),
      hueComm.baseURL.c_str()
    );
    // check if we found the already learned-in bridge
    bool learnIn = false;
    if (hueComm.uuid==bridgeUuid) {
      // this is the bridge that was learned in previously. Learn it out
      // - delete it from the whitelist
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
    }
    // report successful learn event
    getDeviceContainer().reportLearnEvent(learnIn, ErrorPtr());
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "No hue bridge found to register, error = %s\n", aError->description().c_str());
  }
}





