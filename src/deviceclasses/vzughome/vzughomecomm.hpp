//
//  Copyright (c) 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__vzughomecomm__
#define __vdcd__vzughomecomm__

#include "vdcd_common.hpp"

#if ENABLE_VZUGHOME

#include "socketcomm.hpp"
#include "jsonwebclient.hpp"

using namespace std;

namespace p44 {

  typedef std::list<string> StringList;

  class VZugHomeDiscovery : public SocketComm
  {
    typedef SocketComm inherited;

    long timeoutTicket;
    StatusCB statusCB; // discopvery status callback

  public:
    /// create driver Voxnet
    VZugHomeDiscovery();

    /// destructor
    ~VZugHomeDiscovery();

    /// discover V-ZUG devices
    void discover(StatusCB aStatusCB, MLMicroSeconds aTimeout);

    /// stop discovery early
    void stopDiscovery();

    /// list of API base URLs
    StringList baseURLs;

  private:

    void socketStatusHandler(MLMicroSeconds aTimeout, ErrorPtr aError);
    void searchTimedOut();
    void gotData(ErrorPtr aError);

  };
  typedef boost::intrusive_ptr<VZugHomeDiscovery> VZugHomeDiscoveryPtr;



  class VZugHomeComm : public P44Obj
  {
    typedef P44Obj inherited;


  public:
    /// create driver Voxnet
    VZugHomeComm();

    /// destructor
    ~VZugHomeComm();

    // HTTP communication object
    JsonWebClient apiComm;

    // volatile vars
    string baseURL; ///< base URL for API calls

  };
  typedef boost::intrusive_ptr<VZugHomeComm> VZugHomeCommPtr;


} // namespace p44

#endif // ENABLE_VZUGHOME

#endif /* defined(__vdcd__vzughomecomm__) */

