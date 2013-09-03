//
//  enoceandevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevicecontainer.hpp"

using namespace p44;


EnoceanDeviceContainer::EnoceanDeviceContainer(int aInstanceNumber) :
  DeviceClassContainer(aInstanceNumber),
  learningMode(false),
	enoceanComm(SyncIOMainLoop::currentMainLoop())
{
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanDeviceContainer::handleRadioPacket, this, _2, _3));
}



const char *EnoceanDeviceContainer::deviceClassIdentifier() const
{
  return "EnOcean_Bus_Container";
}


#pragma mark - DB and initialisation


#define ENOCEAN_SCHEMA_VERSION 4

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


void EnoceanDeviceContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), ENOCEAN_SCHEMA_VERSION, aFactoryReset);
	aCompletedCB(error); // return status of DB init
}




#pragma mark - collect devices

void EnoceanDeviceContainer::forgetDevices()
{
  inherited::forgetDevices();
  enoceanDevices.clear();
}



void EnoceanDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  // start with zero
  forgetDevices();
  // - read learned-in enOcean button IDs from DB
  sqlite3pp::query qry(db);
  if (qry.prepare("SELECT enoceanAddress, subdevice, eeProfile, eeManufacturer FROM knownDevices")==SQLITE_OK) {
    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
      EnoceanSubDevice numSubdevices;
      EnoceanDevicePtr newdev = EnoceanDevice::newDevice(
        this,
        i->get<int>(0), i->get<int>(1), // address / subdevice
        i->get<int>(2), i->get<int>(3), // profile / manufacturer
        numSubdevices,
        false // don't send teach-in responses
      );
      if (newdev) {
        // we fetched this from DB, so it is already known (don't save again!)
        addKnownDevice(newdev);
      }
      else {
        LOG(LOG_ERR,
          "EnOcean device could not be created for addr=%08X, subdevice=%d, profile=%06X, manufacturer=%d",
          i->get<int>(0), i->get<int>(1), // address / subdevice
          i->get<int>(2), i->get<int>(3) // profile / manufacturer
        );
      }
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


void EnoceanDeviceContainer::addKnownDevice(EnoceanDevicePtr aEnoceanDevice)
{
  inherited::addDevice(aEnoceanDevice);
  enoceanDevices.insert(make_pair(aEnoceanDevice->getAddress(), aEnoceanDevice));
}



void EnoceanDeviceContainer::addAndRemeberDevice(EnoceanDevicePtr aEnoceanDevice)
{
  addKnownDevice(aEnoceanDevice);
  // save enocean ID to DB
  sqlite3pp::query qry(db);
  // - check if this subdevice is already stored
  db.executef(
    "INSERT OR REPLACE INTO knownDevices (enoceanAddress, subdevice, eeProfile, eeManufacturer) VALUES (%d,%d,%d,%d)",
    aEnoceanDevice->getAddress(),
    aEnoceanDevice->getSubDevice(),
    aEnoceanDevice->getEEProfile(),
    aEnoceanDevice->getEEManufacturer()
  );
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
  // remove all logical devices with same physical enOcean address
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


#pragma mark - learn and unlearn devices


//#ifdef DEBUG
//#define MIN_LEARN_DBM -255 // any signal strength
//#warning "DEBUG Learning with weak signal enabled!"
//#else
//#define MIN_LEARN_DBM -50 // within approx one meter of the TCM310 (experimental luz v1 patched bridge)
//#endif

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
  // check learning mode
  if (learningMode) {
    // no learn/unlearn actions detected so far
    // - check if we know that device address already. If so, it is a learn-out
    bool learnIn = enoceanDevices.find(aEsp3PacketPtr->radioSender())==enoceanDevices.end();
    // now add/remove the device (if the action is a valid learn/unlearn)
    // detect implicit (RPS) learn in only with sufficient radio strength, explicit ones are always recognized
    if (aEsp3PacketPtr->eepHasTeachInfo(MIN_LEARN_DBM, false)) {
      // This is actually a valid learn action
      ErrorPtr learnStatus;
      if (learnIn) {
        // new device learned in, add logical devices for it
        int numNewDevices = EnoceanDevice::createDevicesFromEEP(this, aEsp3PacketPtr);
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
    } // learn action
  }
  else {
    // not learning, dispatch packet to all devices known for that address
    for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEsp3PacketPtr->radioSender()); pos!=enoceanDevices.upper_bound(aEsp3PacketPtr->radioSender()); ++pos) {
      pos->second->handleRadioPacket(aEsp3PacketPtr);
    }
  }
}


#pragma mark - learning / unlearning


void EnoceanDeviceContainer::setLearnMode(bool aEnableLearning)
{
  learningMode = aEnableLearning;
}



