/*
 * main.cpp
 * vdcd
 *
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include "application.hpp"

#include "devicecontainer.hpp"

#include "dalidevicecontainer.hpp"
#include "huedevicecontainer.hpp"
#include "enoceandevicecontainer.hpp"
#include "staticdevicecontainer.hpp"

#include "jsonrpccomm.hpp"

#include "digitalio.hpp"


#define DEFAULT_USE_MODERN_DSIDS 0 // 0: no, 1: yes

#define DEFAULT_DALIPORT 2101
#define DEFAULT_ENOCEANPORT 2102
#define DEFAULT_VDSMSERVICE "8440"
#define DEFAULT_DBDIR "/tmp"

#define DEFAULT_LOGLEVEL LOG_NOTICE


#define MAINLOOP_CYCLE_TIME_uS 20000 // 20mS

using namespace p44;


class P44bridged : public CmdLineApp
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

  // command line defined devices
  DeviceConfigMap staticDeviceConfigs;

  // App status
  bool factoryResetWait;
  AppStatus appStatus;
  TempStatus currentTempStatus;
  long tempStatusTicket;

  // the device container
  DeviceContainer deviceContainer;

  // indicators and button
  IndicatorOutput redLED;
  IndicatorOutput greenLED;
  ButtonInput button;

  // learning
  long learningTimerTicket;

  // Configuration API
  SocketComm configApiServer;

public:

  P44bridged() :
    redLED("gpioNS9XXXX.ledred", false, false),
    greenLED("gpioNS9XXXX.ledgreen", false, false),
    button("gpioNS9XXXX.button", true),
    appStatus(status_busy),
    currentTempStatus(tempstatus_none),
    factoryResetWait(false),
    tempStatusTicket(0),
    learningTimerTicket(0),
    configApiServer(SyncIOMainLoop::currentMainLoop())
  {
    showAppStatus();
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
            redLED.steadyOn();
            greenLED.steadyOn();
          }
          else {
            currentTempStatus = tempstatus_none;
          }
          break;
        case tempstatus_buttonpressed:
          // just yellow
          redLED.steadyOn();
          greenLED.steadyOn();
          break;
        case tempstatus_buttonpressedlong:
          // just red
          redLED.steadyOn();
          greenLED.steadyOff();
          break;
        case tempstatus_factoryresetwait:
          // fast red blinking
          greenLED.steadyOff();
          redLED.blinkFor(p44::Infinite, 200*MilliSecond, 20);
          break;
        case tempstatus_success:
          timer = 1600*MilliSecond;
          redLED.steadyOff();
          greenLED.blinkFor(timer, 400*MilliSecond, 30);
          break;
        case tempstatus_failure:
          timer = 1600*MilliSecond;
          greenLED.steadyOff();
          redLED.blinkFor(timer, 400*MilliSecond, 30);
          break;
        default:
          break;
      }
      if (timer!=Never) {
        tempStatusTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::endTempStatus, this), timer);
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
          redLED.steadyOff();
          greenLED.steadyOn();
          break;
        case status_busy:
          greenLED.steadyOn();
          redLED.steadyOn();
          break;
        case status_interaction:
          greenLED.blinkFor(p44::Infinite, 400*MilliSecond, 80);
          redLED.blinkFor(p44::Infinite, 400*MilliSecond, 80);
          break;
        case status_error:
          greenLED.steadyOff();
          redLED.steadyOn();
          break;
        case status_fatalerror:
          greenLED.steadyOff();
          redLED.blinkFor(p44::Infinite, 800*MilliSecond, 50);;
          break;
      }
    }
  }

  void activitySignal()
  {
    indicateTempStatus(tempstatus_activityflash);
  }



  virtual bool processOption(const CmdLineOptionDescriptor &aOptionDescriptor, const char *aOptionValue)
  {
    if (strcmp(aOptionDescriptor.longOptionName,"digitalio")==0) {
      staticDeviceConfigs.insert(make_pair("digitalio", aOptionValue));
    }
    else if (strcmp(aOptionDescriptor.longOptionName,"consoleio")==0) {
      staticDeviceConfigs.insert(make_pair("console", aOptionValue));
    }
    else {
      return inherited::processOption(aOptionDescriptor, aOptionValue);
    }
    return true;
  }


  virtual int main(int argc, char **argv)
  {
    const char *usageText =
      "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 0  , "modernids",     true,  "enabled;1=use modern (GS1/UUID based) 34 hex dsUIDs, 0=classic 24 hex dsids" },
      { 0  , "dsuid",         true,  "dsuid;set dsuid for this vDC (usually UUIDv1 generated on the host)" },
      { 0  , "sgtin",         true,  "part,gcp,itemref,serial;set dSUID for this vDC as SGTIN" },
      { 'a', "dali",          true,  "bridge;DALI bridge serial port device or proxy host[:port]" },
      { 0  , "daliportidle",  true,  "seconds;DALI serial port will be closed after this timeout and re-opened on demand only" },
      { 'b', "enocean",       true,  "bridge;enOcean modem serial port device or proxy host[:port]" },
      { 0,   "huelights",     false, "enable support for hue LED lamps (via hue bridge)" },
      { 'C', "vdsmport",      true,  "port;port number/service name for vdSM to connect to (default=" DEFAULT_VDSMSERVICE ")" },
      { 'i', "vdsmnonlocal",  false, "allow vdSM connections from non-local clients" },
      { 'w', "startupdelay",  true,  "seconds;delay startup" },
      { 'l', "loglevel",      true,  "level;set max level of log message detail to show on stdout" },
      { 0  , "errlevel",      true,  "level;set max level for log messages to go to stderr as well" },
      { 0  , "dontlogerrors", false, "don't duplicate error messages (see --errlevel) on stdout" },
      { 's', "sqlitedir",     true,  "dirpath;set SQLite DB directory (default = " DEFAULT_DBDIR ")" },
      { 'W', "cfgapiport",    true,  "port;server port number for web configuration JSON API (default=none)" },
      { 0  , "cfgapinonlocal",false, "allow web configuration JSON API from non-local clients" },
      { 'g', "digitalio",     true,  "ioname[:[!](in|out)];add static digital input or output device\n"
                                     "Use ! for inverted polarity (default is noninverted input)\n"
                                     "ioname is of form [bus.[device.]]pin:\n"
                                     "- gpio.gpionumber : generic Linux GPIO\n"
                                     "- i2cN.device@i2caddr.pinNumber : numbered pin of I2C device at i2caddr\n"
                                     "  (supported for device : TCA9555" },
      { 'k', "consoleio",     true,  "name[:(in|out|io)];add static debug device which reads and writes console\n"
                                     "(for inputs: first char of name=action key)" },
      { 'h', "help",          false, "show this text" },
      { 0, NULL } // list terminator
    };

    // parse the command line, exits when syntax errors occur
    setCommandDescriptors(usageText, options);
    parseCommandLine(argc, argv);

    if ((numOptions()<1 && staticDeviceConfigs.size()==0) || numArguments()>0) {
      // show usage
      showUsage();
      terminateApp(EXIT_SUCCESS);
    }

    // log level?
    int loglevel = DEFAULT_LOGLEVEL;
    getIntOption("loglevel", loglevel);
    SETLOGLEVEL(loglevel);
    int errlevel = LOG_ERR;
    getIntOption("errlevel", errlevel);
    SETERRLEVEL(errlevel, !getOption("dontlogerrors"));

    // startup delay?
    int startupDelay = 0; // no delay
    getIntOption("startupdelay", startupDelay);


    // before starting anything, delay
    if (startupDelay>0) {
      LOG(LOG_NOTICE, "Delaying startup by %d seconds (-w command line option)\n", startupDelay);
      sleep(startupDelay);
    }

    // Check for factory reset as very first action, to avoid that corrupt data might already crash the daemon
    // before we can do the factory reset
    if (button.isSet()) {
      // started with button pressed - go into factory reset wait mode
      factoryResetWait = true;
      indicateTempStatus(tempstatus_factoryresetwait);
    }
    else {
      // Init the device container root object
      // - set DB dir
      const char *dbdir = DEFAULT_DBDIR;
      getStringOption("sqlitedir", dbdir);
      deviceContainer.setPersistentDataDir(dbdir);

      // - set dSUID mode
      int modernids = DEFAULT_USE_MODERN_DSIDS;
      getIntOption("modernids", modernids);
      DsUidPtr externalDsid;
      string dsuidStr;
      if (getStringOption("dsuid", dsuidStr)) {
        externalDsid = DsUidPtr(new DsUid(dsuidStr));
      }
      else if (getStringOption("sgtin", dsuidStr)) {
        int part;
        uint64_t gcp;
        uint32_t itemref;
        uint64_t serial;
        sscanf(dsuidStr.c_str(), "%d,%llu,%u,%llu", &part, &gcp, &itemref, &serial);
        externalDsid = DsUidPtr(new DsUid(dsuidStr));
        externalDsid->setGTIN(gcp, itemref, part);
        externalDsid->setSerial(serial);
      }
      deviceContainer.setIdMode(modernids!=0, externalDsid);

      // Create Web configuration JSON API server
      const char *configApiPort = getOption("cfgapiport");
      if (configApiPort) {
        configApiServer.setConnectionParams(NULL, configApiPort, SOCK_STREAM, AF_INET);
        configApiServer.setAllowNonlocalConnections(getOption("cfgapinonlocal"));
        configApiServer.startServer(boost::bind(&P44bridged::configApiConnectionHandler, this, _1), 3);
      }

      // set up server for vdSM to connect to
      const char *vdsmport = (char *) DEFAULT_VDSMSERVICE;
      getStringOption("vdsmport", vdsmport);
      deviceContainer.vdcApiServer.setConnectionParams(NULL, vdsmport, SOCK_STREAM, AF_INET);
      deviceContainer.vdcApiServer.setAllowNonlocalConnections(getOption("vdsmnonlocal"));

      // Create static container structure
      // - Add DALI devices class if DALI bridge serialport/host is specified
      const char *daliname = getOption("dali");
      if (daliname) {
        int sec = 0;
        getIntOption("daliportidle", sec);
        DaliDeviceContainerPtr daliDeviceContainer = DaliDeviceContainerPtr(new DaliDeviceContainer(1));
        daliDeviceContainer->daliComm.setConnectionSpecification(daliname, DEFAULT_DALIPORT, sec*Second);
        deviceContainer.addDeviceClassContainer(daliDeviceContainer);
      }
      // - Add enOcean devices class if enOcean modem serialport/host is specified
      const char *enoceanname = getOption("enocean");
      if (enoceanname) {
        EnoceanDeviceContainerPtr enoceanDeviceContainer = EnoceanDeviceContainerPtr(new EnoceanDeviceContainer(1));
        enoceanDeviceContainer->enoceanComm.setConnectionSpecification(enoceanname, DEFAULT_ENOCEANPORT);
        deviceContainer.addDeviceClassContainer(enoceanDeviceContainer);
      }
      // - Add hue support
      if (getOption("huelights")) {
        HueDeviceContainerPtr hueDeviceContainer = HueDeviceContainerPtr(new HueDeviceContainer(1));
        deviceContainer.addDeviceClassContainer(hueDeviceContainer);
      }
      // - Add static devices if we have collected any config from the command line
      if (staticDeviceConfigs.size()>0) {
        StaticDeviceContainerPtr staticDeviceContainer = StaticDeviceContainerPtr(new StaticDeviceContainer(1, staticDeviceConfigs));
        deviceContainer.addDeviceClassContainer(staticDeviceContainer);
        staticDeviceConfigs.clear(); // no longer needed, free memory
      }

      // install activity monitor
      deviceContainer.setActivityMonitor(boost::bind(&P44bridged::activitySignal, this));
    }
    // app now ready to run
    return run();
  }




  SocketCommPtr configApiConnectionHandler(SocketComm *aServerSocketCommP)
  {
    JsonCommPtr conn = JsonCommPtr(new JsonComm(SyncIOMainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&P44bridged::configApiRequestHandler, this, _1, _2, _3));
    return conn;
  }


  void configApiRequestHandler(JsonComm *aJsonCommP, ErrorPtr aError, JsonObjectPtr aJsonObject)
  {
    ErrorPtr err;
    // TODO: actually do something
    JsonObjectPtr json = JsonObject::newObj();
    if (Error::isOK(aError)) {
      // %%% just show
      LOG(LOG_DEBUG,"Config API request: %s", aJsonObject->c_strValue());
      // %%% and return dummy response
      json->add("Echo", aJsonObject);
    }
    else {
      LOG(LOG_DEBUG,"Invalid JSON request");
      json->add("Error", JsonObject::newString(aError->description()));
    }
    aJsonCommP->sendMessage(json);
  }


  #define LEARN_TIMEOUT (20*Second)


  void deviceLearnHandler(bool aLearnIn, ErrorPtr aError)
  {
    // back to normal...
    stopLearning(false);
    // ...but as we acknowledge the learning with the LEDs, schedule a update for afterwards
    MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::showAppStatus, this), 2*Second);
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
      LOG(LOG_ERR,"Learning error: %s\n", aError->description().c_str());
    }
  }


  void stopLearning(bool aFromTimeout)
  {
    deviceContainer.stopLearning();
    MainLoop::currentMainLoop().cancelExecutionTicket(learningTimerTicket);
    setAppStatus(status_ok);
    if (aFromTimeout) {
      // letting learn run into timeout will re-collect all devices
      collectDevices(true);
    }
  }



  virtual bool buttonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    LOG(LOG_NOTICE, "Device button event: state=%d, hasChanged=%d\n", aState, aHasChanged);
    // LED yellow as long as button pressed
    if (aHasChanged) {
      if (aState) indicateTempStatus(tempstatus_buttonpressed);
      else endTempStatus();
    }
    if (aState==true && !aHasChanged) {
      // keypress reported again
      if (aTimeSincePreviousChange>=5*Second) {
        // visually acknowledge long keypress by turning LED red
        indicateTempStatus(tempstatus_buttonpressedlong);
        LOG(LOG_WARNING,"Button held for >5 seconds now...\n");
      }
      // check for very long keypress
      if (aTimeSincePreviousChange>=15*Second) {
        // very long press (labelled "Factory reset" on the case)
        setAppStatus(status_error);
        LOG(LOG_WARNING,"Very long button press detected -> clean exit(-2) in 2 seconds\n");
        button.setButtonHandler(NULL, true); // disconnect button
        deviceContainer.setActivityMonitor(NULL); // no activity monitoring any more
        // for now exit(-2) is switching off daemon, so we switch off the LEDs as well
        redLED.steadyOff();
        greenLED.steadyOff();
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::terminateApp, this, -2), 2*Second);
        return true;
      }
    }
    if (aState==false) {
      // keypress release
      if (aTimeSincePreviousChange>=5*Second) {
        // long press (labelled "Software Update" on the case)
        setAppStatus(status_busy);
        LOG(LOG_WARNING,"Long button press detected -> upgrade to latest firmware requested -> clean exit(-3) in 500 mS\n");
        button.setButtonHandler(NULL, true); // disconnect button
        deviceContainer.setActivityMonitor(NULL); // no activity monitoring any more
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::terminateApp, this, -3), 500*MilliSecond);
      }
      else {
        // short press: start/stop learning
        if (!learningTimerTicket) {
          // start
          setAppStatus(status_interaction);
          learningTimerTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::stopLearning, this, true), LEARN_TIMEOUT);
          deviceContainer.startLearning(boost::bind(&P44bridged::deviceLearnHandler, this, _1, _2));
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
    LOG(LOG_NOTICE, "Device button pressed from start event: state=%d, hasChanged=%d\n", aState, aHasChanged);
    if (aHasChanged && aState==false) {
      // released
      if (factoryResetWait && aTimeSincePreviousChange>20*Second) {
        // held in waiting-for-reset state more than 20 seconds -> FACTORY RESET
        LOG(LOG_WARNING,"Button pressed at startup and 20-30 seconds beyond -> FACTORY RESET = clean exit(-42) in 2 seconds\n");
        // indicate red "error/danger" state
        redLED.steadyOn();
        greenLED.steadyOff();
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::terminateApp, this, -42), 2*Second);
        return true;
      }
      else {
        // held in waiting-for-reset state less than 20 seconds or more than 30 seconds -> just restart
        LOG(LOG_WARNING,"Button pressed at startup but less than 20 or more than 30 seconds -> normal restart = clean exit(0) in 0.5 seconds\n");
        // indicate yellow "busy" state
        redLED.steadyOn();
        greenLED.steadyOn();
        // give mainloop some time to close down API connections
        MainLoop::currentMainLoop().executeOnce(boost::bind(&P44bridged::terminateApp, this, 0), 500*MilliSecond);
        return true;
      }
    }
    // if button is stuck, turn nervously yellow to indicate: something needs to be done
    if (factoryResetWait && !aHasChanged && aState) {
      if (aTimeSincePreviousChange>30*Second) {
        // end factory reset wait, assume button stuck or something
        factoryResetWait = false;
        // fast yellow blinking
        greenLED.blinkFor(p44::Infinite, 200*MilliSecond, 60);
        redLED.blinkFor(p44::Infinite, 200*MilliSecond, 60);
        // when button is released, a normal restart will occur, otherwise we'll remain in this state
      }
      else if (aTimeSincePreviousChange>20*Second) {
        // if released now, factory reset will occur (but if held still longer, will enter "button stuck" mode
        redLED.steadyOn();
        greenLED.steadyOff();
      }
    }
    return true;
  }




  virtual void initialize()
  {
    if (factoryResetWait) {
      // button held during startup, check for factory reset
      // - connect special button hander
      button.setButtonHandler(boost::bind(&P44bridged::fromStartButtonHandler, this, _2, _3, _4), true, 1*Second);
    }
    else {
      // normal init
      // - connect button
      button.setButtonHandler(boost::bind(&P44bridged::buttonHandler, this, _2, _3, _4), true, 1*Second);
      // - initialize the device container
      deviceContainer.initialize(boost::bind(&P44bridged::initialized, this, _1), false); // no factory reset
    }
  }


  virtual void initialized(ErrorPtr aError)
  {
    if (!Error::isOK(aError)) {
      // cannot initialize, this is a fatal error
      setAppStatus(status_fatalerror);
      // TODO: what should happen next? Wait for restart?
    }
    else {
      // collect devices
      collectDevices(false);
    }
  }


  virtual void collectDevices(bool aIncremental)
  {
    // initiate device collection
    setAppStatus(status_busy);
    deviceContainer.collectDevices(boost::bind(&P44bridged::devicesCollected, this, _1), aIncremental, false); // no forced full scan (only if needed)
  }


  virtual void devicesCollected(ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      setAppStatus(status_ok);
      DBGLOG(LOG_INFO, deviceContainer.description().c_str());
    }
    else
      setAppStatus(status_error);
  }

};


int main(int argc, char **argv)
{
  // create the mainloop
  SyncIOMainLoop::currentMainLoop().setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static P44bridged application;
  // pass control
  return application.main(argc, argv);
}
