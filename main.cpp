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

#include "gpio.hpp"


#define DEFAULT_DALIPORT 2101
#define DEFAULT_ENOCEANPORT 2102

#define MAINLOOP_CYCLE_TIME_uS 20000 // 20mS

using namespace p44;


class P44bridged : public Application
{
	// the device container
	DeviceContainer deviceContainer;
		
  IndicatorOutput yellowLED;
  IndicatorOutput greenLED;
  ButtonInput button;

  // Enocean device learning
  EnoceanDeviceContainerPtr enoceanDeviceContainer;
  bool deviceLearning;

	
public:

  P44bridged() :
    yellowLED("ledyellow", true, false),
    greenLED("ledgreen", true, false),
    button("button", true)
  {
  }

	void usage(char *name)
	{
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "  %s [options]\n", name);
		fprintf(stderr, "    -a dalipath : DALI serial port device or DALI proxy ipaddr\n");
		fprintf(stderr, "    -A daliport : port number for DALI proxy ipaddr (default=%d)", DEFAULT_DALIPORT);
		fprintf(stderr, "    -e enoceanpath : enOcean serial port device or enocean proxy ipaddr\n");
		fprintf(stderr, "    -E enoceanport : port number for enocean proxy ipaddr (default=%d)", DEFAULT_ENOCEANPORT);
		fprintf(stderr, "    -d : fully daemonize and suppress showing byte transfer messages on stdout\n");
	};

	virtual int main(int argc, char **argv)
	{
		if (argc<2) {
			// show usage
			usage(argv[0]);
			exit(1);
		}
		bool daemonMode = false;
		bool verbose = false;
		
		char *daliname = NULL;
		int daliport = DEFAULT_DALIPORT;

		char *enoceanname = NULL;
		int enoceanport = DEFAULT_ENOCEANPORT;
		
		

		int c;
		while ((c = getopt(argc, argv, "da:A:b:B:")) != -1)
		{
			switch (c) {
				case 'd':
					daemonMode = true;
					verbose = true;
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
				default:
					exit(-1);
			}
		}

		// daemonize now if requested and in proxy mode
		if (daemonMode) {
			printf("Starting background daemon\n");
			daemonize();
		}

		// Create static container structure
		// - Add DALI devices class
		if (daliname) {
			DaliDeviceContainerPtr daliDeviceContainer(new DaliDeviceContainer(1));
			daliDeviceContainer->daliComm.setConnectionParameters(daliname, daliport);
			deviceContainer.addDeviceClassContainer(daliDeviceContainer);
		}

		if (enoceanname) {
			enoceanDeviceContainer = EnoceanDeviceContainerPtr(new EnoceanDeviceContainer(1));
			enoceanDeviceContainer->enoceanComm.setConnectionParameters(daliname, daliport);
			deviceContainer.addDeviceClassContainer(enoceanDeviceContainer);
		}
		
		// app now ready to run
		return run();
	}


  void deviceLearnHandler(ErrorPtr aStatus)
  {
    yellowLED.off(); // end of learn (whatever reason)
    if (Error::isError(aStatus,EnoceanError::domain(), EnoceanDeviceLearned)) {
      // show device learned
      greenLED.blinkFor(1*Second, 333*MilliSecond, 30);
    }
    else if (Error::isError(aStatus,EnoceanError::domain(), EnoceanDeviceUnlearned)) {
      // show device unlearned
      yellowLED.blinkFor(1*Second, 333*MilliSecond, 30);
    }
  }


	virtual bool buttonHandler(bool aNewState, MLMicroSeconds aTimeStamp)
	{
    // TODO: %%% clean up, test hacks for now
    //*/
    if (aNewState==false) {
      // released
      // TODO: check for long press
      if (!enoceanDeviceContainer->isLearning()) {
        // start device learning
        yellowLED.blinkFor(p44::Infinite, 800*MilliSecond, 80);
        enoceanDeviceContainer->learnSwitchDevice(boost::bind(&P44bridged::deviceLearnHandler, this, _1), 10*Second);
      }
      else {
        // abort device learning
        enoceanDeviceContainer->stopLearning();
      }
    }
    /*/
    static bool testBlink = false;

    if (aNewState==false) {
      // key released
      testBlink = !testBlink;
      if (testBlink)
        yellowLED.blinkFor(p44::Infinite, 800*MilliSecond, 80);
      else
        yellowLED.stop();
    }
    //*/

	  //greenLED.set(aNewState);
	  return true;
	}


	virtual void initialize()
	{
    // start button test
	  greenLED.off();
    button.setButtonHandler(boost::bind(&P44bridged::buttonHandler, this, _2, _3), true);

		// initiate device collection
    /*/
    #warning DALI scanning disabled for now
    /*/
    SETLOGLEVEL(LOG_INFO);
    yellowLED.on();
		deviceContainer.collectDevices(boost::bind(&P44bridged::devicesCollected, this, _1), false); // no forced full scan (only if needed)
    //*/

	}

	virtual void devicesCollected(ErrorPtr aError)
	{
    yellowLED.off();
		DBGLOG(LOG_INFO, deviceContainer.description().c_str());
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
