//
//  device.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "device.hpp"

#include "dsparams.hpp"

using namespace p44;


#pragma mark - digitalSTROM behaviour

//  virtual uint16_t functionId() = 0;
//  virtual uint16_t productId() = 0;
//  virtual uint16_t groupMemberShip() = 0;
//  virtual uint8_t ltMode() = 0;
//  virtual uint8_t outputMode() = 0;
//  virtual uint8_t buttonIdGroup() = 0;




DSBehaviour::DSBehaviour(Device *aDeviceP) :
  deviceP(aDeviceP)
{
}


DSBehaviour::~DSBehaviour()
{
}


ErrorPtr DSBehaviour::handleMessage(string &aOperation, JsonObjectPtr aParams)
{
  // base class behaviour does not support any operations
  return ErrorPtr(new vdSMError(
    vdSMErrorInvalidParameter,
    string_format(
      "unknown device behaviour operation '%s' for %s/%s",
      aOperation.c_str(), shortDesc().c_str(), deviceP->shortDesc().c_str()
    )
  ));
}


ErrorPtr DSBehaviour::getBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t &aValue)
{
  ErrorPtr err;
  if (aParamName=="FID") {
    aValue = functionId();
  }
  else if (aParamName=="PID") {
    aValue = productId();
  }
  else {
    aValue = 0;
    err = ErrorPtr(new vdSMError(
      vdSMErrorInvalidParameter,
      string_format(
        "unknown device behaviour parameter '%s' for %s/%s",
        aParamName.c_str(), shortDesc().c_str(), deviceP->shortDesc().c_str()
      )
    ));
  }
  return err;
}


ErrorPtr DSBehaviour::setBehaviourParam(const string &aParamName, int aArrayIndex, uint32_t aValue)
{
  ErrorPtr err;
  if (false) {

  }
  else {
    err = ErrorPtr(new vdSMError(
      vdSMErrorInvalidParameter,
      string_format(
        "unknown device behaviour parameter '%s' for %s/%s",
        aParamName.c_str(), shortDesc().c_str(), deviceP->shortDesc().c_str()
      )
    ));
  }
  return err;
}




bool DSBehaviour::sendMessage(const char *aOperation, JsonObjectPtr aParams)
{
  // just forward to device
  return deviceP->sendMessage(aOperation, aParams);
}




#pragma mark - Device


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


bool Device::isPublicDS()
{
  // base class assumes that devices with a dS behaviour are public
  // (subclasses might decide otherwise)
  return behaviourP!=NULL;
}


void Device::ping()
{
  // base class just sends the pong, but derived classes which can actually ping their hardware should
  // do so and send the pong only if the hardware actually responds.
  pong();
}


void Device::pong()
{
  sendMessage("Pong", JsonObjectPtr());
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

//  if request['operation'] == 'DeviceRegistrationAck':
//      self.address = request['parameter']['BusAddress']
//      self.zone = request['parameter']['Zone']
//      self.groups = request['parameter']['GroupMemberships']
//      print 'BusAddress:', request['parameter']['BusAddress']
//      print 'Zone:', request['parameter']['Zone']
//      print 'Groups:', request['parameter']['GroupMemberships']



ErrorPtr Device::handleMessage(string &aOperation, JsonObjectPtr aParams)
{
  ErrorPtr err;

  // check for generic device operations
  if (aOperation=="ping") {
    // ping hardware (if possible), creates pong now or after hardware was queried
    ping();
  }
  else if (aOperation=="callscene") {
    // TODO: implement callscene
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="undoscenenumber") {
    // TODO: implement undoscenenumber
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="undoscene") {
    // TODO: implement undoscene
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="blink") {
    // TODO: implement Blink
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="setlocalprio") {
    // TODO: implement SetLocalPrio
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="callscenemin") {
    // TODO: implement CallSceneMin
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="progmodeon") {
    // TODO: implement ProgModeOn
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="progmodeoff") {
    // TODO: implement ProgModeOff
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="savescene") {
    // TODO: implement SaveScene
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="setoutval") {
    // TODO: implement SetOutval
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="setzone") {
    // TODO: implement SetZone
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="getdevicesensortype") {
    // TODO: implement GetDeviceSensorType
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="getdevicesensorvalue") {
    // TODO: implement GetDeviceSensorValue
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (
    aOperation=="getdeviceparameter" ||
    aOperation=="setdeviceparameter"
  ) {
    // convert from Legacy bank/offset device parameter
    bool write = aOperation=="setdeviceparameter";
    uint8_t bank = 0;
    uint8_t offset = 0;
    JsonObjectPtr o = aParams->get("Bank");
    if (o==NULL) {
      err = ErrorPtr(new vdSMError(
        vdSMErrorMissingParameter,
        string_format("missing parameter 'Bank'")
      ));
    }
    else {
      bank = o->int32Value();
      JsonObjectPtr o = aParams->get("Offset");
      if (o==NULL) {
        err = ErrorPtr(new vdSMError(
          vdSMErrorMissingParameter,
          string_format("missing parameter 'Offset'")
        ));
      }
      else {
        offset = o->int32Value();
        // get param entry
        LOG(LOG_NOTICE,"%s Bank=%d, Offset=%d\n", aOperation.c_str(), bank, offset);
        const paramEntry *p = DsParams::paramEntryForBankOffset(bank, offset);
        if (p==NULL) {
          LOG(LOG_NOTICE,"- no named parameter found for bank/offset\n");
          err = ErrorPtr(new vdSMError(
            vdSMErrorInvalidParameter,
            string_format("unknown parameter bank %d / offset %d", bank, offset)
          ));
        }
        else {
          // found named param
          int arrayIndex = offset / p->size;
          uint8_t inFieldOffset = offset-p->offset-(arrayIndex*p->size);
          uint32_t val;
          err = getDeviceParam(p->name, arrayIndex, val);
          if (Error::isOK(err)) {
            if (inFieldOffset<p->size) {
              // now handle read or write
              if (write) {
                // get new value
                JsonObjectPtr o = aParams->get("Value");
                if (o==NULL) {
                  err = ErrorPtr(new vdSMError(
                    vdSMErrorMissingParameter,
                    string_format("missing parameter 'Value'")
                  ));
                }
                else {
                  // replace requested byte
                  uint32_t newVal = o->int32Value();
                  newVal = newVal << ((p->size-inFieldOffset-1)*8); // shift into position
                  val &= ~(0xFF << ((p->size-inFieldOffset-1)*8)); // create mask
                  val |= newVal;
                  // modify param
                  err = setDeviceParam(p->name, arrayIndex, val);
                  LOG(LOG_NOTICE,"- %s[%d] set to %d/0x%X\n", p->name, arrayIndex, val);
                }
              }
              else {
                // extract the requested byte
                val = (val >> ((p->size-inFieldOffset-1)*8)) & 0xFF;
                LOG(LOG_NOTICE,"- %s[%d] = %d/0x%X\n", p->name, arrayIndex, val);
                // create json answer
                // - re-use param
                aParams->add("Value", JsonObject::newInt32(val));
                sendMessage("DeviceParameter", aParams);
              }
            }
            else {
              LOG(LOG_NOTICE,"- %s[%d] : in-field offset %d exceeds field size (%d)\n", p->name, arrayIndex, inFieldOffset, p->size);
              err = ErrorPtr(new vdSMError(
                vdSMErrorInvalidParameter,
                string_format("in-field offset %d exceeds field size (%d)", inFieldOffset, p->size)
              ));
            }
          }
        }
      }
    }
  }
  else if (aOperation=="pushsensorvalue") {
    // TODO: implement PushSensorValue
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else if (aOperation=="setgroups") {
    // TODO: implement SetGroups
    LOG(LOG_NOTICE,"Called unimplemented %s on device %s\n", aOperation.c_str(), shortDesc().c_str());
  }
  else {
    // no generic device operation, let behaviour handle it
    if (behaviourP) {
      err = behaviourP->handleMessage(aOperation, aParams);
    }
    else {
      err = ErrorPtr(new vdSMError(
        vdSMErrorUnknownDeviceOperation,
        string_format("unknown device operation '%s' for %s", aOperation.c_str(), shortDesc().c_str())
      ));
    }
  }
  return err;
}



ErrorPtr Device::getDeviceParam(const string &aParamName, int aArrayIndex, uint32_t &aValue)
{
  ErrorPtr err;
  // check common device parameters
  if (aParamName=="DSID") {
    // TODO: this only works for dsids in the dSD class
    aValue = (uint32_t)dsid.getSerialNo();
  }
  else if (aParamName=="ADR") {
    aValue = busAddress;
  }
  else if (aParamName=="GRP") {
    aValue = busAddress;
  }
  // check behaviour specific params
  else {
    if (behaviourP) {
      err = behaviourP->getBehaviourParam(aParamName, aArrayIndex, aValue);
    }
  }
  return err;
}


ErrorPtr Device::setDeviceParam(const string &aParamName, int aArrayIndex, uint32_t aValue)
{
  ErrorPtr err;
  // check common device parameters
  if (false) {

  }
  // check behaviour specific params
  else {
    if (behaviourP) {
      err = behaviourP->setBehaviourParam(aParamName, aArrayIndex, aValue);
    }
  }
  return err;
}


bool Device::sendMessage(const char *aOperation, JsonObjectPtr aParams)
{
  if (!aParams) {
    // no parameters passed, create new parameter object
    aParams = JsonObject::newObj();
  }
  // add dsid and bus address parameters
  aParams->add("dSID", JsonObject::newString(dsid.getString()));
  if (registered) {
    aParams->add("BusAddress", JsonObject::newInt32(busAddress));
  }
  // have device container send it
  return classContainerP->getDeviceContainerP()->sendMessage(aOperation, aParams);
}



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
