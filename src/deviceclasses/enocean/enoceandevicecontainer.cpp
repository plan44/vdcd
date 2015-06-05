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

#include "enoceandevicecontainer.hpp"

using namespace p44;


EnoceanDeviceContainer::EnoceanDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag),
  learningMode(false),
  selfTesting(false),
  disableProximityCheck(false),
	enoceanComm(MainLoop::currentMainLoop())
{
}



const char *EnoceanDeviceContainer::deviceClassIdentifier() const
{
  return "EnOcean_Bus_Container";
}


bool EnoceanDeviceContainer::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vdc_enocean", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}



#pragma mark - DB and initialisation

// Version history
//  1..3 : development versions
//  4 : first actually used schema
#define ENOCEAN_SCHEMA_MIN_VERSION 4 // minimally supported version, anything older will be deleted
#define ENOCEAN_SCHEMA_VERSION 4 // current version

string EnoceanPersistence::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string sql;
  if (aFromVersion==0) {
    // create DB from scratch
		// - use standard globs table for schema version
    sql = inherited::dbSchemaUpgradeSQL(aFromVersion, aToVersion);
		// - create my tables
    sql.append(
			"CREATE TABLE knownDevices ("
			" enoceanAddress INTEGER,"
      " subdevice INTEGER,"
      " eeProfile INTEGER,"
      " eeManufacturer INTEGER,"
      " PRIMARY KEY (enoceanAddress, subdevice)"
			");"
		);
    // reached final version in one step
    aToVersion = ENOCEAN_SCHEMA_VERSION;
  }
  else if (aFromVersion==1) {
    // V1->V2: eeProfile, eeManufacturer added
    sql =
      "ALTER TABLE knownDevices ADD eeProfile INTEGER;"
      "ALTER TABLE knownDevices ADD eeManufacturer INTEGER;";
    // reached version 2
    aToVersion = 2;
  }
  else if (aFromVersion==2) {
    // V2->V3: channel added
    sql =
      "ALTER TABLE knownDevices ADD channel INTEGER;";
    // reached version 3
    aToVersion = 3;
  }
  else if (aFromVersion==3) {
    // V3->V4: added subdevice (channel gets obsolete but SQLite cannot delete columns, so
    // leave it here. It's ok as the vDCd is not yet in real field use.
    sql =
    "ALTER TABLE knownDevices ADD subdevice INTEGER;";
    // reached version 4
    aToVersion = 4;
  }
  return sql;
}


void EnoceanDeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), ENOCEAN_SCHEMA_VERSION, ENOCEAN_SCHEMA_MIN_VERSION, aFactoryReset);
  if (!Error::isOK(error)) {
    // failed DB, no point in starting communication
    aCompletedCB(error); // return status of DB init
  }
  else {
    // start communication
    enoceanComm.initialize(aCompletedCB);
  }
}




#pragma mark - collect devices

void EnoceanDeviceContainer::removeDevices(bool aForget)
{
  inherited::removeDevices(aForget);
  enoceanDevices.clear();
}



void EnoceanDeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // install standard packet handler
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanDeviceContainer::handleRadioPacket, this, _1, _2));
  // incrementally collecting EnOcean devices makes no sense as the set of devices is defined by learn-in (DB state)
  if (!aIncremental) {
    // start with zero
    removeDevices(aClearSettings);
    // - read learned-in EnOcean button IDs from DB
    sqlite3pp::query qry(db);
    if (qry.prepare("SELECT enoceanAddress, subdevice, eeProfile, eeManufacturer FROM knownDevices")==SQLITE_OK) {
      for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        EnoceanDevicePtr newdev = EnoceanDevice::newDevice(
          this,
          i->get<int>(0), i->get<int>(1), // address / subdeviceIndex
          i->get<int>(2), i->get<int>(3), // profile / manufacturer
          false // don't send teach-in responses
        );
        if (newdev) {
          // we fetched this from DB, so it is already known (don't save again!)
          addKnownDevice(newdev);
        }
        else {
          LOG(LOG_ERR,
            "EnOcean device could not be created for addr=%08X, subdevice=%d, profile=%08X, manufacturer=%d",
            i->get<int>(0), i->get<int>(1), // address / subdevice
            i->get<int>(2), i->get<int>(3) // profile / manufacturer
          );
        }
      }
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


bool EnoceanDeviceContainer::addKnownDevice(EnoceanDevicePtr aEnoceanDevice)
{
  if (inherited::addDevice(aEnoceanDevice)) {
    // not a duplicate, actually added - add to my own list
    enoceanDevices.insert(make_pair(aEnoceanDevice->getAddress(), aEnoceanDevice));
    return true;
  }
  return false;
}



bool EnoceanDeviceContainer::addAndRemeberDevice(EnoceanDevicePtr aEnoceanDevice)
{
  if (addKnownDevice(aEnoceanDevice)) {
    // save enocean ID to DB
    // - check if this subdevice is already stored
    db.executef(
      "INSERT OR REPLACE INTO knownDevices (enoceanAddress, subdevice, eeProfile, eeManufacturer) VALUES (%d,%d,%d,%d)",
      aEnoceanDevice->getAddress(),
      aEnoceanDevice->getSubDevice(),
      aEnoceanDevice->getEEProfile(),
      aEnoceanDevice->getEEManufacturer()
    );
    return true;
  }
  return false;
}


void EnoceanDeviceContainer::removeDevice(DevicePtr aDevice, bool aForget)
{
  EnoceanDevicePtr ed = boost::dynamic_pointer_cast<EnoceanDevice>(aDevice);
  if (ed) {
    // - remove single device from superclass
    inherited::removeDevice(aDevice, aForget);
    // - remove only selected subdevice from my own list, other subdevices might be other devices
    EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(ed->getAddress());
    while (pos!=enoceanDevices.upper_bound(ed->getAddress())) {
      if (pos->second->getSubDevice()==ed->getSubDevice()) {
        // this is the subdevice we want deleted
        enoceanDevices.erase(pos);
        break; // done
      }
      pos++;
    }
  }
}


void EnoceanDeviceContainer::unpairDevicesByAddress(EnoceanAddress aEnoceanAddress, bool aForgetParams)
{
  // remove all logical devices with same physical EnOcean address
  typedef list<EnoceanDevicePtr> TbdList;
  TbdList toBeDeleted;
  // collect those we need to remove
  for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEnoceanAddress); pos!=enoceanDevices.upper_bound(aEnoceanAddress); ++pos) {
    toBeDeleted.push_back(pos->second);
  }
  // now call vanish (which will in turn remove devices from the container's list
  for (TbdList::iterator pos = toBeDeleted.begin(); pos!=toBeDeleted.end(); ++pos) {
    (*pos)->hasVanished(aForgetParams);
  }
}


#pragma mark - EnOcean specific methods


ErrorPtr EnoceanDeviceContainer::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-addProfile") {
    // create a composite device out of existing single-channel ones
    respErr = addProfile(aRequest, aParams);
  }
  else {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}



ErrorPtr EnoceanDeviceContainer::addProfile(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  // add an EnOcean profile
  ErrorPtr respErr;
  ApiValuePtr o;
  respErr = checkParam(aParams, "eep", o); // EEP with variant in MSB
  if (Error::isOK(respErr)) {
    EnoceanProfile eep = o->uint32Value();
    respErr = checkParam(aParams, "address", o);
    if (Error::isOK(respErr)) {
      // remote device address
      // if 0xFF800000..0xFF80007F : bit0..6 = ID base offset to ID base of modem
      // if 0xFF8000FF : automatically take next unused ID base offset
      EnoceanAddress addr = o->uint32Value();
      if ((addr & 0xFFFFFF00)==0xFF800000) {
        // relative to ID base
        // - get map of already used offsets
        string usedOffsetMap;
        usedOffsetMap.assign(128,'0');
        for (EnoceanDeviceMap::iterator pos = enoceanDevices.begin(); pos!=enoceanDevices.end(); ++pos) {
          pos->second->markUsedBaseOffsets(usedOffsetMap);
        }
        addr &= 0xFF; // extract offset
        if (addr==0xFF) {
          // auto-determine offset
          for (addr=0; addr<128; addr++) {
            if (usedOffsetMap[addr]=='0') break; // free offset here
          }
          if (addr>128) {
            respErr = ErrorPtr(new WebError(400, "no more free base ID offsets"));
          }
        }
        else {
          if (usedOffsetMap[addr]!='0') {
            respErr = ErrorPtr(new WebError(400, "invalid or already used base ID offset specifier"));
          }
        }
        // add-in my own ID base
        addr += enoceanComm.idBase();
      }
      // now create device(s)
      if (Error::isOK(respErr)) {
        // create devices as if this was a learn-in
        int newDevices = EnoceanDevice::createDevicesFromEEP(this, addr, eep, manufacturer_unknown);
        if (newDevices<1) {
          respErr = ErrorPtr(new WebError(400, "Unknown EEP specification, no device(s) created"));
        }
        else {
          ApiValuePtr r = aRequest->newApiValue();
          r->setType(apivalue_object);
          r->add("newDevices", r->newUint64(newDevices));
          respErr = aRequest->sendResult(r);
        }
      }
    }
  }
  return respErr;
}





#pragma mark - learn and unlearn devices


#define MIN_LEARN_DBM -50 
// -50 = for experimental luz v1 patched bridge: within approx one meter of the TCM310
// -50 = for v2 bridge 223: very close to device, about 10-20cm
// -55 = for v2 bridge 223: within approx one meter of the TCM310



void EnoceanDeviceContainer::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  if (aError) {
    LOG(LOG_INFO, "Radio packet error: %s\n", aError->description().c_str());
    return;
  }
  // suppress radio packets send by one of my secondary IDs
  if ((aEsp3PacketPtr->radioSender() & 0xFFFFFF80) == enoceanComm.idBase()) {
    LOG(LOG_DEBUG, "Suppressed radio packet coming from one of my own base IDs: %0lX\n", aEsp3PacketPtr->radioSender());
    return;
  }
  // check learning mode
  if (learningMode) {
    // no learn/unlearn actions detected so far
    // - check if we know that device address already. If so, it is a learn-out
    bool learnIn = enoceanDevices.find(aEsp3PacketPtr->radioSender())==enoceanDevices.end();
    // now add/remove the device (if the action is a valid learn/unlearn)
    // detect implicit (RPS) learn in only with sufficient radio strength (or explicit override of that check),
    // explicit ones are always recognized
    if (aEsp3PacketPtr->eepHasTeachInfo(disableProximityCheck ? 0 : MIN_LEARN_DBM, false)) {
      LOG(LOG_NOTICE, "Received EnOcean learn packet while learn mode enabled: %s\n", aEsp3PacketPtr->description().c_str());
      // This is actually a valid learn action
      if (learnIn) {
        // new device learned in, add logical devices for it
        int numNewDevices = EnoceanDevice::createDevicesFromEEP(this, aEsp3PacketPtr->radioSender(), aEsp3PacketPtr->eepProfile(), aEsp3PacketPtr->eepManufacturer());
        if (numNewDevices>0) {
          // successfully learned at least one device
          // - update learn status (device learned)
          getDeviceContainer().reportLearnEvent(true, ErrorPtr());
        }
      }
      else {
        // device learned out, un-pair all logical dS devices it has represented
        // but keep dS level config in case it is reconnected
        unpairDevicesByAddress(aEsp3PacketPtr->radioSender(), false);
        getDeviceContainer().reportLearnEvent(false, ErrorPtr());
      }
      // - only allow one learn action (to prevent learning out device when
      //   button is released or other repetition of radio packet)
      learningMode = false;
    } // learn action
  }
  else {
    // not learning mode, dispatch packet to all devices known for that address
    for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEsp3PacketPtr->radioSender()); pos!=enoceanDevices.upper_bound(aEsp3PacketPtr->radioSender()); ++pos) {
      if (aEsp3PacketPtr->eepHasTeachInfo(MIN_LEARN_DBM, false) && aEsp3PacketPtr->eepRorg()!=rorg_RPS) {
        // learning packet in non-learn mode -> report as non-regular user action, might be attempt to identify a device
        // Note: RPS devices are excluded because for these all telegrams are regular user actions.
        // signalDeviceUserAction() will be called from button and binary input behaviours
        if (getDeviceContainer().signalDeviceUserAction(*(pos->second), false)) {
          // consumed for device identification purposes, suppress further processing
          break;
        }
      }
      // handle regularily (might be RPS switch which does not have separate learn/action packets
      pos->second->handleRadioPacket(aEsp3PacketPtr);
    }
  }
}


void EnoceanDeviceContainer::setLearnMode(bool aEnableLearning, bool aDisableProximityCheck)
{
  learningMode = aEnableLearning;
  disableProximityCheck = aDisableProximityCheck;
}


#pragma mark - Self test

void EnoceanDeviceContainer::selfTest(StatusCB aCompletedCB)
{
  // install test packet handler
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanDeviceContainer::handleTestRadioPacket, this, aCompletedCB, _1, _2));
  // start watchdog
  enoceanComm.initialize(NULL);
}


void EnoceanDeviceContainer::handleTestRadioPacket(StatusCB aCompletedCB, Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  // ignore packets with error
  if (Error::isOK(aError)) {
    if (aEsp3PacketPtr->eepRorg()==rorg_RPS && aEsp3PacketPtr->radioDBm()>MIN_LEARN_DBM && enoceanComm.modemAppVersion()>0) {
      // uninstall handler
      enoceanComm.setRadioPacketHandler(NULL);
      // seen both watchdog response (modem works) and independent RPS telegram (RF is ok)
      LOG(LOG_NOTICE,
        "- enocean modem info: appVersion=0x%08X, apiVersion=0x%08X, modemAddress=0x%08X, idBase=0x%08X\n",
        enoceanComm.modemAppVersion(), enoceanComm.modemApiVersion(), enoceanComm.modemAddress(), enoceanComm.idBase()
      );
      aCompletedCB(ErrorPtr());
      // done
      return;
    }
  }
  // - still waiting
  LOG(LOG_NOTICE, "- enocean test: still waiting for RPS telegram in learn distance\n");
}


