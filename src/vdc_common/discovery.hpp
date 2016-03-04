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

#ifndef __vdcd__discovery__
#define __vdcd__discovery__

#include "vdcd_common.hpp"

#if !DISABLE_DISCOVERY

#include "dsuid.hpp"
#include "devicecontainer.hpp"


// Avahi includes
#if USE_AVAHI_CORE
// - directly using core, good for small embedded with single process using avahi
#include <avahi-core/core.h>
#include <avahi-core/publish.h>
#include <avahi-core/lookup.h>
#define AvahiService AvahiServer
//#define AvahiServiceState AvahiServerState
#define avahi_service_errno avahi_server_errno
#define avahi_add_service avahi_server_add_service
#define avahi_entry_group_commit avahi_s_entry_group_commit
#define AvahiEntryGroup AvahiSEntryGroup
#define AvahiServiceBrowser AvahiSServiceBrowser
#define AvahiServiceBrowser AvahiSServiceBrowser
#define AvahiServiceResolver AvahiSServiceResolver
#define avahi_entry_group_new avahi_s_entry_group_new
#define avahi_entry_group_reset avahi_s_entry_group_reset
#define avahi_entry_group_free avahi_s_entry_group_free
#define avahi_service_browser_new avahi_s_service_browser_new
#define avahi_service_browser_free avahi_s_service_browser_free
#define avahi_service_resolver_new avahi_s_service_resolver_new
#define avahi_service_resolver_free avahi_s_service_resolver_free
#else
// - use avahi client, desktop/larger embedded (which uses system wide avahi server, together with other clients)
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#define AvahiService AvahiClient
//#define AvahiServiceState AvahiClientState
#define avahi_service_errno avahi_client_errno
#define avahi_add_service(srv,eg,...) avahi_entry_group_add_service(eg,##__VA_ARGS__)
#endif

#include <avahi-core/log.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>


using namespace std;

namespace p44 {



  typedef boost::function<void (bool aShouldRun)> AuxVdsmStatusHandler;

  /// Implements service announcement and discovery (via avahi) for vdcd and (if configured) a associated vdsm
  class DiscoveryManager : public P44Obj
  {
    typedef P44Obj inherited;

    AvahiSimplePoll *simple_poll;
    AvahiService *service;
    AvahiEntryGroup *entryGroup;
    AvahiServiceBrowser *serviceBrowser;
    AvahiServiceBrowser *debugServiceBrowser;

    // publishing information
    // - common for all services
    DeviceContainerPtr deviceContainer;
    string hostname;
    bool noAuto;
    int publishWebPort;
    int publishSshPort;
    bool igmpSnoopingHints;
    #if ENABLE_AUXVDSM
    // - an optionally running auxiliary vdsm
    DsUidPtr auxVdsmDsUid;
    int auxVdsmPort;
    bool auxVdsmRunning;
    bool vdsmAuxiliary;
    AuxVdsmStatusHandler auxVdsmStatusHandler;
    #endif // ENABLE_AUXVDSM


    // state
    enum {
      dm_starting, // waiting for pusblishing to happen
      dm_started, // published
      dm_previously_not_detected_master, // in previous scan, we did NOT see a master, and also not (yet) in current scan
      dm_previously_detected_master, // in previous scan, we did see a master, but not (yet) in current scan
      dm_detected_master, // at least one master vdsm has been detected
      dm_lost_vdsm, // just lost a vdsm, re-discovering what is on the network
      dm_auxvdsm_needs_change, // need for auxiliary vdsm run state change (needs to be started or stopped)
    } dmState;

    long rescanTicket;
    long evaluateTicket;
    long igmpQueryTicket;


  public:

    DiscoveryManager();
    virtual ~DiscoveryManager();

    /// start advertising vdcd (or the vdsm, if same platform hosts a auxiliary vdsm and no master vdsm is found)
    /// @param aDeviceContainer the device container to be published
    /// @param aNoAuto if set, the published vdsm or vdc will not be automatically connected (only when explicitly whitelisted)
    /// @param aHostname unique hostname to be published
    /// @param aAuxVdsmDsUid if not NULL, discovery manager will also manage advertising of the vdsm
    /// @param aAuxVdsmPort port to connect the vdsm from ds485p
    /// @param aAuxVdsmRunning must be true if the auxiliary vdsm is running right now, false if not.
    /// @param aAuxVdsmStatusHandler will be called when discovery detects or looses master vdsm
    /// @param aNotAuxiliary if set, vdsm will always run and will not include the "auxiliary" TXT record in the advertisement
    /// @param aIgmpSnoopingHints if set, some extra IGMP packets will be sent to help IGMP snooping in broken networks
    /// @return error in case discovery manager could not be started
    ErrorPtr start(
      DeviceContainerPtr aDeviceContainer,
      const char *aHostname,
      bool aNoAuto,
      int aWebPort,
      int aSshPort,
      bool aIgmpSnoopingHints,
      DsUidPtr aAuxVdsmDsUid, int aAuxVdsmPort, bool aAuxVdsmRunning, AuxVdsmStatusHandler aAuxVdsmStatusHandler, bool aNotAuxiliary
    );

    /// stop advertising and scanning
    void stop();

  private:

    void startServices();
    void stopServices();
    void restartServices();
    void periodicIgmpQuery();

    #if ENABLE_AUXVDSM
    void startBrowsingVdms(AvahiService *aService);
    void rescanVdsms(AvahiService *aService);
    void evaluateState();
    #endif

    // callbacks
    static void avahi_log(AvahiLogLevel level, const char *txt);
    #if USE_AVAHI_CORE
    static void server_callback(AvahiServer *s, AvahiServerState state, void* userdata);
    static void entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata);
    void avahi_server_callback(AvahiServer *s, AvahiServerState state);
    #else
    static void client_callback(AvahiClient *c, AvahiClientState state, void* userdata);
    static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void* userdata);
    void avahi_client_callback(AvahiClient *c, AvahiClientState state);
    #endif

    bool avahi_poll();
    void create_services(AvahiService *aService);
    void avahi_entry_group_callback(AvahiService *aService, AvahiEntryGroup *g, AvahiEntryGroupState state);

    #if ENABLE_AUXVDSM
    static void browse_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void* userdata);
    static void resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata);
    void avahi_browse_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags);
    void avahi_resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags);
    #endif
  };

  typedef boost::intrusive_ptr<DiscoveryManager> DiscoveryManagerPtr;


} // namespace p44

#endif // !DISABLE_DISCOVERY
#endif // __vdcd__discovery__
