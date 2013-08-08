//
//  jsoncomm.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "jsoncomm.hpp"

using namespace p44;


JsonComm::JsonComm(SyncIOMainLoop *aMainLoopP) :
  inherited(aMainLoopP),
  tokener(NULL),
  ignoreUntilNextEOM(false)
{
  setReceiveHandler(boost::bind(&JsonComm::gotData, this, _2));
}


JsonComm::~JsonComm()
{
  if (tokener) {
    json_tokener_free(tokener);
    tokener = NULL;
  }
}


void JsonComm::setMessageHandler(JSonMessageCB aJsonMessageHandler)
{
  jsonMessageHandler = aJsonMessageHandler;
}


void JsonComm::gotData(ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // no error, read data we've got so far
    size_t dataSz = numBytesReady();
    if (dataSz>0) {
      // temporary buffer
      uint8_t *buf = new uint8_t[dataSz];
      size_t receivedBytes = receiveBytes(dataSz, buf, aError);
      if (Error::isOK(aError)) {
        // check for end-of-message (LF), make spaces from any other ctrl char
        size_t bom = 0;
        while (bom<receivedBytes) {
          // data to process, scan for EOM
          size_t eom = bom;
          bool messageComplete = false;
          while (eom<receivedBytes) {
            if (buf[eom]<0x20) {
              if (buf[eom]=='\n') {
                // end of message
                buf[eom] = 0; // terminate message here
                messageComplete = true;
                break;
              }
              else {
                // other control char, convert to space
                buf[eom] = ' ';
              }
            }
            eom++;
          }
          // create tokener to parse message, if none found already
          if (!tokener) {
            tokener = json_tokener_new();
          }
          if (eom>0 && !ignoreUntilNextEOM) {
            // feed data to tokener
            struct json_object *o = json_tokener_parse_ex(tokener, (const char *)buf+bom, (int)(eom-bom));
            if (o==NULL) {
              // error (or incomplete JSON, which is fine)
              JsonCommErrors err = json_tokener_get_error(tokener);
              if (err!=json_tokener_continue) {
                // real error
                if (jsonMessageHandler) {
                  jsonMessageHandler(this, ErrorPtr(new JsonCommError(err)), JsonObjectPtr());
                }
                // reset the parser
                ignoreUntilNextEOM = true;
                json_tokener_reset(tokener);
              }
            }
            else {
              // got JSON object
              JsonObjectPtr message = JsonObject::newObj(o);
              if (jsonMessageHandler) {
                // pass json_object into handler, will consume it
                jsonMessageHandler(this, ErrorPtr(), message);
              }
              ignoreUntilNextEOM = true;
              json_tokener_reset(tokener);
            }
          }
          // now check for having reached the end of the message in this data chunk
          if (messageComplete) {
            // new message starts, don't ignore any more
            ignoreUntilNextEOM = false;
            // skip any control chars
            while (eom<receivedBytes && buf[eom]<0x20) eom++;
          }
          // now eom becomes the new bom
          bom = eom;
        } // while data to process
      } // no read error
      free(buf); buf = NULL;
    } // some data seems to be ready
  } // no connection error
  if (!Error::isOK(aError)) {
    // error occurred, report
    if (jsonMessageHandler) {
      jsonMessageHandler(this, aError, JsonObjectPtr());
    }
    ignoreUntilNextEOM = false;
    if (tokener) json_tokener_reset(tokener);
  }
}


ErrorPtr JsonComm::sendMessage(JsonObjectPtr aJsonObject)
{
  ErrorPtr err;
  string json_string = aJsonObject->json_c_str();
  json_string.append("\n");
  size_t jsonSize = json_string.size();
  if (transmitBuffer.size()>0) {
    // other messages are already waiting, append entire message
    transmitBuffer.append(json_string);
  }
  else {
    // nothing in buffer yet, start new send
    size_t sentBytes = transmitBytes(jsonSize, (uint8_t *)json_string.c_str(), err);
    if (Error::isOK(err)) {
      // check if all could be sent
      if (sentBytes<jsonSize) {
        // Not everything (or maybe nothing, transmitBytes() can return 0) was sent
        // - enable callback for ready-for-send
        setTransmitHandler(boost::bind(&JsonComm::canSendData, this, _2));
        // buffer the rest, canSendData handler will take care of writing it out
        transmitBuffer.assign(json_string.c_str()+sentBytes, jsonSize-sentBytes);
      }
			else {
				// all sent
				// - disable transmit handler
        setTransmitHandler(NULL);
			}
    }
  }
  return err;
}


void JsonComm::canSendData(ErrorPtr aError)
{
  size_t bytesToSend = transmitBuffer.size();
  if (bytesToSend>0 && Error::isOK(aError)) {
    // send data from transmit buffer
    size_t sentBytes = transmitBytes(bytesToSend, (const uint8_t *)transmitBuffer.c_str(), aError);
    if (Error::isOK(aError)) {
      if (sentBytes==bytesToSend) {
        // all sent
        transmitBuffer.erase();
				// - disable transmit handler
        setTransmitHandler(NULL);
      }
      else {
        // partially sent, remove sent bytes
        transmitBuffer.erase(0, sentBytes);
      }
    }
  }
}

