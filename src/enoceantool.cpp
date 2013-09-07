//
//  enoceantool.cpp
//  enoceantool
//
//  Created by Lukas Zeller on 09.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "application.hpp"

#include "enoceancomm.hpp"

#define MAINLOOP_CYCLE_TIME_uS 20000 // 20mS
#define DEFAULT_LOGLEVEL LOG_NOTICE
#define DEFAULT_ENOCEANPORT 2102


using namespace p44;

class EnoceanTool : public Application
{

  EnoceanComm enoceanComm; // enOcean communication

  uint32_t destinationAddr = 0xFFFFFFFF;

  bool send4BS = false;
  uint32_t data4BS = 0x00000000;

public:

  EnoceanTool() :
    enoceanComm(SyncIOMainLoop::currentMainLoop())
  {
  }


  void usage(char *name)
  {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s [options]\n", name);
    fprintf(stderr, "    -b enoceanpath : enOcean serial port device or enocean proxy ipaddr\n");
    fprintf(stderr, "    -B enoceanport : port number for enocean proxy ipaddr (default=%d)\n", DEFAULT_ENOCEANPORT);
    fprintf(stderr, "    -d destination  : enOcean destination hex address (default: 0xFFFFFFFF = broadcast)\n");
    fprintf(stderr, "    -4 D3D2D1D0     : send enOcean 4BS packet\n");
    fprintf(stderr, "    -l loglevel     : set loglevel (default = %d)\n", DEFAULT_LOGLEVEL);
  };

  virtual int main(int argc, char **argv)
  {
    if (argc<2) {
      // show usage
      usage(argv[0]);
      exit(1);
    }

    int loglevel = DEFAULT_LOGLEVEL; // use defaults

    char *enoceanname = NULL;
    int enoceanport = DEFAULT_ENOCEANPORT;

    int c;
    while ((c = getopt(argc, argv, "B:b:d:4:l:")) != -1)
    {
      switch (c) {
        case 'b':
          enoceanname = optarg;
          break;
        case 'B':
          enoceanport = atoi(optarg);
          break;
        case '4':
          send4BS = true;
          sscanf(optarg, "%x", &data4BS);
          break;
        case 'd':
          sscanf(optarg, "%x", &destinationAddr);
          break;
        case 'l':
          loglevel = atoi(optarg);
          break;
        default:
          exit(-1);
      }
    }

    SETLOGLEVEL(loglevel);

    // set enocean comm params
    if (enoceanname) {
      enoceanComm.setConnectionParameters(enoceanname, enoceanport);
    }

    // app now ready to run
    return run();
  }


  virtual void initialize()
  {
    // send
    if (send4BS) {
      Esp3PacketPtr packet = Esp3PacketPtr(new Esp3Packet());
      packet->initForRorg(rorg_4BS);
      packet->set4BSdata(data4BS);
      // set destination
      packet->setRadioDestination(destinationAddr);
      // send it
      enoceanComm.sendPacket(packet);
    }
  }


};


int main(int argc, char **argv)
{
  // create the mainloop
  SyncIOMainLoop::currentMainLoop().setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static EnoceanTool application;
  // pass control
  return application.main(argc, argv);
}
