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


#define DEFAULT_DALIPORT 2101
#define DEFAULT_ENOCEANPORT 2102
#define DEFAULT_VDSMSERVICE "8440"
#define DEFAULT_DBDIR "/tmp"

#ifdef __APPLE__
#define DEFAULT_DAEMON_LOGLEVEL LOG_INFO
#else
#define DEFAULT_DAEMON_LOGLEVEL LOG_WARNING
#endif
#define DEFAULT_LOGLEVEL LOG_INFO


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
  } P44bridgeStatus;

  // command line defined devices
  DeviceConfigMap staticDeviceConfigs;

  // App status
  P44bridgeStatus appStatus;

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
    learningTimerTicket(0),
    configApiServer(SyncIOMainLoop::currentMainLoop())
  {
    showAppStatus();
  }

  void setAppStatus(P44bridgeStatus aStatus)
  {
    appStatus = aStatus;
    // update LEDs
    showAppStatus();
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
      { 'a', "dali",          true,  "bridge;DALI bridge serial port device or proxy host[:port]" },
      { 'b', "enocean",       true,  "bridge;enOcean modem serial port device or proxy host[:port]" },
      { 0,   "huelights",     false, "enable support for hue LED lamps (via hue bridge)" },
      { 'C', "vdsmport",      true,  "port;port number/service name for vdSM to connect to (default=" DEFAULT_VDSMSERVICE ")" },
      { 'i', "vdsmnonlocal",  false, "allow vdSM connections from non-local clients" },
      { 'd', "daemonize",     false, "fully daemonize after startup" },
      { 'w', "startupdelay",  true,  "seconds;delay startup" },
      { 'l', "loglevel",      true,  "level;set loglevel" },
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

    if (numOptions()<1 || numArguments()>0) {
      // show usage
      showUsage();
      terminateApp(EXIT_SUCCESS);
    }

    // daemon mode?
    bool daemonMode = getOption("daemonize");

    // log level?
    int loglevel = -1; // none specified
    getIntOption("loglevel", loglevel);

    // startup delay?
    int startupDelay = 0; // no delay
    getIntOption("startupdelay", startupDelay);

    // daemonize now if requested and in proxy mode
    if (daemonMode) {
      LOG(LOG_NOTICE, "Starting background daemon\n");
      daemonize();
      if (loglevel<0) loglevel = DEFAULT_DAEMON_LOGLEVEL;
    }
    else {
      if (loglevel<0) loglevel = DEFAULT_LOGLEVEL;
    }

    // set log level
    SETLOGLEVEL(loglevel);

    // before starting anything, delay
    if (startupDelay>0) {
      LOG(LOG_NOTICE, "Delaying startup by %d seconds (-w command line option)\n", startupDelay);
      sleep(startupDelay);
    }

    // Create the device container root object
    const char *dbdir = DEFAULT_DBDIR;
    getStringOption("sqlitedir", dbdir);
    deviceContainer.setPersistentDataDir(dbdir);

    // Create Web configuration JSON API server
    const char *configApiPort = getOption("cfgapiport");
    if (configApiPort) {
      configApiServer.setConnectionParams(NULL, configApiPort, SOCK_STREAM, AF_INET);
      configApiServer.setAllowNonlocalConnections(false); // config port is always local (mg44 will expose it to outside if needed)
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
      DaliDeviceContainerPtr daliDeviceContainer = DaliDeviceContainerPtr(new DaliDeviceContainer(1));
      daliDeviceContainer->daliComm.setConnectionSpecification(daliname, DEFAULT_DALIPORT);
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

    // app now ready to run
    return run();
  }


  // show global status on LEDs
  void showAppStatus()
  {
    greenLED.stop();
    redLED.stop();
    switch (appStatus) {
      case status_ok:
        greenLED.on();
        break;
      case status_busy:
        greenLED.on();
        redLED.on();
        break;
      case status_interaction:
        greenLED.blinkFor(p44::Infinite, 400*MilliSecond, 80);
        redLED.blinkFor(p44::Infinite, 400*MilliSecond, 80);
        break;
      case status_error:
        redLED.on();
        break;
      case status_fatalerror:
        redLED.blinkFor(p44::Infinite, 800*MilliSecond, 50);;
        break;
    }
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
    stopLearning();
    // ...but as we acknowledge the learning with the LEDs, schedule a update for afterwards
    MainLoop::currentMainLoop()->executeOnce(boost::bind(&P44bridged::showAppStatus, this), 2*Second);
    // acknowledge the learning (if any, can also be timeout or manual abort)
    if (Error::isOK(aError)) {
      if (aLearnIn) {
        // show device learned
        redLED.stop();
        greenLED.blinkFor(1600*MilliSecond, 400*MilliSecond, 30);
      }
      else {
        // show device unlearned
        greenLED.stop();
        redLED.blinkFor(1600*MilliSecond, 400*MilliSecond, 30);
      }
    }
    else {
      LOG(LOG_ERR,"Learning error: %s\n", aError->description().c_str());
    }
  }


  void stopLearning()
  {
    deviceContainer.stopLearning();
    MainLoop::currentMainLoop()->cancelExecutionTicket(learningTimerTicket);
    setAppStatus(status_ok);
  }



  virtual bool buttonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    // TODO: %%% clean up, test hacks for now
    if (aState==true && !aHasChanged) {
      // keypress reported again, check for very long keypress
      if (aTimeSincePreviousChange>=10*Second) {
        // very long press (labelled "Factory reset" on the case)
        setAppStatus(status_error);
        LOG(LOG_WARNING,"Very long button press detected -> exit(3) in 2 seconds\n");
        sleep(2);
        exit(3); // %%% for now, so starting script knows why we exit
      }
    }
    if (aState==false) {
      // keypress release
      if (aTimeSincePreviousChange>=3*Second) {
        // long press (labelled "Software Update" on the case)
        setAppStatus(status_error);
        LOG(LOG_WARNING,"Long button press detected -> collect devices (again)\n");
        // collect devices again
        collectDevices();
        return true;
      }
      else {
        // short press: start/stop learning
        if (!learningTimerTicket) {
          // start
          setAppStatus(status_interaction);
          learningTimerTicket = MainLoop::currentMainLoop()->executeOnce(boost::bind(&P44bridged::stopLearning, this), LEARN_TIMEOUT);
          deviceContainer.startLearning(boost::bind(&P44bridged::deviceLearnHandler, this, _1, _2));
        }
        else {
          // stop
          stopLearning();
        }
      }
    }
    return true;
  }


  virtual void initialize()
  {
    // connect button
    button.setButtonHandler(boost::bind(&P44bridged::buttonHandler, this, _2, _3, _4), true, 1*Second);
    // initialize the device container
    deviceContainer.initialize(boost::bind(&P44bridged::initialized, this, _1), false); // no factory reset
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
      collectDevices();
    }
  }


  virtual void collectDevices()
  {
    // initiate device collection
    setAppStatus(status_busy);
    deviceContainer.collectDevices(boost::bind(&P44bridged::devicesCollected, this, _1), false); // no forced full scan (only if needed)
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
  SyncIOMainLoop::currentMainLoop()->setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static P44bridged application;
  // pass control
  return application.main(argc, argv);
}
