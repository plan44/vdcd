//
//  enoceandevicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevicecontainer.hpp"

using namespace p44;


EnoceanDeviceContainer::EnoceanDeviceContainer(int aInstanceNumber) :
  DeviceClassContainer(aInstanceNumber),
	enoceanComm(SyncIOMainLoop::currentMainLoop())
{
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanDeviceContainer::handleRadioPacket, this, _2, _3));
}



const char *EnoceanDeviceContainer::deviceClassIdentifier() const
{
  return "EnOcean_Bus_Container";
}


#pragma mark - DB and initialisation


#define SCHEMA_VERSION 1

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
			" ROWID INTEGER PRIMARY KEY AUTOINCREMENT,"
			" enoceanAddress INTEGER"
			");"
		);
    // reached final version in one step
    aToVersion = SCHEMA_VERSION;
  }
  return sql;
}


void EnoceanDeviceContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", deviceClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), SCHEMA_VERSION, aFactoryReset);
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
  if (qry.prepare("SELECT enoceanAddress FROM knownDevices")==SQLITE_OK) {
    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
      EnoceanDevicePtr newdev = EnoceanDevicePtr(new EnoceanDevice(this));
      newdev->setEnoceanAddress(i->get<int>(0));
      addDevice(newdev);
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


void EnoceanDeviceContainer::addDevice(DevicePtr aDevice)
{
  inherited::addDevice(aDevice);
  EnoceanDevicePtr ed = boost::dynamic_pointer_cast<EnoceanDevice>(aDevice);
  if (ed) {
    enoceanDevices[ed->getEnoceanAddress()] = ed;
    // save enocean ID to DB
    sqlite3pp::query qry(db);
    // - check if already saved
    if (qry.prepare("SELECT ROWID FROM knownDevices WHERE enoceanAddress=?1")==SQLITE_OK) {
      qry.bind(1, (int)ed->getEnoceanAddress());
      if (qry.begin()==qry.end()) {
        qry.reset();
        // - does not exist yet
        db.executef("INSERT INTO knownDevices (enoceanAddress) VALUES (%d)", ed->getEnoceanAddress());
      }
    }
  }
}


void EnoceanDeviceContainer::removeDevice(DevicePtr aDevice)
{
  inherited::removeDevice(aDevice);
  EnoceanDevicePtr ed = boost::dynamic_pointer_cast<EnoceanDevice>(aDevice);
  if (ed) {
    enoceanDevices.erase(ed->getEnoceanAddress());
    // remove from DB
    db.executef("DELETE FROM knownDevices WHERE enoceanAddress=%d", ed->getEnoceanAddress());
  }
}





EnoceanDevicePtr EnoceanDeviceContainer::getDeviceByAddress(EnoceanAddress aDeviceAddress)
{
  EnoceanDeviceMap::iterator pos = enoceanDevices.find(aDeviceAddress);
  if (pos!=enoceanDevices.end()) {
    return pos->second;
  }
  // none found
  return EnoceanDevicePtr();
}






#pragma mark - learn and unlearn devices


#ifdef DEBUG
#define MIN_LEARN_DBM -255 // any signal strength
#warning "DEBUG Learning with weak signal enabled!"
#else
#define MIN_LEARN_DBM -50 // within approx one meter of the TCM310 (experimental luz v1 patched bridge)
#endif




void EnoceanDeviceContainer::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  if (isLearning()) {
    // in learn mode, check if strong signal and if so, learn/unlearn
    if (aEsp3PacketPtr->radio_dBm()>MIN_LEARN_DBM)
    {
      // learn device where at least one button was released
      for (int bi=0; bi<aEsp3PacketPtr->rps_numRockers(); bi++) {
        uint8_t a = aEsp3PacketPtr->rps_action(bi);
        if ((a & rpsa_released)!=0) {
          EnoceanDevicePtr dev = getDeviceByAddress(aEsp3PacketPtr->radio_sender());
          if (dev) {
            // device exists - unlearn
            removeDevice(dev);
            endLearning(ErrorPtr(new EnoceanError(EnoceanDeviceUnlearned)));
          }
          else {
            // device does not exist - learn = create it
            EnoceanDevicePtr newdev = EnoceanDevicePtr(new EnoceanDevice(this));
            newdev->setEnoceanAddress(aEsp3PacketPtr->radio_sender());
            addDevice(newdev);
            endLearning(ErrorPtr(new EnoceanError(EnoceanDeviceLearned)));
          }
          // learn action detected, don't create more!
          break;
        }
      }
    } // strong enough signal
  }
  else {
    // not learning
    // TODO: refine
    if (keyEventHandler) {
      // - check if device already exists
      EnoceanDevicePtr dev = getDeviceByAddress(aEsp3PacketPtr->radio_sender());
      if (dev) {
        // known device
        for (int bi=0; bi<aEsp3PacketPtr->rps_numRockers(); bi++) {
          uint8_t a = aEsp3PacketPtr->rps_action(bi);
          if (a!=rpsa_none) {
            // create event
            keyEventHandler(dev, (a & rpsa_multiple)==0 ? bi : -1, a);
          }
        }
      }
    }
  }
}


void EnoceanDeviceContainer::learnSwitchDevice(CompletedCB aCompletedCB, MLMicroSeconds aLearnTimeout)
{
  if (isLearning()) return; // already learning -> NOP
  // start timer for timeout
  learningCompleteHandler = aCompletedCB;
  MainLoop::currentMainLoop()->executeOnce(boost::bind(&EnoceanDeviceContainer::stopLearning, this), aLearnTimeout);
}


bool EnoceanDeviceContainer::isLearning()
{
  return !learningCompleteHandler.empty();
}


void EnoceanDeviceContainer::stopLearning()
{
  endLearning(ErrorPtr(new EnoceanError(EnoceanLearnAborted)));
}


void EnoceanDeviceContainer::endLearning(ErrorPtr aError)
{
  MainLoop::currentMainLoop()->cancelExecutionsFrom(this); // cancel timeout
  if (isLearning()) {
    CompletedCB cb = learningCompleteHandler;
    learningCompleteHandler = NULL;
    cb(ErrorPtr(aError));
  }
}


void EnoceanDeviceContainer::setKeyEventHandler(KeyEventHandlerCB aKeyEventHandler)
{
  keyEventHandler = aKeyEventHandler;
}



