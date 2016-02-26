//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "staticdevicecontainer.hpp"

#include "digitaliodevice.hpp"
#include "analogiodevice.hpp"
#include "consoledevice.hpp"
#include "evaluatordevice.hpp"
#include "sparkiodevice.hpp"
#include "elsnerp03weatherstation.hpp"

using namespace p44;



#pragma mark - StaticDevice


StaticDevice::StaticDevice(DeviceClassContainer *aClassContainerP) :
  Device(aClassContainerP), staticDeviceRowID(0)
{
}


bool StaticDevice::isSoftwareDisconnectable()
{
  return staticDeviceRowID>0; // disconnectable by software if it was created from DB entry (and not on the command line)
}

StaticDeviceContainer &StaticDevice::getStaticDeviceContainer()
{
  return *(static_cast<StaticDeviceContainer *>(classContainerP));
}


void StaticDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // clear learn-in data from DB
  if (staticDeviceRowID) {
    getStaticDeviceContainer().db.executef("DELETE FROM devConfigs WHERE rowid=%d", staticDeviceRowID);
  }
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}


#pragma mark - DB and initialisation


// Version history
//  1 : First version
#define STATICDEVICES_SCHEMA_MIN_VERSION 1 // minimally supported version, anything older will be deleted
#define STATICDEVICES_SCHEMA_VERSION 1 // current version

string StaticDevicePersistence::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string sql;
  if (aFromVersion==0) {
    // create DB from scratch
    // - use standard globs table for schema version
    sql = inherited::dbSchemaUpgradeSQL(aFromVersion, aToVersion);
    // - create my tables
    sql.append(
      "CREATE TABLE devConfigs ("
      " devicetype TEXT,"
      " deviceconfig TEXT"
      ");"
    );
    // reached final version in one step
    aToVersion = STATICDEVICES_SCHEMA_VERSION;
  }
  return sql;
}



StaticDeviceContainer::StaticDeviceContainer(int aInstanceNumber, DeviceConfigMap aDeviceConfigs, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag),
	deviceConfigs(aDeviceConfigs)
{
}


void StaticDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  string databaseName = getPersistentDataDir();
  string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), STATICDEVICES_SCHEMA_VERSION, STATICDEVICES_SCHEMA_MIN_VERSION, aFactoryReset);
  aCompletedCB(error); // return status of DB init
}



// device class name
const char *StaticDeviceContainer::deviceClassIdentifier() const
{
  return "Static_Device_Container";
}


StaticDevicePtr StaticDeviceContainer::addStaticDevice(string aDeviceType, string aDeviceConfig)
{
  DevicePtr newDev;
  if (aDeviceType=="digitalio") {
    // Digital IO based device
    newDev = DevicePtr(new DigitalIODevice(this, aDeviceConfig));
  }
  else if (aDeviceType=="analogio") {
    // Analog IO based device
    newDev = DevicePtr(new AnalogIODevice(this, aDeviceConfig));
  }
  else if (aDeviceType=="console") {
    // console based simulated device
    newDev = DevicePtr(new ConsoleDevice(this, aDeviceConfig));
  }
  else if (aDeviceType=="evaluator") {
    // virtual input or button evaluating other device's sensor values
    newDev = DevicePtr(new EvaluatorDevice(this, aDeviceConfig));
  }
  else if (aDeviceType=="spark") {
    // spark core based device
    newDev = DevicePtr(new SparkIoDevice(this, aDeviceConfig));
  }
  else if (aDeviceType=="ElsnerP03") {
    // spark core based device
    newDev = DevicePtr(new ElsnerP03WeatherStation(this, aDeviceConfig));
  }
  // add to container if device was created
  if (newDev) {
    // add to container
    addDevice(newDev);
    return boost::dynamic_pointer_cast<StaticDevice>(newDev);
  }
  // none added
  return StaticDevicePtr();
}


/// collect devices from this device class
/// @param aCompletedCB will be called when device scan for this device class has been completed
void StaticDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // incrementally collecting static devices makes no sense. The devices are "static"!
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(aClearSettings);
    // create devices from command line config
    for (DeviceConfigMap::iterator pos = deviceConfigs.begin(); pos!=deviceConfigs.end(); ++pos) {
      // create device of appropriate class
      StaticDevicePtr dev = addStaticDevice(pos->first, pos->second);
      if (dev) {
        dev->initializeName(pos->second); // for command line devices, use config as name
      }
    }
    // then add those from the DB
    sqlite3pp::query qry(db);
    if (qry.prepare("SELECT devicetype, deviceconfig, rowid FROM devConfigs")==SQLITE_OK) {
      for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        StaticDevicePtr dev = addStaticDevice(i->get<string>(0), i->get<string>(1));
        if (dev) {
          dev->staticDeviceRowID = i->get<int>(2);
        }
      }
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


ErrorPtr StaticDeviceContainer::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-addDevice") {
    // add a new static device
    string deviceType;
    string deviceConfig;
    respErr = checkStringParam(aParams, "deviceType", deviceType);
    if (Error::isOK(respErr)) {
      respErr = checkStringParam(aParams, "deviceConfig", deviceConfig);
      if (Error::isOK(respErr)) {
        // optional name
        string name; // default to config
        checkStringParam(aParams, "name", name);
        // try to create device
        StaticDevicePtr dev = addStaticDevice(deviceType, deviceConfig);
        if (!dev) {
          respErr = ErrorPtr(new WebError(500, "invalid configuration for static device -> none created"));
        }
        else {
          // set name
          if (name.size()>0) dev->setName(name);
          // insert into database
          db.executef(
            "INSERT OR REPLACE INTO devConfigs (devicetype, deviceconfig) VALUES ('%s','%s')",
            deviceType.c_str(), deviceConfig.c_str()
          );
          dev->staticDeviceRowID = db.last_insert_rowid();
          // confirm
          ApiValuePtr r = aRequest->newApiValue();
          r->setType(apivalue_object);
          r->add("dSUID", r->newBinary(dev->dSUID.getBinary()));
          r->add("rowid", r->newUint64(dev->staticDeviceRowID));
          r->add("name", r->newString(dev->getName()));
          respErr = aRequest->sendResult(r);
        }
      }
    }
  }
  else {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}



