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

#include "application.hpp"

#include "p44_vdcd_host.hpp"

// APIs to be used
#include "jsonvdcapi.hpp"
#include "pbufvdcapi.hpp"

// device classes to be used
#if !DISABLE_DALI
#include "dalidevicecontainer.hpp"
#endif
#if !DISABLE_ENOCEAN
#include "enoceandevicecontainer.hpp"
#endif
#if !DISABLE_HUE
#include "huedevicecontainer.hpp"
#endif
#if !DISABLE_OLA
#include "oladevicecontainer.hpp"
#endif
#if !DISABLE_LEDCHAIN
#include "ledchaindevicecontainer.hpp"
#endif
#if !DISABLE_STATIC
#include "staticdevicecontainer.hpp"
#endif
#if !DISABLE_EXTERNAL
#include "externaldevicecontainer.hpp"
#endif

#if ENABLE_VOXNET
#include "voxnetdevicecontainer.hpp"
#endif
#if ENABLE_VZUGHOME
#include "vzughomedevicecontainer.hpp"
#endif


#if !DISABLE_DISCOVERY
#include "discovery.hpp"
#endif

#include "digitalio.hpp"


#define DEFAULT_USE_PROTOBUF_API 1 // 0: no, 1: yes

#define DEFAULT_DALIPORT 2101
#define DEFAULT_ENOCEANPORT 2102
#define DEFAULT_JSON_VDSMSERVICE "8440"
#define DEFAULT_PBUF_VDSMSERVICE "8340"
#define DEFAULT_DBDIR "/tmp"

#define DEFAULT_DS485_AUXVDSMPORT 8441

#define DEFAULT_LOGLEVEL LOG_NOTICE


#define MAINLOOP_CYCLE_TIME_uS 33333 // 33mS


#define P44_EXIT_LOCALMODE 2 // request daemon restart in "local mode"
#define P44_EXIT_FIRMWAREUPDATE 3 // request check for new firmware, installation if available, platform restart
#define P44_EXIT_FACTORYRESET 42 // request a factory reset and platform restart
#define P44_EXIT_START_AUXVDSM 4 // exit, start auxiliary vdsm, and restart daemon
#define P44_EXIT_STOP_AUXVDSM 5 // exit, stop auxiliary vdsm, and restart daemon



using namespace p44;


/// Main program for plan44.ch P44-DSB-DEH in form of the "vdcd" daemon)
class P44Vdcd : public CmdLineApp
{
  typedef CmdLineApp inherited;

  // status
  typedef enum {
    status_ok,  // ok, all normal
    status_busy,  // busy, but normal
    status_interaction, // expecting user interaction
    status_error, // error
    status_fatalerror  // fatal error that needs user interaction
  } AppStatus;

  typedef enum {
    tempstatus_none,  // no temp activity status
    tempstatus_activityflash,  // activity LED flashing (yellow flash)
    tempstatus_buttonpressed, // button is pressed (steady yellow)
    tempstatus_buttonpressedlong, // button is pressed longer (steady red)
    tempstatus_factoryresetwait, // detecting possible factory reset (blinking red)
    tempstatus_success,  // success/learn-in indication (green blinking)
    tempstatus_failure,  // failure/learn-out indication (red blinking)
  } TempStatus;

  #if !DISABLE_STATIC
  // command line defined devices
  DeviceConfigMap staticDeviceConfigs;
  #endif

  // App status
  bool factoryResetWait;
  AppStatus appStatus;
  TempStatus currentTempStatus;
  long tempStatusTicket;
  bool selfTesting;

  // the device container
  // Note: must be a intrusive ptr, as it is referenced by intrusive ptrs later. Statically defining it leads to crashes.
  P44VdcHostPtr p44VdcHost;

  // indicators and button
  IndicatorOutputPtr redLED;
  IndicatorOutputPtr greenLED;
  ButtonInputPtr button;

  // learning
  long learningTimerTicket;

  // discovery
  #if !DISABLE_DISCOVERY
  DiscoveryManagerPtr discoveryManager;
  #endif


public:

  P44Vdcd() :
    appStatus(status_busy),
    currentTempStatus(tempstatus_none),
    factoryResetWait(false),
    tempStatusTicket(0),
    learningTimerTicket(0),
    selfTesting(false)
  {
  }

  void setAppStatus(AppStatus aStatus)
  {
    appStatus = aStatus;
    // update LEDs
    showAppStatus();
  }

  void indicateTempStatus(TempStatus aStatus)
  {
    if (aStatus>=currentTempStatus) {
      // higher priority than current temp status, apply
      currentTempStatus = aStatus; // overrides app status updates for now
      MainLoop::currentMainLoop().cancelExecutionTicket(tempStatusTicket);
      // initiate
      MLMicroSeconds timer = Never;
      switch (aStatus) {
        case tempstatus_activityflash:
          // short yellow LED flash
          if (appStatus==status_ok) {
            // activity flashes only during normal operation
            timer = 50*MilliSecond;
            redLED->steadyOn();
            greenLED->steadyOn();
          }
          else {
            currentTempStatus = tempstatus_none;
          }
          break;
        case tempstatus_buttonpressed:
          // just yellow
          redLED->steadyOn();
          greenLED->steadyOn();
          break;
        case tempstatus_buttonpressedlong:
          // just red
          redLED->steadyOn();
          greenLED->steadyOff();
          break;
        case tempstatus_factoryresetwait:
          // fast red blinking
          greenLED->steadyOff();
          redLED->blinkFor(p44::Infinite, 200*MilliSecond, 20);
          break;
        case tempstatus_success:
          timer = 1600*MilliSecond;
          redLED->steadyOff();
          greenLED->blinkFor(timer, 400*MilliSecond, 30);
          break;
        case tempstatus_failure:
          timer = 1600*MilliSecond;
          greenLED->steadyOff();
          redLED->blinkFor(timer, 400*MilliSecond, 30);
          break;
        default:
          break;
      }
      if (timer!=Never) {
        tempStatusTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::endTempStatus, this), timer);
      }
    }
  }


  void endTempStatus()
  {
    MainLoop::currentMainLoop().cancelExecutionTicket(tempStatusTicket);
    currentTempStatus = tempstatus_none;
    showAppStatus();
  }


  // show global status on LEDs
  void showAppStatus()
  {
    if (currentTempStatus==tempstatus_none) {
      switch (appStatus) {
        case status_ok:
          redLED->steadyOff();
          greenLED->steadyOn();
          break;
        case status_busy:
          greenLED->steadyOn();
          redLED->steadyOn();
          break;
        case status_interaction:
          greenLED->blinkFor(p44::Infinite, 400*MilliSecond, 80);
          redLED->blinkFor(p44::Infinite, 400*MilliSecond, 80);
          break;
        case status_error:
          LOG(LOG_ERR, "****** Error - operation may not continue - check logs!");
          greenLED->steadyOff();
          redLED->steadyOn();
          break;
        case status_fatalerror:
          LOG(LOG_ALERT, "****** Fatal error - operation cannot continue - try restarting!");
          greenLED->steadyOff();
          redLED->blinkFor(p44::Infinite, 800*MilliSecond, 50);;
          break;
      }
    }
  }

  void activitySignal()
  {
    indicateTempStatus(tempstatus_activityflash);
  }



  #if !DISABLE_STATIC
  virtual bool processOption(const CmdLineOptionDescriptor &aOptionDescriptor, const char *aOptionValue)
  {
    if (strcmp(aOptionDescriptor.longOptionName,"digitalio")==0) {
      staticDeviceConfigs.insert(make_pair("digitalio", aOptionValue));
    }
    if (strcmp(aOptionDescriptor.longOptionName,"analogio")==0) {
      staticDeviceConfigs.insert(make_pair("analogio", aOptionValue));
    }
    else if (strcmp(aOptionDescriptor.longOptionName,"consoleio")==0) {
      staticDeviceConfigs.insert(make_pair("console", aOptionValue));
    }
    else if (strcmp(aOptionDescriptor.longOptionName,"sparkcore")==0) {
      staticDeviceConfigs.insert(make_pair("spark", aOptionValue));
    }
    else {
      return inherited::processOption(aOptionDescriptor, aOptionValue);
    }
    return true;
  }
  #endif


  virtual int main(int argc, char **argv)
  {
    const char *usageText =
      "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 0  , "dsuid",         true,  "dSUID;set dSUID for this vDC host (usually UUIDv1 generated on the host)" },
      { 0  , "sgtin",         true,  "part,gcp,itemref,serial;set dSUID for this vDC as SGTIN" },
      { 0  , "productname",   true,  "name;set product name for this vdc host and its vdcs" },
      { 0  , "productversion",true,  "version;set version string for this vdc host and its vdcs" },
      { 0  , "deviceid",      true,  "device id;a string that may identify the device to the end user, e.g. a serial number" },
      #if !DISABLE_DALI
      { 'a', "dali",          true,  "bridge;DALI bridge serial port device or proxy host[:port]" },
      { 0  , "daliportidle",  true,  "seconds;DALI serial port will be closed after this timeout and re-opened on demand only" },
      { 0  , "dalitxadj",     true,  "adjustment;DALI signal adjustment for sending" },
      { 0  , "dalirxadj",     true,  "adjustment;DALI signal adjustment for receiving" },
      #endif
      #if !DISABLE_ENOCEAN
      { 'b', "enocean",       true,  "bridge;EnOcean modem serial port device or proxy host[:port]" },
      { 0,   "enoceanreset",  true,  "pinspec;set I/O pin connected to EnOcean module reset" },
      #endif
      #if !DISABLE_HUE
      { 0,   "huelights",     false, "enable support for hue LED lamps (via hue bridge)" },
      { 0,   "hueapiurl",     true,  "hue API url;use hue bridge API at specific location (disables UPnP/SSDP search)" },
      #endif
      #if !DISABLE_OLA
      { 0,   "ola",           false, "enable support for OLA (Open Lighting Architecture) server" },
      #endif
      #if !DISABLE_LEDCHAIN
      { 0,   "ledchain",      true,  "numleds;enable support for LED chains forming one or multiple RGB lights" },
      { 0,   "ledchainmax",   true,  "max;max output value (0..255) sent to LED. Defaults to 128" },
      #endif
      #if ENABLE_VOXNET
      { 0,   "voxnet",        false, "enable support for Revox Voxnet" },
      #endif
      #if ENABLE_VZUGHOME
      { 0,   "vzughome",      false, "enable support for V-Zug Home" },
      #endif
      #if !DISABLE_EXTERNAL
      { 0,   "externaldevices",true, "port/socketpath;enable support for external devices connecting via specified port or local socket path" },
      { 0,   "externalnonlocal", false, "allow external device connections from non-local clients" },
      #endif
      #if !DISABLE_STATIC
      { 0,   "staticdevices", false, "enable support for statically defined devices" },
      { 0  , "sparkcore",     true,  "sparkCoreID:authToken;add spark core based cloud device" },
      { 'g', "digitalio",     true,  "iospec:[!](button|light|relay);add static digital input or output device\n"
                                     "Use ! for inverted polarity (default is noninverted input)\n"
                                     "iospec is of form [bus.[device.]]pin:"
                                     "\n- gpio.gpionumber : generic Linux GPIO"
      #if !DISABLE_I2C
                                     "\n- i2cN.DEVICE@i2caddr.pinNumber : numbered pin of device at i2caddr on i2c bus N "
                                     "(supported for DEVICE : TCA9555, PCF8574)"
      #endif
                                     },
      { 0  , "analogio",      true,  "iospec:(dimmer|rgbdimmer|valve);add static analog input or output device\n"
                                     "iospec is of form [bus.[device.]]pin:"
      #if !DISABLE_I2C
                                     "\n- i2cN.DEVICE@i2caddr.pinNumber : numbered pin of device at i2caddr on i2c bus N "
                                     "(supported for DEVICE : PCA9685)"
      #endif
                                     },
      { 'k', "consoleio",     true,  "name[:(dimmer|colordimmer|button|valve)];add static debug device which reads and writes console "
                                     "(for inputs: first char of name=action key)" },
      #endif // !DISABLE_STATIC
      { 0  , "protobufapi",   true,  "enabled;1=use Protobuf API, 0=use JSON RPC 2.0 API" },
      #if !DISABLE_DISCOVERY
      { 0  , "noauto",        false, "prevent auto-connection to this vdc host" },
      { 0  , "nodiscovery",   false, "completely disable discovery (no publishing of services)" },
      { 0  , "hostname",      true,  "hostname;host name to use to publish this vdc host" },
      { 0  , "auxvdsmdsuid",  true,  NULL /* dSUID; dsuid of auxiliary vdsm to be managed */ },
      { 0  , "auxvdsmport",   true,  NULL /* port; port of auxiliary vdsm's ds485 server */ },
      { 0  , "auxvdsmrunning",false, NULL /* must be set when auxiliary vdsm is running */ },
      { 0  , "vdsmnotaux"    ,false, NULL /* can be set to make vdsm non-auxiliary */ },
      { 0  , "sshport",       true,  "portno;publish ssh access at given port" },
      #endif
      { 0  , "webuiport",     true,  "portno;publish a Web-UI service at given port" },
      { 'C', "vdsmport",      true,  "port;port number/service name for vdSM to connect to (default pbuf:" DEFAULT_PBUF_VDSMSERVICE ", JSON:" DEFAULT_JSON_VDSMSERVICE ")" },
      { 'i', "vdsmnonlocal",  false, "allow vdSM connections from non-local clients" },
      { 'w', "startupdelay",  true,  "seconds;delay startup" },
      { 'l', "loglevel",      true,  "level;set max level of log message detail to show on stdout" },
      { 0  , "errlevel",      true,  "level;set max level for log messages to go to stderr as well" },
      { 0  , "mainloopstats", true,  "interval;0=no stats, 1..N interval (5Sec steps)" },
      { 0  , "dontlogerrors", false, "don't duplicate error messages (see --errlevel) on stdout" },
      { 's', "sqlitedir",     true,  "dirpath;set SQLite DB directory (default = " DEFAULT_DBDIR ")" },
      { 0  , "icondir",       true,  "icon directory;specifiy path to directory containing device icons" },
      { 'W', "cfgapiport",    true,  "port;server port number for web configuration JSON API (default=none)" },
      { 0  , "cfgapinonlocal",false, "allow web configuration JSON API from non-local clients" },

      { 0  , "greenled",      true,  "pinspec;set I/O pin connected to green part of status LED" },
      { 0  , "redled",        true,  "pinspec;set I/O pin connected to red part of status LED" },
      { 0  , "button",        true,  "pinspec;set I/O pin connected to learn button" },
      { 0,   "selftest",      false, "run in self test mode" },
      { 'h', "help",          false, "show this text" },
      { 0, NULL } // list terminator
    };

    // parse the command line, exits when syntax errors occur
    setCommandDescriptors(usageText, options);
    parseCommandLine(argc, argv);

    if ((numOptions()<1) || numArguments()>0) {
      // show usage
      showUsage();
      terminateApp(EXIT_SUCCESS);
    }

    // create the root object
    p44VdcHost = P44VdcHostPtr(new P44VdcHost);

    // test or operation
    selfTesting = getOption("selftest");

    // log level?
    int loglevel = DEFAULT_LOGLEVEL;
    getIntOption("loglevel", loglevel);
    SETLOGLEVEL(loglevel);
    int errlevel = selfTesting ? LOG_EMERG: LOG_ERR; // testing by default only reports to stdout
    getIntOption("errlevel", errlevel);
    SETERRLEVEL(errlevel, !getOption("dontlogerrors"));

    // startup delay?
    int startupDelay = 0; // no delay
    getIntOption("startupdelay", startupDelay);

    // web ui
    int webUiPort = 0;
    getIntOption("webuiport", webUiPort);
    p44VdcHost->webUiPort = webUiPort;

    // before starting anything, delay
    if (startupDelay>0) {
      LOG(LOG_NOTICE, "Delaying startup by %d seconds (-w command line option)", startupDelay);
      sleep(startupDelay);
    }

    // Connect LEDs and button
    const char *pinName;
    pinName = "missing";
    getStringOption("greenled", pinName);
    greenLED = IndicatorOutputPtr(new IndicatorOutput(pinName, false, false));
    pinName = "missing";
    getStringOption("redled", pinName);
    redLED = IndicatorOutputPtr(new IndicatorOutput(pinName, false, false));
    pinName = "missing";
    getStringOption("button", pinName);
    button = ButtonInputPtr(new ButtonInput(pinName, true));

    // now show status for the first time
    showAppStatus();

    // Check for factory reset as very first action, to avoid that corrupt data might already crash the daemon
    // before we can do the factory reset
    if (button->isSet()) {
      // started with button pressed - go into factory reset wait mode
      factoryResetWait = true;
      indicateTempStatus(tempstatus_factoryresetwait);
    }
    else {
      // Init the device container root object
      // - set DB dir
      const char *dbdir = DEFAULT_DBDIR;
      getStringOption("sqlitedir", dbdir);
      p44VdcHost->setPersistentDataDir(dbdir);

      // - set icon directory
      const char *icondir = NULL;
      getStringOption("icondir", icondir);
      p44VdcHost->setIconDir(icondir);
      string s;

      // - set dSUID mode
      DsUidPtr externalDsUid;
      if (getStringOption("dsuid", s)) {
        externalDsUid = DsUidPtr(new DsUid(s));
      }
      else if (getStringOption("sgtin", s)) {
        int part;
        uint64_t gcp;
        uint32_t itemref;
        uint64_t serial;
        sscanf(s.c_str(), "%d,%llu,%u,%llu", &part, &gcp, &itemref, &serial);
        externalDsUid = DsUidPtr(new DsUid(s));
        externalDsUid->setGTIN(gcp, itemref, part);
        externalDsUid->setSerial(serial);
      }
      p44VdcHost->setIdMode(externalDsUid);

      // - set product name and version
      if (getStringOption("productname", s)) {
        p44VdcHost->setProductName(s);
      }
      // - set product name and version
      if (getStringOption("productversion", s)) {
        p44VdcHost->setProductVersion(s);
      }
      // - set product name and version
      if (getStringOption("deviceid", s)) {
        p44VdcHost->setDeviceHardwareId(s);
      }

      // - set custom mainloop statistics output interval
      int mainloopStatsInterval;
      if (getIntOption("mainloopstats", mainloopStatsInterval)){
        p44VdcHost->setMainloopStatsInterval(mainloopStatsInterval);
      }

      // - set API
      int protobufapi = DEFAULT_USE_PROTOBUF_API;
      getIntOption("protobufapi", protobufapi);
      const char *vdcapiservice;
      if (protobufapi) {
        p44VdcHost->vdcApiServer = VdcApiServerPtr(new VdcPbufApiServer());
        vdcapiservice = (char *) DEFAULT_PBUF_VDSMSERVICE;
      }
      else {
        p44VdcHost->vdcApiServer = VdcApiServerPtr(new VdcJsonApiServer());
        vdcapiservice = (char *) DEFAULT_JSON_VDSMSERVICE;
      }
      // set up server for vdSM to connect to
      getStringOption("vdsmport", vdcapiservice);
      p44VdcHost->vdcApiServer->setConnectionParams(NULL, vdcapiservice, SOCK_STREAM, AF_INET);
      p44VdcHost->vdcApiServer->setAllowNonlocalConnections(getOption("vdsmnonlocal"));


      // Create Web configuration JSON API server
      const char *configApiPort = getOption("cfgapiport");
      if (configApiPort) {
        p44VdcHost->configApiServer->setConnectionParams(NULL, configApiPort, SOCK_STREAM, AF_INET);
        p44VdcHost->configApiServer->setAllowNonlocalConnections(getOption("cfgapinonlocal"));
        p44VdcHost->startConfigApi();
      }

      // Create static container structure
      // - Add DALI devices class if DALI bridge serialport/host is specified
      #if !DISABLE_DALI
      const char *daliname = getOption("dali");
      if (daliname) {
        int sec = 0;
        getIntOption("daliportidle", sec);
        DaliDeviceContainerPtr daliDeviceContainer = DaliDeviceContainerPtr(new DaliDeviceContainer(1, p44VdcHost.get(), 1)); // Tag 1 = DALI
        daliDeviceContainer->daliComm->setConnectionSpecification(daliname, DEFAULT_DALIPORT, sec*Second);
        int adj;
        if (getIntOption("dalitxadj", adj)) daliDeviceContainer->daliComm->setDaliSendAdj(adj);
        if (getIntOption("dalirxadj", adj)) daliDeviceContainer->daliComm->setDaliSampleAdj(adj);
        daliDeviceContainer->addClassToDeviceContainer();
      }
      #endif
      #if !DISABLE_ENOCEAN
      // - Add EnOcean devices class if EnOcean modem serialport/host is specified
      const char *enoceanname = getOption("enocean");
      const char *enoceanresetpin = getOption("enoceanreset");
      if (enoceanname) {
        EnoceanDeviceContainerPtr enoceanDeviceContainer = EnoceanDeviceContainerPtr(new EnoceanDeviceContainer(1, p44VdcHost.get(), 2)); // Tag 2 = EnOcean
        enoceanDeviceContainer->enoceanComm.setConnectionSpecification(enoceanname, DEFAULT_ENOCEANPORT, enoceanresetpin);
        // add
        enoceanDeviceContainer->addClassToDeviceContainer();
      }
      #endif
      #if !DISABLE_HUE
      // - Add hue support
      if (getOption("huelights")) {
        HueDeviceContainerPtr hueDeviceContainer = HueDeviceContainerPtr(new HueDeviceContainer(1, p44VdcHost.get(), 3)); // Tag 3 = hue
        string apiurl;
        if (getStringOption("hueapiurl", apiurl)) {
          hueDeviceContainer->hueComm.fixedBaseURL = apiurl;
        }
        hueDeviceContainer->addClassToDeviceContainer();
      }
      #endif
      #if !DISABLE_OLA
      // - Add OLA support
      if (getOption("ola")) {
        OlaDeviceContainerPtr olaDeviceContainer = OlaDeviceContainerPtr(new OlaDeviceContainer(1, p44VdcHost.get(), 5)); // Tag 5 = ola
        olaDeviceContainer->addClassToDeviceContainer();
      }
      #endif
      #if !DISABLE_LEDCHAIN
      // - Add Led chain support
      int numleds;
      if (getIntOption("ledchain", numleds)) {
        if (numleds>0) {
          LedChainDeviceContainerPtr ledChainDeviceContainer = LedChainDeviceContainerPtr(new LedChainDeviceContainer(1, numleds, p44VdcHost.get(), 6)); // Tag 6 = led chain
          ledChainDeviceContainer->addClassToDeviceContainer();
          int maxOutValue;
          if (getIntOption("ledchainmax", maxOutValue)) {
            ledChainDeviceContainer->setMaxOutValue(maxOutValue);
          }
        }
      }
      #endif

      #if ENABLE_VOXNET
      // - Add Voxnet support
      if (getOption("voxnet")) {
        VoxnetDeviceContainerPtr voxnetDeviceContainer = VoxnetDeviceContainerPtr(new VoxnetDeviceContainer(1, p44VdcHost.get(), 50)); // Tag 50 = Voxnet
        voxnetDeviceContainer->addClassToDeviceContainer();
      }
      #endif
      #if ENABLE_VZUGHOME
      // - Add V-Zug Home support
      if (getOption("vzughome")) {
        VZugHomeDeviceContainerPtr vzughomeDeviceContainer = VZugHomeDeviceContainerPtr(new VZugHomeDeviceContainer(1, p44VdcHost.get(), 51)); // Tag 51 = VZugHome
        vzughomeDeviceContainer->addClassToDeviceContainer();
      }
      #endif


      #if !DISABLE_STATIC
      // - Add static devices if we explictly want it or have collected any config from the command line
      if (getOption("staticdevices") || staticDeviceConfigs.size()>0) {
        StaticDeviceContainerPtr staticDeviceContainer = StaticDeviceContainerPtr(new StaticDeviceContainer(1, staticDeviceConfigs, p44VdcHost.get(), 4)); // Tag 4 = static
        staticDeviceContainer->addClassToDeviceContainer();
        staticDeviceConfigs.clear(); // no longer needed, free memory
      }
      #endif
      #if !DISABLE_EXTERNAL
      // - Add support for external devices connecting via socket
      const char *extdevname = getOption("externaldevices");
      if (extdevname) {
        ExternalDeviceContainerPtr externalDeviceContainer = ExternalDeviceContainerPtr(new ExternalDeviceContainer(1, extdevname, getOption("externalnonlocal"), p44VdcHost.get(), 7)); // Tag 7 = external
        externalDeviceContainer->addClassToDeviceContainer();
      }
      #endif
      // install activity monitor
      p44VdcHost->setActivityMonitor(boost::bind(&P44Vdcd::activitySignal, this));
    }
    // app now ready to run
    return run();
  }


  #define LEARN_TIMEOUT (20*Second)


  void deviceLearnHandler(bool aLearnIn, ErrorPtr aError)
  {
    // back to normal...
    stopLearning(false);
    // ...but as we acknowledge the learning with the LEDs, schedule a update for afterwards
    MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::showAppStatus, this), 2*Second);
    // acknowledge the learning (if any, can also be timeout or manual abort)
    if (Error::isOK(aError)) {
      if (aLearnIn) {
        // show device learned
        indicateTempStatus(tempstatus_success);
      }
      else {
        // show device unlearned
        indicateTempStatus(tempstatus_failure);
      }
    }
    else {
      LOG(LOG_ERR, "Learning error: %s", aError->description().c_str());
    }
  }


  void stopLearning(bool aFromTimeout)
  {
    p44VdcHost->stopLearning();
    MainLoop::currentMainLoop().cancelExecutionTicket(learningTimerTicket);
    setAppStatus(status_ok);
    if (aFromTimeout) {
      // letting learn run into timeout will re-collect all devices
      collectDevices(true);
    }
  }



  virtual bool buttonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    LOG(LOG_NOTICE, "Device button event: state=%d, hasChanged=%d", aState, aHasChanged);
    // LED yellow as long as button pressed
    if (aHasChanged) {
      if (aState) indicateTempStatus(tempstatus_buttonpressed);
      else endTempStatus();
    }
    if (aState==true && !aHasChanged) {
      // keypress reported again
      if (aTimeSincePreviousChange>=5*Second) {
        // visually acknowledge long keypress by turning LED red
        indicateTempStatus(tempstatus_buttonpressedlong);
        LOG(LOG_WARNING, "Button held for >5 seconds now...");
      }
      // check for very long keypress
      if (aTimeSincePreviousChange>=15*Second) {
        // very long press (labelled "Factory reset" on the case)
        setAppStatus(status_error);
        LOG(LOG_WARNING, "Very long button press detected -> clean exit(%d) in 2 seconds", P44_EXIT_LOCALMODE);
        button->setButtonHandler(NULL, true); // disconnect button
        p44VdcHost->setActivityMonitor(NULL); // no activity monitoring any more
        // for now exit(2) is switching off daemon, so we switch off the LEDs as well
        redLED->steadyOff();
        greenLED->steadyOff();
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::terminateApp, this, P44_EXIT_LOCALMODE), 2*Second);
        return true;
      }
    }
    if (aState==false) {
      // keypress release
      if (aTimeSincePreviousChange>=5*Second) {
        // long press (labelled "Software Update" on the case)
        setAppStatus(status_busy);
        LOG(LOG_WARNING, "Long button press detected -> upgrade to latest firmware requested -> clean exit(%d) in 500 mS", P44_EXIT_FIRMWAREUPDATE);
        button->setButtonHandler(NULL, true); // disconnect button
        p44VdcHost->setActivityMonitor(NULL); // no activity monitoring any more
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::terminateApp, this, P44_EXIT_FIRMWAREUPDATE), 500*MilliSecond);
      }
      else {
        // short press: start/stop learning
        if (!learningTimerTicket) {
          // start
          setAppStatus(status_interaction);
          learningTimerTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::stopLearning, this, true), LEARN_TIMEOUT);
          p44VdcHost->startLearning(boost::bind(&P44Vdcd::deviceLearnHandler, this, _1, _2));
        }
        else {
          // stop
          stopLearning(false);
        }
      }
    }
    return true;
  }


  virtual bool fromStartButtonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    LOG(LOG_NOTICE, "Device button pressed from start event: state=%d, hasChanged=%d", aState, aHasChanged);
    if (aHasChanged && aState==false) {
      // released
      if (factoryResetWait && aTimeSincePreviousChange>20*Second) {
        // held in waiting-for-reset state more than 20 seconds -> FACTORY RESET
        LOG(LOG_WARNING, "Button pressed at startup and 20-30 seconds beyond -> FACTORY RESET = clean exit(%d) in 2 seconds", P44_EXIT_FACTORYRESET);
        // indicate red "error/danger" state
        redLED->steadyOn();
        greenLED->steadyOff();
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::terminateApp, this, P44_EXIT_FACTORYRESET), 2*Second);
        return true;
      }
      else {
        // held in waiting-for-reset state less than 20 seconds or more than 30 seconds -> just restart
        LOG(LOG_WARNING, "Button pressed at startup but less than 20 or more than 30 seconds -> normal restart = clean exit(0) in 0.5 seconds");
        // indicate yellow "busy" state
        redLED->steadyOn();
        greenLED->steadyOn();
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::terminateApp, this, EXIT_SUCCESS), 500*MilliSecond);
        return true;
      }
    }
    // if button is stuck, turn nervously yellow to indicate: something needs to be done
    if (factoryResetWait && !aHasChanged && aState) {
      if (aTimeSincePreviousChange>30*Second) {
        // end factory reset wait, assume button stuck or something
        factoryResetWait = false;
        // fast yellow blinking
        greenLED->blinkFor(p44::Infinite, 200*MilliSecond, 60);
        redLED->blinkFor(p44::Infinite, 200*MilliSecond, 60);
        // when button is released, a normal restart will occur, otherwise we'll remain in this state
      }
      else if (aTimeSincePreviousChange>20*Second) {
        // if released now, factory reset will occur (but if held still longer, will enter "button stuck" mode
        redLED->steadyOn();
        greenLED->steadyOff();
      }
    }
    return true;
  }




  virtual void initialize()
  {
    if (selfTesting) {
      // self testing
      // - initialize the device container
      p44VdcHost->initialize(boost::bind(&P44Vdcd::initialized, this, _1), false); // no factory reset
    }
    else if (factoryResetWait) {
      // button held during startup, check for factory reset
      // - connect special button hander
      button->setButtonHandler(boost::bind(&P44Vdcd::fromStartButtonHandler, this, _1, _2, _3), true, 1*Second);
    }
    else {
      // normal init
      // - connect button
      button->setButtonHandler(boost::bind(&P44Vdcd::buttonHandler, this, _1, _2, _3), true, 1*Second);
      // - initialize the device container
      p44VdcHost->initialize(boost::bind(&P44Vdcd::initialized, this, _1), false); // no factory reset
    }
  }



  virtual void initialized(ErrorPtr aError)
  {
    if (selfTesting) {
      // self test mode
      if (Error::isOK(aError)) {
        // start self testing (which might do some collecting if needed for testing)
        p44VdcHost->selfTest(boost::bind(&P44Vdcd::selfTestDone, this, _1), button, redLED, greenLED); // do the self test
      }
      else {
        // - init already unsuccessful, consider test failed, call test end routine directly
        selfTestDone(aError);
      }
    }
    else if (!Error::isOK(aError)) {
      // cannot initialize, this is a fatal error
      setAppStatus(status_fatalerror);
      // exit in 15 seconds
      LOG(LOG_ALERT, "****** Fatal error - vdc host initialisation failed: %s", aError->description().c_str());
      MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::terminateAppWith, this, aError), 15*Second);
      return;
    }
    else {
      // Initialized ok and not testing
      #if !DISABLE_DISCOVERY
      // - initialize discovery
      if (!getOption("nodiscovery")) {
        initDiscovery();
      }
      #endif
      // - start running normally
      p44VdcHost->startRunning();
      // - collect devices
      collectDevices(false);
    }
  }


  void selfTestDone(ErrorPtr aError)
  {
    // test done, exit with success or failure
    terminateApp(Error::isOK(aError) ? EXIT_SUCCESS : EXIT_FAILURE);
  }


  virtual void collectDevices(bool aIncremental)
  {
    // initiate device collection
    setAppStatus(status_busy);
    p44VdcHost->collectDevices(boost::bind(&P44Vdcd::devicesCollected, this, _1), aIncremental, false, false); // no forced full scan (only if needed), no settings clearing
  }


  virtual void devicesCollected(ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      setAppStatus(status_ok);
      DBGLOG(LOG_INFO, "%s", p44VdcHost->description().c_str());
    }
    else
      setAppStatus(status_error);
  }



  #if !DISABLE_DISCOVERY

  void initDiscovery()
  {
    discoveryManager = DiscoveryManagerPtr(new DiscoveryManager);
    // get discovery params
    // - host name
    string s;
    string hostname;
    if (!getStringOption("hostname", hostname)) {
      // none specified, create default
      hostname = string_format("plan44-vdcd-%s", p44VdcHost->getDsUid().getString().c_str());
    }
    // - optional auxiliary vdsm
    DsUidPtr auxVdsmDsuid;
    int auxVdsmPort = 0;
    if (getStringOption("auxvdsmdsuid", s)) {
      auxVdsmDsuid = DsUidPtr(new DsUid(s));
      auxVdsmPort = DEFAULT_DS485_AUXVDSMPORT;
      getIntOption("auxvdsmport", auxVdsmPort);
    }
    int sshPort = 0;
    getIntOption("sshport", sshPort);
    // start discovery manager
    ErrorPtr err = discoveryManager->start(
      p44VdcHost,
      hostname.c_str(),
      getOption("noauto"),
      p44VdcHost->webUiPort,
      sshPort,
      auxVdsmDsuid,
      auxVdsmPort,
      getOption("auxvdsmrunning"),
      boost::bind(&P44Vdcd::discoveryStatusHandler, this, _1),
      getOption("vdsmnotaux")
    );
    if (!Error::isOK(err)) {
      LOG(LOG_ERR, "**** Cannot start discovery manager: %s", err->description().c_str());
    }
  }


  void discoveryStatusHandler(bool aAuxVdsmShouldRun)
  {
    if (aAuxVdsmShouldRun) {
      LOG(LOG_WARNING, "***** auxiliary vdSM should start -> clean exit(%d) in 1 seconds", P44_EXIT_START_AUXVDSM);
    }
    else {
      LOG(LOG_WARNING, "***** auxiliary vdSM should stop -> clean exit(%d) in 1 seconds", P44_EXIT_STOP_AUXVDSM);
    }
    // needs to switch vdsms, means device is busy
    setAppStatus(status_busy);
    // give mainloop some time to close down API connections
    MainLoop::currentMainLoop().executeOnce(boost::bind(&P44Vdcd::terminateApp, this, aAuxVdsmShouldRun ? P44_EXIT_START_AUXVDSM : P44_EXIT_STOP_AUXVDSM), 1*Second);
  }

  #endif // discovery enabled


};





int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  // create the mainloop
  MainLoop::currentMainLoop().setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static P44Vdcd application;
  // pass control
  return application.main(argc, argv);
}
