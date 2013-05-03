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

#define DEFAULT_CONNECTIONPORT 2101

#define MAINLOOP_CYCLE_TIME_uS 100000 // 100mS

using namespace p44;


class P44bridged : public Application
{
	// the device container
	DeviceContainer deviceContainer;

  Gpio yellowLED;
  Gpio greenLED;
  Gpio button;

public:

  P44bridged() :
    yellowLED("ledyellow", true, true, false),
    greenLED("ledgreen", true, true, false),
    button("button", false, true, false)
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
		if (argc<1) {
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
	
	virtual void initialize()
	{
		// initiate device collection
    yellowLED.setState(true);
		deviceContainer.collectDevices(boost::bind(&P44bridged::devicesCollected, this, _1), false); // no forced full scan (only if needed)
	}
	
	virtual void devicesCollected(ErrorPtr aError)
	{
    yellowLED.setState(false);
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
