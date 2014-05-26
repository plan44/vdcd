//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
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

#include <ifaddrs.h>
#include <arpa/inet.h>

bool p44::getIfInfo(uint64_t *aMacAddressP, uint32_t *aIPv4AddressP)
{
  bool found = false;

  // MAC address
  if (aMacAddressP) {
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
      return false; // failed
    }
    else {
      // Get the size of the data available (store in len)
      if (sysctl(mgmtInfoBase, 6, NULL, &length, NULL, 0) < 0) {
        return false; // failed
      }
      else {
        // Alloc memory based on above call
        if ((msgBuffer = (char *)malloc(length)) == NULL) {
          return false; // failed
        }
        else {
          // Get system information, store in buffer
          if (sysctl(mgmtInfoBase, 6, msgBuffer, &length, NULL, 0) < 0) {
            free(msgBuffer); // Release the buffer memory
            return false; // failed
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
    if (mac!=0) {
      found = true;
      *aMacAddressP = mac;
    }
  }
  // IPv4 address
  if (aIPv4AddressP) {
    // From: http://zachwaugh.me/posts/programmatically-retrieving-ip-address-of-iphone/ (MIT licensed)
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    int success = 0;
    uint32_t ip = 0;

    // retrieve the current interfaces - returns 0 on success
    success = getifaddrs(&interfaces);
    if (success == 0) {
      // Loop through linked list of interfaces
      temp_addr = interfaces;
      while (temp_addr != NULL) {
        if(temp_addr->ifa_addr->sa_family == AF_INET) {
          // Check if interface is en0 (every Apple device has one, it's considered the main interface)
          if (strcmp(temp_addr->ifa_name,"lo0")!=0) {
            uint8_t *addr = (uint8_t *)&(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr.s_addr);
            ip = (addr[0]<<24) + (addr[1]<<16) + (addr[2]<<8) + addr[3];
            if (ip!=0) {
              *aIPv4AddressP = ip;
              found = true;
              break;
            }
          }
        }
        temp_addr = temp_addr->ifa_next;
      }
    }
    
    // Free memory
    freeifaddrs(interfaces);

  }
  return found;
}

#else

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>


bool p44::getIfInfo(uint64_t *aMacAddressP, uint32_t *aIPv4AddressP)
{
  int sock;
  int ifIndex;
  struct ifreq ifr;
  int res;
  uint64_t mac = 0;
  uint32_t ip = 0;
  bool found = false;

  // any socket type will do
  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock>=0) {
    // enumerate interfaces
    ifIndex = 1; // start with 1
    do {
      // - init struct
      memset(&ifr, 0x00, sizeof(ifr));
      // - get name of interface by index
      ifr.ifr_ifindex = ifIndex;
      res = ioctl(sock, SIOCGIFNAME, &ifr);
      if (res<0) {
        break; // no more names, end
      }
      // got name for index
      // - get flags for it
      if (ioctl(sock, SIOCGIFFLAGS, &ifr)>=0) {
        // skip loopback interfaces
        if ((ifr.ifr_flags & IFF_LOOPBACK)==0) {
          // not loopback
          // - now get HWADDR
          if (aMacAddressP && ioctl(sock, SIOCGIFHWADDR, &ifr)>=0) {
            // compose int64
            for (int i=0; i<6; ++i) {
              mac = (mac<<8) + ((uint8_t *)(ifr.ifr_hwaddr.sa_data))[i];
            }
            // this is our MAC unless it is zero
            if (mac!=0) {
              *aMacAddressP = mac; // found, return it
              found=true; // done, use it (even if IP is 0)
            }
          }
          // - also get IPv4
          if (aIPv4AddressP && ioctl(sock, SIOCGIFADDR, &ifr)>=0) {
            for (int i=0; i<4; ++i) {
              ip = (ip<<8) + ((uint8_t *)(ifr.ifr_addr.sa_data))[i];
            }
            *aIPv4AddressP = ip;
            found = true;
          }
          // done if found something
          if (found)
            break;
        }
      }
      // next
      ifIndex++;
    } while(true);
    close(sock);
  }
  return found;
}


#endif // non-apple



uint64_t p44::macAddress()
{
  uint64_t mac;
  if (getIfInfo(&mac, NULL))
    return mac;
  return 0; // none
}


uint32_t p44::ipv4Address()
{
  uint32_t ip;
  if (getIfInfo(NULL, &ip))
    return ip;
  return 0; // none
}
