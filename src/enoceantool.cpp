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

#include "enoceancomm.hpp"

#define MAINLOOP_CYCLE_TIME_uS 20000 // 20mS
#define DEFAULT_LOGLEVEL LOG_NOTICE
#define DEFAULT_ENOCEANPORT 2102


using namespace p44;

class EnoceanTool : public Application
{

  EnoceanComm enoceanComm; // EnOcean communication

  uint32_t destinationAddr = 0xFFFFFFFF;

  bool send4BS = false;
  uint32_t data4BS = 0x00000000;

public:

  EnoceanTool() :
    enoceanComm(MainLoop::currentMainLoop())
  {
  }


  void usage(char *name)
  {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s [options]\n", name);
    fprintf(stderr, "    -b enoceanpath : EnOcean serial port device or enocean proxy ipaddr[:port]\n");
    fprintf(stderr, "    -d destination  : EnOcean destination hex address (default: 0xFFFFFFFF = broadcast)\n");
    fprintf(stderr, "    -4 D3D2D1D0     : send EnOcean 4BS packet\n");
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
      enoceanComm.setConnectionSpecification(enoceanname, DEFAULT_ENOCEANPORT);
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
  MainLoop::currentMainLoop().setLoopCycleTime(MAINLOOP_CYCLE_TIME_uS);
  // create app with current mainloop
  static EnoceanTool application;
  // pass control
  return application.main(argc, argv);
}
