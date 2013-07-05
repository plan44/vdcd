//
//  devicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "devicecontainer.hpp"

#include "deviceclasscontainer.hpp"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include "device.hpp"

#include "fnv.hpp"

using namespace p44;


DeviceContainer::DeviceContainer() :
  vdsmJsonComm(SyncIOMainLoop::currentMainLoop()),
  collecting(false),
  announcementTicket(0)
{
  vdsmJsonComm.setMessageHandler(boost::bind(&DeviceContainer::vdsmMessageHandler, this, _2, _3));
  #warning "// TODO: %%%% use final dsid scheme"
  // create a hash of the deviceContainerInstanceIdentifier
  string s = deviceContainerInstanceIdentifier();
  Fnv64 hash;
  hash.addBytes(s.size(), (uint8_t *)s.c_str());
  #if FAKE_REAL_DSD_IDS
  containerDsid.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
  containerDsid.setSerialNo(hash.getHash32());
  #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
  #else
  // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  containerDsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS);
  containerDsid.setSerialNo(0x7000000000000ll+hash.getHash48());
  #endif
}


void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers.push_back(aDeviceClassContainerPtr);
  aDeviceClassContainerPtr->setDeviceContainer(this);
}



string DeviceContainer::deviceContainerInstanceIdentifier() const
{
  string identifier;

  struct ifreq ifr;
  struct ifconf ifc;
  char buf[1024];
  int success = 0;

  do {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { /* handle error*/ };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
      break;
    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (; it != end; ++it) {
      strcpy(ifr.ifr_name, it->ifr_name);
      if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
          #ifdef __APPLE__
          #warning MAC address retrieval on OSX not supported
          break;
          #else
          if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            success = 1;
            break;
          }
          #endif
        }
      }
      else
        break;
    }
  } while(false);
  // extract ID if we have one
  if (success) {
    for (int i=0; i<6; ++i) {
      #ifndef __APPLE__
      string_format_append(identifier, "%02X",(uint8_t *)(ifr.ifr_hwaddr.sa_data)[i]);
      #endif
    }
  }
  else {
    identifier = "UnknownMACAddress";
  }
  return identifier;
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
  list<DeviceClassContainerPtr>::iterator nextContainer;
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
      (*nextContainer)->initialize(boost::bind(&DeviceClassInitializer::containerInitialized, this, _1), factoryReset);
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
    MainLoop::currentMainLoop()->executeOnce(boost::bind(&DeviceContainer::periodicTask, deviceContainerP, _2), 1*Second, deviceContainerP);
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
  // install a connection handler
  vdsmJsonComm.setConnectionStatusHandler(boost::bind(&DeviceContainer::vdsmConnStatusHandler, this, _2));
  // try to initiate connection to vdsm, connectionStatusHandler will take care of retries etc.
  initiateVdsmConnection();
  // initialize dsParamsDB database
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "DsParams.sqlite3");
  ErrorPtr error = dsParamStore.connectAndInitialize(databaseName.c_str(), DSPARAMS_SCHEMA_VERSION, aFactoryReset);

  // start initialisation of class containers
  DeviceClassInitializer::initialize(this, aCompletedCB, aFactoryReset);
}



void DeviceContainer::initiateVdsmConnection()
{
  LOG(LOG_DEBUG, ".............. Initiating connection to vdSM\n");
  vdsmJsonComm.initiateConnection();
}


void DeviceContainer::vdsmConnStatusHandler(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // vdSM connection successfully opened
    LOG(LOG_NOTICE, "++++++++++++++ Connection to vdSM established\n");
    // start container session
    startContainerSession();
  }
  else {
    // error on vdSM connection, was closed
    LOG(LOG_NOTICE, "-------------- Connection to vdSM terminated: %s\n\n", aError->description().c_str());
    // end container session
    endContainerSession();
    // re-initiate connection in a while
    MainLoop::currentMainLoop()->executeOnce(boost::bind(&DeviceContainer::initiateVdsmConnection,this), 10*Second);
  }
}




#pragma mark - collect devices

namespace p44 {

/// collects and initializes all devices
class DeviceClassCollector
{
  CompletedCB callback;
  bool exhaustive;
  list<DeviceClassContainerPtr>::iterator nextContainer;
  DeviceContainer *deviceContainerP;
  DsDeviceMap::iterator nextDevice;
public:
  static void collectDevices(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aExhaustive)
  {
    // create new instance, deletes itself when finished
    new DeviceClassCollector(aDeviceContainerP, aCallback, aExhaustive);
  };
private:
  DeviceClassCollector(DeviceContainer *aDeviceContainerP, CompletedCB aCallback, bool aExhaustive) :
    callback(aCallback),
    deviceContainerP(aDeviceContainerP),
    exhaustive(aExhaustive)
  {
    nextContainer = deviceContainerP->deviceClassContainers.begin();
    queryNextContainer(ErrorPtr());
  }


  void queryNextContainer(ErrorPtr aError)
  {
    if (!aError && nextContainer!=deviceContainerP->deviceClassContainers.end())
      (*nextContainer)->collectDevices(boost::bind(&DeviceClassCollector::containerQueried, this, _1), exhaustive);
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



void DeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  if (!collecting) {
    collecting = true;
    dSDevices.clear(); // forget existing ones
    DeviceClassCollector::collectDevices(this, aCompletedCB, aExhaustive);
  }
}

} // namespace


#pragma mark - adding/removing devices


// add a new device, replaces possibly existing one based on dsid
void DeviceContainer::addDevice(DevicePtr aDevice)
{
  // set for given dsid in the container-wide map of devices
  dSDevices[aDevice->dsid] = aDevice;
  LOG(LOG_NOTICE,"--- added device: %s", aDevice->description().c_str());
  // load the device's persistent params
  aDevice->load();
  // unless collecting now, register new device right away
  if (!collecting) {
    announceDevices();
  }
}


// remove a device
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
  // send vanish message
  JsonObjectPtr params = JsonObject::newObj();
  params->add("dSidentifier", JsonObject::newString(aDevice->dsid.getString()));
  sendMessage("Vanish", params);
  // remove from container-wide map of devices
  dSDevices.erase(aDevice->dsid);
  LOG(LOG_NOTICE,"--- removed device: %s", aDevice->description().c_str());
  // TODO: maybe unregister from vdSM???
}



#pragma mark - periodic activity


#define PERIODIC_TASK_INTERVAL (3*Second)

void DeviceContainer::periodicTask(MLMicroSeconds aCycleStartTime)
{
  // cancel any pending executions
  MainLoop::currentMainLoop()->cancelExecutionsFrom(this);
  if (!collecting) {
    // check for devices that need registration
    announceDevices();
    // do a save run as well
    for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
      pos->second->save();
    }
  }
  // schedule next run
  MainLoop::currentMainLoop()->executeOnce(boost::bind(&DeviceContainer::periodicTask, this, _2), PERIODIC_TASK_INTERVAL, this);
}



#pragma mark - message dispatcher

void DeviceContainer::vdsmMessageHandler(ErrorPtr aError, JsonObjectPtr aJsonObject)
{
  LOG(LOG_DEBUG, "Received vdSM API message: %s\n", aJsonObject->json_c_str());
  ErrorPtr err;
  JsonObjectPtr opObj = aJsonObject->get("operation");
  if (!opObj) {
    // no operation
    err = ErrorPtr(new vdSMError(vdSMErrorMissingOperation, "missing 'operation'"));
  }
  else {
    // get operation as lowercase string (to make comparisons case insensitive, we do all in lowercase)
    string o = opObj->lowercaseStringValue();
    // check for parameter addressing a device
    DevicePtr dev;
    JsonObjectPtr paramsObj = aJsonObject->get("parameter");
    bool doesAddressDevice = false;
    if (paramsObj) {
      // first check for dSID
      JsonObjectPtr dsidObj = paramsObj->get("dSidentifier");
      if (dsidObj) {
        string s = dsidObj->stringValue();
        dSID dsid(s);
        doesAddressDevice = true;
        DsDeviceMap::iterator pos = dSDevices.find(dsid);
        if (pos!=dSDevices.end())
          dev = pos->second;
      }
    }
    // dev now set to target device if one could be found
    if (dev) {
      // check operations targeting a device
      if (o=="deviceregistrationack") {
        dev->announcementAck(paramsObj);
        // signal device registered, so next can be issued
        deviceAnnounced();
      }
      else {
        // just forward message to device
        err = dev->handleMessage(o, paramsObj);
      }
    }
    else {
      // could not find a device to send the message to
      if (doesAddressDevice) {
        // if the message was actually targeting a device, this is an error
        err = ErrorPtr(new vdSMError(vdSMErrorDeviceNotFound, "device not found"));
      }
      else {
        // check operations not targeting a device
        // TODO: add operations
        // unknown operation
        err = ErrorPtr(new vdSMError(vdSMErrorUnknownContainerOperation, string_format("unknown container operation '%s'", o.c_str())));
      }
    }
  }
  if (!Error::isOK(err)) {
    LOG(LOG_ERR, "vdSM message processing error: %s\n", err->description().c_str());
    // send back error response
    JsonObjectPtr params = JsonObject::newObj();
    params->add("dSidentifier", JsonObject::newString(containerDsid.getString()));
    params->add("Message", JsonObject::newString(err->description()));
    sendMessage("Error", params);
  }
}


bool DeviceContainer::sendMessage(const char *aOperation, JsonObjectPtr aParams)
{
  JsonObjectPtr req = JsonObject::newObj();
  req->add("operation", JsonObject::newString(aOperation));
  if (aParams) {
    req->add("parameter", aParams);
  }
  ErrorPtr err;
  vdsmJsonComm.sendMessage(req, err);
  LOG(LOG_DEBUG, "Sent vdSM API message: %s\n", req->json_c_str());
  if (!Error::isOK(err)) {
    LOG(LOG_INFO, "Error sending JSON message: %s\n", err->description().c_str());
    return false;
  }
  return true;
}


// TODO: %%% hack implementation only
void DeviceContainer::localSwitchOutput(const dSID &aDsid, bool aNewOutState)
{
  if (localSwitchOutputCallback) {
    localSwitchOutputCallback(aDsid, aNewOutState);
  }
}




#pragma mark - session management


/// start vDC session (say Hello to the vdSM)
void DeviceContainer::startContainerSession()
{
  // end previous container session first (set all devices unannounced)
  endContainerSession();
  // send Hello
  JsonObjectPtr params = JsonObject::newObj();
  params->add("dSidentifier", JsonObject::newString(containerDsid.getString()));
  params->add("APIVersion", JsonObject::newInt32(1)); // TODO: %%% luz: must be 1=aizo, dsa cannot expand other ids so far
  sendMessage("Hello", params);
  #warning "For now, vdSM does not understand Hello, so we are not waiting for an answer yet"
  // %%% continue with announcing devices
  announceDevices();
}


/// end vDC session
void DeviceContainer::endContainerSession()
{
  // end pending announcement
  MainLoop::currentMainLoop()->cancelExecutionTicket(announcementTicket);
  // end all device sessions
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    dev->announced = Never;
    dev->announcing = Never;
  }
}


// how long until a not acknowledged registrations is considered timed out (and next device can be attempted)
#define REGISTRATION_TIMEOUT (15*Second)

// how long until a not acknowledged announcement for a device is retried again for the same device
#define REGISTRATION_RETRY_TIMEOUT (300*Second)

/// announce all not-yet announced devices to the vdSM
void DeviceContainer::announceDevices()
{
  if (!collecting && announcementTicket==0 && vdsmJsonComm.connected()) {
    // check all devices for unnannounced ones and announce those
    for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
      DevicePtr dev = pos->second;
      if (
        dev->isPublicDS() && // only public ones
        dev->announced==Never &&
        (dev->announcing==Never || MainLoop::now()>dev->announcing+REGISTRATION_RETRY_TIMEOUT)
      ) {
        // mark device as being in process of getting registered
        dev->announcing = MainLoop::now();
        // send registration request
        #warning "// TODO: for new vDC API, replace this by "Announce" method
        if (!sendMessage("DeviceRegistration", dev->registrationParams())) {
          LOG(LOG_ERR, "Could not send announcement message for device %s\n", dev->shortDesc().c_str());
          dev->announcing = Never; // not registering
        }
        else {
          LOG(LOG_INFO, "Sent announcement for device %s\n", dev->shortDesc().c_str());
        }
        // don't register too fast, prevent re-registering for a while
        announcementTicket = MainLoop::currentMainLoop()->executeOnce(boost::bind(&DeviceContainer::deviceAnnounced, this), REGISTRATION_TIMEOUT);
        // done for now, continues after REGISTRATION_TIMEOUT or when registration acknowledged
        break;
      }
    }
  }
}


void DeviceContainer::deviceAnnounced()
{
  MainLoop::currentMainLoop()->cancelExecutionTicket(announcementTicket);
  announcementTicket = 0;
  // try next announcement
  announceDevices();
}


#pragma mark - description

string DeviceContainer::description()
{
  string d = string_format("DeviceContainer with %d device classes:\n", deviceClassContainers.size());
  for (ContainerList::iterator pos = deviceClassContainers.begin(); pos!=deviceClassContainers.end(); ++pos) {
    d.append((*pos)->description());
  }
  return d;
}



