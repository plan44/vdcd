//
//  macaddress.hpp
//  p44utils
//
//  Created by Lukas Zeller on 25.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__macaddress__
#define __vdcd__macaddress__

#include <stdint.h>

namespace p44 {

  /// get MAC address of this machine
  /// @return MAC address as 64bit int (upper 16bits zero)
  /// Notes:
  /// - On Linux, the first non-loopback interface's MAC will be used (as enumerated by ifr_ifindex 1..n)
  /// - On OS X, the MAC address of the "en0" device will be used (every Mac has a en0, which is the
  ///   built-in network port of the machine; ethernet port for Macs that have one, WiFi port otherwise)
  uint64_t macAddress();

} // namespace p44


#endif /* defined(__vdcd__macaddress__) */
