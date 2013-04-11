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


#define BAUDRATE B9600

#define DEFAULT_PROXYPORT 2101
#define DEFAULT_CONNECTIONPORT 2101



static void usage(char *name)
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s (DALI serialportdevice|DALI proxy ipaddr)\n", name);
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

  int c;
  while ((c = getopt(argc, argv, "d")) != -1)
  {
    switch (c) {
      case 'd':
        daemonMode = true;
        verbose = true;
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
  DaliCommPtr daliComm(new DaliComm());

  daliComm->allOn();


  /*
  int outputfd =0;
  int res;
  char *outputname = argv[optind++];
  struct termios oldtio,newtio;

  serialMode = *outputname=='/';

  // check type of output
  if (serialMode) {
    // assume it's a serial port
    outputfd = open(outputname, O_RDWR | O_NOCTTY);
    if (outputfd <0) {
      perror(outputname); exit(-1);
    }
    tcgetattr(outputfd,&oldtio); // save current port settings

    // see "man termios" for details
    memset(&newtio, 0, sizeof(newtio));
    // - baudrate, 8-N-1, no modem control lines (local), reading enabled
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    // - ignore parity errors
    newtio.c_iflag = IGNPAR;
    // - no output control
    newtio.c_oflag = 0;
    // - no input control (non-canonical)
    newtio.c_lflag = 0;
    // - no inter-char time
    newtio.c_cc[VTIME]    = 0;   // inter-character timer unused
    // - receive every single char seperately
    newtio.c_cc[VMIN]     = 1;   // blocking read until 1 chars received
    // - set new params
    tcflush(outputfd, TCIFLUSH);
    tcsetattr(outputfd,TCSANOW,&newtio);
  }
  else {
    // assume it's an IP address or hostname
    struct sockaddr_in conn_addr;
    if ((outputfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      printf("Error: Could not create socket\n");
      exit(1);
    }
    // prepare IP address
    memset(&conn_addr, '0', sizeof(conn_addr));
    conn_addr.sin_family = AF_INET;
    conn_addr.sin_port = htons(connPort);

    struct hostent *server;
    server = gethostbyname(outputname);
    if (server == NULL) {
      printf("Error: no such host");
      exit(1);
    }
    memcpy((char *)&conn_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);

    if ((res = connect(outputfd, (struct sockaddr *)&conn_addr, sizeof(conn_addr))) < 0) {
      printf("Error: %s\n", strerror(errno));
      exit(1);
    }
  }

  // done
  if (serialMode) {
    tcsetattr(outputfd,TCSANOW,&oldtio);
  }

  // close
  close(outputfd);
  */



  // return
  return 0;
}
