//
//  device.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "device.hpp"

using namespace p44;


DSBehaviour::DSBehaviour(Device *aDeviceP) :
  deviceP(aDeviceP)
{
}

DSBehaviour::~DSBehaviour()
{
}



Device::Device(DeviceClassContainer *aClassContainerP) :
  registered(Never),
  registering(Never),
  busAddress(0),
  classContainerP(aClassContainerP),
  behaviourP(NULL)
{

}


Device::~Device()
{
  setDSBehaviour(NULL);
}


void Device::setDSBehaviour(DSBehaviour *aBehaviour)
{
  if (behaviourP)
    delete behaviourP;
  behaviourP = aBehaviour;
}



JsonObjectPtr Device::registrationParams()
{
  // create the registration request
  JsonObjectPtr req = JsonObject::newObj();
  // add the parameters
  req->add("dSID", JsonObject::newString(dsid.getString()));
  req->add("VendorId", JsonObject::newInt32(1)); // TODO: %%% luz: must be 1=aizo, dsa cannot expand other ids so far
  req->add("FunctionId", JsonObject::newInt32(behaviourP->functionId()));
  req->add("ProductId", JsonObject::newInt32(behaviourP->productId()));
  req->add("Version", JsonObject::newInt32(behaviourP->version()));
  req->add("LTMode", JsonObject::newInt32(behaviourP->ltMode()));
  req->add("Mode", JsonObject::newInt32(behaviourP->outputMode()));
  // return it
  return req;
}


void Device::confirmRegistration(JsonObjectPtr aParams)
{
  JsonObjectPtr o = aParams->get("BusAddress");
  if (o) {
    busAddress = o->int32Value();
  }
  // registered now
  registered = MainLoop::now();
  registering = Never;
}




//  json_object* operationObj = json_object_object_get(json_request, "operation");


//  json_object *call = json_object_new_object();
//  json_object *params = json_object_new_object();
//
//  json_object_object_add(params, "Bank", json_object_new_int(paramBank));
//  json_object_object_add(params, "Offset", json_object_new_int(paramOffset));
//
//  json_object_object_add(call, "operation", json_object_new_string("GetDeviceParameter"));
//  json_object_object_add(call, "parameter", params);
//
//  device_api_send_device(deviceId, call);



string Device::shortDesc()
{
  // short description is dsid
  return dsid.getString();
}



string Device::description()
{
  string s = string_format("Device %s", shortDesc().c_str());
  if (registered)
    string_format_append(s, " (BusAddress %d)", busAddress);
  else
    s.append(" (unregistered)");
  s.append("\n");
  if (behaviourP) {
    string_format_append(s, "- Input: %d/%d, DSBehaviour : %s\n", getInputIndex()+1, getNumInputs(), behaviourP->shortDesc().c_str());
  }
  return s;
}
