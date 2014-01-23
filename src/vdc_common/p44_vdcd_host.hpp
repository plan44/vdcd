//
//  Copyright (c) 2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__p44_vdcd_host__
#define __vdcd__p44_vdcd_host__

#include "devicecontainer.hpp"

#include "jsoncomm.hpp"

using namespace std;

namespace p44 {

  class P44VdcHost : public DeviceContainer
  {
    typedef DeviceContainer inherited;

  public:

    P44VdcHost();

    /// JSON API for web interface
    SocketComm configApiServer;

    void startConfigApi();

  private:

    SocketCommPtr configApiConnectionHandler(SocketComm *aServerSocketCommP);
    void configApiRequestHandler(JsonComm *aJsonCommP, ErrorPtr aError, JsonObjectPtr aJsonObject);

  };



}

#endif /* defined(__vdcd__p44_vdcd_host__) */
