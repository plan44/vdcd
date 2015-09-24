//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

  /// generic callback for delivering a received Text line
  typedef boost::function<void (ErrorPtr aError, string aTextLine)> TextLineCB;


  typedef boost::intrusive_ptr<JsonComm> JsonCommPtr;
  /// A class providing low level access to the DALI bus
  class JsonComm : public SocketComm
  {
    typedef SocketComm inherited;

    JSonMessageCB jsonMessageHandler;
    TextLineCB rawMessageHandler;

    // Raw message receiving
    string textLine;

    // JSON parsing
    struct json_tokener* tokener;
    bool ignoreUntilNextEOM;

    // JSON sending
    string transmitBuffer;
    bool closeWhenSent;

  public:

    JsonComm(MainLoop &aMainLoop);
    virtual ~JsonComm();

    /// install callback for received JSON messages
    /// @param aJsonMessageHandler will be called when a JSON message has been received
    /// @note setting the JSON message handler will disable raw message processing
    void setMessageHandler(JSonMessageCB aJsonMessageHandler);

    /// install callback for received raw messages (single line)
    /// @param aRawMessageHandler will be called when a complete text line has been received
    /// @note setting the raw message handler will disable JSON message processing
    void setRawMessageHandler(TextLineCB aRawMessageHandler);


    /// send a JSON message
    /// @param aJsonObject the JSON that is to be sent
    /// @result empty or Error object in case of error sending message
    ErrorPtr sendMessage(JsonObjectPtr aJsonObject);


    /// send raw text
    /// @param aRawBytes bytes to be sent
    /// @result empty or Error object in case of error sending raw data
    ErrorPtr sendRaw(string &aRawBytes);

    /// request closing connection after last message has been sent
    void closeAfterSend();

    /// clear all callbacks
    /// @note this is important because handlers might cause retain cycles when they have smart ptr arguments
    virtual void clearCallbacks() { jsonMessageHandler = NULL; inherited::clearCallbacks(); }


  private:
    void gotData(ErrorPtr aError);
    void canSendData(ErrorPtr aError);
    
  };
  
} // namespace p44


#endif /* defined(__p44utils__jsoncomm__) */
