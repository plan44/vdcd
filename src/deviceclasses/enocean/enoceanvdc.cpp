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

#include "enoceanvdc.hpp"

#if ENABLE_ENOCEAN

using namespace p44;


EnoceanVdc::EnoceanVdc(int aInstanceNumber, VdcHost *aVdcHostP, int aTag) :
  Vdc(aInstanceNumber, aVdcHostP, aTag),
  learningMode(false),
  selfTesting(false),
  disableProximityCheck(false),
	enoceanComm(MainLoop::currentMainLoop())
{
}



const char *EnoceanVdc::vdcClassIdentifier() const
{
  return "EnOcean_Bus_Container";
}


bool EnoceanVdc::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
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
//  5 : subdevice indices of 2-way enocean buttons must be adjusted (now 2-spaced to leave room for single button mode)
#define ENOCEAN_SCHEMA_MIN_VERSION 4 // minimally supported version, anything older will be deleted
#define ENOCEAN_SCHEMA_VERSION 5 // current version

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
  else if (aFromVersion==4) {
    // V4->V5: subdevice indices of 2-way enocean buttons must be adjusted (now 2-spaced to leave room for single button mode)
    // - affected profiles = 00-F6-02-FF and 00-F6-03-FF
    sql =
      "UPDATE knownDevices SET subdevice = 2*subdevice WHERE eeProfile=16122623 OR eeProfile=16122879;";
    // reached version 5
    aToVersion = 5;
  }
  return sql;
}


void EnoceanVdc::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", vdcClassIdentifier(), getInstanceNumber());
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

void EnoceanVdc::removeDevices(bool aForget)
{
  inherited::removeDevices(aForget);
  enoceanDevices.clear();
}



void EnoceanVdc::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  // install standard packet handler
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanVdc::handleRadioPacket, this, _1, _2));
  enoceanComm.setEventPacketHandler(boost::bind(&EnoceanVdc::handleEventPacket, this, _1, _2));
  // incrementally collecting EnOcean devices makes no sense as the set of devices is defined by learn-in (DB state)
  if (!aIncremental) {
    // start with zero
    removeDevices(aClearSettings);
    // - read learned-in EnOcean button IDs from DB
    sqlite3pp::query qry(db);
    if (qry.prepare("SELECT enoceanAddress, subdevice, eeProfile, eeManufacturer FROM knownDevices")==SQLITE_OK) {
      for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        EnoceanSubDevice subDeviceIndex = i->get<int>(1);
        EnoceanDevicePtr newdev = EnoceanDevice::newDevice(
          this,
          i->get<int>(0), subDeviceIndex, // address / subdeviceIndex
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
            i->get<int>(0), subDeviceIndex, // address / subdevice
            i->get<int>(2), i->get<int>(3) // profile / manufacturer
          );
        }
      }
    }
  }
  // assume ok
  aCompletedCB(ErrorPtr());
}


bool EnoceanVdc::addKnownDevice(EnoceanDevicePtr aEnoceanDevice)
{
  if (inherited::addDevice(aEnoceanDevice)) {
    // not a duplicate, actually added - add to my own list
    enoceanDevices.insert(make_pair(aEnoceanDevice->getAddress(), aEnoceanDevice));
    return true;
  }
  return false;
}



bool EnoceanVdc::addAndRememberDevice(EnoceanDevicePtr aEnoceanDevice)
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


void EnoceanVdc::removeDevice(DevicePtr aDevice, bool aForget)
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


void EnoceanVdc::unpairDevicesByAddress(EnoceanAddress aEnoceanAddress, bool aForgetParams, EnoceanSubDevice aFromIndex, EnoceanSubDevice aNumIndices)
{
  // remove all logical devices with same physical EnOcean address
  typedef list<EnoceanDevicePtr> TbdList;
  TbdList toBeDeleted;
  // collect those we need to remove
  for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEnoceanAddress); pos!=enoceanDevices.upper_bound(aEnoceanAddress); ++pos) {
    // check subdevice index
    EnoceanSubDevice i = pos->second->getSubDevice();
    if (i>=aFromIndex && ((aNumIndices==0) || (i<aFromIndex+aNumIndices))) {
      toBeDeleted.push_back(pos->second);
    }
  }
  // now call vanish (which will in turn remove devices from the container's list
  for (TbdList::iterator pos = toBeDeleted.begin(); pos!=toBeDeleted.end(); ++pos) {
    (*pos)->hasVanished(aForgetParams);
  }
}


#pragma mark - EnOcean specific methods


ErrorPtr EnoceanVdc::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-addProfile") {
    // create a composite device out of existing single-channel ones
    respErr = addProfile(aRequest, aParams);
  }
  else if (aMethod=="x-p44-simulatePacket") {
    // simulate reception of a ESP packet
    respErr = simulatePacket(aRequest, aParams);
  }
  else {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}



ErrorPtr EnoceanVdc::addProfile(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
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


ErrorPtr EnoceanVdc::simulatePacket(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  ApiValuePtr o;
  respErr = checkParam(aParams, "data", o); // ESP packet data, no need for matching CRCs
  if (Error::isOK(respErr)) {
    Esp3PacketPtr simPacket = Esp3PacketPtr(new Esp3Packet);
    // input string is hex bytes, optionally separated by spaces, colons or dashes
    string dataStr = o->stringValue();
    string bs = hexToBinaryString(dataStr.c_str(), true);
    // process with no CRC checks
    if (simPacket->acceptBytes(bs.size(), (const uint8_t *)bs.c_str(), true)!=bs.size()) {
      respErr = ErrorPtr(new WebError(400, "Wrong number of bytes in simulated ESP3 packet data"));
    }
    else {
      // process if complete
      if (simPacket->isComplete()) {
        LOG(LOG_DEBUG, "Simulated Enocean Packet:\n%s", simPacket->description().c_str());
        if (simPacket->packetType()==pt_radio) {
          handleRadioPacket(simPacket, ErrorPtr());
        }
        else if (simPacket->packetType()==pt_event_message) {
          handleEventPacket(simPacket, ErrorPtr());
        }
        // done
        aRequest->sendResult(ApiValuePtr());
      }
      else {
        respErr = ErrorPtr(new WebError(400, "invalid simulated ESP3 packet data"));
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

Tristate EnoceanVdc::processLearn(EnoceanAddress aDeviceAddress, EnoceanProfile aEEProfile, EnoceanManufacturer aManufacturer)
{
  // no learn/unlearn actions detected so far
  // - check if we know that device address already. If so, it is a learn-out
  bool learnIn = enoceanDevices.find(aDeviceAddress)==enoceanDevices.end();
  if (learnIn) {
    // new device learned in, add logical devices for it
    int numNewDevices = EnoceanDevice::createDevicesFromEEP(this, aDeviceAddress, aEEProfile, aManufacturer);
    if (numNewDevices>0) {
      // successfully learned at least one device
      // - update learn status (device learned)
      getVdc().reportLearnEvent(true, ErrorPtr());
      return yes; // learned in
    }
    return undefined; // failure - could not learn a device with this profile
  }
  else {
    // device learned out, un-pair all logical dS devices it has represented
    // but keep dS level config in case it is reconnected
    unpairDevicesByAddress(aDeviceAddress, false);
    getVdc().reportLearnEvent(false, ErrorPtr());
    return no; // always successful learn out
  }
}


void EnoceanVdc::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  if (aError) {
    LOG(LOG_INFO, "Radio packet error: %s", aError->description().c_str());
    return;
  }
  // suppress radio packets send by one of my secondary IDs
  if ((aEsp3PacketPtr->radioSender() & 0xFFFFFF80) == enoceanComm.idBase()) {
    LOG(LOG_DEBUG, "Suppressed radio packet coming from one of my own base IDs: %0X", aEsp3PacketPtr->radioSender());
    return;
  }
  // check learning mode
  if (learningMode) {
    // now add/remove the device (if the action is a valid learn/unlearn)
    // detect implicit (RPS) learn in only with sufficient radio strength (or explicit override of that check),
    // explicit ones are always recognized
    if (aEsp3PacketPtr->radioHasTeachInfo(disableProximityCheck ? 0 : MIN_LEARN_DBM, false)) {
      LOG(LOG_NOTICE, "Learn mode enabled: processing EnOcean learn packet: %s", aEsp3PacketPtr->description().c_str());
      processLearn(aEsp3PacketPtr->radioSender(), aEsp3PacketPtr->eepProfile(), aEsp3PacketPtr->eepManufacturer());
      // - only allow one learn action (to prevent learning out device when
      //   button is released or other repetition of radio packet)
      learningMode = false;
    } // learn action
    else {
      LOG(LOG_INFO, "Learn mode enabled: Received non-learn EnOcean packet -> ignored: %s", aEsp3PacketPtr->description().c_str());
    }
  }
  else {
    // not learning mode, dispatch packet to all devices known for that address
    bool reachedDevice = false;
    for (EnoceanDeviceMap::iterator pos = enoceanDevices.lower_bound(aEsp3PacketPtr->radioSender()); pos!=enoceanDevices.upper_bound(aEsp3PacketPtr->radioSender()); ++pos) {
      if (aEsp3PacketPtr->radioHasTeachInfo(MIN_LEARN_DBM, false) && aEsp3PacketPtr->eepRorg()!=rorg_RPS) {
        // learning packet in non-learn mode -> report as non-regular user action, might be attempt to identify a device
        // Note: RPS devices are excluded because for these all telegrams are regular user actions.
        // signalDeviceUserAction() will be called from button and binary input behaviours
        if (getVdc().signalDeviceUserAction(*(pos->second), false)) {
          // consumed for device identification purposes, suppress further processing
          break;
        }
      }
      // handle regularily (might be RPS switch which does not have separate learn/action packets
      pos->second->handleRadioPacket(aEsp3PacketPtr);
      reachedDevice = true;
    }
    if (!reachedDevice) {
      LOG(LOG_INFO, "Received EnOcean packet not directed to any known device -> ignored: %s", aEsp3PacketPtr->description().c_str());
    }
  }
}


#define SMART_ACK_RESPONSE_TIME_MS 100

void EnoceanVdc::handleEventPacket(Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  if (aError) {
    LOG(LOG_INFO, "Event packet error: %s", aError->description().c_str());
    return;
  }
  uint8_t *dataP = aEsp3PacketPtr->data();
  uint8_t eventCode = dataP[0];
  if (eventCode==SA_CONFIRM_LEARN) {
    uint8_t confirmCode = 0x13; // default: reject with "controller has no place for further mailbox"
    if (learningMode) {
      // process smart-ack learn
      // - extract learn data
      uint8_t postmasterFlags = dataP[1];
      EnoceanManufacturer manufacturer =
        ((EnoceanManufacturer)(dataP[2] & 0x03)<<8) +
        dataP[3];
      EnoceanProfile profile =
        ((EnoceanProfile)dataP[4]<<16) +
        ((EnoceanProfile)dataP[5]<<8) +
        dataP[6];
      int rssi = -dataP[7];
      EnoceanAddress postmasterAddress =
        ((EnoceanAddress)dataP[8]<<24) +
        ((EnoceanAddress)dataP[9]<<16) +
        ((EnoceanAddress)dataP[10]<<8) +
        dataP[11];
      EnoceanAddress deviceAddress =
        ((EnoceanAddress)dataP[12]<<24) +
        ((EnoceanAddress)dataP[13]<<16) +
        ((EnoceanAddress)dataP[14]<<8) +
        dataP[15];
      uint8_t hopCount = dataP[16];
      if (LOGENABLED(LOG_NOTICE)) {
        const char *mn = EnoceanComm::manufacturerName(manufacturer);
        LOG(LOG_NOTICE,
          "ESP3 SA_CONFIRM_LEARN, sender=0x%08X, rssi=%d, hops=%d"
          "\n- postmaster=0x%08X (priority flags = 0x%1X)"
          "\n- EEP RORG/FUNC/TYPE: %02X %02X %02X, Manufacturer = %s (%03X)",
          deviceAddress,
          rssi,
          hopCount,
          postmasterAddress,
          postmasterFlags,
          EEP_RORG(profile),
          EEP_FUNC(profile),
          EEP_TYPE(profile),
          mn ? mn : "<unknown>",
          manufacturer
        );
      }
      // try to process
      Tristate lrn = processLearn(deviceAddress, profile, manufacturer);
      if (lrn==no) {
        // learn out
        confirmCode = 0x20;
      }
      else if (lrn==yes) {
        // learn in
        confirmCode = 0x00;
      }
      else {
        // unknown EEP
        confirmCode = 0x11;
        LOG(LOG_WARNING, "Received SA_CONFIRM_LEARN with unknown EEP %06X -> rejecting", profile);
      }
    }
    else {
      LOG(LOG_WARNING, "Received SA_CONFIRM_LEARN while not in learning mode -> rejecting");
    }
    // always send response for SA_CONFIRM_LEARN
    Esp3PacketPtr respPacket = Esp3Packet::newEsp3Message(pt_response, RET_OK, 3);
    respPacket->data()[1] = (SMART_ACK_RESPONSE_TIME_MS>>8) & 0xFF;
    respPacket->data()[2] = SMART_ACK_RESPONSE_TIME_MS & 0xFF;
    respPacket->data()[3] = confirmCode;
    // issue response
    enoceanComm.sendPacket(respPacket); // immediate response, not in queue
  }
  else {
    LOG(LOG_INFO, "Unknown Event code: %d", eventCode);
  }
}



void EnoceanVdc::setLearnMode(bool aEnableLearning, bool aDisableProximityCheck)
{
  // put normal radio packet evaluator into learn mode
  learningMode = aEnableLearning;
  disableProximityCheck = aDisableProximityCheck;
  // also enable smartAck learn mode in the EnOcean module
  enoceanComm.smartAckLearnMode(aEnableLearning, 60*Second); // actual timeout of learn is usually smaller
}


#pragma mark - Self test

void EnoceanVdc::selfTest(StatusCB aCompletedCB)
{
  // install test packet handler
  enoceanComm.setRadioPacketHandler(boost::bind(&EnoceanVdc::handleTestRadioPacket, this, aCompletedCB, _1, _2));
  // start watchdog
  enoceanComm.initialize(NULL);
}


void EnoceanVdc::handleTestRadioPacket(StatusCB aCompletedCB, Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)
{
  // ignore packets with error
  if (Error::isOK(aError)) {
    if (aEsp3PacketPtr->eepRorg()==rorg_RPS && aEsp3PacketPtr->radioDBm()>MIN_LEARN_DBM && enoceanComm.modemAppVersion()>0) {
      // uninstall handler
      enoceanComm.setRadioPacketHandler(NULL);
      // seen both watchdog response (modem works) and independent RPS telegram (RF is ok)
      LOG(LOG_NOTICE,
        "- enocean modem info: appVersion=0x%08X, apiVersion=0x%08X, modemAddress=0x%08X, idBase=0x%08X",
        enoceanComm.modemAppVersion(), enoceanComm.modemApiVersion(), enoceanComm.modemAddress(), enoceanComm.idBase()
      );
      aCompletedCB(ErrorPtr());
      // done
      return;
    }
  }
  // - still waiting
  LOG(LOG_NOTICE, "- enocean test: still waiting for RPS telegram in learn distance");
}

#endif // ENABLE_ENOCEAN

