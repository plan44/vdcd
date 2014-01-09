//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "devicecontainer.hpp"

#include "deviceclasscontainer.hpp"

#include <string.h>

#include "device.hpp"

#include "macaddress.hpp"
#include "fnv.hpp"

// for local behaviour
#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"


using namespace p44;


DeviceContainer::DeviceContainer() :
  mac(0),
  DsAddressable(this),
  collecting(false),
  learningMode(false),
  announcementTicket(0),
  periodicTaskTicket(0),
  localDimTicket(0),
  localDimDown(false),
  dsUids(false)
{
  // obtain MAC address
  mac = macAddress();
}



string DeviceContainer::macAddressString()
{
  string macStr;
  if (mac!=0) {
    for (int i=0; i<6; ++i) {
      string_format_append(macStr, "%02X",(mac>>((5-i)*8)) & 0xFF);
    }
  }
  else {
    macStr = "UnknownMACAddress";
  }
  return macStr;
}



void DeviceContainer::deriveDsUid()
{
  if (!externalDsuid) {
    // we don't have a fixed external dSUID to base everything on, derive a dSUID of our own
    if (usingDsUids()) {
      // single vDC per MAC-Adress scenario: generate UUIDv5 with name = macaddress
      // - calculate UUIDv5 based dSUID
      DsUid vdcNamespace(DSUID_VDC_NAMESPACE_UUID);
      dSUID.setNameInSpace(macAddressString(), vdcNamespace);
    }
    else {
      // classic dsids: create a hash from MAC hex string
      Fnv64 hash;
      string s = macAddressString();
      hash.addBytes(s.size(), (uint8_t *)s.c_str());
      #if FAKE_REAL_DSD_IDS
      dSUID.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
      dSUID.setDsSerialNo(hash.getHash32());
      #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
      #else
      // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
      dSUID.setObjectClass(DSID_OBJECTCLASS_MACADDRESS);
      dSUID.setSerialNo(0x7000000000000ll+hash.getHash48());
      #endif
    }
  }
}


void DeviceContainer::setIdMode(bool aDsUids, DsUidPtr aExternalDsUid)
{
  dsUids = aDsUids;
  if (aExternalDsUid) {
    externalDsuid = true;
    dSUID = *aExternalDsUid;
  }
  deriveDsUid(); // derive my dSUID now (again), if necessary
}



void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers[aDeviceClassContainerPtr->dSUID] = aDeviceClassContainerPtr;
}



void DeviceContainer::setPersistentDataDir(const char *aPersistentDataDir)
{
	persistentDataDir = nonNullCStr(aPersistentDataDir);
	if (!persistentDataDir.empty() && persistentDataDir[persistentDataDir.length()-1]!='/') {
		persistentDataDir.append("/");
	}
}


const char *DeviceContainer::getPersistentDataDir()
{
	return persistentDataDir.c_str();
}





#pragma mark - initializisation of DB and containers


class DeviceClassInitializer
{
  CompletedCB callback;
  ContainerMap::iterator nextContainer;
  DeviceContainer *deviceContainerP;
  bool factoryReset;
public:
  static void initialize(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aFactoryReset)
  {
    // create new instance, deletes itself when finished
    new DeviceClassInitializer(aDeviceContainerP, aCallback, aFactoryReset);
  };
private:
  DeviceClassInitializer(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aFactoryReset) :
		callback(aCallback),
		deviceContainerP(aDeviceContainerP),
    factoryReset(aFactoryReset)
  {
    nextContainer = deviceContainerP->deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }


  void queryNextContainer(ErrorPtr aError)
  {
    if ((!aError || factoryReset) && nextContainer!=deviceContainerP->deviceClassContainers.end())
      nextContainer->second->initialize(boost::bind(&DeviceClassInitializer::containerInitialized, this, _1), factoryReset);
    else
      completed(aError);
  }

  void containerInitialized(ErrorPtr aError)
  {
    // check next
    ++nextContainer;
    queryNextContainer(aError);
  }

  void completed(ErrorPtr aError)
  {
    // start periodic tasks like registration checking and saving parameters
    MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::periodicTask, deviceContainerP, _2), 1*Second, deviceContainerP);
    // callback
    callback(aError);
    // done, delete myself
    delete this;
  }

};


#define DSPARAMS_SCHEMA_VERSION 1

string DsParamStore::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string sql;
  if (aFromVersion==0) {
    // create DB from scratch
		// - use standard globs table for schema version
    sql = inherited::dbSchemaUpgradeSQL(aFromVersion, aToVersion);
		// - no devicecontainer level table to create at this time
    //   (PersistentParams create and update their tables as needed)
    // reached final version in one step
    aToVersion = DSPARAMS_SCHEMA_VERSION;
  }
  return sql;
}


void DeviceContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // Log start message
  LOG(LOG_NOTICE,"\n****** starting vDC initialisation, MAC: %s, dSUID (%s) = %s\n", macAddressString().c_str(), externalDsuid ? "external" : "MAC-derived", dSUID.getString().c_str());
  // start the API server
  if (vdcApiServer) {
    vdcApiServer->setConnectionStatusHandler(boost::bind(&DeviceContainer::vdcApiConnectionStatusHandler, this, _1, _2));
    vdcApiServer->start();
  }
  // initialize dsParamsDB database
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "DsParams.sqlite3");
  ErrorPtr error = dsParamStore.connectAndInitialize(databaseName.c_str(), DSPARAMS_SCHEMA_VERSION, aFactoryReset);

  // start initialisation of class containers
  DeviceClassInitializer::initialize(this, aCompletedCB, aFactoryReset);
}




#pragma mark - collect devices

namespace p44 {

/// collects and initializes all devices
class DeviceClassCollector
{
  CompletedCB callback;
  bool exhaustive;
  bool incremental;
  ContainerMap::iterator nextContainer;
  DeviceContainer *deviceContainerP;
  DsDeviceMap::iterator nextDevice;
public:
  static void collectDevices(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aIncremental, bool aExhaustive)
  {
    // create new instance, deletes itself when finished
    new DeviceClassCollector(aDeviceContainerP, aCallback, aIncremental, aExhaustive);
  };
private:
  DeviceClassCollector(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aIncremental, bool aExhaustive) :
    callback(aCallback),
    deviceContainerP(aDeviceContainerP),
    incremental(aIncremental),
    exhaustive(aExhaustive)
  {
    nextContainer = deviceContainerP->deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }


  void queryNextContainer(ErrorPtr aError)
  {
    if (!aError && nextContainer!=deviceContainerP->deviceClassContainers.end())
      nextContainer->second->collectDevices(boost::bind(&DeviceClassCollector::containerQueried, this, _1), incremental, exhaustive);
    else
      collectedAll(aError);
  }

  void containerQueried(ErrorPtr aError)
  {
    // check next
    ++nextContainer;
    queryNextContainer(aError);
  }


  void collectedAll(ErrorPtr aError)
  {
    // now have each of them initialized
    nextDevice = deviceContainerP->dSDevices.begin();
    initializeNextDevice(ErrorPtr());
  }


  void initializeNextDevice(ErrorPtr aError)
  {
    if (!aError && nextDevice!=deviceContainerP->dSDevices.end())
      // TODO: now never doing factory reset init, maybe parametrize later
      nextDevice->second->initializeDevice(boost::bind(&DeviceClassCollector::deviceInitialized, this, _1), false);
    else
      completed(aError);
  }


  void deviceInitialized(ErrorPtr aError)
  {
    // check next
    ++nextDevice;
    initializeNextDevice(aError);
  }


  void completed(ErrorPtr aError)
  {
    callback(aError);
    deviceContainerP->collecting = false;
    // done, delete myself
    delete this;
  }

};



void DeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  if (!collecting) {
    collecting = true;
    if (!aIncremental) {
      // only for non-incremental collect, close vdsm connection
      if (activeSessionConnection) {
        activeSessionConnection->closeConnection(); // close the API connection
        resetAnnouncing();
        activeSessionConnection.reset(); // forget connection
      }
      dSDevices.clear(); // forget existing ones
    }
    DeviceClassCollector::collectDevices(this, aCompletedCB, aIncremental, aExhaustive);
  }
}

} // namespace




#pragma mark - adding/removing devices


// add a new device, replaces possibly existing one based on dSUID
bool DeviceContainer::addDevice(DevicePtr aDevice)
{
  if (!aDevice)
    return false; // no device, nothing added
  // check if device with same dSUID already exists
  DsDeviceMap::iterator pos = dSDevices.find(aDevice->dSUID);
  if (pos!=dSDevices.end()) {
    LOG(LOG_INFO, "- device %s already registered, not added again\n",aDevice->dSUID.getString().c_str());
    return false; // duplicate dSUID, not added
  }
  // set for given dSUID in the container-wide map of devices
  dSDevices[aDevice->dSUID] = aDevice;
  LOG(LOG_NOTICE,"--- added device: %s\n",aDevice->shortDesc().c_str());
  LOG(LOG_INFO, "- device description: %s",aDevice->description().c_str());
  // load the device's persistent params
  aDevice->load();
  // register new device right away (unless collecting or already announcing)
  startAnnouncing();
  return true;
}


// remove a device from container list (but does not disconnect it!)
void DeviceContainer::removeDevice(DevicePtr aDevice, bool aForget)
{
  if (aForget) {
    // permanently remove from DB
    aDevice->forget();
  }
  else {
    // save, as we don't want to forget the settings associated with the device
    aDevice->save();
  }
  // remove from container-wide map of devices
  dSDevices.erase(aDevice->dSUID);
  LOG(LOG_NOTICE,"--- removed device: %s\n", aDevice->shortDesc().c_str());
}



void DeviceContainer::startLearning(LearnCB aLearnHandler)
{
  // enable learning in all class containers
  learnHandler = aLearnHandler;
  learningMode = true;
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos != deviceClassContainers.end(); ++pos) {
    pos->second->setLearnMode(true);
  }
}


void DeviceContainer::stopLearning()
{
  // disable learning in all class containers
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos != deviceClassContainers.end(); ++pos) {
    pos->second->setLearnMode(false);
  }
  learningMode = false;
  learnHandler.clear();
}


void DeviceContainer::reportLearnEvent(bool aLearnIn, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aLearnIn)
      LOG(LOG_NOTICE,"--- learned in (paired) new device(s)\n");
    else
      LOG(LOG_NOTICE,"--- learned out (unpaired) device(s)\n");
  }
  // report status
  if (learnHandler) {
    learnHandler(aLearnIn, aError);
  }
}



#pragma mark - activity monitor


void DeviceContainer::setActivityMonitor(DoneCB aActivityCB)
{
  activityHandler = aActivityCB;
}


void DeviceContainer::signalActivity()
{
  if (activityHandler) {
    activityHandler();
  }
}


#pragma mark - periodic activity


#define PERIODIC_TASK_INTERVAL (5*Second)

void DeviceContainer::periodicTask(MLMicroSeconds aCycleStartTime)
{
  // cancel any pending executions
  MainLoop::currentMainLoop().cancelExecutionTicket(periodicTaskTicket);
  if (!collecting) {
    // check again for devices that need to be announced
    startAnnouncing();
    // do a save run as well
    for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
      pos->second->save();
    }
  }
  // schedule next run
  periodicTaskTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::periodicTask, this, _2), PERIODIC_TASK_INTERVAL, this);
}


#pragma mark - local operation mode


void DeviceContainer::localDimHandler()
{
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    if (dev->isMember(group_yellow_light)) {
      signalActivity();
      dev->callScene(localDimDown ? DEC_S : INC_S, true);
    }
  }
  localDimTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::localDimHandler, this), 250*MilliSecond, this);
}



void DeviceContainer::checkForLocalClickHandling(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType)
{
  if (!activeSessionConnection) {
    // not connected to a vdSM, handle clicks locally
    handleClickLocally(aButtonBehaviour, aClickType);
  }
}


void DeviceContainer::handleClickLocally(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType)
{
  // TODO: Not really conforming to ds-light yet...
  int scene = -1; // none
  int direction = aButtonBehaviour.localFunctionElement()==buttonElement_up ? 1 : (aButtonBehaviour.localFunctionElement()==buttonElement_down ? -1 : 0); // -1=down/off, 1=up/on, 0=toggle
  switch (aClickType) {
    case ct_tip_1x:
    case ct_click_1x:
      scene = T0_S1;
      break;
    case ct_tip_2x:
    case ct_click_2x:
      scene = T0_S2;
      break;
    case ct_tip_3x:
    case ct_click_3x:
      scene = T0_S3;
      break;
    case ct_tip_4x:
      scene = T0_S4;
      break;
    case ct_hold_start:
      scene = INC_S;
      localDimTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::localDimHandler, this), 250*MilliSecond, this);
      if (direction!=0)
        localDimDown = direction<0;
      else {
        localDimDown = !localDimDown; // just toggle direction
        direction = localDimDown ? -1 : 1; // adjust direction as well
      }
      break;
    case ct_hold_end:
      MainLoop::currentMainLoop().cancelExecutionTicket(localDimTicket); // stop dimming
      scene = STOP_S; // stop any still ongoing dimming
      direction = 1; // really send STOP, not main off!
      break;
  }
  if (scene>=0) {
    if (aClickType!=ct_hold_start) {
      // safety: any scene call except hold start stops ongoing dimming
      MainLoop::currentMainLoop().cancelExecutionTicket(localDimTicket);
    }
    for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
      DevicePtr dev = pos->second;
      if (dev->isMember(group_yellow_light)) {
        // this is a light related device (but not necessarily a light output!)
        LightBehaviourPtr lightBehaviour;
        if (dev->outputs.size()>0) {
          lightBehaviour = boost::dynamic_pointer_cast<LightBehaviour>(dev->outputs[0]);
          if (lightBehaviour) {
            // this device has a light behaviour output
            if (direction==0) {
              // get direction from current value of first encountered light
              direction = lightBehaviour->getLogicalBrightness()>1 ? -1 : 1;
            }
            // determine the scene to call
            int effScene = scene;
            if (scene==INC_S) {
              // dimming
              if (direction<0)
                effScene = DEC_S;
              else {
                // increment - check if we need to do a MIN_S first
                if (lightBehaviour && lightBehaviour->getLogicalBrightness()==0)
                  effScene = MIN_S; // after calling this once, light should be logically on
              }
            }
            else {
              // switching
              if (direction<0) effScene = T0_S0; // main off
            }
            // call the effective scene
            signalActivity(); // local activity
            dev->callScene(effScene, true);
          } // if light behaviour
        } // if any outputs
      } // if in light group
    }
  }
}



#pragma mark - vDC API


#define SESSION_TIMEOUT (3*Minute) // 3 minutes



bool DeviceContainer::sendApiRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  // TODO: once allowDisconnect is implemented, check here for creating a connection back to the vdSM
  if (activeSessionConnection) {
    signalActivity();
    return Error::isOK(activeSessionConnection->sendRequest(aMethod, aParams, aResponseHandler));
  }
  // cannot send
  return false;
}



void DeviceContainer::sessionTimeoutHandler()
{
  LOG(LOG_INFO,"vDC API session timed out -> ends here\n");
  if (activeSessionConnection) {
    activeSessionConnection->closeConnection();
    resetAnnouncing(); // stop possibly ongoing announcing
    activeSessionConnection.reset();
  }
}



void DeviceContainer::vdcApiConnectionStatusHandler(VdcApiConnectionPtr aApiConnection, ErrorPtr &aError)
{
  if (Error::isOK(aError)) {
    // new connection, set up reequest handler
    aApiConnection->setRequestHandler(boost::bind(&DeviceContainer::vdcApiRequestHandler, this, _1, _2, _3, _4));
  }
  else {
    // error or connection closed
    // - close if not already closed
    aApiConnection->closeConnection();
    if (aApiConnection==activeSessionConnection) {
      // this is the active session connection
      resetAnnouncing(); // stop possibly ongoing announcing
      activeSessionConnection.reset();
      LOG(LOG_INFO,"vDC API session ends because connection closed \n");
    }
    else {
      LOG(LOG_INFO,"vDC API connection (not yet in session) closed \n");
    }
  }
}


void DeviceContainer::vdcApiRequestHandler(VdcApiConnectionPtr aApiConnection, VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  signalActivity();
  // retrigger session timout
  MainLoop::currentMainLoop().cancelExecutionTicket(sessionActivityTicket);
  sessionActivityTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::sessionTimeoutHandler,this), SESSION_TIMEOUT);
  // now process
  if (aRequest) {
    // Methods
    // - Check session init/end methods
    if (aMethod=="hello") {
      respErr = helloHandler(aRequest, aParams);
    }
    else if (aMethod=="bye") {
      respErr = byeHandler(aRequest, aParams);
    }
    else {
      if (!activeSessionConnection) {
        // all following methods must have an active session
        respErr = ErrorPtr(new VdcApiError(401,"no vDC session - cannot call method"));
      }
      else {
        // session active - all commands need dSUID parameter
        string dsuidstring;
        if (Error::isOK(respErr = checkStringParam(aParams, "dSUID", dsuidstring))) {
          // operation method
          respErr = handleMethodForDsUid(aMethod, aRequest, DsUid(dsuidstring), aParams);
        }
      }
    }
  }
  else {
    // Notifications
    if (activeSessionConnection) {
      // out of session, notifications are simply ignored
      string dsuidstring;
      if (Error::isOK(respErr = checkStringParam(aParams, "dSUID", dsuidstring))) {
        handleNotificationForDsUid(aMethod, DsUid(dsuidstring), aParams);
      }
    }
  }
  // check error
  if (!Error::isOK(respErr)) {
    if (aRequest) {
      // report back in case of method call
      aRequest->sendError(respErr);
    }
    else {
      // just log in case of notification
      LOG(LOG_WARNING, "Notification '%s' processing error: %s\n", aMethod.c_str(), respErr->description().c_str());
    }
  }
}



ErrorPtr DeviceContainer::helloHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  string s;
  // check API version
  if (Error::isOK(respErr = checkStringParam(aParams, "APIVersion", s))) {
    if (s!="1.0" && s!="1")
      respErr = ErrorPtr(new VdcApiError(505, "Incompatible vDC API version - expected '1.0'"));
    else {
      // API version ok, check dSUID
      if (Error::isOK(respErr = checkStringParam(aParams, "dSUID", s))) {
        DsUid vdsmDsUid = DsUid(s);
        // same vdSM can restart session any time. Others will be rejected
        if (!activeSessionConnection || vdsmDsUid==connectedVdsm) {
          // ok to start new session
          if (activeSessionConnection) {
            // session connection was already there, re-announce
            resetAnnouncing();
          }
          // - start session with this vdSM
          connectedVdsm = vdsmDsUid;
          // - remember the session's connection
          activeSessionConnection = aRequest->connection();
          // - create answer
          ApiValuePtr result = activeSessionConnection->newApiValue();
          result->setType(apivalue_object);
          result->add("dSUID", aParams->newString(dSUID.getString()));
          result->add("allowDisconnect", aParams->newBool(false));
          aRequest->sendResult(result);
          // - trigger announcing devices
          startAnnouncing();
        }
        else {
          // not ok to start new session, reject
          respErr = ErrorPtr(new VdcApiError(503, string_format("this vDC already has an active session with vdSM %s",connectedVdsm.getString().c_str())));
          aRequest->sendError(respErr);
          // close after send
          aRequest->connection()->closeAfterSend();
          // prevent sending error again
          respErr.reset();
        }
      }
    }
  }
  return respErr;
}


ErrorPtr DeviceContainer::byeHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  // always confirm Bye, even out-of-session, so using aJsonRpcComm directly to answer (jsonSessionComm might not be ready)
  aRequest->sendResult(ApiValuePtr());
  // close after send
  aRequest->connection()->closeAfterSend();
  // success
  return ErrorPtr();
}



ErrorPtr DeviceContainer::handleMethodForDsUid(const string &aMethod, VdcApiRequestPtr aRequest, const DsUid &aDsUid, ApiValuePtr aParams)
{
  if (aDsUid==dSUID) {
    // container level method
    return handleMethod(aRequest, aMethod, aParams);
  }
  else {
    // Must be device or deviceClassContainer level method
    // - find device to handle it (more probable case)
    DsDeviceMap::iterator pos = dSDevices.find(aDsUid);
    if (pos!=dSDevices.end()) {
      DevicePtr dev = pos->second;
      // check special case of Remove command - we must execute this because device should not try to remove itself
      if (aMethod=="remove") {
        return removeHandler(aRequest, dev);
      }
      else {
        // let device handle it
        return dev->handleMethod(aRequest, aMethod, aParams);
      }
    }
    else {
      // is not a device, try deviceClassContainer
      ContainerMap::iterator pos = deviceClassContainers.find(aDsUid);
      if (pos!=deviceClassContainers.end()) {
        // found
        return pos->second->handleMethod(aRequest, aMethod, aParams);
      }
      else {
        LOG(LOG_WARNING, "Target entity %s not found for method '%s'\n", aDsUid.getString().c_str(), aMethod.c_str());
        return ErrorPtr(new VdcApiError(404, "unknown dSUID"));
      }
    }
  }
}



void DeviceContainer::handleNotificationForDsUid(const string &aMethod, const DsUid &aDsUid, ApiValuePtr aParams)
{
  if (aDsUid==dSUID) {
    // container level notification
    handleNotification(aMethod, aParams);
  }
  else {
    // Must be device level notification
    // - find device to handle it
    DsDeviceMap::iterator pos = dSDevices.find(aDsUid);
    if (pos!=dSDevices.end()) {
      DevicePtr dev = pos->second;
      dev->handleNotification(aMethod, aParams);
    }
    else {
      // is not a device, try deviceClassContainer
      ContainerMap::iterator pos = deviceClassContainers.find(aDsUid);
      if (pos!=deviceClassContainers.end()) {
        // found
        return pos->second->handleNotification(aMethod, aParams);
      }
      else {
        LOG(LOG_WARNING, "Target entity %s not found for notification '%s'\n", aDsUid.getString().c_str(), aMethod.c_str());
      }
    }
  }
}



#pragma mark - vDC level methods and notifications


ErrorPtr DeviceContainer::removeHandler(VdcApiRequestPtr aRequest, DevicePtr aDevice)
{
  // dS system wants to disconnect this device from this vDC. Try it and report back success or failure
  // Note: as disconnect() removes device from all containers, only aDevice may keep it alive until disconnection is complete
  aDevice->disconnect(true, boost::bind(&DeviceContainer::removeResultHandler, this, aRequest, _1, _2));
  return ErrorPtr();
}


void DeviceContainer::removeResultHandler(VdcApiRequestPtr aRequest, DevicePtr aDevice, bool aDisconnected)
{
  if (aDisconnected)
    aRequest->sendResult(ApiValuePtr()); // disconnected successfully
  else
    aRequest->sendError(ErrorPtr(new VdcApiError(403, "Device cannot be removed, is still connected")));
}






#pragma mark - session management



/// reset announcing devices (next startAnnouncing will restart from beginning)
void DeviceContainer::resetAnnouncing()
{
  // end pending announcement
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // end all device sessions
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    dev->announced = Never;
    dev->announcing = Never;
  }
  // end all vdc sessions
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
    DeviceClassContainerPtr vdc = pos->second;
    vdc->announced = Never;
    vdc->announcing = Never;
  }
}


// how long until a not acknowledged registrations is considered timed out (and next device can be attempted)
#define ANNOUNCE_TIMEOUT (15*Second)

// how long until a not acknowledged announcement for a device is retried again for the same device
#define ANNOUNCE_RETRY_TIMEOUT (300*Second)

// how long vDC waits after receiving ok from one announce until it fires the next
#define ANNOUNCE_PAUSE (1*Second)

/// start announcing all not-yet announced entities to the vdSM
void DeviceContainer::startAnnouncing()
{
  if (!collecting && announcementTicket==0 && activeSessionConnection) {
    announceNext();
  }
}


#warning "no VDC announce for now"
#define HAS_VDCANNOUNCE false

void DeviceContainer::announceNext()
{
  if (collecting) return; // prevent announcements during collect.
  // cancel re-announcing
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // first check for unnannounced device classes
  if (dsUids && HAS_VDCANNOUNCE) {
    // only announce vdcs when using modern dSUIDs
    for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
      DeviceClassContainerPtr vdc = pos->second;
      if (
        vdc->announced==Never &&
        (vdc->announcing==Never || MainLoop::now()>vdc->announcing+ANNOUNCE_RETRY_TIMEOUT)
      ) {
        // mark device as being in process of getting announced
        vdc->announcing = MainLoop::now();
        // call announce method
        if (!vdc->sendRequest("announcevdc", ApiValuePtr(), boost::bind(&DeviceContainer::announceResultHandler, this, vdc, _2, _3, _4))) {
          LOG(LOG_ERR, "Could not send announcement message for %s %s\n", vdc->entityType(), vdc->shortDesc().c_str());
          vdc->announcing = Never; // not registering
        }
        else {
          LOG(LOG_NOTICE, "Sent announcement for %s %s\n", vdc->entityType(), vdc->shortDesc().c_str());
        }
        // schedule a retry
        announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::announceNext, this), ANNOUNCE_TIMEOUT);
        // done for now, continues after ANNOUNCE_TIMEOUT or when registration acknowledged
        return;
      }
    }
  }
  // check all devices for unnannounced ones and announce those
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    if (
      dev->isPublicDS() && // only public ones
      (!dsUids || dev->classContainerP->announced!=Never) && // old dsids don't announce vdcs, with new dSUIDs, class container must have already completed an announcement
      dev->announced==Never &&
      (dev->announcing==Never || MainLoop::now()>dev->announcing+ANNOUNCE_RETRY_TIMEOUT)
    ) {
      // mark device as being in process of getting announced
      dev->announcing = MainLoop::now();
      // call announce method
      ApiValuePtr params = getSessionConnection()->newApiValue();
      params->setType(apivalue_object);
      if (dsUids) {
        // vcds were announced, include link to vdc for device announcements
        params->add("vdcdSUID", params->newString(dev->classContainerP->dSUID.getString()));
      }
      if (!dev->sendRequest("announce", params, boost::bind(&DeviceContainer::announceResultHandler, this, dev, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send announcement message for %s %s\n", dev->entityType(), dev->shortDesc().c_str());
        dev->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent announcement for %s %s\n", dev->entityType(), dev->shortDesc().c_str());
      }
      // schedule a retry
      announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::announceNext, this), ANNOUNCE_TIMEOUT);
      // done for now, continues after ANNOUNCE_TIMEOUT or when registration acknowledged
      return;
    }
  }
}


void DeviceContainer::announceResultHandler(DsAddressablePtr aAddressable, VdcApiRequestPtr aRequest, ErrorPtr &aError, ApiValuePtr aResultOrErrorData)
{
  if (Error::isOK(aError)) {
    // set device announced successfully
    LOG(LOG_NOTICE, "Announcement for %s %s acknowledged by vdSM\n", aAddressable->entityType(), aAddressable->shortDesc().c_str());
    aAddressable->announced = MainLoop::now();
    aAddressable->announcing = Never; // not announcing any more
  }
  // cancel retry timer
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // try next announcement, after a pause
  announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::announceNext, this), ANNOUNCE_PAUSE);
}


#pragma mark - DsAddressable API implementation

ErrorPtr DeviceContainer::handleMethod(VdcApiRequestPtr aRequest,  const string &aMethod, ApiValuePtr aParams)
{
  return inherited::handleMethod(aRequest, aMethod, aParams);
}


void DeviceContainer::handleNotification(const string &aMethod, ApiValuePtr aParams)
{
  inherited::handleNotification(aMethod, aParams);
}



#pragma mark - DsAddressable API implementation


#pragma mark - property access

static char devicecontainer_key;

enum {
  vdcs_key,
  numDeviceContainerProperties
};



int DeviceContainer::numProps(int aDomain)
{
  return inherited::numProps(aDomain)+numDeviceContainerProperties;
}


const PropertyDescriptor *DeviceContainer::getPropertyDescriptor(int aPropIndex, int aDomain)
{
  static const PropertyDescriptor properties[numDeviceContainerProperties] = {
    { "x-p44-vdcs", apivalue_string, true, vdcs_key, &devicecontainer_key }
  };
  int n = inherited::numProps(aDomain);
  if (aPropIndex<n)
    return inherited::getPropertyDescriptor(aPropIndex, aDomain); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return &properties[aPropIndex];
}


bool DeviceContainer::accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  if (aPropertyDescriptor.objectKey==&devicecontainer_key) {
    if (aPropertyDescriptor.accessKey==vdcs_key) {
      if (aIndex==PROP_ARRAY_SIZE) {
        // return size of array
        aPropValue->setUint32Value((uint32_t)deviceClassContainers.size());
        return true;
      }
      else if (aIndex<deviceClassContainers.size()) {
        // return dSUID of contained vdc
        // - just iterate into map, we'll never have more than a few logical vdcs!
        int i = 0;
        for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
          if (i==aIndex) {
            // found
            aPropValue->setStringValue(pos->first.getString());
            return true;
          }
          i++;
        }
      }
      return false;
    }
  }
  return inherited::accessField(aForWrite, aPropValue, aPropertyDescriptor, aIndex);
}





#pragma mark - description

string DeviceContainer::description()
{
  string d = string_format("DeviceContainer with %d device classes:\n", deviceClassContainers.size());
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
    d.append(pos->second->description());
  }
  return d;
}



