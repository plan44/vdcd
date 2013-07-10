/*
 * main.cpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include "application.hpp"

#include "devicecontainer.hpp"

#include "dalidevicecontainer.hpp"
#include "enoceandevicecontainer.hpp"
#include "staticdevicecontainer.hpp"

#include "jsoncomm.hpp"

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


class P44bridged : public Application
{
  // status
  typedef enum {
    status_ok,  // ok, all normal
    status_busy,  // busy, but normal
    status_interaction, // expecting user interaction
    status_error, // error
    status_fatalerror  // fatal error that needs user interaction
  } P44bridgeStatus;

  // App status
  P44bridgeStatus appStatus;

  // the device container
  DeviceContainer deviceContainer;

  IndicatorOutput redLED;
  IndicatorOutput greenLED;
  ButtonInput button;

  // Enocean device learning
  EnoceanDeviceContainerPtr enoceanDeviceContainer;
  bool deviceLearning;
  // Direct DALI control from enocean switches
  DaliDeviceContainerPtr daliDeviceContainer;

  // Configuration API
  SocketComm configApiServer;

public:

  P44bridged() :
    redLED("ledred", false, false),
    greenLED("ledgreen", false, false),
    button("button", true),
    appStatus(status_busy),
    deviceLearning(false),
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


  void usage(char *name)
  {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s [options]\n", name);
    fprintf(stderr, "    -a dalipath    : DALI serial port device or DALI proxy ipaddr\n");
    fprintf(stderr, "    -A daliport    : port number for DALI proxy ipaddr (default=%d)\n", DEFAULT_DALIPORT);
    fprintf(stderr, "    -b enoceanpath : enOcean serial port device or enocean proxy ipaddr\n");
    fprintf(stderr, "    -B enoceanport : port number for enocean proxy ipaddr (default=%d)\n", DEFAULT_ENOCEANPORT);
    fprintf(stderr, "    -c vdsmhost    : vdSM hostname/IP\n");
    fprintf(stderr, "    -C vdsmport    : port number/service name for vdSM (default=%s)\n", DEFAULT_VDSMSERVICE);
    fprintf(stderr, "    -d             : fully daemonize\n");
    fprintf(stderr, "    -w seconds     : delay startup\n");
    fprintf(stderr, "    -l loglevel    : set loglevel (default = %d, daemon mode default=%d)\n", LOGGER_DEFAULT_LOGLEVEL, DEFAULT_DAEMON_LOGLEVEL);
    fprintf(stderr, "    -s dirpath     : set SQLite DB directory (default = %s)\n", DEFAULT_DBDIR);
    fprintf(stderr, "    -W apiport     : server port number for web configuration JSON API (default=none)\n");
    fprintf(stderr, "    -g gpio[:[!](in|out)] : add static GPIO input or output device\n");
    fprintf(stderr, "                     use ! for inverted polarity (default is noninverted input)\n");
    fprintf(stderr, "    -k name[:(in|out|io)] : add static device which reads and writes console\n");
    fprintf(stderr, "                     (for inputs: first char of name=action key)\n");
  };

  virtual int main(int argc, char **argv)
  {
    if (argc<2) {
      // show usage
      usage(argv[0]);
      exit(1);
    }
    bool daemonMode = false;

    char *daliname = NULL;
    int daliport = DEFAULT_DALIPORT;

    char *enoceanname = NULL;
    int enoceanport = DEFAULT_ENOCEANPORT;

    char *vdsmname = NULL;
    char *vdsmport = (char *) DEFAULT_VDSMSERVICE;

    char *configApiPort = NULL;

    
    DeviceConfigMap staticDeviceConfigs;

    const char *dbdir = DEFAULT_DBDIR;

    int loglevel = -1; // use defaults

    int startupDelay = 0; // no delay

    int c;
    while ((c = getopt(argc, argv, "da:A:b:B:c:C:g:k:l:s:w:W:")) != -1)
    {
      switch (c) {
        case 'd':
          daemonMode = true;
          break;
        case 'l':
          loglevel = atoi(optarg);
          break;
        case 'a':
          daliname = optarg;
          break;
        case 'A':
          daliport = atoi(optarg);
          break;
        case 'b':
          enoceanname = optarg;
          break;
        case 'B':
          enoceanport = atoi(optarg);
          break;
        case 'c':
          vdsmname = optarg;
          break;
        case 'C':
          vdsmport = optarg;
          break;
        case 'W':
          configApiPort = optarg;
          break;
        case 'g':
          staticDeviceConfigs.insert(make_pair("gpio", optarg));
          break;
        case 'k':
          staticDeviceConfigs.insert(make_pair("console", optarg));
          break;
        case 's':
          dbdir = optarg;
          break;
        case 'w':
          startupDelay = atoi(optarg);
          break;
        default:
          exit(-1);
      }
    }

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

    // Set the persistent data directory
    deviceContainer.setPersistentDataDir(dbdir);


    // Create Web configuration JSON API server
    if (configApiPort) {
      configApiServer.setConnectionParams(NULL, configApiPort, SOCK_STREAM, AF_INET);
      configApiServer.startServer(boost::bind(&P44bridged::configApiConnectionHandler, this, _1), 3, false);
    }

    // Create JSON interface to vdSM
    if (vdsmname) {
      deviceContainer.vdsmJsonComm.setConnectionParams(vdsmname, vdsmport, SOCK_STREAM);
    }

    // Create static container structure
    // - Add DALI devices class if DALI bridge serialport/host is specified
    if (daliname) {
      daliDeviceContainer = DaliDeviceContainerPtr(new DaliDeviceContainer(1));
      daliDeviceContainer->daliComm.setConnectionParameters(daliname, daliport);
      deviceContainer.addDeviceClassContainer(daliDeviceContainer);
    }
    // - Add enOcean devices class if enOcean modem serialport/host is specified
    if (enoceanname) {
      enoceanDeviceContainer = EnoceanDeviceContainerPtr(new EnoceanDeviceContainer(1));
      enoceanDeviceContainer->enoceanComm.setConnectionParameters(enoceanname, enoceanport);
      deviceContainer.addDeviceClassContainer(enoceanDeviceContainer);
    }
    // - Add static devices if we have collected any config from the command line
    if (staticDeviceConfigs.size()>0) {
      StaticDeviceContainerPtr staticDeviceContainer = StaticDeviceContainerPtr(new StaticDeviceContainer(1, staticDeviceConfigs));
      deviceContainer.addDeviceClassContainer(staticDeviceContainer);
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
    conn->setMessageHandler(boost::bind(&P44bridged::apiRequestHandler, this, _1, _2, _3));
    return conn;
  }


  void apiRequestHandler(JsonComm *aJsonCommP, ErrorPtr aError, JsonObjectPtr aJsonObject)
  {
    ErrorPtr err;
    // TODO: actually do something
    JsonObjectPtr json = JsonObject::newObj();
    if (Error::isOK(aError)) {
      // %%% just show
      LOG(LOG_DEBUG,"API request: %s", aJsonObject->c_strValue());
      // %%% and return dummy response
      json->add("Echo", aJsonObject);
    }
    else {
      LOG(LOG_DEBUG,"Invalid JSON request");
      json->add("Error", JsonObject::newString(aError->description()));
    }
    aJsonCommP->sendMessage(json, err);
  }



//  bool localKeyHandler(EnoceanDevicePtr aEnoceanDevicePtr, int aSubDeviceIndex, uint8_t aAction)
//  {
//    #warning // TODO: refine - now just switches all lamps on/off
//    if (daliDeviceContainer) {
//      if (aAction&rpsa_pressed) {
//        daliDeviceContainer->daliComm.daliSendDirectPower(DaliBroadcast, (aAction&rpsa_offOrUp)!=0 ? 0 : 254);
//      }
//      return true;
//    }
//    return false;
//  }



  void deviceLearnHandler(ErrorPtr aStatus)
  {
    // back to normal...
    setAppStatus(status_ok);
    // ...but as we acknowledge the learning with the LEDs, schedule a update for afterwards
    MainLoop::currentMainLoop()->executeOnce(boost::bind(&P44bridged::showAppStatus, this), 2*Second);
    // acknowledge the learning (if any, can also be timeout or manual abort)
    if (Error::isError(aStatus,EnoceanError::domain(), EnoceanDeviceLearned)) {
      // show device learned
      redLED.stop();
      greenLED.blinkFor(1600*MilliSecond, 400*MilliSecond, 30);
    }
    else if (Error::isError(aStatus,EnoceanError::domain(), EnoceanDeviceUnlearned)) {
      // show device unlearned
      greenLED.stop();
      redLED.blinkFor(1600*MilliSecond, 400*MilliSecond, 30);
    }
  }


  virtual bool buttonHandler(bool aNewState, MLMicroSeconds aTimeStamp)
  {
    // TODO: %%% clean up, test hacks for now
    if (aNewState==false) {
      // released
      // TODO: check for long press
      if (enoceanDeviceContainer) {
        if (!enoceanDeviceContainer->isLearning()) {
          // start device learning
          setAppStatus(status_interaction);
          enoceanDeviceContainer->learnDevice(boost::bind(&P44bridged::deviceLearnHandler, this, _1), 10*Second);
        }
        else {
          // abort device learning
          enoceanDeviceContainer->stopLearning();
        }
      }
    }
    return true;
  }


  virtual void initialize()
  {
    // connect button
    button.setButtonHandler(boost::bind(&P44bridged::buttonHandler, this, _2, _3), true);
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
      // initiate device collection
      setAppStatus(status_busy);
      deviceContainer.collectDevices(boost::bind(&P44bridged::devicesCollected, this, _1), false); // no forced full scan (only if needed)
    }
  }


  void localSwitchOutputHandler(const dSID &aDsid, bool aOutputOn)
  {
    if (daliDeviceContainer) {
      daliDeviceContainer->daliComm.daliSendDirectPower(DaliBroadcast, !aOutputOn ? 0 : 254);
    }
  }

  
  virtual void devicesCollected(ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      setAppStatus(status_ok);
      DBGLOG(LOG_INFO, deviceContainer.description().c_str());
      /// TODO: %%% q&d: install local toggle handler
      deviceContainer.setLocalSwitchOutputHandler(boost::bind(&P44bridged::localSwitchOutputHandler, this, _1, _2));
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
