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

using namespace p44;


DeviceContainer::DeviceContainer() :
  vdsmJsonComm(SyncIOMainLoop::currentMainLoop()),
  collecting(false)
{
  vdsmJsonComm.setMessageHandler(boost::bind(&DeviceContainer::vdsmMessageHandler, this, _2, _3));
}


void DeviceContainer::addDeviceClassContainer(DeviceClassContainerPtr aDeviceClassContainerPtr)
{
  deviceClassContainers.push_back(aDeviceClassContainerPtr);
  aDeviceClassContainerPtr->setDeviceContainer(this);
}



string DeviceContainer::deviceContainerInstanceIdentifier() const
{
  string identifier;

  //
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


#pragma mark - initialize


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
    // start periodic tasks like registration checking
    MainLoop::currentMainLoop()->executeOnce(boost::bind(&DeviceContainer::periodicTask, deviceContainerP, _2), 1*Second, deviceContainerP);
    // callback
    callback(aError);
    // done, delete myself
    delete this;
  }
	
};


#define PERIODIC_TASK_INTERVAL (3*Second)

void DeviceContainer::periodicTask(MLMicroSeconds aCycleStartTime)
{
  // cancel any pending executions
  MainLoop::currentMainLoop()->cancelExecutionsFrom(this);
  if (!collecting) {
    // check for devices that need registration
    registerDevices();
  }
  // schedule next run
  MainLoop::currentMainLoop()->executeOnce(boost::bind(&DeviceContainer::periodicTask, this, _2), PERIODIC_TASK_INTERVAL, this);
}


void DeviceContainer::initialize(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // initialize class containers
  DeviceClassInitializer::initialize(this, aCompletedCB, aFactoryReset);
}




#pragma mark - collect devices


class p44::DeviceClassCollector
{
  CompletedCB callback;
  bool exhaustive;
  list<DeviceClassContainerPtr>::iterator nextContainer;
  DeviceContainer *deviceContainerP;
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
      completed(aError);
  }

  void containerQueried(ErrorPtr aError)
  {
    // check next
    ++nextContainer;
    queryNextContainer(aError);
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




#pragma mark - adding/removing devices


// add a new device, replaces possibly existing one based on dsid
void DeviceContainer::addDevice(DevicePtr aDevice)
{
  // set for given dsid in the container-wide map of devices
  dSDevices[aDevice->dsid] = aDevice;
  LOG(LOG_NOTICE,"--- added device: %s", aDevice->description().c_str());
  // unless collecting now, register new device right away
  if (!collecting) {
    registerDevices();
  }
}


// remove a device
void DeviceContainer::removeDevice(DevicePtr aDevice)
{
  // add to container-wide map of devices
  dSDevices.erase(aDevice->dsid);
  busDevices.erase(aDevice->busAddress);
  LOG(LOG_NOTICE,"--- removed device: %s", aDevice->description().c_str());
  // TODO: maybe unregister from vdSM???
}



#pragma mark - message dispatcher

void DeviceContainer::vdsmMessageHandler(ErrorPtr aError, JsonObjectPtr aJsonObject)
{
  JsonObjectPtr opObj = aJsonObject->get("operation");
  if (!opObj) {
    // no operation
    LOG(LOG_ERR, "vdSM message error: missing 'operation'\n");
    return;
  }
  string o = opObj->stringValue();
  // check for parameter addressing a device
  DevicePtr dev;
  JsonObjectPtr paramsObj = aJsonObject->get("parameter");
  if (paramsObj) {
    // first check for dSID
    JsonObjectPtr dsidObj = paramsObj->get("dSID");
    if (dsidObj) {
      string s = dsidObj->stringValue();
      dSID dsid(s);
      DsDeviceMap::iterator pos = dSDevices.find(dsid);
      if (pos!=dSDevices.end())
        dev = pos->second;
    }
    if (!dev) {
      // not found by dSID, try BusAddress
      JsonObjectPtr baObj = paramsObj->get("BusAddress");
      if (baObj) {
        BusAddressMap::iterator pos = busDevices.find(baObj->int32Value());
        if (pos!=busDevices.end())
          dev = pos->second;
      }
    }
  }
  // dev now set to target device if one could be found
  if (dev) {
    // check operations targeting a device
    if (o=="DeviceRegistrationAck") {
      dev->confirmRegistration(paramsObj);
      // %%% TODO: probably remove later
      // save by bus address
      JsonObjectPtr baObj = paramsObj->get("BusAddress");
      if (baObj) {
        busDevices[baObj->int32Value()] = dev;
      }
    }
  }
  else {
    // check operations not targeting a device
    // TODO: add operations
    // unknown operation
    LOG(LOG_ERR, "vdSM message error: unknown operation '%s'\n", o.c_str());
  }
}

//  if request['operation'] == 'DeviceRegistrationAck':
//      self.address = request['parameter']['BusAddress']
//      self.zone = request['parameter']['Zone']
//      self.groups = request['parameter']['GroupMemberships']
//      print 'BusAddress:', request['parameter']['BusAddress']
//      print 'Zone:', request['parameter']['Zone']
//      print 'Groups:', request['parameter']['GroupMemberships']



#pragma mark - registration


#define REGISTRATION_TIMEOUT (15*Second)

void DeviceContainer::registerDevices(MLMicroSeconds aLastRegBefore)
{
  // check all devices for unregistered ones and register those
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    if (
      dev->registered<=aLastRegBefore && // no or outdated registration
      (dev->registering==Never || MainLoop::now()>dev->registering+REGISTRATION_TIMEOUT)
    ) {
      // mark device as being in process of getting registered
      dev->registering = MainLoop::now();
      // create registration request
      JsonObjectPtr req = JsonObject::newObj();
      req->add("operation", JsonObject::newString("DeviceRegistration"));
      // ask the device to provide the registration parameters
      JsonObjectPtr params = dev->registrationParams();
      req->add("parameter", params);
      // issue the request
      ErrorPtr err;
      vdsmJsonComm.sendMessage(req, err);
      if (!Error::isOK(err)) {
        LOG(LOG_ERR, "Cannot send registration message for %s: %s\n", dev->shortDesc().c_str(), err->description().c_str());
        dev->registering = Never; // not registering
      }
    }
  }
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



