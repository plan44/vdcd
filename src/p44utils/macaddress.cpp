//
//  macaddress.cpp
//  p44utils
//
//  Created by Lukas Zeller on 25.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "macaddress.hpp"

#include <stdlib.h>
#include <string.h>

using namespace p44;

#ifdef __APPLE__

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>

uint64_t p44::macAddress()
{
  int mgmtInfoBase[6];
  char *msgBuffer = NULL;
  size_t length;
  unsigned char macAddress[6];
  struct if_msghdr *interfaceMsgStruct;
  struct sockaddr_dl *socketStruct;

  // Setup the management Information Base (mib)
  mgmtInfoBase[0] = CTL_NET; // Request network subsystem
  mgmtInfoBase[1] = AF_ROUTE; // Routing table info
  mgmtInfoBase[2] = 0;
  mgmtInfoBase[3] = AF_LINK; // Request link layer information
  mgmtInfoBase[4] = NET_RT_IFLIST; // Request all configured interfaces

  // With all configured interfaces requested, get handle index
  if ((mgmtInfoBase[5] = if_nametoindex("en0")) == 0) {
    return 0; // failed
  }
  else {
    // Get the size of the data available (store in len)
    if (sysctl(mgmtInfoBase, 6, NULL, &length, NULL, 0) < 0) {
      return 0; // failed
    }
    else {
      // Alloc memory based on above call
      if ((msgBuffer = (char *)malloc(length)) == NULL) {
        return 0; // failed
      }
      else {
        // Get system information, store in buffer
        if (sysctl(mgmtInfoBase, 6, msgBuffer, &length, NULL, 0) < 0) {
          free(msgBuffer); // Release the buffer memory
          return 0; // failed
        }
      }
    }
  }
  // Map msgbuffer to interface message structure
  interfaceMsgStruct = (struct if_msghdr *) msgBuffer;
  // Map to link-level socket structure
  socketStruct = (struct sockaddr_dl *) (interfaceMsgStruct + 1);
  // Copy link layer address data in socket structure to an array
  memcpy(&macAddress, socketStruct->sdl_data + socketStruct->sdl_nlen, 6);
  free(msgBuffer); // Release the buffer memory
  // compose int64
  uint64_t mac = 0;
  for (int i=0; i<6; ++i) {
    mac = (mac<<8) + macAddress[i];
  }
  return mac;
}

#else

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>


uint64_t p44::macAddress()
{
  struct ifreq ifr;
  struct ifconf ifc;
  char buf[1024];
  int success = 0;

  do {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { /* handle error*/ };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
      break;
    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (; it != end; ++it) {
      strcpy(ifr.ifr_name, it->ifr_name);
      if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
          if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            success = 1;
            break;
          }
        }
      }
      else
        break;
    }
  } while(false);
  // extract MAC if we have one
  if (!success) {
    return 0; // failed
  }
  // compose int64
  uint64_t mac = 0;
  for (int i=0; i<6; ++i) {
    mac = (mac<<8) + ((uint8_t *)(ifr.ifr_hwaddr.sa_data))[i];
  }
  return mac;
}


#endif // non-apple
