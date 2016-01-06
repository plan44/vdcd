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


#include "application.hpp"

#include "device.hpp"
#include "devicecontainer.hpp"

// APIs to be used
#include "jsonvdcapi.hpp"
#include "pbufvdcapi.hpp"

// device classes to be used
#include "demodevicecontainer.hpp"
#include "upnpdevicecontainer.hpp"

#define DEFAULT_USE_MODERN_DSIDS 1 // 0: no, 1: yes
#define DEFAULT_USE_PROTOBUF_API 1 // 0: no, 1: yes

#define DEFAULT_JSON_VDSMSERVICE "8440"
#define DEFAULT_PBUF_VDSMSERVICE "8340"
#define DEFAULT_DBDIR "/tmp"

#define DEFAULT_LOGLEVEL LOG_NOTICE

#define MAINLOOP_CYCLE_TIME_uS 20000 // 20mS

using namespace p44;


/// A command line app for a Demo vdc host
class DemoVdc : public CmdLineApp
{
  typedef CmdLineApp inherited;

  // the device container
  // Note: must be a intrusive ptr, as it is referenced by intrusive ptrs later. Statically defining it leads to crashes.
  DeviceContainerPtr deviceContainer;

public:

  DemoVdc()
  {
  }

  virtual int main(int argc, char **argv)
  {
    const char *usageText =
      "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 0  , "protobufapi",   true,  "enabled;1=use Protobuf API, 0=use JSON RPC 2.0 API" },
      { 'C', "vdsmport",      true,  "port;port number/service name for vdSM to connect to (default pbuf:" DEFAULT_PBUF_VDSMSERVICE ", JSON:" DEFAULT_JSON_VDSMSERVICE ")" },
      { 'i', "vdsmnonlocal",  false, "allow vdSM connections from non-local clients" },
      { 'l', "loglevel",      true,  "level;set max level of log message detail to show on stdout" },
      { 0  , "errlevel",      true,  "level;set max level for log messages to go to stderr as well" },
      { 0  , "dontlogerrors", false, "don't duplicate error messages (see --errlevel) on stdout" },
      { 's', "sqlitedir",     true,  "dirpath;set SQLite DB directory (default = " DEFAULT_DBDIR ")" },
      { 'h', "help",          false, "show this text" },
      { 0, NULL } // list terminator
    };

    // parse the command line, exits when syntax errors occur
    setCommandDescriptors(usageText, options);
    parseCommandLine(argc, argv);

    // create the root object
    deviceContainer = DeviceContainerPtr(new DeviceContainer);

    // log level?
    int loglevel = DEFAULT_LOGLEVEL;
    getIntOption("loglevel", loglevel);
    SETLOGLEVEL(loglevel);
    int errlevel = LOG_ERR;
    getIntOption("errlevel", errlevel);
    SETERRLEVEL(errlevel, !getOption("dontlogerrors"));

    // Init the device container root object
    // - set DB dir
    const char *dbdir = DEFAULT_DBDIR;
    getStringOption("sqlitedir", dbdir);
    deviceContainer->setPersistentDataDir(dbdir);
    // - set up vDC API
    int protobufapi = DEFAULT_USE_PROTOBUF_API;
    getIntOption("protobufapi", protobufapi);
    const char *vdsmport;
    if (protobufapi) {
      deviceContainer->vdcApiServer = VdcApiServerPtr(new VdcPbufApiServer());
      vdsmport = (char *) DEFAULT_PBUF_VDSMSERVICE;
    }
    else {
      deviceContainer->vdcApiServer = VdcApiServerPtr(new VdcJsonApiServer());
      vdsmport = (char *) DEFAULT_JSON_VDSMSERVICE;
    }
    // set up server for vdSM to connect to
    getStringOption("vdsmport", vdsmport);
    deviceContainer->vdcApiServer->setConnectionParams(NULL, vdsmport, SOCK_STREAM, AF_INET);
    deviceContainer->vdcApiServer->setAllowNonlocalConnections(getOption("vdsmnonlocal"));

    // Now add device class(es)
    // - the demo device (dimmer value output to console as bar of hashes ######) class
    DemoDeviceContainerPtr demoDeviceContainer = DemoDeviceContainerPtr(new DemoDeviceContainer(1, deviceContainer.get(), 1));
    demoDeviceContainer->addClassToDeviceContainer();
    // - the UPnP skeleton device from the developer days 2013 hackaton
    UpnpDeviceContainerPtr upnpDeviceContainer = UpnpDeviceContainerPtr(new UpnpDeviceContainer(1, deviceContainer.get(), 2));
    upnpDeviceContainer->addClassToDeviceContainer();
    // now start running the mainloop
    return run();
  }

  virtual void initialize()
  {
    // - initialize the device container
    deviceContainer->initialize(boost::bind(&DemoVdc::initialized, this, _1), false); // no factory reset
  }


  virtual void initialized(ErrorPtr aError)
  {
    if (!Error::isOK(aError)) {
      // cannot initialize, this is a fatal error
      LOG(LOG_ERR, "Cannot initialize device container - fatal error");
      terminateApp(EXIT_FAILURE);
    }
    else {
      // init ok, collect devices
      deviceContainer->collectDevices(boost::bind(&DemoVdc::devicesCollected, this, _1), false, false, false); // no forced full scan (only if needed)
    }
  }

  virtual void devicesCollected(ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      LOG(LOG_INFO, deviceContainer->description().c_str());
    }
    else {
      LOG(LOG_ERR, "Cannot collect devices - fatal error");
      terminateApp(EXIT_FAILURE);
    }
  }

};


int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  // create the mainloop
  MainLoop::currentMainLoop().setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static DemoVdc application;
  // pass control
  return application.main(argc, argv);
}
