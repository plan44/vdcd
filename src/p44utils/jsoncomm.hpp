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

#ifndef __p44utils__jsoncomm__
#define __p44utils__jsoncomm__

#include "socketcomm.hpp"

#include "jsonobject.hpp"

using namespace std;

namespace p44 {

  class JsonComm;

  /// generic callback for delivering a received JSON object or an error occurred when receiving JSON
  typedef boost::function<void (ErrorPtr aError, JsonObjectPtr aJsonObject)> JSonMessageCB;

  typedef boost::intrusive_ptr<JsonComm> JsonCommPtr;
  /// A class providing low level access to the DALI bus
  class JsonComm : public SocketComm
  {
    typedef SocketComm inherited;

    JSonMessageCB jsonMessageHandler;

    // JSON parsing
    struct json_tokener* tokener;
    bool ignoreUntilNextEOM;

    // JSON sending
    string transmitBuffer;
    bool closeWhenSent;

  public:

    JsonComm(SyncIOMainLoop &aMainLoop);
    virtual ~JsonComm();

    /// install callback for received JSON messages
    /// @param aJsonMessageHandler will be called when a JSON message has been received
    void setMessageHandler(JSonMessageCB aJsonMessageHandler);

    /// send a JSON message
    /// @param aJsonObject the JSON that is to be sent
    /// @result empty or Error object in case of error sending message
    ErrorPtr sendMessage(JsonObjectPtr aJsonObject);


    /// send a JSON message
    /// @param aRawBytes bytes to be sent
    /// @result empty or Error object in case of error sending raw data
    ErrorPtr sendRaw(string &aRawBytes);

    /// request closing connection after last message has been sent
    void closeAfterSend();


  private:
    void gotData(ErrorPtr aError);
    void canSendData(ErrorPtr aError);
    
  };
  
} // namespace p44


#endif /* defined(__p44utils__jsoncomm__) */
