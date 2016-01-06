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

#include "oladevicecontainer.hpp"

#if !DISABLE_OLA

#include "oladevice.hpp"

#include <ola/DmxBuffer.h>

using namespace p44;



#pragma mark - DB and initialisation


// Version history
//  1 : First version
#define OLADEVICES_SCHEMA_MIN_VERSION 1 // minimally supported version, anything older will be deleted
#define OLADEVICES_SCHEMA_VERSION 1 // current version

string OlaDevicePersistence::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
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
    aToVersion = OLADEVICES_SCHEMA_VERSION;
  }
  return sql;
}



OlaDeviceContainer::OlaDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
}


#define DMX512_INTERFRAME_PAUSE (50*MilliSecond)
#define DMX512_RETRY_INTERVAL (15*Second)
#define OLA_SETUP_RETRY_INTERVAL (30*Second)
#define DMX512_UNIVERSE 42

void OlaDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  ErrorPtr err;
  // initialize database
  string databaseName = getPersistentDataDir();
  string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  err = db.connectAndInitialize(databaseName.c_str(), OLADEVICES_SCHEMA_VERSION, OLADEVICES_SCHEMA_MIN_VERSION, aFactoryReset);
  // launch OLA thread
  pthread_mutex_init(&olaBufferAccess, NULL);
  olaThread = MainLoop::currentMainLoop().executeInThread(boost::bind(&OlaDeviceContainer::olaThreadRoutine, this, _1), NULL);
  // done
  aCompletedCB(ErrorPtr());
}


void OlaDeviceContainer::olaThreadRoutine(ChildThreadWrapper &aThread)
{
  // turn on OLA logging when loglevel is debugging, otherwise off
  ola::InitLogging(LOGENABLED(LOG_DEBUG) ? ola::OLA_LOG_WARN : ola::OLA_LOG_NONE, ola::OLA_LOG_STDERR);
  dmxBufferP = new ola::DmxBuffer;
  ola::client::StreamingClient::Options options;
  options.auto_start = false; // do not start olad from client
  olaClientP = new ola::client::StreamingClient(options);
  if (olaClientP && dmxBufferP) {
    dmxBufferP->Blackout();
    while (!aThread.shouldTerminate()) {
      if (!olaClientP->Setup()) {
        // cannot start yet, wait a little
        usleep(OLA_SETUP_RETRY_INTERVAL);
      }
      else {
        while (!aThread.shouldTerminate()) {
          pthread_mutex_lock(&olaBufferAccess);
          bool ok = olaClientP->SendDMX(DMX512_UNIVERSE, *dmxBufferP, ola::client::StreamingClient::SendArgs());
          pthread_mutex_unlock(&olaBufferAccess);
          if (ok) {
            // successful send
            usleep(DMX512_INTERFRAME_PAUSE); // sleep a little between frames.
          }
          else {
            // unsuccessful send, do not try too often
            usleep(DMX512_RETRY_INTERVAL); // sleep longer between failed attempts
          }
        }
      }
    }
  }
}


void OlaDeviceContainer::setDMXChannel(DmxChannel aChannel, DmxValue aChannelValue)
{
  if (dmxBufferP && aChannel>=1 && aChannel<=512) {
    pthread_mutex_lock(&olaBufferAccess);
    dmxBufferP->SetChannel(aChannel-1, aChannelValue);
    pthread_mutex_unlock(&olaBufferAccess);
  }
}


bool OlaDeviceContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vdc_ola", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


// device class name
const char *OlaDeviceContainer::deviceClassIdentifier() const
{
  return "OLA_Device_Container";
}


OlaDevicePtr OlaDeviceContainer::addOlaDevice(string aDeviceType, string aDeviceConfig)
{
  DevicePtr newDev;
  // TODO: for now, all devices are OlaDevice
  string cfg = aDeviceType;
  cfg += ":";
  cfg += aDeviceConfig;
  newDev = DevicePtr(new OlaDevice(this, cfg));
  // add to container if device was created
  if (newDev) {
    // add to container
    addDevice(newDev);
    return boost::dynamic_pointer_cast<OlaDevice>(newDev);
  }
  // none added
  return OlaDevicePtr();
}


/// collect devices from this device class
/// @param aCompletedCB will be called when device scan for this device class has been completed
void OlaDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // incrementally collecting static devices makes no sense. The devices are "static"!
  if (!aIncremental) {
    // non-incremental, re-collect all devices
    removeDevices(aClearSettings);
    // then add those from the DB
    sqlite3pp::query qry(db);
    if (qry.prepare("SELECT devicetype, deviceconfig, rowid FROM devConfigs")==SQLITE_OK) {
      for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        OlaDevicePtr dev =addOlaDevice(i->get<string>(0), i->get<string>(1));
        dev->olaDeviceRowID = i->get<int>(2);
      }
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


ErrorPtr OlaDeviceContainer::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-addDevice") {
    // add a new OLA device
    string deviceType;
    string deviceConfig;
    respErr = checkStringParam(aParams, "deviceType", deviceType);
    if (Error::isOK(respErr)) {
      respErr = checkStringParam(aParams, "deviceConfig", deviceConfig);
      if (Error::isOK(respErr)) {
        // optional name
        string name;
        checkStringParam(aParams, "name", name);
        // try to create device
        OlaDevicePtr dev = addOlaDevice(deviceType, deviceConfig);
        if (!dev) {
          respErr = ErrorPtr(new WebError(500, "invalid configuration for OLA device -> none created"));
        }
        else {
          // set name
          if (name.size()>0) dev->setName(name);
          // insert into database
          db.executef(
            "INSERT OR REPLACE INTO devConfigs (devicetype, deviceconfig) VALUES ('%s','%s')",
            deviceType.c_str(), deviceConfig.c_str()
          );
          dev->olaDeviceRowID = db.last_insert_rowid();
          // confirm
          ApiValuePtr r = aRequest->newApiValue();
          r->setType(apivalue_object);
          r->add("dSUID", r->newBinary(dev->dSUID.getBinary()));
          r->add("rowid", r->newUint64(dev->olaDeviceRowID));
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

#endif // !DISABLE_OLA


