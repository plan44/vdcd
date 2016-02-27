//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__voxnetcomm__
#define __vdcd__voxnetcomm__

#include "vdcd_common.hpp"

#if ENABLE_VOXNET

#include "socketcomm.hpp"


using namespace std;

namespace p44 {

  typedef boost::function<bool (const string aVoxnetID, const string aVoxnetStatus)> VoxnetStatusCB;

  class VoxnetComm : public SocketComm
  {
    typedef SocketComm inherited;

    typedef enum {
      commState_unknown,
      commState_discovery,
      commState_servercheck,
      commState_menuwait,
      commState_servicesread,
      commState_idle
    } CommState;

    CommState commState;

    StatusCB initializedCB;

    VoxnetStatusCB voxnetStatusHandler;

    string manualServerIP;
    long searchTimeoutTicket;

    long statusRequestTicket;


  public:

    typedef map<string, string> StringStringMap;

    StringStringMap rooms;
    StringStringMap users;
    StringStringMap sources;
    StringStringMap aliases;

    /// create driver Voxnet
    VoxnetComm();

    /// destructor
    ~VoxnetComm();


    /// set the connection parameters to connect to the voxnet server
    /// @param aVoxnetHost voxnet server host address, or NULL/empty string to use autodiscovery
    void setConnectionSpecification(const char *aVoxnetHost);

    /// initialize (start) communication with voxnet server
    void initialize(StatusCB aCompletedCB);

    /// set status handler
    void setVoxnetStatusHandler(VoxnetStatusCB aVoxnetStatusCB) { voxnetStatusHandler = aVoxnetStatusCB; }

    /// send voxnet text command
    void sendVoxnetText(const string aVoxNetText);

    /// request status
    void requestStatus();

    /// resolve a voxnet reference (i.e. convert to ID if given an alias)
    void resolveVoxnetRef(string &aVoxNetRef);


  private:

    void discoverAndStart();
    void stopDiscovery();
    void searchSocketStatusHandler(ErrorPtr aError);
    void searchTimedOut();
    void searchDataHandler(ErrorPtr aError);

    void start();
    void connectionStatusHandler(ErrorPtr aError);
    void dataHandler(ErrorPtr aError);
    void voxnetInitialized(ErrorPtr aError);


  };
  typedef boost::intrusive_ptr<VoxnetComm> VoxnetCommPtr;


} // namespace p44

#endif // ENABLE_VOXNET
#endif // __vdcd__voxnetcomm__

