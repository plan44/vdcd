//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 7


#include "discovery.hpp"


using namespace p44;

#define VDSM_SERVICE_TYPE "_ds-vdsm._tcp"
#define VDC_SERVICE_TYPE "_ds-vdc._tcp"
#define DS485P_SERVICE_TYPE "_ds-ds485p._tcp"
#define DSS_SERVICE_TYPE "_dssweb._tcp"
#define HTTP_SERVICE_TYPE "_http._tcp"
#define SSH_SERVICE_TYPE "_ssh._tcp"

#define VDSM_ROLE_MASTER "master"
#define VDSM_ROLE_AUXILIARY "auxiliary"
#define VDSM_ROLE_COLLECTING "collecting"
#define VDSM_ROLE_COLLECTABLE "collectable"
#define VDSM_ROLE_NOAUTO "noauto"

#define STARTUP_RETRY_DELAY (30*Second) // how long to wait before retrying to start avahi server when failed because of missing network
#define SERVER_RESTART_DELAY (5*Minute) // how long to wait before restarting the server (after a problem that caused calling restartServer())
#define VDSM_RESCAN_DELAY (20*Minute) // how often to start a complete rescan anyway (even if no vdsm is lost)
#define VDSM_LOST_RESCAN_DELAY (10*Second) // how fast to start a rescan after a vdsm has been lost
#define RESCAN_EVALUATION_DELAY (10*Second) // how long to browse until re-evaluating state
#define INITIAL_EVALUATION_DELAY (30*Second) // how long to browse until evaluating state for the first time (only when no auxvdsm is running)
#define REEVALUATION_DELAY (30*Second) // how long to browse until reevaluating state (only when no auxvdsm is running)


DiscoveryManager::DiscoveryManager() :
  simple_poll(NULL),
  server(NULL),
  serviceBrowser(NULL),
  entryGroup(NULL),
  debugServiceBrowser(NULL),
  auxVdsmRunning(false),
  noAuto(false),
  publishWebPort(0),
  publishSshPort(0),
  rescanTicket(0),
  evaluateTicket(0)
{
  // register a cleanup handler
  MainLoop::currentMainLoop().registerCleanupHandler(boost::bind(&DiscoveryManager::stop, this));
  // route avahi logs to our own log system
  avahi_set_log_function(&DiscoveryManager::avahi_log);
}


void DiscoveryManager::avahi_log(AvahiLogLevel level, const char *txt)
{
  // show all avahi log stuff only when we have focus
  FOCUSLOG("avahi(%d): %s\n", level, txt);
}



string DiscoveryManager::publishedName()
{
  // derive the descriptive name
  // - descriptive name: vendor name + Model name + optional custom name + optional serial
  string n = deviceContainer->vendorName();
  if (!n.empty()) n+=" ";
  n += deviceContainer->modelName();
  if (!deviceContainer->getName().empty()) {
    // append custom name
    string_format_append(n, " \"%s\"", deviceContainer->getName().c_str());
  }
  string s = deviceContainer->getDeviceHardwareId();
  if (s.empty()) {
    // use dSUID if no other ID is specified
    s = deviceContainer->getDsUid().getString();
  }
  string_format_append(n, " (%s)", s.c_str());
  return n;
}



DiscoveryManager::~DiscoveryManager()
{
  // full stop
  stop();
}


void DiscoveryManager::stop()
{
  // unregister idle handler
  MainLoop::currentMainLoop().unregisterIdleHandlers(this);
  // stop server
  stopServer();
  // stop polling
  if (simple_poll) {
    avahi_simple_poll_quit(simple_poll);
    avahi_simple_poll_free(simple_poll);
    simple_poll = NULL;
  }
}


ErrorPtr DiscoveryManager::start(
  DeviceContainerPtr aDeviceContainer,
  const char *aHostname, bool aNoAuto,
  int aWebPort, int aSshPort,
  DsUidPtr aAuxVdsmDsUid, int aAuxVdsmPort, bool aAuxVdsmRunning, AuxVdsmStatusHandler aAuxVdsmStatusHandler
) {
  ErrorPtr err;
  // stop current operation
  stop();
  // store the new params
  deviceContainer = aDeviceContainer;
  hostname = aHostname;
  noAuto = aNoAuto;
  publishWebPort = aWebPort;
  publishSshPort = aSshPort;
  auxVdsmPort = aAuxVdsmPort;
  auxVdsmDsUid = aAuxVdsmDsUid;
  auxVdsmRunning = aAuxVdsmRunning;
  auxVdsmStatusHandler = aAuxVdsmStatusHandler;
  // init state
  dmState = dm_starting; // starting
  // allocate the simple-poll object
  if (!(simple_poll = avahi_simple_poll_new())) {
    err = TextError::err("Avahi: Failed to create simple poll object.");
  }
  // start polling
  MainLoop::currentMainLoop().registerIdleHandler(this, boost::bind(&DiscoveryManager::avahi_poll, this));
  if (Error::isOK(err)) {
    // prepare server config
    startServer();
  }
  return err;
}


void DiscoveryManager::stopServer()
{
  // stop the timers
  MainLoop::currentMainLoop().cancelExecutionTicket(rescanTicket);
  MainLoop::currentMainLoop().cancelExecutionTicket(evaluateTicket);
  // clean up server
  if (entryGroup) {
    avahi_s_entry_group_free(entryGroup);
    entryGroup = NULL;
    LOG(LOG_NOTICE, "discovery: unpublished '%s'.\n", publishedName().c_str());
  }
  if (serviceBrowser) {
    avahi_s_service_browser_free(serviceBrowser);
    serviceBrowser = NULL;
  }
  if (debugServiceBrowser) {
    avahi_s_service_browser_free(debugServiceBrowser);
    debugServiceBrowser = NULL;
  }
  if (server) {
    avahi_server_free(server);
    server = NULL;
  }
}


void DiscoveryManager::restartServer()
{
  LOG(LOG_WARNING, "discovery: restarting avahi server in %\n");
  stopServer();
  MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::startServer, this), SERVER_RESTART_DELAY);
}



void DiscoveryManager::startServer()
{
  // only start if not already started
  if (!server) {
    int avahiErr;
    AvahiServerConfig config;
    avahi_server_config_init(&config);
    // basic info
    config.host_name = avahi_strdup(hostname.c_str()); // unique hostname
    #ifdef __APPLE__
    config.disallow_other_stacks = 0; // om OS X, we always have a mDNS, so allow more than one for testing
    #else
    config.disallow_other_stacks = 1; // we wants to be the only mdNS (also avoids problems with SO_REUSEPORT on older Linux kernels)
    #endif
    // IPv4 only at this time!
    config.use_ipv4 = 1;
    config.use_ipv6 = 0; // NONE
    // publishing options
    config.publish_aaaa_on_ipv4 = 0; // prevent publishing IPv6 AAAA record on IPv4
    config.publish_a_on_ipv6 = 0; // prevent publishing IPv4 A on IPV6
    config.publish_hinfo = 0; // no CPU specifics
    config.publish_addresses = 1; // publish addresses
    config.publish_workstation = 0; // no workstation
    config.publish_domain = 1; // announce the local domain for browsing
    // create server with prepared config
    server = avahi_server_new(avahi_simple_poll_get(simple_poll), &config, server_callback, this, &avahiErr);
    avahi_server_config_free(&config); // don't need it any more
    if (!server) {
      if (avahiErr==AVAHI_ERR_NO_NETWORK) {
        // no network to publish to - might be that it is not yet up, try again later
        LOG(LOG_WARNING, "avahi: no network available to publish services now -> retry later\n");
        MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::startServer, this), STARTUP_RETRY_DELAY);
        return;
      }
      else {
        // other problem, report it
        LOG(LOG_ERR, "Failed to create server: %s (%d)", avahi_strerror(avahiErr), avahiErr);
      }
    }
    else {
      // server created
      // - when debugging, browse for http
      if (FOCUSLOGENABLED) {
        debugServiceBrowser = avahi_s_service_browser_new(server, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, HTTP_SERVICE_TYPE, NULL, (AvahiLookupFlags)0, browse_callback, this);
        if (!debugServiceBrowser) {
          FOCUSLOG("Failed to create debug service browser: %s\n", avahi_strerror(avahi_server_errno(server)));
        }
      }
    }
  }
}


void DiscoveryManager::startBrowsingVdms(AvahiServer *aServer)
{
  // Note: may NOT use global "server" var, because this can be called from within server callback, which might be called before avahi_server_new() returns
  // stop previous, if any
  if (serviceBrowser) {
    avahi_s_service_browser_free(serviceBrowser);
  }
  serviceBrowser = avahi_s_service_browser_new(aServer, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, VDSM_SERVICE_TYPE, NULL, (AvahiLookupFlags)0, browse_callback, this);
  if (!serviceBrowser) {
    LOG(LOG_ERR, "Failed to create service browser for vdsms: %s\n", avahi_strerror(avahi_server_errno(aServer)));
  }
  if (auxVdsmDsUid && !auxVdsmRunning) {
    // if no auxiliary vdsm is running now, schedule a check to detect if we've found no master vdsms (otherwise, only FINDING a master is relevant)
    evaluateTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::evaluateState, this), INITIAL_EVALUATION_DELAY);
  }
  // schedule a rescan now and then
  MainLoop::currentMainLoop().cancelExecutionTicket(rescanTicket); // cancel possibly pending overall timeout
  rescanTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::rescanVdsms, this, aServer), VDSM_RESCAN_DELAY);
}


void DiscoveryManager::rescanVdsms(AvahiServer *aServer)
{
  FOCUSLOG("rescanVdsms: restart browsing for vdsms now\n");
  // - restart browsing
  startBrowsingVdms(aServer);
  // - schedule an evaluation in a while
  MainLoop::currentMainLoop().cancelExecutionTicket(evaluateTicket);
  evaluateTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::evaluateState, this), RESCAN_EVALUATION_DELAY);
}


void DiscoveryManager::evaluateState()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(evaluateTicket);
  FOCUSLOG("evaluateState: auxVdsmRunning=%d, dmState=%d, sessionConnection=%d\n", auxVdsmRunning, (int)dmState, deviceContainer->getSessionConnection()!=NULL);
  if (dmState>=dm_started && auxVdsmDsUid) {
    // avahi up and we are managing an auxiliary vdsm
    if (auxVdsmRunning) {
      // we have a auxiliary vdsm running and must decide whether to shut it down now
      if (dmState==dm_detected_master) {
        // there is indeed a master vdsm in the network, let it take over
        LOG(LOG_WARNING, "***** Detected master vdsm -> shut down auxiliary vdsm\n");
        if (auxVdsmStatusHandler) auxVdsmStatusHandler(false);
      }
    }
    else {
      // we don't have a auxiliary vdsm running and must decide whether we should start one
      // - only start auxiliary vdsm if our vdc API is not connected
      if (!deviceContainer->getSessionConnection()) {
        // no active session
        if (dmState!=dm_detected_master) {
          // and we haven't detected a master vdsm
          LOG(LOG_WARNING, "***** Detected no master vdsm, and vdc has no connection -> need auxiliary vdsm\n");
          if (auxVdsmStatusHandler) auxVdsmStatusHandler(true);
        }
      }
      else {
        // as long as we have a connection, apparently a vdsm is taking care, so we don't need the aux vdsm
        // - but check again in a while
        MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::evaluateState, this), REEVALUATION_DELAY);
      }
    }
  }
}


#pragma mark - Avahi poll and callbacks


bool DiscoveryManager::avahi_poll()
{
  if (simple_poll) {
    avahi_simple_poll_iterate(simple_poll, 0);
  }
  return true; // done for this cycle
}


// C stub for avahi server callback
void DiscoveryManager::server_callback(AvahiServer *s, AvahiServerState state, void* userdata)
{
  DiscoveryManager *discoveryManager = static_cast<DiscoveryManager*>(userdata);
  discoveryManager->avahi_server_callback(s, state);
}


// actual avahi server callback implementation
void DiscoveryManager::avahi_server_callback(AvahiServer *s, AvahiServerState state)
{
  // Avahi server state has changed
  switch (state) {
    case AVAHI_SERVER_RUNNING: {
      // The server has started up successfully and registered its hostname
      // - create services if not yet created already
      if (!entryGroup) {
        create_services(s);
      }
      break;
    }
    case AVAHI_SERVER_COLLISION: {
      // Host name collision detected
      // - create alternative name
      char *newName = avahi_alternative_host_name(avahi_server_get_host_name(s));
      LOG(LOG_WARNING, "avahi: host name collision, retrying with '%s'\n", newName);
      int avahiErr = avahi_server_set_host_name(s, newName);
      avahi_free(newName);
      if (avahiErr<0) {
        LOG(LOG_ERR, "avahi: cannot set new host name\n");
        restartServer();
        break;
      }
      // otherwise fall through to AVAHI_SERVER_REGISTERING
    }
    case AVAHI_SERVER_REGISTERING: {
      // drop service registration, will be recreated once server is running
      if (entryGroup) {
        avahi_s_entry_group_reset(entryGroup);
        entryGroup = NULL;
      }
      break;
    }
    case AVAHI_SERVER_FAILURE: {
      LOG(LOG_ERR, "avahi: server failure: %s\n", avahi_strerror(avahi_server_errno(s)));
      restartServer();
      break;
    }
    case AVAHI_SERVER_INVALID: break;
  }
}


void DiscoveryManager::create_services(AvahiServer *aAvahiServer)
{
  string descriptiveName = publishedName();
  // create entry group if needed
  if (!entryGroup) {
    if (!(entryGroup = avahi_s_entry_group_new(aAvahiServer, entry_group_callback, this))) {
      LOG(LOG_ERR, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_server_errno(aAvahiServer)));
      goto fail;
    }
  }
  // add the services
  int avahiErr;
  if (publishWebPort) {
    // - web UI
    if ((avahiErr = avahi_server_add_service(
      aAvahiServer,
      entryGroup,
      AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, // all interfaces and protocols (as enabled at server level)
      (AvahiPublishFlags)0, // no flags
      descriptiveName.c_str(),
      HTTP_SERVICE_TYPE, // Web service
      NULL, // no domain
      NULL, // no host
      publishWebPort, // web port
      NULL // no TXT records
    ))<0) {
      LOG(LOG_ERR, "avahi: failed to add _http._tcp service: %s\n", avahi_strerror(avahiErr));
      goto fail;
    }
  }
  if (publishSshPort) {
    // - ssh access
    if ((avahiErr = avahi_server_add_service(
      aAvahiServer,
      entryGroup,
      AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, // all interfaces and protocols (as enabled at server level)
      (AvahiPublishFlags)0, // no flags
      descriptiveName.c_str(),
      SSH_SERVICE_TYPE, // ssh
      NULL, // no domain
      NULL, // no host
      publishSshPort, // ssh port
      NULL // no TXT records
    ))<0) {
      LOG(LOG_ERR, "avahi: failed to add _ssh._tcp service: %s\n", avahi_strerror(avahiErr));
      goto fail;
    }
  }
  // - advertise the auxiliary vdsm if it is running, vdcd itself otherwise
  if (auxVdsmRunning && auxVdsmDsUid) {
    // The auxiliary vdsm is running, advertise it to the network
    // - create the vdsm dsuid TXT record
    string txt_dsuid = string_format("dSUID=%s", auxVdsmDsUid->getString().c_str());
    if ((avahiErr = avahi_server_add_service(
      aAvahiServer,
      entryGroup,
      AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, // all interfaces and protocols (as enabled at server level)
      (AvahiPublishFlags)0, // no flags
      descriptiveName.c_str(),
      VDSM_SERVICE_TYPE, // vdsm
      NULL, // no domain
      NULL, // no host
      auxVdsmPort, // auxiliary vdsm's ds485 port
      txt_dsuid.c_str(), // TXT record for the auxiliary vdsm's dSUID
      VDSM_ROLE_COLLECTABLE, // TXT record signalling this vdsm may be collected by ds485p
      VDSM_ROLE_AUXILIARY, // TXT record signalling this vdsm is auxiliary
      noAuto ? VDSM_ROLE_NOAUTO : NULL, // noauto flag or early TXT terminator
      NULL // TXT record terminator
    ))<0) {
      LOG(LOG_ERR, "avahi: failed to add _ds-vdsm._tcp service: %s\n", avahi_strerror(avahiErr));
      goto fail;
    }
  }
  else {
    // The auxiliary vdsm is NOT running or not present at all, advertise the vdc host (vdcd) to the network
    int vdcPort = 0;
    sscanf(deviceContainer->vdcApiServer->getPort(), "%d", &vdcPort);
    if ((avahiErr = avahi_server_add_service(
      aAvahiServer,
      entryGroup,
      AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, // all interfaces and protocols (as enabled at server level)
      (AvahiPublishFlags)0, // no flags
      descriptiveName.c_str(),
      VDC_SERVICE_TYPE, // vdc
      NULL, // no domain
      NULL, // no host
      vdcPort, // the vdc API host port
      noAuto ? VDSM_ROLE_NOAUTO : NULL, // noauto flag or early TXT terminator
      NULL // TXT record terminator
    ))<0) {
      LOG(LOG_ERR, "avahi: failed to add _ds-vdc._tcp service: %s\n", avahi_strerror(avahiErr));
      goto fail;
    }
  }
  // register the services
  if ((avahiErr = avahi_s_entry_group_commit(entryGroup)) < 0) {
    LOG(LOG_ERR, "avahi: Failed to commit entry_group: %s\n", avahi_strerror(avahiErr));
    goto fail;
  }
  return;
fail:
  restartServer();
}


// C stub for avahi server callback
void DiscoveryManager::entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata)
{
  DiscoveryManager *discoveryManager = static_cast<DiscoveryManager*>(userdata);
  discoveryManager->avahi_entry_group_callback(s, g, state);
}


void DiscoveryManager::avahi_entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state)
{
  // entry group state has changed
  switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED: {
      LOG(LOG_NOTICE, "discovery: successfully published %s service '%s'.\n", auxVdsmRunning ? "vdSM" : "vDC", publishedName().c_str());
      if (dmState<dm_started)
        dmState = dm_started;
      // start scanning for master vdsms
      if (auxVdsmDsUid) {
        // We have an auxiliary vdsm we need to monitor
        // - create browser to look out for master vdsms
        startBrowsingVdms(s);
      }
      break;
    }
    case AVAHI_ENTRY_GROUP_COLLISION: {
      // service name collision detected
      // Note: we don't handle this as it can't really happen (publishedName contains the deviceId or the vdcHost dSUID which MUST be unique)
      LOG(LOG_CRIT, "avahi: service name collision, '%s' is apparently not unique\n", publishedName().c_str());
      break;
    }
    case AVAHI_ENTRY_GROUP_FAILURE: {
      LOG(LOG_ERR, "avahi: entry group failure: %s -> terminating avahi announcements\n", avahi_strerror(avahi_server_errno(s)));
      restartServer();
      break;
    }
    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
      break;
  }
}



void DiscoveryManager::browse_callback(AvahiSServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void* userdata)
{
  DiscoveryManager *discoveryManager = static_cast<DiscoveryManager*>(userdata);
  discoveryManager->avahi_browse_callback(b, interface, protocol, event, name, type, domain, flags);
}


void DiscoveryManager::avahi_browse_callback(AvahiSServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags)
{
  // Called whenever a new services becomes available on the LAN or is removed from the LAN
  // - may use global "server" var, because browsers are no set up within server callbacks, but only afterwards, when "server" is defined
  if (b==debugServiceBrowser) {
    // debug (show http services)
    switch (event) {
      case AVAHI_BROWSER_NEW:
        FOCUSLOG("avahi: new http service '%s' in domain '%s' discovered\n", name, domain);
        break;
      case AVAHI_BROWSER_REMOVE:
        FOCUSLOG("avahi: http service '%s' in domain '%s' has disappeared\n", name, domain);
        break;
      default:
        break;
    }
  }
  else {
    // actual browser for vdsms
    switch (event) {
      case AVAHI_BROWSER_FAILURE:
        LOG(LOG_ERR, "avahi: browser failure: %s\n", avahi_strerror(avahi_server_errno(server)));
        restartServer();
        break;
      case AVAHI_BROWSER_NEW:
        LOG(LOG_DEBUG, "avahi: BROWSER_NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
        // Note: local vdsm record (my own) is ignored
        if (strcmp(type, VDSM_SERVICE_TYPE)==0 && (flags & AVAHI_LOOKUP_RESULT_LOCAL)==0) {
          // new vdsm found. Only for vdsms, we need to resolve to get the TXT records in order to see if we've found a master vdsm
          LOG(LOG_NOTICE, "discovery: vdsm '%s' has appeared\n", name);
          // Note: the returned resolver object can be ignored, it is freed in the callback
          //   if the server terminates before the callback has been executes, the server deletes the resolver.
          if (!(avahi_s_service_resolver_new(server, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0, resolve_callback, this))) {
            LOG(LOG_ERR, "avahi: failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_server_errno(server)));
            break;
          }
        }
        break;
      case AVAHI_BROWSER_REMOVE:
        LOG(LOG_DEBUG, "avahi: BROWSER_REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
        if (strcmp(type, VDSM_SERVICE_TYPE)==0) {
          // a vdsm has disappeared
          LOG(LOG_NOTICE, "discovery: vdsm '%s' no longer online\n", name);
          // we have lost a vdsm, we need to rescan in a while (unless another master appears in the meantime)
          dmState = dm_lost_vdsm;
          MainLoop::currentMainLoop().cancelExecutionTicket(rescanTicket); // cancel possibly pending overall timeout
          rescanTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&DiscoveryManager::rescanVdsms, this, server), VDSM_LOST_RESCAN_DELAY);
        }
        break;
      case AVAHI_BROWSER_ALL_FOR_NOW:
        LOG(LOG_DEBUG, "avahi: BROWSER_ALL_FOR_NOW\n");
        break;
      case AVAHI_BROWSER_CACHE_EXHAUSTED:
        LOG(LOG_DEBUG, "avahi: BROWSER_CACHE_EXHAUSTED\n");
        break;
    }
  }
}


void DiscoveryManager::resolve_callback(AvahiSServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata)
{
  DiscoveryManager *discoveryManager = static_cast<DiscoveryManager*>(userdata);
  discoveryManager->avahi_resolve_callback(r, interface, protocol, event, name, type, domain, host_name, a, port, txt, flags);
}


void DiscoveryManager::avahi_resolve_callback(AvahiSServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags)
{
  switch (event) {
    case AVAHI_RESOLVER_FAILURE:
      FOCUSLOG("avahi: failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_server_errno(server)));
      break;
    case AVAHI_RESOLVER_FOUND: {
      char addrtxt[AVAHI_ADDRESS_STR_MAX];
      avahi_address_snprint(addrtxt, sizeof(addrtxt), a);
      FOCUSLOG("avahi: resolved service '%s' of type '%s' in domain '%s' at %s:\n", name, type, domain, addrtxt);
      // Note: as we only start resolving for vdsms, this should be a vdsm
      if (FOCUSLOGENABLED) {
        // display full info
        char *txttxt;
        txttxt = avahi_string_list_to_string(txt);
        FOCUSLOG(
          "- avahi resolver returns: %s:%u (%s)\n"
          "- TXT=%s\n"
          "- cookie is %u\n"
          "- is_local: %i\n"
          "- wide_area: %i\n"
          "- multicast: %i\n"
          "- cached: %i\n",
          host_name, port, addrtxt,
          txttxt,
          avahi_string_list_get_service_cookie(txt),
          !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
          !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
          !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
          !!(flags & AVAHI_LOOKUP_RESULT_CACHED)
        );
        avahi_free(txttxt);
      }
      // check if this is a master vdsm
      if (strcmp(type, VDSM_SERVICE_TYPE)==0) {
        // is indeed a vdsm
        if (avahi_string_list_find(txt, VDSM_ROLE_MASTER)) {
          // there IS a master vdsm
          LOG(LOG_NOTICE, "discovery: detected presence of master vdsm '%s' at %s\n", name, addrtxt);
          dmState = dm_detected_master;
          evaluateState();
        }
      }
    }
  }
  avahi_s_service_resolver_free(r);
}









