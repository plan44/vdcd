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

#ifndef __vdcd__voxnetcomm__
#define __vdcd__voxnetcomm__

#include "vdcd_common.hpp"

#if ENABLE_VOXNET

#include "socketcomm.hpp"


using namespace std;

namespace p44 {

  class VoxnetComm : public SocketComm
  {
    typedef SocketComm inherited;

    typedef enum {
      commState_unknown,
      commState_menuwait,
      commState_servicesread,
      commState_idle
    } CommState;

    CommState commState;

  public:
    /// create driver Voxnet
    VoxnetComm();

    /// destructor
    ~VoxnetComm();

    /// set the connection parameters to connect to the EnOcean TCM310 modem
    /// @param aVoxnetHost voxnet server host address
    void setConnectionSpecification(const char *aVoxnetHost);

    /// initialize (start) communication with voxnet server
    void initialize(StatusCB aCompletedCB);

  private:

    void start();
    void connectionStatusHandler(ErrorPtr aError);
    void dataHandler(ErrorPtr aError);


  };
  typedef boost::intrusive_ptr<VoxnetComm> VoxnetCommPtr;


} // namespace p44

#endif // ENABLE_VOXNET

#endif /* defined(__vdcd__voxnetcomm__) */

