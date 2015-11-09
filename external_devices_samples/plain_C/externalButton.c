//
//  Simple external vdcd button device implemented in plain C
//
//  Created 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  This sample code is in the public domain


#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, const char * argv[])
{
  int sock;
  struct sockaddr_in addr;
  // check arguments
  if ((argc != 3)) {
    fprintf(stderr, "Usage: %s <IPv4> <Port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))< 0) {
    fprintf(stderr, "cannot create socket\n");
    exit(EXIT_FAILURE);
  }
  // construct addr
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET; // IPv4
  addr.sin_addr.s_addr = inet_addr(argv[1]); // first argument is IPv4
  addr.sin_port = htons(atoi(argv[2])); // second argument is port

  // establish connection
  if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    fprintf(stderr, "cannot connect to %s:%s - %s\n", argv[1], argv[2], strerror(errno));
    exit(EXIT_FAILURE);
  }
  // the init string for the device
  const char *initJSON =
    "{"
    "'message':'init',"
    "'protocol':'simple'," // after sending this JSON initialisation, we want simple text protocol, not JSON
    "'uniqueid':'externalButtonSample'," // a unique string for this sample, will be used to derive dSUID
    "'name':'ext light button'," // default name for the device (can be changed via dSS)
    "'buttons':[{'buttontype':1,'group':1,'hardwarename':'push','element':0}]" // a single light button
    "}\n";
  if (send(sock, initJSON, strlen(initJSON), 0)<0) {
    fprintf(stderr, "cannot send init message\n");
    close(sock);
    exit(EXIT_FAILURE);
  }
  // wait for keypress
  while(1) {
    printf("press enter to simulate button click, or q+enter to quit\n");
    char c = getchar();
    if (c=='q') {
      printf("q pressed -> terminating\n");
      break;
    }
    // simulate 200mS button press
    printf("key pressed -> generated button click\n");
    const char *buttonPress =
      "B0=200\n";
    if (send(sock, buttonPress, strlen(buttonPress), 0)<0) {
      fprintf(stderr, "cannot send button press message\n");
      close(sock);
      exit(EXIT_FAILURE);
    }
  }
  exit(EXIT_SUCCESS);
}
