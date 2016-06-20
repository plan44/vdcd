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

#include "devicecontainer.hpp"

#include "deviceclasscontainer.hpp"

#include <string.h>

#include "device.hpp"

#include "macaddress.hpp"

// for local behaviour
#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"


// TODO: move scene processing to output?
// TODO: enocean outputs need to have a channel, too - which one? For now: always channel 0
// TODO: review output value updating mechanisms, especially in light of MOC transactions


using namespace p44;

// how often to write mainloop statistics into log output
#define DEFAULT_MAINLOOP_STATS_INTERVAL (60) // every 5 min (with periodic activity every 5 seconds: 60*5 = 300 = 5min)

// how long vDC waits after receiving ok from one announce until it fires the next
#define ANNOUNCE_PAUSE (10*MilliSecond)

// how long until a not acknowledged registrations is considered timed out (and next device can be attempted)
#define ANNOUNCE_TIMEOUT (30*Second)

// how long until a not acknowledged announcement for a device is retried again for the same device
#define ANNOUNCE_RETRY_TIMEOUT (300*Second)

// default product name
#define DEFAULT_PRODUCT_NAME "plan44.ch vdcd"

// default description template
#define DEFAULT_DESCRIPTION_TEMPLATE "%V %M%N #%S"


DeviceContainer::DeviceContainer() :
  inheritedParams(dsParamStore),
  mac(0),
  externalDsuid(false),
  storedDsuid(false),
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
  productName(DEFAULT_PRODUCT_NAME)
{
  // obtain MAC address
  mac = macAddress();
}


void DeviceContainer::setEventMonitor(VdchostEventCB aEventCB)
{
  eventMonitorHandler = aEventCB;
}



void DeviceContainer::setName(const string &aName)
{
  if (aName!=getAssignedName()) {
    // has changed
    inherited::setName(aName);
    // make sure it will be saved
    markDirty();
    // is a global event - might need re-advertising services
    if (eventMonitorHandler) {
      eventMonitorHandler(vdchost_descriptionchanged);
    }
  }
}



string DeviceContainer::macAddressString()
{
  string macStr;
  if (mac!=0) {
    for (int i=0; i<6; ++i) {
      string_format_append(macStr, "%02llX",(mac>>((5-i)*8)) & 0xFF);
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



void DeviceContainer::setIdMode(DsUidPtr aExternalDsUid)
{
  if (aExternalDsUid) {
    externalDsuid = true;
    dSUID = *aExternalDsUid;
  }
}



void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers[aDeviceClassContainerPtr->getDsUid()] = aDeviceClassContainerPtr;
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



string DeviceContainer::publishedDescription()
{
  // derive the descriptive name
  // "%V %M%N %S"
  string n = descriptionTemplate;
  if (n.empty()) n = DEFAULT_DESCRIPTION_TEMPLATE;
  string s;
  size_t i;
  // Vendor
  while ((i = n.find("%V"))!=string::npos) { n.replace(i, 2, vendorName()); }
  // Model
  while ((i = n.find("%M"))!=string::npos) { n.replace(i, 2, modelName()); }
  // (optional) Name
  s = getName();
  if (!s.empty()) {
    s = " \""+s+"\"";
  }
  while ((i = n.find("%N"))!=string::npos) { n.replace(i, 2, s); }
  // Serial/hardware ID
  s = getDeviceHardwareId();
  if (s.empty()) {
    // use dSUID if no other ID is specified
    s = getDsUid().getString();
  }
  while ((i = n.find("%S"))!=string::npos) { n.replace(i, 2, s); }
  return n;
}



#pragma mark - initializisation of DB and containers


class DeviceClassInitializer
{
  StatusCB callback;
  ContainerMap::iterator nextContainer;
  DeviceContainer &deviceContainer;
  bool factoryReset;
public:
  static void initialize(DeviceContainer &aDeviceContainer, StatusCB aCallback, bool aFactoryReset)
  {
    // create new instance, deletes itself when finished
    new DeviceClassInitializer(aDeviceContainer, aCallback, aFactoryReset);
  };
private:
  DeviceClassInitializer(DeviceContainer &aDeviceContainer, StatusCB aCallback, bool aFactoryReset) :
		callback(aCallback),
		deviceContainer(aDeviceContainer),
    factoryReset(aFactoryReset)
  {
    nextContainer = deviceContainer.deviceClassContainers.begin();
    initNextContainer(ErrorPtr());
  }


  void initNextContainer(ErrorPtr aError)
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
    initNextContainer(aError);
  }

  void completed(ErrorPtr aError)
  {
    // callback
    callback(aError);
    // done, delete myself
    delete this;
  }

};


// Version history
//  1 : alpha/beta phase DB
//  2 : no schema change, but forced re-creation due to changed scale of brightness (0..100 now, was 0..255 before)
//  3 : no schema change, but forced re-creation due to bug in storing output behaviour settings
#define DSPARAMS_SCHEMA_MIN_VERSION 3 // minimally supported version, anything older will be deleted
#define DSPARAMS_SCHEMA_VERSION 3 // current version

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



void DeviceContainer::prepareForDeviceClasses(bool aFactoryReset)
{
  // initialize dsParamsDB database
  string databaseName = getPersistentDataDir();
  string_format_append(databaseName, "DsParams.sqlite3");
  ErrorPtr error = dsParamStore.connectAndInitialize(databaseName.c_str(), DSPARAMS_SCHEMA_VERSION, DSPARAMS_SCHEMA_MIN_VERSION, aFactoryReset);
  // load the vdc host settings and determine the dSUID (external > stored > mac-derived)
  loadAndFixDsUID();
}


void DeviceContainer::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  // Log start message
  LOG(LOG_NOTICE,
    "\n\n\n*** starting initialisation of vcd host '%s'\n*** dSUID (%s) = %s, MAC: %s, IP = %s\n",
    publishedDescription().c_str(),
    externalDsuid ? "external" : "MAC-derived",
    shortDesc().c_str(),
    macAddressString().c_str(),
    ipv4AddressString().c_str()
  );
  // start the API server
  if (vdcApiServer) {
    vdcApiServer->setConnectionStatusHandler(boost::bind(&DeviceContainer::vdcApiConnectionStatusHandler, this, _1, _2));
    vdcApiServer->start();
  }
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
  StatusCB callback;
  bool exhaustive;
  bool incremental;
  bool clear;
  ContainerMap::iterator nextContainer;
  DeviceContainer *deviceContainerP;
  DsDeviceMap::iterator nextDevice;
public:
  static void collectDevices(DeviceContainer *aDeviceContainerP, StatusCB aCallback, bool aIncremental, bool aExhaustive, bool aClearSettings)
  {
    // create new instance, deletes itself when finished
    new DeviceClassCollector(aDeviceContainerP, aCallback, aIncremental, aExhaustive, aClearSettings);
  };
private:
  DeviceClassCollector(DeviceContainer *aDeviceContainerP, StatusCB aCallback, bool aIncremental, bool aExhaustive, bool aClearSettings) :
    callback(aCallback),
    deviceContainerP(aDeviceContainerP),
    incremental(aIncremental),
    exhaustive(aExhaustive),
    clear(aClearSettings)
  {
    nextContainer = deviceContainerP->deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }


  void queryNextContainer(ErrorPtr aError)
  {
    if (!aError && nextContainer!=deviceContainerP->deviceClassContainers.end()) {
      DeviceClassContainerPtr vdc = nextContainer->second;
      LOG(LOG_NOTICE,
        "=== collecting devices from vdc %s (%s #%d)",
        vdc->shortDesc().c_str(),
        vdc->deviceClassIdentifier(),
        vdc->getInstanceNumber()
      );
      nextContainer->second->collectDevices(boost::bind(&DeviceClassCollector::containerQueried, this, _1), incremental, exhaustive, clear);
    }
    else
      collectedAll(aError);
  }

  void containerQueried(ErrorPtr aError)
  {
    // load persistent params
    nextContainer->second->load();
    LOG(LOG_NOTICE, "=== done collecting from %s\n", nextContainer->second->shortDesc().c_str());
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



void DeviceContainer::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  if (!collecting) {
    collecting = true;
    if (!aIncremental) {
      // only for non-incremental collect, close vdsm connection
      if (activeSessionConnection) {
        LOG(LOG_NOTICE, "requested to re-collect devices -> closing vDC API connection");
        activeSessionConnection->closeConnection(); // close the API connection
        resetAnnouncing();
        activeSessionConnection.reset(); // forget connection
      }
      dSDevices.clear(); // forget existing ones
    }
    DeviceClassCollector::collectDevices(this, aCompletedCB, aIncremental, aExhaustive, aClearSettings);
  }
}

} // namespace




#pragma mark - adding/removing devices


// add a new device, replaces possibly existing one based on dSUID
bool DeviceContainer::addDevice(DevicePtr aDevice)
{
  if (!aDevice)
    return false; // no device, nothing added
  // check if device with same dSUID already exists
  DsDeviceMap::iterator pos = dSDevices.find(aDevice->getDsUid());
  if (pos!=dSDevices.end()) {
    LOG(LOG_INFO, "- device %s already registered, not added again",aDevice->shortDesc().c_str());
    return false; // duplicate dSUID, not added
  }
  // set for given dSUID in the container-wide map of devices
  dSDevices[aDevice->getDsUid()] = aDevice;
  LOG(LOG_NOTICE, "--- added device: %s (not yet initialized)",aDevice->shortDesc().c_str());
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
  dSDevices.erase(aDevice->getDsUid());
  LOG(LOG_NOTICE, "--- removed device: %s", aDevice->shortDesc().c_str());
}



void DeviceContainer::startLearning(LearnCB aLearnHandler, bool aDisableProximityCheck)
{
  // enable learning in all class containers
  learnHandler = aLearnHandler;
  learningMode = true;
  LOG(LOG_NOTICE, "=== start learning%s", aDisableProximityCheck ? " with proximity check disabled" : "");
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos != deviceClassContainers.end(); ++pos) {
    pos->second->setLearnMode(true, aDisableProximityCheck);
  }
}


void DeviceContainer::stopLearning()
{
  // disable learning in all class containers
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos != deviceClassContainers.end(); ++pos) {
    pos->second->setLearnMode(false, false);
  }
  LOG(LOG_NOTICE, "=== stopped learning");
  learningMode = false;
  learnHandler.clear();
}


void DeviceContainer::reportLearnEvent(bool aLearnIn, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aLearnIn) {
      LOG(LOG_NOTICE, "--- learned in (paired) new device(s)");
    }
    else {
      LOG(LOG_NOTICE, "--- learned out (unpaired) device(s)");
    }
  }
  // report status
  if (learnHandler) {
    learnHandler(aLearnIn, aError);
  }
}






#pragma mark - activity monitoring


void DeviceContainer::signalActivity()
{
  lastActivity = MainLoop::now();
  if (eventMonitorHandler) {
    eventMonitorHandler(vdchost_activitysignal);
  }
}



void DeviceContainer::setUserActionMonitor(DeviceUserActionCB aUserActionCB)
{
  deviceUserActionHandler = aUserActionCB;
}


bool DeviceContainer::signalDeviceUserAction(Device &aDevice, bool aRegular)
{
  LOG(LOG_INFO, "vdSD %s: reports %s user action", aDevice.shortDesc().c_str(), aRegular ? "regular" : "identification");
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
      // - myself
      save();
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
      scene = ROOM_ON;
      // toggle direction if click has none
      if (newDirection==0)
        localDimDirection *= -1; // reverse if already determined
      break;
    case ct_tip_2x:
    case ct_click_2x:
      scene = PRESET_2;
      break;
    case ct_tip_3x:
    case ct_click_3x:
      scene = PRESET_3;
      break;
    case ct_tip_4x:
      scene = PRESET_4;
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
              // starting dimming up from minimum
              l->brightness->setChannelValue(l->brightness->getMinDim(), 0, true);
            }
            // now dim (safety timeout after 10 seconds)
            dev->dimChannelForArea(channeltype, localDimDirection>0 ? dimmode_up : dimmode_down, 0, 10*Second);
          }
          else {
            // call a scene
            if (localDimDirection<0)
              scene = ROOM_OFF; // switching off a scene = call off scene
            dev->callScene(scene, true);
          }
        }
      }
    }
  }
}



#pragma mark - vDC API


bool DeviceContainer::sendApiRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  if (activeSessionConnection) {
    signalActivity();
    return Error::isOK(activeSessionConnection->sendRequest(aMethod, aParams, aResponseHandler));
  }
  // cannot send
  return false;
}


void DeviceContainer::vdcApiConnectionStatusHandler(VdcApiConnectionPtr aApiConnection, ErrorPtr &aError)
{
  if (Error::isOK(aError)) {
    // new connection, set up reequest handler
    aApiConnection->setRequestHandler(boost::bind(&DeviceContainer::vdcApiRequestHandler, this, _1, _2, _3, _4));
  }
  else {
    // error or connection closed
    LOG(LOG_ERR, "vDC API connection closing, reason: %s", aError->description().c_str());
    // - close if not already closed
    aApiConnection->closeConnection();
    if (aApiConnection==activeSessionConnection) {
      // this is the active session connection
      resetAnnouncing(); // stop possibly ongoing announcing
      activeSessionConnection.reset();
      LOG(LOG_NOTICE, "vDC API session ends because connection closed ");
    }
    else {
      LOG(LOG_NOTICE, "vDC API connection (not yet in session) closed ");
    }
  }
}


void DeviceContainer::vdcApiRequestHandler(VdcApiConnectionPtr aApiConnection, VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  signalActivity();
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
      LOG(LOG_DEBUG, "Received notification '%s' out of session -> ignored", aMethod.c_str());
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
      LOG(LOG_WARNING, "Notification '%s' processing error: %s", aMethod.c_str(), respErr->description().c_str());
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
          result->add("dSUID", aParams->newBinary(getDsUid().getBinary()));
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
  if (aDsUid==getDsUid()) {
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
    // normal addressable or not remove -> just let addressable handle the method itself
    return addressable->handleMethod(aRequest, aMethod, aParams);
  }
  else {
    LOG(LOG_WARNING, "Target entity %s not found for method '%s'", aDsUid.getString().c_str(), aMethod.c_str());
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
    LOG(LOG_WARNING, "Target entity %s not found for notification '%s'", aDsUid.getString().c_str(), aMethod.c_str());
  }
}



#pragma mark - vDC level methods and notifications


ErrorPtr DeviceContainer::removeHandler(VdcApiRequestPtr aRequest, DevicePtr aDevice)
{
  // dS system wants to disconnect this device from this vDC. Try it and report back success or failure
  // Note: as disconnect() removes device from all containers, only aDevice may keep it alive until disconnection is complete.
  //   That's why we are passing aDevice to the handler, so we can be certain the device lives long enough
  aDevice->disconnect(true, boost::bind(&DeviceContainer::removeResultHandler, this, aDevice, aRequest, _1));
  return ErrorPtr();
}


void DeviceContainer::removeResultHandler(DevicePtr aDevice, VdcApiRequestPtr aRequest, bool aDisconnected)
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
      (vdc->announcing==Never || MainLoop::now()>vdc->announcing+ANNOUNCE_RETRY_TIMEOUT) &&
      (!vdc->invisibleWhenEmpty() || vdc->getNumberOfDevices()>0)
    ) {
      // mark device as being in process of getting announced
      vdc->announcing = MainLoop::now();
      // call announcevdc method (need to construct here, because dSUID must be sent as vdcdSUID)
      ApiValuePtr params = getSessionConnection()->newApiValue();
      params->setType(apivalue_object);
      params->add("dSUID", params->newBinary(vdc->getDsUid().getBinary()));
      if (!sendApiRequest("announcevdc", params, boost::bind(&DeviceContainer::announceResultHandler, this, vdc, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send vdc announcement message for %s %s", vdc->entityType(), vdc->shortDesc().c_str());
        vdc->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent vdc announcement for %s %s", vdc->entityType(), vdc->shortDesc().c_str());
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
      params->add("vdc_dSUID", params->newBinary(dev->classContainerP->getDsUid().getBinary()));
      if (!dev->sendRequest("announcedevice", params, boost::bind(&DeviceContainer::announceResultHandler, this, dev, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send device announcement message for %s %s", dev->entityType(), dev->shortDesc().c_str());
        dev->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent device announcement for %s %s", dev->entityType(), dev->shortDesc().c_str());
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
    LOG(LOG_NOTICE, "Announcement for %s %s acknowledged by vdSM", aAddressable->entityType(), aAddressable->shortDesc().c_str());
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
static char vdc_container_key;
static char vdc_key;

enum {
  vdcs_key,
  valueSources_key,
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
    { "x-p44-valueSources", apivalue_null, valueSources_key, OKEY(devicecontainer_key) },
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
        case valueSources_key:
          aPropValue->setType(apivalue_object); // make object (incoming object is NULL)
          createValueSourcesList(aPropValue);
          return true;
        case webui_url_key:
          aPropValue->setStringValue(webuiURLString());
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - value sources

void DeviceContainer::createValueSourcesList(ApiValuePtr aApiObjectValue)
{
  // iterate through all devices and all of their sensors and inputs
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    // Sensors
    for (BehaviourVector::iterator pos2 = dev->sensors.begin(); pos2!=dev->sensors.end(); ++pos2) {
      DsBehaviourPtr b = *pos2;
      ValueSource *vs = dynamic_cast<ValueSource *>(b.get());
      if (vs) {
        aApiObjectValue->add(string_format("%s_S%zu",dev->getDsUid().getString().c_str(), b->getIndex()), aApiObjectValue->newString(vs->getSourceName().c_str()));
      }
    }
    // Inputs
    for (BehaviourVector::iterator pos2 = dev->binaryInputs.begin(); pos2!=dev->binaryInputs.end(); ++pos2) {
      DsBehaviourPtr b = *pos2;
      ValueSource *vs = dynamic_cast<ValueSource *>(b.get());
      if (vs) {
        aApiObjectValue->add(string_format("%s_I%zu",dev->getDsUid().getString().c_str(), b->getIndex()), aApiObjectValue->newString(vs->getSourceName().c_str()));
      }
    }
  }
}


ValueSource *DeviceContainer::getValueSourceById(string aValueSourceID)
{
  ValueSource *valueSource = NULL;
  // value source ID is
  //  dSUID:Sx for sensors (x=sensor index)
  //  dSUID:Ix for inputs (x=input index)
  // - extract dSUID
  size_t i = aValueSourceID.find("_");
  if (i!=string::npos) {
    DsUid dsuid(aValueSourceID.substr(0,i));
    DsDeviceMap::iterator pos = dSDevices.find(dsuid);
    if (pos!=dSDevices.end()) {
      // is a device
      DevicePtr dev = pos->second;
      const char *p = aValueSourceID.c_str()+i+1;
      if (*p) {
        char ty = *p++;
        // scan index
        int idx = 0;
        if (sscanf(p, "%d", &idx)==1) {
          if (ty=='S' && idx<dev->sensors.size()) {
            // sensor
            valueSource = dynamic_cast<ValueSource *>(dev->sensors[idx].get());
          }
          else if (ty=='I' && idx<dev->binaryInputs.size()) {
            // input
            valueSource = dynamic_cast<ValueSource *>(dev->binaryInputs[idx].get());
          }
        }
      }
    }
  }
  return valueSource;
}



#pragma mark - persistent vdc host level parameters

ErrorPtr DeviceContainer::loadAndFixDsUID()
{
  ErrorPtr err;
  // generate a default dSUID if no external one is given
  if (!externalDsuid) {
    // we don't have a fixed external dSUID to base everything on, so create a dSUID of our own:
    // single vDC per MAC-Adress scenario: generate UUIDv5 with name = macaddress
    // - calculate UUIDv5 based dSUID
    DsUid vdcNamespace(DSUID_VDC_NAMESPACE_UUID);
    dSUID.setNameInSpace(macAddressString(), vdcNamespace);
  }
  DsUid originalDsUid = dSUID;
  // load the vdc host settings, which might override the default dSUID
  err = loadFromStore(entityType()); // is a singleton, identify by type
  if (!Error::isOK(err)) LOG(LOG_ERR,"Error loading settings for vdc host: %s", err->description().c_str());
  // check for settings from files
  loadSettingsFromFiles();
  // now check
  if (!externalDsuid) {
    if (storedDsuid) {
      // a dSUID was loaded from DB -> check if different from default
      if (!(originalDsUid==dSUID)) {
        // stored dSUID is not same as MAC derived -> we are running a migrated config
        LOG(LOG_WARNING,"Running a migrated configuration: dSUID collisions with original unit possible");
        LOG(LOG_WARNING,"- native vDC host dSUID of this instance would be %s", originalDsUid.getString().c_str());
        LOG(LOG_WARNING,"- if this is not a replacement unit -> factory reset recommended!");
      }
    }
    else {
      // no stored dSUID was found so far -> we need to save the current one
      markDirty();
      save();
    }
  }
  return ErrorPtr();
}



ErrorPtr DeviceContainer::save()
{
  ErrorPtr err;
  // save the vdc settings
  err = saveToStore(entityType(), false); // is a singleton, identify by type, single instance
  return ErrorPtr();
}


ErrorPtr DeviceContainer::forget()
{
  // delete the vdc settings
  deleteFromStore();
  return ErrorPtr();
}



void DeviceContainer::loadSettingsFromFiles()
{
  // try to open config file
  string fn = getPersistentDataDir();
  fn += "vdchostsettings.csv";
  // if vdc has already stored properties, only explicitly marked properties will be applied
  if (loadSettingsFromFile(fn.c_str(), rowid!=0)) markClean();
}


#pragma mark - persistence implementation

// SQLIte3 table name to store these parameters to
const char *DeviceContainer::tableName()
{
  return "VdcHostSettings";
}


// data field definitions

static const size_t numFields = 2;

size_t DeviceContainer::numFieldDefs()
{
  return inheritedParams::numFieldDefs()+numFields;
}


const FieldDefinition *DeviceContainer::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "vdcHostName", SQLITE_TEXT },
    { "vdcHostDSUID", SQLITE_TEXT },
  };
  if (aIndex<inheritedParams::numFieldDefs())
    return inheritedParams::getFieldDef(aIndex);
  aIndex -= inheritedParams::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void DeviceContainer::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inheritedParams::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the name
  setName(nonNullCStr(aRow->get<const char *>(aIndex++)));
  // get the vdc host dSUID
  if (!externalDsuid) {
    // only if dSUID is not set externally, we try to load it
    DsUid loadedDsUid;
    if (loadedDsUid.setAsString(nonNullCStr(aRow->get<const char *>(aIndex)))) {
      // dSUID string from DB is valid
      dSUID = loadedDsUid; // activate it as the vdc host dSUID
      storedDsuid = true; // we're using a stored dSUID now
    }
  }
  aIndex++;
}


// bind values to passed statement
void DeviceContainer::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inheritedParams::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, getAssignedName().c_str(), false); // c_str() ist not static in general -> do not rely on it (even if static here)
  if (externalDsuid)
    aStatement.bind(aIndex++); // do not save externally defined dSUIDs
  else
    aStatement.bind(aIndex++, dSUID.getString().c_str(), false); // not static, string is local obj
}



#pragma mark - description

string DeviceContainer::description()
{
  string d = string_format("DeviceContainer with %lu device classes:", deviceClassContainers.size());
  for (ContainerMap::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
    d.append("\n");
    d.append(pos->second->description());
  }
  return d;
}



