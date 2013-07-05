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
//  virtual uint8_t ltMode() = 0;
//  virtual uint8_t outputMode() = 0;
//  virtual uint8_t buttonIdGroup() = 0;




DSBehaviour::DSBehaviour(Device *aDeviceP) :
  deviceP(aDeviceP),
  deviceColorGroup(group_black_joker)
{
}


DSBehaviour::~DSBehaviour()
{
}


#warning "// TODO: remove when new vDC API is implemented"
void DSBehaviour::confirmRegistration(JsonObjectPtr aParams)
{
  // - group membership
  JsonObjectPtr o = aParams->get("GroupMemberships");
  if (o) {
    groupMembership = 0; // clear
    for (int i=0; i<o->arrayLength(); i++) {
      JsonObjectPtr elem = o->arrayGet(i);
      if (elem) {
        DsGroup grp = (DsGroup)elem->int32Value();
        groupMembership |= ((DsGroupMask)1)<<grp;
      }
    }
  }
}



void DSBehaviour::setDeviceColor(DsGroup aColorGroup)
{
  // set primary color of the device
  deviceColorGroup = aColorGroup;
  // derive the initial group membership: primary color plus group 0
  groupMembership =
    ((DsGroupMask)1)<<aColorGroup | // primary color
    0x1; // Group 0
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
  else if (aParamName=="GRP") {
    if (aArrayIndex>7)
      aValue = 0;
    else
      aValue = (groupMembership>>(8*aArrayIndex)) & 0xFF;
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
  if (aParamName=="GRP") {
    if (aArrayIndex<=7) {
      DsGroupMask m = ((DsGroupMask)(aValue & 0xFF))<<(8*aArrayIndex);
      groupMembership = groupMembership & ~(DsGroupMask)((DsGroupMask)0xFF<<(8*aArrayIndex));
      groupMembership |= m;
    }
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
  announced(Never),
  announcing(Never),
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
  // TODO: %%% prelim, not needed any more for new API
  // create the registration request
  JsonObjectPtr req = JsonObject::newObj();
  // add the parameters
  req->add("dSidentifier", JsonObject::newString(dsid.getString()));
  req->add("VendorId", JsonObject::newInt32(1)); // TODO: %%% luz: must be 1=aizo, dsa cannot expand other ids so far
  req->add("FunctionId", JsonObject::newInt32(behaviourP->functionId()));
  req->add("ProductId", JsonObject::newInt32(behaviourP->productId()));
  req->add("Version", JsonObject::newInt32(behaviourP->version()));
  req->add("LTMode", JsonObject::newInt32(behaviourP->ltMode()));
  req->add("Mode", JsonObject::newInt32(behaviourP->outputMode()));
  // return it
  return req;
}


void Device::announcementAck(JsonObjectPtr aParams)
{
  // have behaviour look at this
  // TODO: %%% prelim, not needed any more for new API
  behaviourP->confirmRegistration(aParams);
  // registered now
  announced = MainLoop::now();
  announcing = Never;
}

//  if request['operation'] == 'DeviceRegistrationAck':
//      self.address = request['parameter']['BusAddress']
//      self.zone = request['parameter']['Zone']
//      self.groups = request['parameter']['GroupMemberships']
//      print 'BusAddress:', request['parameter']['BusAddress']
//      print 'Zone:', request['parameter']['Zone']
//      print 'Groups:', request['parameter']['GroupMemberships']

#define RETURN_ZERO_FOR_READ_ERRORS 1

ErrorPtr Device::handleMessage(string &aOperation, JsonObjectPtr aParams)
{
  ErrorPtr err;

  // check for generic device operations
  if (aOperation=="ping") {
    // ping hardware (if possible), creates pong now or after hardware was queried
    ping();
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
          err = ErrorPtr(new vdSMError(
            vdSMErrorInvalidParameter,
            string_format("unknown parameter bank %d / offset %d", bank, offset)
          ));
        }
        else {
          // found named param
          int arrayIndex = (offset-p->offset) / p->size;
          uint8_t inFieldOffset = offset-p->offset-(arrayIndex*p->size);
          uint32_t val;
          LOG(LOG_NOTICE,"- accessing %s[%d] byte#%d\n", p->name, arrayIndex, (int)inFieldOffset);
          err = getDeviceParam(p->name, arrayIndex, val);
          if (Error::isOK(err)) {
            LOG(LOG_NOTICE,"- current value : 0x%08X/%d\n", val, val);
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
                  LOG(LOG_NOTICE,"- changing byte#%d to 0x%02X/%d\n", inFieldOffset, newVal, newVal);
                  newVal = newVal << ((p->size-inFieldOffset-1)*8); // shift into position
                  val &= ~(0xFF << ((p->size-inFieldOffset-1)*8)); // create mask
                  val |= newVal;
                  // modify param
                  err = setDeviceParam(p->name, arrayIndex, val);
                  LOG(LOG_NOTICE,"- updated value : 0x%08X/%d\n", val, val);
                }
              }
              else {
                // extract the requested byte
                LOG(LOG_NOTICE,"- %s[%d] = %d/0x%X\n", p->name, arrayIndex, val, val);
                val = (val >> ((p->size-inFieldOffset-1)*8)) & 0xFF;
                LOG(LOG_NOTICE,"- extracting byte#%d = 0x%02X/%d\n", inFieldOffset, val, val);
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
    #if RETURN_ZERO_FOR_READ_ERRORS
    // never error, just return 0 for params we don't know
    if (!Error::isOK(err) && !write) {
      // Error reading param -> just return zero value instead of error
      LOG(LOG_ERR, "getdeviceparameter error: %s\n", err->description().c_str());
      // - re-use param
      aParams->add("Value", JsonObject::newInt32(0)); // zero
      sendMessage("DeviceParameter", aParams);
      err = ErrorPtr();
    }
    #endif
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
  else if (aParamName=="GRP") {
    #warning "// TODO: what is the structure of this group membership mask"
    aValue = 0; //%%% wrong
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
  aParams->add("dSidentifier", JsonObject::newString(dsid.getString()));
  // have device container send it
  return classContainerP->getDeviceContainer().sendMessage(aOperation, aParams);
}


ErrorPtr Device::save()
{
  if (behaviourP) {
    return behaviourP->save();
  }
  return ErrorPtr();
}


ErrorPtr Device::load()
{
  if (behaviourP) {
    behaviourP->load();
    // show loaded behaviour
    LOG(LOG_INFO, "Device %s: behaviour loaded:\n%s", shortDesc().c_str(), behaviourP->description().c_str());
  }
  return ErrorPtr();
}


ErrorPtr Device::forget()
{
  if (behaviourP)
    behaviourP->forget();
  return ErrorPtr();
}



#pragma mark - Device description/shortDesc

string Device::shortDesc()
{
  // short description is dsid
  return dsid.getString();
}



string Device::description()
{
  string s = string_format("Device %s", shortDesc().c_str());
  if (announced!=Never)
    string_format_append(s, " (Announced %lld)", announced);
  else
    s.append(" (not yet announced)");
  s.append("\n");
  if (behaviourP) {
    string_format_append(s, "- Button: %d/%d, DSBehaviour : %s\n", getButtonIndex()+1, getNumButtons(), behaviourP->shortDesc().c_str());
  }
  return s;
}
