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

#include "gpio.hpp"


#define DEFAULT_CONNECTIONPORT 2101

#define MAINLOOP_CYCLE_TIME_uS 20000 // 20mS

using namespace p44;


class P44bridged : public Application
{
	// the device container
	DeviceContainer deviceContainer;

  IndicatorOutput yellowLED;
  IndicatorOutput greenLED;
  ButtonInput button;

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
		fprintf(stderr, "  %s [options] (DALI serialportdevice|DALI proxy ipaddr)\n", name);
		fprintf(stderr, "    -P port : port to connect to (default: %d)\n", DEFAULT_CONNECTIONPORT);
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
		int outputport = DEFAULT_CONNECTIONPORT;

		int c;
		while ((c = getopt(argc, argv, "dP:")) != -1)
		{
			switch (c) {
				case 'd':
					daemonMode = true;
					verbose = true;
					break;
				case 'P':
					outputport = atoi(optarg);
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

		char *outputname = argv[optind++];


		// Create static container structure
		// - Add DALI devices class
		DaliDeviceContainerPtr daliDeviceContainer(new DaliDeviceContainer(1));
		daliDeviceContainer->daliComm.setConnectionParameters(outputname, outputport);
		deviceContainer.addDeviceClassContainer(daliDeviceContainer);

		// app now ready to run
		return run();
	}

	virtual bool buttonHandler(bool aNewState)
	{
    static bool testBlink = false;

    if (aNewState==false) {
      // key released
      //*/
      testBlink = !testBlink;
      if (testBlink)
        yellowLED.blinkFor(p44::Infinite, 800*MilliSecond, 80);
      else
        yellowLED.stop();
      /*/
      yellowLED.blinkFor(5*Second, 200*MilliSecond);
      //*/
    }

	  //greenLED.set(aNewState);
	  return true;
	}


	virtual void initialize()
	{
    // start button test
	  greenLED.off();
    button.setButtonHandler(boost::bind(&P44bridged::buttonHandler, this, _2), true);

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
