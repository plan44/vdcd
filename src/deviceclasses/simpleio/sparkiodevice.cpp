//
//  sparkiodevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 20.01.14.
//  Copyright (c) 2014 plan44.ch. All rights reserved.
//

#include "sparkiodevice.hpp"


#include "fnv.hpp"

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"


using namespace p44;


SparkIoDevice::SparkIoDevice(StaticDeviceContainer *aClassContainerP, const string &aDeviceConfig) :
  Device((DeviceClassContainer *)aClassContainerP),
  sparkCloudComm(SyncIOMainLoop::currentMainLoop()),
  apiVersion(0),
  outputValue(0),
  outputChangePending(false)
{
  // config must be: sparkCoreId:accessToken
  size_t i = aDeviceConfig.find_first_of(':');
  if (i!=string::npos) {
    sparkCoreID = aDeviceConfig.substr(0,i);
    sparkCoreToken = aDeviceConfig.substr(i+1,string::npos);
  }
  // Simulate light device
  // - defaults to yellow (light)
  primaryGroup = group_yellow_light;
  // - use light settings, which include a scene table
  deviceSettings = DeviceSettingsPtr(new LightDeviceSettings(*this));
  // - create one output
  LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
  l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, -1);
  l->setHardwareName("spark core output");
  addBehaviour(l);
  // dsuid
	deriveDsUid();
}



void SparkIoDevice::initializeDevice(CompletedCB aCompletedCB, bool aFactoryReset)
{
  // get vdsd API version
  string url = string_format("https://api.spark.io/v1/devices/%s/vdsd", sparkCoreID.c_str());
  string data;
  HttpComm::appendFormValue(data, "access_token", sparkCoreToken);
  HttpComm::appendFormValue(data, "args", "version");
  sparkCloudComm.jsonReturningRequest(url.c_str(), boost::bind(&SparkIoDevice::apiVersionReceived, this, aCompletedCB, aFactoryReset, _2, _3), "POST", data);
}


void SparkIoDevice::apiVersionReceived(CompletedCB aCompletedCB, bool aFactoryReset, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    JsonObjectPtr o = aJsonResponse->get("return_value");
    if (o) {
      apiVersion = o->int32Value();
    }
    o = aJsonResponse->get("name");
    if (o) {
      initializeName(o->stringValue());
    }
  }
  // initialized
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}



void SparkIoDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (aOutputBehaviour.getIndex()==0) {
    outputValue = aOutputBehaviour.valueForHardware();
    // set output value
    postOutputValue(aOutputBehaviour);
  }
  else
    return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}


void SparkIoDevice::postOutputValue(OutputBehaviour &aOutputBehaviour)
{
  if (apiVersion==1) {
    string url = string_format("https://api.spark.io/v1/devices/%s/vdsd", sparkCoreID.c_str());
    string data;
    HttpComm::appendFormValue(data, "access_token", sparkCoreToken);
    HttpComm::appendFormValue(data, "args", string_format("output0=%d", outputValue));
    // posting might fail if done too early
    if (!sparkCloudComm.jsonReturningRequest(url.c_str(), boost::bind(&SparkIoDevice::outputChanged, this, aOutputBehaviour, _2, _3), "POST", data))
      outputChangePending = true; // retry when previous request done
  }
}



void SparkIoDevice::outputChanged(OutputBehaviour &aOutputBehaviour, JsonObjectPtr aJsonResponse, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (outputChangePending) {
      outputChangePending = false;
      postOutputValue(aOutputBehaviour); // one more change pending
    }
    else {
      aOutputBehaviour.outputValueApplied(); // confirm having applied the value
      outputChangePending = false;
    }
  }
}







void SparkIoDevice::deriveDsUid()
{
  if (getDeviceContainer().usingDsUids()) {
    // vDC implementation specific UUID:
    //   UUIDv5 with name = classcontainerinstanceid::SparkCoreID
    DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
    string s = classContainerP->deviceClassContainerInstanceIdentifier();
    s += "::" + sparkCoreID;
    dSUID.setNameInSpace(s, vdcNamespace);
  }
  else {
    Fnv64 hash;
    // we have no unqiquely defining device information, construct something as reproducible as possible
    // - use class container's ID
    string s = classContainerP->deviceClassContainerInstanceIdentifier();
    hash.addBytes(s.size(), (uint8_t *)s.c_str());
    // - add-in the spark core ID
    hash.addCStr(sparkCoreID.c_str());
    #if FAKE_REAL_DSD_IDS
    dSUID.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
    dSUID.setDsSerialNo(hash.getHash28()<<4); // leave lower 4 bits for input number
    #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
    #else
    dSUID.setObjectClass(DSID_OBJECTCLASS_MACADDRESS); // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
    dSUID.setSerialNo(0x7000000000000ll+hash.getHash48());
    #endif
    // TODO: validate, now we are using the MAC-address class with bits 48..51 set to 7
  }
}


string SparkIoDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- has output controlling brightness via spark cloud API\n");
  return s;
}
