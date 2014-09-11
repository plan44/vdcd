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

#include "devicecontainer.hpp"

#include "deviceclasscontainer.hpp"

#include <string.h>

#include "device.hpp"

#include "macaddress.hpp"
#include "fnv.hpp"

// for local behaviour
#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"


// TODO: move scene processing to output?
// TODO: enocean outputs need to have a channel, too - which one? For now: always channel 0
// TODO: review output value updating mechanisms, especially in light of MOC transactions


using namespace p44;


// how long vDC waits after receiving ok from one announce until it fires the next
//#define DEFAULT_ANNOUNCE_PAUSE (100*MilliSecond)
#define DEFAULT_ANNOUNCE_PAUSE (10*MilliSecond)

// how often to write mainloop statistics into log output
#define DEFAULT_MAINLOOP_STATS_INTERVAL (60) // every 5 min (with periodic activity every 5 seconds: 60*5 = 300 = 5min)

// how long until a not acknowledged registrations is considered timed out (and next device can be attempted)
#define ANNOUNCE_TIMEOUT (30*Second)

// how long until a not acknowledged announcement for a device is retried again for the same device
#define ANNOUNCE_RETRY_TIMEOUT (300*Second)


DeviceContainer::DeviceContainer() :
  mac(0),
  DsAddressable(this),
  collecting(false),
  lastActivity(0),
  lastPeriodicRun(0),
  learningMode(false),
  announcementTicket(0),
  periodicTaskTicket(0),
  localDimDirection(0), // undefined
  mainloopStatsInterval(DEFAULT_MAINLOOP_STATS_INTERVAL),
  mainLoopStatsCounter(0),
  announcePause(DEFAULT_ANNOUNCE_PAUSE)
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


string DeviceContainer::ipv4AddressString()
{
  uint32_t ip = ipv4Address();
  string ipStr = string_format("%d.%d.%d.%d", (ip>>24) & 0xFF, (ip>>16) & 0xFF, (ip>>8) & 0xFF, ip & 0xFF);
  return ipStr;
}



void DeviceContainer::deriveDsUid()
{
  if (!externalDsuid) {
    // we don't have a fixed external dSUID to base everything on, so create a dSUID of our own:
    // single vDC per MAC-Adress scenario: generate UUIDv5 with name = macaddress
    // - calculate UUIDv5 based dSUID
    DsUid vdcNamespace(DSUID_VDC_NAMESPACE_UUID);
    dSUID.setNameInSpace(macAddressString(), vdcNamespace);
  }
}


void DeviceContainer::setIdMode(DsUidPtr aExternalDsUid)
{
  if (aExternalDsUid) {
    externalDsuid = true;
    dSUID = *aExternalDsUid;
  }
  deriveDsUid(); // derive my dSUID now (again), if necessary
}



void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers[aDeviceClassContainerPtr->getApiDsUid()] = aDeviceClassContainerPtr;
}



void DeviceContainer::setIconDir(const char *aIconDir)
{
	iconDir = nonNullCStr(aIconDir);
	if (!iconDir.empty() && iconDir[iconDir.length()-1]!='/') {
		iconDir.append("/");
	}
}


const char *DeviceContainer::getIconDir()
{
	return iconDir.c_str();
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
  DeviceContainer &deviceContainer;
  bool factoryReset;
public:
  static void initialize(DeviceContainer &aDeviceContainer, CompletedCB aCallback, bool aFactoryReset)
  {
    // create new instance, deletes itself when finished
    new DeviceClassInitializer(aDeviceContainer, aCallback, aFactoryReset);
  };
private:
  DeviceClassInitializer(DeviceContainer &aDeviceContainer, CompletedCB aCallback, bool aFactoryReset) :
		callback(aCallback),
		deviceContainer(aDeviceContainer),
    factoryReset(aFactoryReset)
  {
    nextContainer = deviceContainer.deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }


  void queryNextContainer(ErrorPtr aError)
  {
    if ((!aError || factoryReset) && nextContainer!=deviceContainer.deviceClassContainers.end())
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
  LOG(LOG_NOTICE,"\n****** starting vdcd (vdc host) initialisation, MAC: %s, dSUID (%s) = %s, IP = %s\n", macAddressString().c_str(), externalDsuid ? "external" : "MAC-derived", shortDesc().c_str(), ipv4AddressString().c_str());
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
  DeviceClassInitializer::initialize(*this, aCompletedCB, aFactoryReset);
}


void DeviceContainer::startRunning()
{
  // start periodic tasks needed during normal running like announcement checking and saving parameters
  MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::periodicTask, deviceContainerP, _1), 1*Second);
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
    if (!aError && nextContainer!=deviceContainerP->deviceClassContainers.end()) {
      DeviceClassContainerPtr vdc = nextContainer->second;
      LOG(LOG_NOTICE,
        "=== collecting devices from vdc %s #%d with dSUID = %s\n",
        vdc->deviceClassIdentifier(),
        vdc->getInstanceNumber(),
        vdc->getApiDsUid().getString().c_str() // as seen in the API
      );
      nextContainer->second->collectDevices(boost::bind(&DeviceClassCollector::containerQueried, this, _1), incremental, exhaustive);
    }
    else
      collectedAll(aError);
  }

  void containerQueried(ErrorPtr aError)
  {
    // load persistent params
    nextContainer->second->load();
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
    LOG(LOG_NOTICE, "--- initialized device: %s",nextDevice->second->description().c_str());
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
  DsDeviceMap::iterator pos = dSDevices.find(aDevice->getApiDsUid());
  if (pos!=dSDevices.end()) {
    LOG(LOG_INFO, "- device %s already registered, not added again\n",aDevice->shortDesc().c_str());
    return false; // duplicate dSUID, not added
  }
  // set for given dSUID in the container-wide map of devices
  dSDevices[aDevice->getApiDsUid()] = aDevice;
  LOG(LOG_NOTICE,"--- added device: %s (not yet initialized)\n",aDevice->shortDesc().c_str());
  // load the device's persistent params
  aDevice->load();
  // if not collecting, initialize device right away.
  // Otherwise, initialisation will be done when collecting is complete
  if (!collecting) {
    aDevice->initializeDevice(boost::bind(&DeviceContainer::deviceInitialized, this, aDevice), false);
  }
  return true;
}

void DeviceContainer::deviceInitialized(DevicePtr aDevice)
{
  LOG(LOG_NOTICE, "--- initialized device: %s",aDevice->description().c_str());
  // trigger announcing when initialized (no problem when called while already announcing)
  startAnnouncing();
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
  dSDevices.erase(aDevice->getApiDsUid());
  LOG(LOG_NOTICE,"--- removed device: %s\n", aDevice->shortDesc().c_str());
}



void DeviceContainer::startLearning(LearnCB aLearnHandler, bool aDisableProximityCheck)
{
  // enable learning in all class containers
  learnHandler = aLearnHandler;
  learningMode = true;
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos != deviceClassContainers.end(); ++pos) {
    pos->second->setLearnMode(true, aDisableProximityCheck);
  }
  LOG(LOG_NOTICE,"=== started learning%s\n", aDisableProximityCheck ? " with proximity check disabled" : "");
}


void DeviceContainer::stopLearning()
{
  // disable learning in all class containers
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos != deviceClassContainers.end(); ++pos) {
    pos->second->setLearnMode(false, false);
  }
  LOG(LOG_NOTICE,"=== stopped learning\n");
  learningMode = false;
  learnHandler.clear();
}


void DeviceContainer::reportLearnEvent(bool aLearnIn, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aLearnIn) {
      LOG(LOG_NOTICE,"--- learned in (paired) new device(s)\n");
    }
    else {
      LOG(LOG_NOTICE,"--- learned out (unpaired) device(s)\n");
    }
  }
  // report status
  if (learnHandler) {
    learnHandler(aLearnIn, aError);
  }
}






#pragma mark - activity monitoring


void DeviceContainer::setActivityMonitor(DoneCB aActivityCB)
{
  activityHandler = aActivityCB;
}


void DeviceContainer::signalActivity()
{
  lastActivity = MainLoop::now();
  if (activityHandler) {
    activityHandler();
  }
}



void DeviceContainer::setUserActionMonitor(DeviceUserActionCB aUserActionCB)
{
  deviceUserActionHandler = aUserActionCB;
}


bool DeviceContainer::signalDeviceUserAction(Device &aDevice, bool aRegular)
{
  LOG(LOG_INFO,"--- device %s reports %s user action\n", aDevice.shortDesc().c_str(), aRegular ? "regular" : "identification");
  if (deviceUserActionHandler) {
    deviceUserActionHandler(DevicePtr(&aDevice), aRegular);
    return true; // suppress normal action
  }
  if (!aRegular) {
    // this is a non-regular user action, i.e. one for identification purposes. Generate special identification notification
    VdcApiConnectionPtr api = getSessionConnection();
    if (api) {
      // send an identify notification
      aDevice.sendRequest("identify", ApiValuePtr(), NULL);
    }
    return true; // no normal action, prevent further processing
  }
  return false; // normal processing
}




#pragma mark - periodic activity


#define PERIODIC_TASK_INTERVAL (5*Second)
#define PERIODIC_TASK_FORCE_INTERVAL (1*Minute)

#define ACTIVITY_PAUSE_INTERVAL (1*Second)

void DeviceContainer::periodicTask(MLMicroSeconds aCycleStartTime)
{
  // cancel any pending executions
  MainLoop::currentMainLoop().cancelExecutionTicket(periodicTaskTicket);
  // prevent during activity as saving DB might affect performance
  if (
    (aCycleStartTime>lastActivity+ACTIVITY_PAUSE_INTERVAL) || // some time passed after last activity or...
    (aCycleStartTime>lastPeriodicRun+PERIODIC_TASK_FORCE_INTERVAL) // ...too much time passed since last run
  ) {
    lastPeriodicRun = aCycleStartTime;
    if (!collecting) {
      // check again for devices that need to be announced
      startAnnouncing();
      // do a save run as well
      // - device containers
      for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
        pos->second->save();
      }
      // - devices
      for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
        pos->second->save();
      }
    }
  }
  if (mainloopStatsInterval>0) {
    // show mainloop statistics
    if (mainLoopStatsCounter<=0) {
      LOG(LOG_INFO, "%s", MainLoop::currentMainLoop().description().c_str());
      MainLoop::currentMainLoop().statistics_reset();
      mainLoopStatsCounter = mainloopStatsInterval;
    }
    else {
      --mainLoopStatsCounter;
    }
  }
  // schedule next run
  periodicTaskTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::periodicTask, this, _1), PERIODIC_TASK_INTERVAL);
}


#pragma mark - local operation mode


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
  // if button has up/down, direction is derived from button
  int newDirection = aButtonBehaviour.localFunctionElement()==buttonElement_up ? 1 : (aButtonBehaviour.localFunctionElement()==buttonElement_down ? -1 : 0); // -1=down/off, 1=up/on, 0=toggle
  if (newDirection!=0)
    localDimDirection = newDirection;
  switch (aClickType) {
    case ct_tip_1x:
    case ct_click_1x:
      scene = T0_S1;
      // toggle direction if click has none
      if (newDirection==0)
        localDimDirection *= -1; // reverse if already determined
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
      scene = INC_S; // just as a marker to start dimming (we'll use dimChannelForArea(), not legacy dimming!)
      // toggle direction if click has none
      if (newDirection==0)
        localDimDirection *= -1; // reverse if already determined
      break;
    case ct_hold_end:
      scene = STOP_S; // just as a marker to stop dimming (we'll use dimChannelForArea(), not legacy dimming!)
      break;
    default:
      break;
  }
  if (scene>=0) {
    DsChannelType channeltype = channeltype_brightness; // default to brightness
    if (aButtonBehaviour.buttonChannel!=channeltype_default) {
      channeltype = aButtonBehaviour.buttonChannel;
    }
    signalActivity(); // local activity
    // some action to perform on every light device
    for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
      DevicePtr dev = pos->second;
      if (scene==STOP_S) {
        // stop dimming
        dev->dimChannelForArea(channeltype, dimmode_stop, 0, 0);
      }
      else {
        // call scene or start dimming
        LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(dev->output);
        if (l) {
          // - figure out direction if not already known
          if (localDimDirection==0 && l->brightness->getLastSync()!=Never) {
            // get initial direction from current value of first encountered light with synchronized brightness value
            localDimDirection = l->brightness->getChannelValue() >= l->brightness->getMinDim() ? -1 : 1;
          }
          if (scene==INC_S) {
            // Start dimming
            // - minimum scene if not already there
            if (localDimDirection>0 && l->brightness->getChannelValue()==0) {
              // starting dimming up from 0, first call MIN_S
              dev->callScene(MIN_S, true);
            }
            // now dim (safety timeout after 10 seconds)
            dev->dimChannelForArea(channeltype, localDimDirection>0 ? dimmode_up : dimmode_down, 0, 10*Second);
          }
          else {
            // call a scene
            if (localDimDirection<0)
              scene = T0_S0; // switching off a scene = call off scene
            dev->callScene(scene, true);
          }
        }
      }
    }
  }
}



#pragma mark - vDC API


#define SESSION_TIMEOUT (3*Minute) // 3 minutes



bool DeviceContainer::sendApiRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
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
        DsUid dsuid;
        if (Error::isOK(respErr = checkDsuidParam(aParams, "dSUID", dsuid))) {
          // operation method
          respErr = handleMethodForDsUid(aMethod, aRequest, dsuid, aParams);
        }
      }
    }
  }
  else {
    // Notifications
    // Note: out of session, notifications are simply ignored
    if (activeSessionConnection) {
      // Notifications can be adressed to one or multiple dSUIDs
      // Notes
      // - for protobuf API, dSUID is always an array (as it is a repeated field in protobuf)
      // - for JSON API, caller may provide an array or a single dSUID.
      ApiValuePtr o;
      respErr = checkParam(aParams, "dSUID", o);
      if (Error::isOK(respErr)) {
        DsUid dsuid;
        // can be single dSUID or array of dSUIDs
        if (o->isType(apivalue_array)) {
          // array of dSUIDs
          for (int i=0; i<o->arrayLength(); i++) {
            ApiValuePtr e = o->arrayGet(i);
            dsuid.setAsBinary(e->binaryValue());
            handleNotificationForDsUid(aMethod, dsuid, aParams);
          }
        }
        else {
          // single dSUID
          dsuid.setAsBinary(o->binaryValue());
          handleNotificationForDsUid(aMethod, dsuid, aParams);
        }
      }
    }
    else {
      LOG(LOG_DEBUG,"Received notification '%s' out of session -> ignored\n", aMethod.c_str());
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


/// vDC API version
/// 1 (aka 1.0 in JSON) : first version, used in P44-DSB-DEH versions up to 0.5.0.x
/// 2 : cleanup, no official JSON support any more, added MOC extensions
#define VDC_API_VERSION 2

ErrorPtr DeviceContainer::helloHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  ApiValuePtr v;
  string s;
  // check API version
  if (Error::isOK(respErr = checkParam(aParams, "api_version", v))) {
    if (v->int32Value()!=VDC_API_VERSION)
      respErr = ErrorPtr(new VdcApiError(505, string_format("Incompatible vDC API version - found %d, expected %d", v->int32Value(), VDC_API_VERSION)));
    else {
      // API version ok, check dSUID
      DsUid vdsmDsUid;
      if (Error::isOK(respErr = checkDsuidParam(aParams, "dSUID", vdsmDsUid))) {
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
          result->add("dSUID", aParams->newBinary(getApiDsUid().getBinary()));
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



DsAddressablePtr DeviceContainer::addressableForParams(const DsUid &aDsUid, ApiValuePtr aParams)
{
  if (aDsUid.empty()) {
    // not addressing by dSUID, check for alternative addressing methods
    ApiValuePtr o = aParams->get("x-p44-itemSpec");
    if (o) {
      string query = o->stringValue();
      if(query.find("vdc:")==0) {
        // starts with "vdc:" -> look for vdc by class identifier and instance no
        query.erase(0, 4); // remove "vdc:" prefix
        // ccccccc[:ii] cccc=deviceClassIdentifier(), ii=instance
        size_t i=query.find(':');
        int instanceNo = 1; // default to first instance
        if (i!=string::npos) {
          // with instance number
          instanceNo = atoi(query.c_str()+i+1);
          query.erase(i); // cut off :iii part
        }
        for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
          DeviceClassContainerPtr c = pos->second;
          if (
            strcmp(c->deviceClassIdentifier(), query.c_str())==0 &&
            c->getInstanceNumber()==instanceNo
          ) {
            // found - return this device class container
            return c;
          }
        }
      }
      // x-p44-query specified, but nothing found
      return DsAddressablePtr();
    }
    // empty dSUID but no special query: default to vdc-host itself (root object)
    return DsAddressablePtr(this);
  }
  // not special query, not empty dSUID
  if (aDsUid==getApiDsUid()) {
    // my own dSUID: vdc-host is addressed
    return DsAddressablePtr(this);
  }
  else {
    // Must be device or deviceClassContainer level method
    // - find device to handle it (more probable case)
    DsDeviceMap::iterator pos = dSDevices.find(aDsUid);
    if (pos!=dSDevices.end()) {
      return pos->second;
    }
    else {
      // is not a device, try deviceClassContainer
      ContainerMap::iterator pos = deviceClassContainers.find(aDsUid);
      if (pos!=deviceClassContainers.end()) {
        return pos->second;
      }
    }
  }
  // not found
  return DsAddressablePtr();
}



ErrorPtr DeviceContainer::handleMethodForDsUid(const string &aMethod, VdcApiRequestPtr aRequest, const DsUid &aDsUid, ApiValuePtr aParams)
{
  DsAddressablePtr addressable = addressableForParams(aDsUid, aParams);
  if (addressable) {
    // check special case of device remove command - we must execute this because device should not try to remove itself
    DevicePtr dev = boost::dynamic_pointer_cast<Device>(addressable);
    if (dev && aMethod=="remove") {
      return removeHandler(aRequest, dev);
    }
    // normal addressable, just let it handle the method
    return addressable->handleMethod(aRequest, aMethod, aParams);
  }
  else {
    LOG(LOG_WARNING, "Target entity %s not found for method '%s'\n", aDsUid.getString().c_str(), aMethod.c_str());
    return ErrorPtr(new VdcApiError(404, "unknown dSUID"));
  }
}



void DeviceContainer::handleNotificationForDsUid(const string &aMethod, const DsUid &aDsUid, ApiValuePtr aParams)
{
  DsAddressablePtr addressable = addressableForParams(aDsUid, aParams);
  if (addressable) {
    addressable->handleNotification(aMethod, aParams);
  }
  else {
    LOG(LOG_WARNING, "Target entity %s not found for notification '%s'\n", aDsUid.getString().c_str(), aMethod.c_str());
  }
}



#pragma mark - vDC level methods and notifications


ErrorPtr DeviceContainer::removeHandler(VdcApiRequestPtr aRequest, DevicePtr aDevice)
{
  // dS system wants to disconnect this device from this vDC. Try it and report back success or failure
  // Note: as disconnect() removes device from all containers, only aDevice may keep it alive until disconnection is complete
  aDevice->disconnect(true, boost::bind(&DeviceContainer::removeResultHandler, this, aRequest, _1));
  return ErrorPtr();
}


void DeviceContainer::removeResultHandler(VdcApiRequestPtr aRequest, bool aDisconnected)
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



/// start announcing all not-yet announced entities to the vdSM
void DeviceContainer::startAnnouncing()
{
  if (!collecting && announcementTicket==0 && activeSessionConnection) {
    announceNext();
  }
}


void DeviceContainer::announceNext()
{
  if (collecting) return; // prevent announcements during collect.
  // cancel re-announcing
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // announce vdcs first
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
    DeviceClassContainerPtr vdc = pos->second;
    if (
      vdc->announced==Never &&
      (vdc->announcing==Never || MainLoop::now()>vdc->announcing+ANNOUNCE_RETRY_TIMEOUT)
    ) {
      // mark device as being in process of getting announced
      vdc->announcing = MainLoop::now();
      // call announcevdc method (need to construct here, because dSUID must be sent as vdcdSUID)
      ApiValuePtr params = getSessionConnection()->newApiValue();
      params->setType(apivalue_object);
      params->add("dSUID", params->newBinary(vdc->getApiDsUid().getBinary()));
      if (!sendApiRequest("announcevdc", params, boost::bind(&DeviceContainer::announceResultHandler, this, vdc, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send vdc announcement message for %s %s\n", vdc->entityType(), vdc->shortDesc().c_str());
        vdc->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent vdc announcement for %s %s\n", vdc->entityType(), vdc->shortDesc().c_str());
      }
      // schedule a retry
      announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::announceNext, this), ANNOUNCE_TIMEOUT);
      // done for now, continues after ANNOUNCE_TIMEOUT or when registration acknowledged
      return;
    }
  }
  // check all devices for unnannounced ones and announce those
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    if (
      dev->isPublicDS() && // only public ones
      (dev->classContainerP->announced!=Never) && // class container must have already completed an announcement
      dev->announced==Never &&
      (dev->announcing==Never || MainLoop::now()>dev->announcing+ANNOUNCE_RETRY_TIMEOUT)
    ) {
      // mark device as being in process of getting announced
      dev->announcing = MainLoop::now();
      // call announce method
      ApiValuePtr params = getSessionConnection()->newApiValue();
      params->setType(apivalue_object);
      // include link to vdc for device announcements
      params->add("vdc_dSUID", params->newBinary(dev->classContainerP->getApiDsUid().getBinary()));
      if (!dev->sendRequest("announcedevice", params, boost::bind(&DeviceContainer::announceResultHandler, this, dev, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send device announcement message for %s %s\n", dev->entityType(), dev->shortDesc().c_str());
        dev->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent device announcement for %s %s\n", dev->entityType(), dev->shortDesc().c_str());
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
  announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DeviceContainer::announceNext, this), announcePause);
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
static char vdc_container_key;
static char vdc_key;

enum {
  vdcs_key,
  webui_url_key,
  numDeviceContainerProperties
};



int DeviceContainer::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->hasObjectKey(vdc_container_key)) {
    return (int)deviceClassContainers.size();
  }
  return inherited::numProps(aDomain, aParentDescriptor)+numDeviceContainerProperties;
}


// note: is only called when getDescriptorByName does not resolve the name
PropertyDescriptorPtr DeviceContainer::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numDeviceContainerProperties] = {
    { "x-p44-vdcs", apivalue_object+propflag_container, vdcs_key, OKEY(vdc_container_key) },
    { "configURL", apivalue_string, webui_url_key, OKEY(devicecontainer_key) }
  };
  int n = inherited::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


PropertyDescriptorPtr DeviceContainer::getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor && aParentDescriptor->hasObjectKey(vdc_container_key)) {
    // accessing one of the vdcs by numeric index
    return getDescriptorByNumericName(
      aPropMatch, aStartIndex, aDomain, aParentDescriptor,
      OKEY(vdc_key)
    );
  }
  // None of the containers within Device - let base class handle Device-Level properties
  return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aParentDescriptor);
}


PropertyContainerPtr DeviceContainer::getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  if (aPropertyDescriptor->isArrayContainer()) {
    // local container
    return PropertyContainerPtr(this); // handle myself
  }
  else if (aPropertyDescriptor->hasObjectKey(vdc_key)) {
    // - just iterate into map, we'll never have more than a few logical vdcs!
    int i = 0;
    for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
      if (i==aPropertyDescriptor->fieldKey()) {
        // found
        aPropertyDescriptor.reset(); // next level is "root" again (is a DsAddressable)
        return pos->second;
      }
      i++;
    }
  }
  // unknown here
  return NULL;
}


bool DeviceContainer::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(devicecontainer_key)) {
    if (aMode==access_read) {
      switch (aPropertyDescriptor->fieldKey()) {
        case webui_url_key:
          aPropValue->setStringValue(webuiURLString());
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
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



