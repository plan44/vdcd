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

#ifndef __vdcd__macaddress__
#define __vdcd__macaddress__

#include <stdint.h>

namespace p44 {

  /// get MAC address of this machine
  /// @return MAC address as 64bit int (upper 16bits zero) or 0 if none could be determined
  /// @note see getIfInfo() for details how MAC address is obtained
  uint64_t macAddress();

  /// get MAC address of this machine
  /// @return IPv4 address as 32bit int or 0 if none could be determined
  /// @note see getIfInfo() for details how interface is determined
  uint32_t ipv4Address();

  /// get network interface information
  /// @param aMacAddress if not NULL: is set to the MAC address of this machine
  /// @param aIPv4Address if not NULL: is set to current IPv4 address of this machine
  /// Notes:
  /// - On Linux, the first non-loopback interface's MAC will be used (as enumerated by ifr_ifindex 1..n)
  /// - On OS X, the MAC address of the "en0" device will be used (every Mac has a en0, which is the
  ///   built-in network port of the machine; ethernet port for Macs that have one, WiFi port otherwise)
  bool getIfInfo(uint64_t *aMacAddressP, uint32_t *aIPv4AddressP);

} // namespace p44


#endif /* defined(__vdcd__macaddress__) */
