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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "dalicomm.hpp"


#define DEFAULT_CONNECTIONPORT 2101



static void usage(char *name)
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s (DALI serialportdevice|DALI proxy ipaddr)\n", name);
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

  // Create DALI communicator
  char *outputname = argv[optind++];
  DaliCommPtr daliComm(new DaliComm(outputname, outputport));

  daliComm->allOn();


  // return
  return 0;
}
