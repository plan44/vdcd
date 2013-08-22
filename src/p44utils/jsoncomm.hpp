//
//  jsoncomm.hpp
//  p44utils
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__jsoncomm__
#define __p44utils__jsoncomm__

#include "socketcomm.hpp"

#include "jsonobject.hpp"

using namespace std;

namespace p44 {

  // Errors
  typedef enum json_tokener_error JsonCommErrors;

  class JsonCommError : public Error
  {
  public:
    static const char *domain() { return "JsonComm"; }
    virtual const char *getErrorDomain() const { return JsonCommError::domain(); };
    JsonCommError(JsonCommErrors aError) : Error(ErrorCode(aError), json_tokener_error_desc(aError)) {};
    JsonCommError(JsonCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  class JsonComm;


  /// generic callback for delivering a received JSON object or an error occurred when receiving JSON
  typedef boost::function<void (JsonComm *aJsonCommP, ErrorPtr aError, JsonObjectPtr aJsonObject)> JSonMessageCB;

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

    JsonComm(SyncIOMainLoop *aMainLoopP);
    virtual ~JsonComm();

    /// install callback for received JSON messages
    /// @param aJsonMessageHandler will be called when a JSON message has been received
    void setMessageHandler(JSonMessageCB aJsonMessageHandler);

    /// send a JSON message
    /// @param aJsonObject the JSON that is to be sent
    /// @result empty or Error object in case of error sending message
    ErrorPtr sendMessage(JsonObjectPtr aJsonObject);


    /// request closing connection after last message has been sent
    void closeAfterSend();


  private:
    void gotData(ErrorPtr aError);
    void canSendData(ErrorPtr aError);
    
  };
  
} // namespace p44


#endif /* defined(__p44utils__jsoncomm__) */
