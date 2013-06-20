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


#define ENOCEAN_SCHEMA_VERSION 3

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
      " channel INTEGER,"
      " eeProfile INTEGER,"
      " eeManufacturer INTEGER,"
      " PRIMARY KEY (enoceanAddress, channel)"
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
    // reached version 2
    aToVersion = 3;
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
  if (qry.prepare("SELECT enoceanAddress, channel, eeProfile, eeManufacturer FROM knownDevices")==SQLITE_OK) {
    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
      EnoceanDevicePtr newdev = EnoceanDevice::newDevice(
        this,
        i->get<int>(0), i->get<int>(1), // address / channel
        i->get<int>(2), i->get<int>(3) // profile / manufacturer
      );
      if (newdev) {
        // we fetched this from DB, so it is already known (don't save again!)
        addKnownDevice(newdev);
      }
      else {
        LOG(LOG_ERR,
          "EnOcean device could not be created for addr=%08X, channel=%d, profile=%06X, manufacturer=%d",
          i->get<int>(0), i->get<int>(1), // address / channel
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
  // - check if this channel is already stored
  db.executef(
    "INSERT OR REPLACE INTO knownDevices (enoceanAddress, channel, eeProfile, eeManufacturer) VALUES (%d,%d,%d,%d)",
    aEnoceanDevice->getAddress(),
    aEnoceanDevice->getChannel(),
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
    // - remove only selected channel from my own list
    EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(ed->getAddress());
    while (pos!=enoceanDevices.upper_bound(ed->getAddress())) {
      if (pos->second->getChannel()==ed->getChannel()) {
        // this is the channel we want deleted
        enoceanDevices.erase(pos);
        break; // done
      }
      pos++;
    }
    // also remove from DB
    db.executef("DELETE FROM knownDevices WHERE enoceanAddress=%d AND channel=%d", ed->getAddress(), ed->getChannel());
  }
}


void EnoceanDeviceContainer::removeDevicesByAddress(EnoceanAddress aEnoceanAddress)
{
  // remove all logical devices with same physical address
  // - remove from superclass (which sees these as completely separate devices)
  for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEnoceanAddress); pos!=enoceanDevices.upper_bound(aEnoceanAddress); ++pos) {
    inherited::removeDevice(pos->second, true);
  }
  // - remove all with that address from my own list
  enoceanDevices.erase(aEnoceanAddress);
  // also remove from DB
  db.executef("DELETE FROM knownDevices WHERE enoceanAddress=%d", aEnoceanAddress);
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
  if (aError) {
    LOG(LOG_INFO, "Radio packet error: %s\n", aError->description().c_str());
    return;
  }
  // check learning mode
  if (isLearning()) {
    // in learn mode, check if strong signal and if so, learn/unlearn
    if (aEsp3PacketPtr->radio_dBm()>MIN_LEARN_DBM)
    {
      // no learn/unlearn actions detected so far
      // - check if we know that device address already. If so, it is a learn-out
      bool learnIn = enoceanDevices.find(aEsp3PacketPtr->radio_sender())==enoceanDevices.end();
      // now add/remove the device (if the action is a valid learn/unlearn)
      if (aEsp3PacketPtr->eep_hasTeachInfo()) {
        // This is actually a valid learn action
        ErrorPtr learnStatus;
        if (learnIn) {
          // new device learned in, add logical devices for it
          int numNewDevices = EnoceanDevice::createDevicesFromEEP(this,aEsp3PacketPtr);
          if (numNewDevices>0) {
            learnStatus = ErrorPtr(new EnoceanError(EnoceanDeviceLearned));
          }
        }
        else {
          // device learned out, remove it
          removeDevicesByAddress(aEsp3PacketPtr->radio_sender()); // remove all logical devices in this physical device
          learnStatus = ErrorPtr(new EnoceanError(EnoceanDeviceUnlearned));
        }
        // - end learning if actually learned or unlearned something
        if (learnStatus)
          endLearning(learnStatus);
      } // learn action
    } // strong enough signal for learning
  }
  else {
    // not learning, dispatch packet to all devices known for that address
    for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEsp3PacketPtr->radio_sender()); pos!=enoceanDevices.upper_bound(aEsp3PacketPtr->radio_sender()); ++pos) {
      pos->second->handleRadioPacket(aEsp3PacketPtr);
    }
  }
}



#pragma mark - learning / unlearning


void EnoceanDeviceContainer::learnDevice(CompletedCB aCompletedCB, MLMicroSeconds aLearnTimeout)
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




