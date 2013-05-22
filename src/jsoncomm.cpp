//
//  jsoncomm.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 22.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "jsoncomm.hpp"

using namespace p44;


#pragma mark - JsonObject

JsonObject::JsonObject()
{
  json_obj = json_object_new_object();
}


JsonObject::JsonObject(struct json_object *obj)
{
  json_obj = obj;
}


JsonObject::~JsonObject()
{
  if (json_obj) {
    json_object_put(json_obj);
    json_obj = NULL;
  }
}


const char *JsonObject::c_str()
{
  if (json_obj) {
    return json_object_to_json_string(json_obj);
  }
  else
    return NULL;
}


#pragma mark - JsonComm

JsonComm::JsonComm(SyncIOMainLoop *aMainLoopP) :
  inherited(aMainLoopP),
  tokener(NULL),
  ignoreUntilNextEOM(false)
{
  setReceiveHandler(boost::bind(&JsonComm::gotData, this, _2));
  setTransmitHandler(boost::bind(&JsonComm::canSendData, this, _2));
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
  jsonMessageHandler= aJsonMessageHandler;
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
            struct json_object *o = json_tokener_parse_ex(tokener, (const char *)buf, (int)eom);
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
              if (jsonMessageHandler) {
                jsonMessageHandler(this, ErrorPtr(), JsonObjectPtr(new JsonObject(o)));
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
        // all ok, nothing to report
        return;
      } // no read error
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


void JsonComm::sendMessage(JsonObjectPtr aJsonObject, ErrorPtr &aError)
{
  const char *json_string = aJsonObject->c_str();
  size_t jsonSize = strlen(json_string);
  if (transmitBuffer.size()>0) {
    // other messages are already waiting, buffer entire message
    transmitBuffer.assign(json_string, jsonSize);
  }
  else {
    // nothing in buffer yet, start new send
    size_t sentBytes = transmitBytes(jsonSize, (uint8_t *)json_string, aError);
    if (Error::isOK(aError)) {
      // check if all could be sent
      if (sentBytes<jsonSize) {
        // buffer the rest, canSendData handler will take care of writing it out
        transmitBuffer.assign(json_string+sentBytes, jsonSize-sentBytes);
      }
    }
  }
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
      }
      else {
        // partially sent, remove sent bytes
        transmitBuffer.erase(0, sentBytes);
      }
    }
  }
}



#ifdef GUGUS

  report(LOG_DEBUG, "--> received device cluster API operation %s\n", rxBuffer);

  enum json_tokener_error json_error;
  struct json_tokener* tok;

  tok = json_tokener_new();
  if (!tok) {
    continue;
  }
  json_object* json_request = json_tokener_parse_ex(tok, rxBuffer, -1);
  json_error = tok->err;

  json_tokener_free(tok);
  if (json_error == json_tokener_success) {

    /* TODO: do something with the JSON requests */
    json_object* operationObj = json_object_object_get(json_request, "operation");
    const char* operation;

    if (operationObj != NULL) {
      json_object* parameterObj = json_object_object_get(json_request, "parameter");
      operation = json_object_get_string(operationObj);

      // first check device registration (which does not need the deviceID parameter)
      if (strcmp("DeviceRegistration", operation) == 0) {
        const char* dsid = json_object_get_string(json_object_object_get(parameterObj, "dSID"));

        // - create device session, as it is needed for lookups
        dSDevice* device = (dSDevice*) malloc(sizeof(dSDevice));
        device->sc.fd = 0;
        device->sc.active = timestamp();
        device->dc = &devCtrl;
        // - add to list
        pthread_mutex_lock(&deviceListMutex);
        LL_PREPEND(deviceList, device);
        pthread_mutex_unlock(&deviceListMutex);
        // save by dsid
        strcpy(device->dsid, dsid);
        SelectB_RegisterDevice(dsid,
                               json_object_get_int(json_object_object_get(parameterObj, "VendorId")),
                               json_object_get_int(json_object_object_get(parameterObj, "FunctionId")),
                               json_object_get_int(json_object_object_get(parameterObj, "ProductId")),
                               json_object_get_int(json_object_object_get(parameterObj, "Version")),
                               json_object_get_int(json_object_object_get(parameterObj, "LTMode")),
                               json_object_get_int(json_object_object_get(parameterObj, "Mode")));
      }
      else {
        // these commands must have a deviceID parameter
        // - get deviceID param
        uint16_t deviceID = json_object_get_int(json_object_object_get(parameterObj, "BusAddress"));
        if (errno==EINVAL) {
          // no deviceID in request
          json_object *call = json_object_new_object();
          json_object_object_add(call, "error", json_object_new_string("Missing BusAddress"));
          device_api_send_direct(NULL, call);
          json_object_put(call);
        }
        else {
          // commands with device
          if (strcmp("DeviceButtonClick", operation) == 0) {
            /* allocate short upstream event */
            ShortUpstreamEvt * e = Q_NEW(ShortUpstreamEvt, SHORT_UPSTREAM_SIG);
            e->source = deviceID;
            e->circuit = 0;
            e->key = json_object_get_int(json_object_object_get(parameterObj, "key"));
            e->click = json_object_get_int(json_object_object_get(parameterObj, "click"));
            e->sensor = 0;
            e->resend = 0;
            e->quality = 62;
            e->flags = 0;
            /* publish short upstream event */
            QF_publish((QEvent*)e);
          } else if (strcmp("DeviceParameter", operation) == 0) {
            DsmApi_SendDeviceParameterEvent(deviceID,
                                            json_object_get_int(json_object_object_get(parameterObj, "Bank")),
                                            json_object_get_int(json_object_object_get(parameterObj, "Offset")),
                                            json_object_get_int(json_object_object_get(parameterObj, "Value")));
          } else if (strcmp("DeviceSensorType", operation) == 0) {
          } else if (strcmp("DeviceSensorValue", operation) == 0) {
            DsmApi_SendSensorValueEvent(deviceID,
                                        json_object_get_int(json_object_object_get(parameterObj, "SensorId")),
                                        json_object_get_int(json_object_object_get(parameterObj, "Value")));
          } else if (strcmp("Pong", operation) == 0) {
            PingResponseEvt * e = Q_NEW(PingResponseEvt, SELECT_B_PING_RESPONSE_SIG);
            e->deviceId = deviceID;
            QActive_postFIFO(AO_SelectB, (QEvent *)e);
          }
          else {
            // Unknown operation
            json_object *call = json_object_new_object();
            json_object_object_add(call, "error", json_object_new_string("Unknown operation"));
            device_api_send_direct(NULL, call);
            json_object_put(call);
          }
        }
      }
      json_object_put(parameterObj);
    }
    json_object_put(operationObj);
  } else {
    json_object *call = json_object_new_object();
    json_object_object_add(call, "error", json_object_new_string("Could not parse request JSON"));
    device_api_send_direct(NULL, call);
    json_object_put(call);
  }
  json_object_put(json_request);
  p = rxBuffer;
  rxLen = 0;
}



#endif