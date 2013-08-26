//
//  enoceandevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 07.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "enoceandevice.hpp"

#include "enoceandevicecontainer.hpp"

#include "buttonbehaviour.hpp"
#include "sensorbehaviour.hpp"
#include "outputbehaviour.hpp"

#include "enoceanrps.hpp"
//#include "enocean1bs.hpp"
#include "enocean4bs.hpp"
//#include "enoceanVld.hpp"


using namespace p44;


#pragma mark - EnoceanChannelHandler

EnoceanChannelHandler::EnoceanChannelHandler(EnoceanDevice &aDevice) :
  device(aDevice)
{
}


#pragma mark - Manufacturer names

typedef struct {
  EnoceanManufacturer manufacturerID;
  const char *name;
} EnoceanManufacturerDesc;


static const EnoceanManufacturerDesc manufacturerDescriptions[] = {
  { 0x001, "Peha" },
  { 0x002, "Thermokon" },
  { 0x003, "Servodan" },
  { 0x004, "EchoFlex Solutions" },
  { 0x005, "Omnio AG" },
  { 0x006, "Hardmeier electronics" },
  { 0x007, "Regulvar Inc" },
  { 0x008, "Ad Hoc Electronics" },
  { 0x009, "Distech Controls" },
  { 0x00A, "Kieback + Peter" },
  { 0x00B, "EnOcean GmbH" },
  { 0x00C, "Probare" },
  { 0x00D, "Eltako" },
  { 0x00E, "Leviton" },
  { 0x00F, "Honeywell" },
  { 0x010, "Spartan Peripheral Devices" },
  { 0x011, "Siemens" },
  { 0x012, "T-Mac" },
  { 0x013, "Reliable Controls Corporation" },
  { 0x014, "Elsner Elektronik GmbH" },
  { 0x015, "Diehl Controls" },
  { 0x016, "BSC Computer" },
  { 0x017, "S+S Regeltechnik GmbH" },
  { 0x018, "Masco Corporation" },
  { 0x019, "Intesis Software SL" },
  { 0x01A, "Res." },
  { 0x01B, "Lutuo Technology" },
  { 0x01C, "CAN2GO" },
  { 0x7FF, "Multi user Manufacturer ID" },
  { 0, NULL /* NULL string terminates list */ }
};


#pragma mark - EnoceanDevice

EnoceanDevice::EnoceanDevice(EnoceanDeviceContainer *aClassContainerP, EnoceanSubDevice aTotalSubdevices) :
  Device(aClassContainerP),
  eeProfile(eep_profile_unknown),
  eeManufacturer(manufacturer_unknown),
	totalSubdevices(aTotalSubdevices),
  subDevice(0)
{
}


EnoceanDeviceContainer &EnoceanDevice::getEnoceanDeviceContainer()
{
  return *(static_cast<EnoceanDeviceContainer *>(classContainerP));
}


EnoceanAddress EnoceanDevice::getAddress()
{
  return enoceanAddress;
}


EnoceanSubDevice EnoceanDevice::getSubDevice()
{
  return subDevice;
}


EnoceanSubDevice EnoceanDevice::getTotalSubDevices()
{
	return totalSubdevices;
}



void EnoceanDevice::setAddressingInfo(EnoceanAddress aAddress, EnoceanSubDevice aSubDevice)
{
  enoceanAddress = aAddress;
  subDevice = aSubDevice;
  deriveDSID();
}


void EnoceanDevice::setEEPInfo(EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer)
{
  eeProfile = aEEProfile;
  eeManufacturer = aEEManufacturer;
}


EnoceanProfile EnoceanDevice::getEEProfile()
{
  return eeProfile;
}


EnoceanManufacturer EnoceanDevice::getEEManufacturer()
{
  return eeManufacturer;
}



void EnoceanDevice::deriveDSID()
{
  #if FAKE_REAL_DSD_IDS
  dsid.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
  dsid.setSerialNo(
    ((uint64_t)getAddress()<<4) + // 32 upper bits, 4..35
    (getSubDevice()&0x0F) // 4 lower bits for up to 16 subdevices
  );
  #warning "TEST ONLY: faking digitalSTROM device addresses, possibly colliding with real devices"
  #else
  dsid.setObjectClass(DSID_OBJECTCLASS_MACADDRESS);
  // TODO: validate, now we are using the MAC-address class with:
  // - bits 48..51 set to 6
  // - bits 40..47 unused
  // - enOcean address encoded into bits 8..39
  // - subdevice encoded into bits 0..7 (max 255 subdevices)
	//   Note: this conforms to the dS convention which mandates that multi-input physical
	//   devices (up to 4) must have adjacent dsids.
  dsid.setSerialNo(
    0x6000000000000ll+
    ((uint64_t)getAddress()<<8) +
    (getSubDevice()&0xFF)
  );
  #endif
}


string EnoceanDevice::hardwareGUID()
{
  // GTIN is 24bit company prefix + 20bit item reference, SGTIN adds a 48bit serial number as third element: urn:epc:id:sgtin:COMPANYPREFIX.ITEMREF.SERIALNO
  // TODO: create a GTIN if there is an official scheme for it
  return string_format("enoceanaddress:%ld", getAddress());
}


string EnoceanDevice::modelName()
{
  return string_format("%s enOcean device", manufacturerName().c_str());
}



string EnoceanDevice::manufacturerName()
{
  const EnoceanManufacturerDesc *manP = manufacturerDescriptions;
  while (manP->name) {
    if (manP->manufacturerID==eeManufacturer) {
      return manP->name;
    }
    manP++;
  }
  // none found
  return "<unknown>";
}


void EnoceanDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  // clear learn-in data from DB
  getEnoceanDeviceContainer().db.executef("DELETE FROM knownDevices WHERE enoceanAddress=%d AND subdevice=%d", getAddress(), getSubDevice());
  // disconnection is immediate, so we can call inherited right now
  inherited::disconnect(aForgetParams, aDisconnectResultHandler);
}



void EnoceanDevice::addChannelHandler(EnoceanChannelHandlerPtr aChannelHandler)
{
  // create channel number
  aChannelHandler->channel = channels.size();
  // add to my local list
  channels.push_back(aChannelHandler);
  // register behaviour of the channel with the device
  addBehaviour(aChannelHandler->behaviour);
}




EnoceanChannelHandlerPtr EnoceanDevice::channelForBehaviour(const DsBehaviour *aBehaviourP)
{
  EnoceanChannelHandlerPtr handler;
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    if ((*pos)->behaviour.get()==static_cast<const DsBehaviour *>(aBehaviourP)) {
      handler = *pos;
      break;
    }
  }
  return handler;
}



void EnoceanDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  // collect data from all channels to compose an outgoing message
  Esp3PacketPtr outgoingEsp3Packet;
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    (*pos)->collectOutgoingMessageData(outgoingEsp3Packet);
  }
  if (outgoingEsp3Packet) {
    // set destination
    outgoingEsp3Packet->setRadioDestination(enoceanAddress); // the target is the device I manage
    // send it
    getEnoceanDeviceContainer().enoceanComm.sendPacket(outgoingEsp3Packet);
  }
  inherited::updateOutputValue(aOutputBehaviour);
}



void EnoceanDevice::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  // pass to every channel
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    (*pos)->handleRadioPacket(aEsp3PacketPtr);
  }
}


string EnoceanDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- Enocean Address = 0x%08lX, subDevice=%d\n", enoceanAddress, subDevice);
  string_format_append(s,
    "- EEP RORG/FUNC/TYPE: %02X %02X %02X, Manufacturer = %s (%03X)\n",
    (eeProfile>>16) & 0xFF,
    (eeProfile>>8) & 0xFF,
    eeProfile & 0xFF,
    manufacturerName().c_str(),
    eeManufacturer
  );
  // show channels
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    string_format_append(s, "- channel #%d: %s\n", (*pos)->channel, (*pos)->shortDesc().c_str());
  }
  return s;
}


#pragma mark - device factory


EnoceanDevicePtr EnoceanDevice::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress, EnoceanSubDevice aSubDevice,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  EnoceanSubDevice *aNumSubdevicesP
) {
  EnoceanDevicePtr newDev;
  RadioOrg rorg = EEP_RORG(aEEProfile);
  // dispatch to factory according to RORG
  switch (rorg) {
    case rorg_RPS:
      newDev = EnoceanRpsHandler::newDevice(aClassContainerP, aAddress, aSubDevice, aEEProfile, aEEManufacturer, aNumSubdevicesP);
      break;
//    case rorg_1BS:
//      newDev = Enocean1bsHandler::newDevice(aClassContainerP, aAddress, aSubDevice, aEEProfile, aEEManufacturer, aNumSubdevicesP);
//      break;
    case rorg_4BS:
      newDev = Enocean4bsHandler::newDevice(aClassContainerP, aAddress, aSubDevice, aEEProfile, aEEManufacturer, aNumSubdevicesP);
      break;
//    case rorg_VLD:
//      newDev = EnoceanVldHandler::newDevice(aClassContainerP, aAddress, aSubDevice, aEEProfile, aEEManufacturer, aNumSubdevicesP);
//      break;
    default:
      LOG(LOG_WARNING,"EnoceanDevice::newDevice: unknown RORG = 0x%02X\n", rorg);
      break;
  }
  // return device (or empty if none created)
  return newDev;
}


int EnoceanDevice::createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, Esp3PacketPtr aLearnInPacket)
{
  EnoceanSubDevice totalSubDevices = 1; // at least one
  EnoceanSubDevice subDevice = 0;
  while (subDevice<totalSubDevices) {
    EnoceanDevicePtr newDev = newDevice(
      aClassContainerP,
      aLearnInPacket->radioSender(), subDevice,
      aLearnInPacket->eepProfile(), aLearnInPacket->eepManufacturer(),
      &totalSubDevices // possibly update total
    );
    if (!newDev) {
      // could not create a device
      break;
    }
    // created device
    // - add it to the container
    aClassContainerP->addAndRemeberDevice(newDev);
    // - count it
    subDevice++;
  }
  // return number of devices created
  return subDevice;
}

