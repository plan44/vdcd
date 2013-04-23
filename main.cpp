/*
 * main.cpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include "devicecontainer.hpp"

#include "dalidevicecontainer.hpp"

#define DEFAULT_CONNECTIONPORT 2101

#define MAINLOOP_TICK_MS 100


static void usage(char *name)
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s [options] (DALI serialportdevice|DALI proxy ipaddr)\n", name);
  fprintf(stderr, "    -P port : port to connect to (default: %d)\n", DEFAULT_CONNECTIONPORT);
  fprintf(stderr, "    -d : fully daemonize and suppress showing byte transfer messages on stdout\n");
}


static void daemonize(void)
{
  pid_t pid, sid;

  /* already a daemon */
  if ( getppid() == 1 ) return;

  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then we can exit the parent process. */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* At this point we are executing as the child process */

  /* Change the file mode mask */
  umask(0);

  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }

  /* Change the current working directory.  This prevents the current
     directory from being locked; hence not being able to remove it. */
  if ((chdir("/")) < 0) {
    exit(EXIT_FAILURE);
  }

  /* Redirect standard files to /dev/null */
  freopen( "/dev/null", "r", stdin);
  freopen( "/dev/null", "w", stdout);
  freopen( "/dev/null", "w", stderr);
}



class CompletionHandler
{
  DeviceContainerPtr deviceContainer;
public:
  CompletionHandler(DeviceContainerPtr aDeviceContainer) : deviceContainer(aDeviceContainer) {};
  void operator()(ErrorPtr aError)
  {
    DBGLOG(LOG_INFO, deviceContainer->description().c_str());
  }
};




int main(int argc, char **argv)
{
  if (argc<1) {
    // show usage
    usage(argv[0]);
    exit(1);
  }
  bool daemonMode = false;
  bool serialMode = false;
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

  int argIdx;
  int data;
  unsigned char byte;

  char *outputname = argv[optind++];

  // Create device container
  DeviceContainerPtr deviceContainer(new DeviceContainer());
  // - Add DALI devices class
  DaliDeviceContainerPtr daliDeviceContainer(new DaliDeviceContainer(1));
  daliDeviceContainer->daliComm.setConnectionParameters(outputname, outputport);
  deviceContainer->addDeviceClassContainer(daliDeviceContainer);

  // initiate device collection
  deviceContainer->collectDevices(CompletionHandler(deviceContainer), false); // no forced full scan (only if needed)
//  daliDeviceContainer->daliComm.testFullBusScan();

//  // Create DALI communicator
//  DaliCommPtr daliComm(new DaliComm());
//  daliComm->setConnectionParameters(outputname, outputport);
//
//  daliComm->test();

//  // Prepare dSDC API socket
//  int listenfd = 0;
//  int servingfd = 0;
//  struct sockaddr_in serv_addr;
//  fd_set readfs; // file descriptor set
//  int    maxrdfd; // maximum file descriptor used
//
//  // - open server socket
//  listenfd = socket(AF_INET, SOCK_STREAM, 0);
//  memset(&serv_addr, '0', sizeof(serv_addr));
//
//  serv_addr.sin_family = AF_INET;
//  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//  serv_addr.sin_port = htons(proxyPort); // port
//  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
//  listen(listenfd, 1); // max one connection for now
//
//  if (verbose) printf("Listening on port %d for connections\n",proxyPort);

  // Main loop
  while (true) {
    // Create bitmap for select call
    int numFDsToTest = 0; // number of file descriptors to test (max+1 of all used FDs)
    fd_set readfs; // file descriptor set
    FD_ZERO(&readfs);
    // - DALI
    int daliFD = daliDeviceContainer->daliComm.toBeMonitoredFD();
    if (daliFD>=0) {
      // DALI FD is active, include it
      numFDsToTest = MAX(daliFD+1, numFDsToTest);
      FD_SET(daliFD, &readfs);  // testing for DALI
    }
    // - client
    int clientFD = -1; // none yet
    if (clientFD>=0) {
      // TODO: %%% test client for receiving data
      numFDsToTest = MAX(clientFD+1, numFDsToTest);
      FD_SET(clientFD, &readfs);  /* set testing for source 1 */
    }
    // block until input becomes available or timeout
    struct timeval tv;
    tv.tv_sec = MAINLOOP_TICK_MS / 1000;
    tv.tv_usec = MAINLOOP_TICK_MS % 1000 * 1000;
    select(numFDsToTest, &readfs, NULL, NULL, &tv);
    bool dataProcessed = false;
    if (daliFD>=0 && FD_ISSET(daliFD,&readfs)) {
      // input from DALI available, have it processed
      daliDeviceContainer->daliComm.dataReadyOnMonitoredFD();
      dataProcessed = true;
    }
    if (clientFD>=0 && FD_ISSET(clientFD,&readfs)) {
      // TODO: input from client available
      dataProcessed = true;
    }
    if (!dataProcessed) {
      // process even if no data processed, to execute timeouts etc.
      daliDeviceContainer->daliComm.process();
    }
  }

  // return
  return 0;
}
