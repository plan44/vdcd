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

#ifndef __vdcd__discovery__
#define __vdcd__discovery__

#include "vdcd_common.hpp"

#include "dsuid.hpp"
#include "devicecontainer.hpp"

// Avahi includes
#include <avahi-core/core.h>
#include <avahi-core/publish.h>
#include <avahi-core/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-core/log.h>


using namespace std;

namespace p44 {



  typedef boost::function<void (bool aShouldRun)> AuxVdsmStatusHandler;

  /// Implements service announcement and discovery (via avahi) for vdcd and (if configured) a associated vdsm
  class DiscoveryManager : public P44Obj
  {
    typedef P44Obj inherited;

    AvahiServer *server;
    AvahiSEntryGroup *entryGroup;
    AvahiSimplePoll *simple_poll;
    AvahiSServiceBrowser *serviceBrowser;
    AvahiSServiceBrowser *debugServiceBrowser;

    // publishing information
    // - common for all services
    DeviceContainerPtr deviceContainer;
    string hostname;
    bool noAuto;
    int publishWebPort;
    int publishSshPort;
    // - an optionally running auxiliary vdsm
    DsUidPtr auxVdsmDsUid;
    int auxVdsmPort;
    bool auxVdsmRunning;
    AuxVdsmStatusHandler auxVdsmStatusHandler;

    // state
    enum {
      dm_starting, // waiting for pusblishing to happen
      dm_started, // published
      dm_detected_master, // at least one master vdsm has been detected
      dm_lost_vdsm, // just lost a vdsm, re-discovering what is on the network
    } dmState;

    long rescanTicket;
    long evaluateTicket;


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
    /// @return error in case discovery manager could not be started
    ErrorPtr start(
      DeviceContainerPtr aDeviceContainer,
      const char *aHostname,
      bool aNoAuto,
      int aWebPort,
      int aSshPort,
      DsUidPtr aAuxVdsmDsUid, int aAuxVdsmPort, bool aAuxVdsmRunning, AuxVdsmStatusHandler aAuxVdsmStatusHandler
    );

    /// stop advertising and scanning
    void stop();

  private:

    string publishedName();
    void startServer();
    void stopServer();
    void restartServer();
    void startBrowsingVdms(AvahiServer *aServer);
    void rescanVdsms(AvahiServer *aServer);
    void evaluateState();

    static void avahi_log(AvahiLogLevel level, const char *txt);
    bool avahi_poll();
    static void server_callback(AvahiServer *s, AvahiServerState state, void* userdata);
    void avahi_server_callback(AvahiServer *s, AvahiServerState state);
    void create_services(AvahiServer *aAvahiServer);
    static void entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state, void* userdata);
    void avahi_entry_group_callback(AvahiServer *s, AvahiSEntryGroup *g, AvahiEntryGroupState state);
    static void browse_callback(AvahiSServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void* userdata);
    void avahi_browse_callback(AvahiSServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags);
    static void resolve_callback(AvahiSServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata);
    void avahi_resolve_callback(AvahiSServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags);
  };

  typedef boost::intrusive_ptr<DiscoveryManager> DiscoveryManagerPtr;


} // namespace p44

#endif /* defined(__vdcd__discovery__) */
