//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#include "enoceandevice.hpp"

#include "enoceandevicecontainer.hpp"

#include "buttonbehaviour.hpp"
#include "sensorbehaviour.hpp"
#include "outputbehaviour.hpp"

#include "enoceanrps.hpp"
#include "enocean1bs.hpp"
#include "enocean4bs.hpp"
//#include "enoceanVld.hpp"
#include "enoceanremotecontrol.hpp"


using namespace p44;


#pragma mark - EnoceanChannelHandler

EnoceanChannelHandler::EnoceanChannelHandler(EnoceanDevice &aDevice) :
  device(aDevice),
  dsChannelIndex(0)
{
}


#pragma mark - EnoceanDevice

EnoceanDevice::EnoceanDevice(EnoceanDeviceContainer *aClassContainerP) :
  Device(aClassContainerP),
  eeProfile(eep_profile_unknown),
  eeManufacturer(manufacturer_unknown),
  alwaysUpdateable(false),
  pendingDeviceUpdate(false),
  updateAtEveryReceive(false),
  subDevice(0)
{
  eeFunctionDesc = "device"; // generic description is "device"
  iconBaseName = "enocean";
  groupColoredIcon = true;
  lastPacketTime = MainLoop::now(); // consider packet received at time of creation (to avoid devices starting inactive)
  lastRSSI = -999; // not valid
  lastRepeaterCount = 0; // dummy
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


void EnoceanDevice::setAddressingInfo(EnoceanAddress aAddress, EnoceanSubDevice aSubDevice)
{
  enoceanAddress = aAddress;
  subDevice = aSubDevice;
  deriveDsUid();
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


void EnoceanDevice::deriveDsUid()
{
  // UUID in EnOcean name space
  //   name = xxxxxxxx:s (x=8 digit enocean hex UPPERCASE address, s=decimal subdevice index, 0..n)
  DsUid enOceanNamespace(DSUID_ENOCEAN_NAMESPACE_UUID);
  string s = string_format("%08lX", getAddress()); // base address comes from
  dSUID.setNameInSpace(s, enOceanNamespace);
  dSUID.setSubdeviceIndex(getSubDevice()*dsUIDIndexStep()); // historically space subdevices in double steps, to (theoretically) allow vdsm to split them (rocker switches) further. Kept this way to prevent existing dSUIDs to change
}


string EnoceanDevice::hardwareGUID()
{
  return string_format("enoceanaddress:%08lX", getAddress());
}


string EnoceanDevice::hardwareModelGUID()
{
  return string_format("enoceaneep:%06lX", EEP_PURE(getEEProfile()));
}


string EnoceanDevice::modelName()
{
  const char *mn = EnoceanComm::manufacturerName(eeManufacturer);
  return string_format("%s%sEnOcean %s (%02X-%02X-%02X)", mn ? mn : "", mn ? " " : "", eeFunctionDesc.c_str(), EEP_RORG(eeProfile), EEP_FUNC(eeProfile), EEP_TYPE(eeProfile));
}


string EnoceanDevice::vendorId()
{
  const char *mn = EnoceanComm::manufacturerName(eeManufacturer);
  return string_format("enoceanvendor:%03X%s%s", eeManufacturer, mn ? ":" : "", mn ? mn : "");
}


bool EnoceanDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  bool iconFound = false;
  if (iconBaseName) {
    if (groupColoredIcon)
      iconFound = getGroupColoredIcon(iconBaseName, getDominantGroup(), aIcon, aWithData, aResolutionPrefix);
    else
      iconFound = getIcon(iconBaseName, aIcon, aWithData, aResolutionPrefix);
  }
  if (iconFound)
    return true;
  // failed
  return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
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



void EnoceanDevice::needOutgoingUpdate(SimpleCB aDoneCB)
{
  // anyway, we need an update
  pendingDeviceUpdate = true;
  // send it right away when possible (line powered devices only)
  if (alwaysUpdateable) {
    sendOutgoingUpdate(aDoneCB);
  }
  else {
    LOG(LOG_NOTICE,"EnOcean device %s: flagged output updated pending -> outgoing package will be sent later\n", shortDesc().c_str());
    if (aDoneCB) aDoneCB();
  }
}


void EnoceanDevice::sendOutgoingUpdate(SimpleCB aDoneCB)
{
  if (pendingDeviceUpdate) {
    // clear flag now, so handlers can trigger yet another update in collectOutgoingMessageData() if needed (e.g. heating valve service sequence)
    pendingDeviceUpdate = false; // done
    // collect data from all channels to compose an outgoing message
    Esp3PacketPtr outgoingEsp3Packet;
    EnoceanChannelHandlerVector::iterator pos = channels.begin();
    nextChannelUpdate(pos, aDoneCB, outgoingEsp3Packet);
  }
  else {
    if (aDoneCB) aDoneCB();
  }
}


void EnoceanDevice::nextChannelUpdate(EnoceanChannelHandlerVector::iterator aPos, SimpleCB aDoneCB, Esp3PacketPtr aOutgoingEsp3Packet)
{
  if (aPos!=channels.end()) {
    // - collect data for a single outgoing packet carrying data for more than one channel
    (*aPos)->collectOutgoingMessageData(aOutgoingEsp3Packet);
    // - and/or issue actions directly to the hardware (separate outgoing messages or sequences of messages
    (*aPos)->issueDirectChannelActions(boost::bind(&EnoceanDevice::nextChannelUpdate, this, ++aPos, aDoneCB, aOutgoingEsp3Packet));
  }
  else {
    if (aOutgoingEsp3Packet) {
      // set destination
      aOutgoingEsp3Packet->setRadioDestination(enoceanAddress); // the target is the device I manage
      aOutgoingEsp3Packet->finalize();
      LOG(LOG_INFO, "EnOcean device %s: sending outgoing packet:\n%s", shortDesc().c_str(), aOutgoingEsp3Packet->description().c_str());
      // send it
      getEnoceanDeviceContainer().enoceanComm.sendPacket(aOutgoingEsp3Packet);
    }
    // done now
    if (aDoneCB) aDoneCB();
  }
}


void EnoceanDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  // trigger updating all device outputs
  for (int i=0; i<numChannels(); i++) {
    if (getChannelByIndex(i, true)) {
      // channel needs update
      pendingDeviceUpdate = true;
      break; // no more checking needed, need device level update anyway
    }
  }
  if (pendingDeviceUpdate) {
    // we need to apply data
    needOutgoingUpdate(boost::bind(&EnoceanDevice::channelValuesApplied, this, aDoneCB, aForDimming));
  }
  else {
    channelValuesApplied(aDoneCB, aForDimming);
  }
}


void EnoceanDevice::channelValuesApplied(SimpleCB aDoneCB, bool aForDimming)
{
  inherited::applyChannelValues(aDoneCB, aForDimming);
}



void EnoceanDevice::handleRadioPacket(Esp3PacketPtr aEsp3PacketPtr)
{
  LOG(LOG_INFO, "EnOcean device %s: now starts processing packet:\n%s", shortDesc().c_str(), aEsp3PacketPtr->description().c_str());
  lastPacketTime = MainLoop::now();
  lastRSSI = aEsp3PacketPtr->radioDBm();
  lastRepeaterCount = aEsp3PacketPtr->radioRepeaterCount();
  // pass to every channel
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    (*pos)->handleRadioPacket(aEsp3PacketPtr);
  }
  // if device cannot be updated whenever output value change is requested, send updates after receiving a message
  if (pendingDeviceUpdate || updateAtEveryReceive) {
    // send updates, if any
    pendingDeviceUpdate = true; // set it in case of updateAtEveryReceive (so message goes out even if no changes pending)
    LOG(LOG_NOTICE,"EnOcean device %s: pending output update is now sent to device\n", shortDesc().c_str());
    sendOutgoingUpdate(NULL);
  }
}


void EnoceanDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  bool present = true;
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    if (!(*pos)->isAlive()) {
      present = false; // one channel not alive -> device not present
      break;
    }
  }
  aPresenceResultHandler(present);
}



string EnoceanDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- Enocean Address = 0x%08lX, subDevice=%d\n", enoceanAddress, subDevice);
  const char *mn = EnoceanComm::manufacturerName(eeManufacturer);
  string_format_append(s,
    "- %s, EEP RORG/FUNC/TYPE: %02X %02X %02X, Manufacturer: %s (%03X), Profile variant: %02X\n",
    eeFunctionDesc.c_str(),
    EEP_RORG(eeProfile),
    EEP_FUNC(eeProfile),
    EEP_TYPE(eeProfile),
    mn ? mn : "<unknown>",
    eeManufacturer,
    EEP_VARIANT(eeProfile)
  );
  // show channels
  for (EnoceanChannelHandlerVector::iterator pos = channels.begin(); pos!=channels.end(); ++pos) {
    string_format_append(s, "- EnOcean device channel #%d: %s\n", (*pos)->channel, (*pos)->shortDesc().c_str());
  }
  return s;
}


#pragma mark - profile variants



bool EnoceanDevice::getProfileVariants(ApiValuePtr aApiObjectValue)
{
  // check if current profile is one of the interchangeable ones
  const profileVariantEntry *currentVariant = profileVariantsTable();
  while (currentVariant && currentVariant->profileGroup!=0) {
    // look for current EEP in the list of variants
    if (getEEProfile()==currentVariant->eep) {
      // create string from all other variants (same profileGroup), if any
      bool anyVariants = false;
      const profileVariantEntry *variant = profileVariantsTable();
      while (variant->profileGroup!=0) {
        if (variant->profileGroup==currentVariant->profileGroup) {
          if (variant->eep!=getEEProfile()) anyVariants = true; // another variant than just myself
          aApiObjectValue->add(string_format("%d",variant->eep), aApiObjectValue->newString(variant->description));
        }
        variant++;
      }
      // there are variants
      return anyVariants;
    }
    currentVariant++;
  }
  return false; // no variants
}


bool EnoceanDevice::setProfileVariant(EnoceanProfile aProfile)
{
  // verify if changeable profile code requested
  // - check for already having that profile
  if (aProfile==getEEProfile()) return true; // we already have that profile -> NOP
  // - find my profileGroup
  const profileVariantEntry *currentVariant = profileVariantsTable();
  while (currentVariant && currentVariant->profileGroup!=0) {
    if (getEEProfile()==currentVariant->eep) {
      // this is my profile group, now check if requested profile is in my profile group as well
      const profileVariantEntry *variant = profileVariantsTable();
      while (variant && variant->profileGroup!=0) {
        if (variant->profileGroup==currentVariant->profileGroup && variant->eep==aProfile) {
          // requested profile is in my group, change now
          switchToProfile(aProfile); // will delete this device, so return immediately afterwards
          return true; // changed profile
        }
        variant++;
      }
    }
    currentVariant++;
  }
  return false; // invalid profile
}


void EnoceanDevice::switchToProfile(EnoceanProfile aProfile)
{
  // make sure object is retained locally
  EnoceanDevicePtr keepMeAlive(this); // make sure this object lives until routine terminates
  // have devices related to current profile deleted, including settings
  // Note: this removes myself from container, and deletes the config (which is valid for the previous profile, i.e. a different type of device)
  getEnoceanDeviceContainer().unpairDevicesByAddress(getAddress(), true);
  // - create new ones, with same address and manufacturer, but new profile
  EnoceanDevice::createDevicesFromEEP(&getEnoceanDeviceContainer(), getAddress(), aProfile, getEEManufacturer());
}



#pragma mark - property access


enum {
  profileVariants_key,
  profile_key,
  packetage_key,
  rssi_key,
  repeaterCount_key,
  numProperties
};

static char enoceanDevice_key;


int EnoceanDevice::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  // Note: only add my own count when accessing root level properties!!
  if (!aParentDescriptor) {
    // Accessing properties at the Device (root) level, add mine
    return inherited::numProps(aDomain, aParentDescriptor)+numProperties;
  }
  // just return base class' count
  return inherited::numProps(aDomain, aParentDescriptor);
}


PropertyDescriptorPtr EnoceanDevice::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numProperties] = {
    { "x-p44-profileVariants", apivalue_null, profileVariants_key, OKEY(enoceanDevice_key) },
    { "x-p44-profile", apivalue_int64, profile_key, OKEY(enoceanDevice_key) },
    { "x-p44-packetAge", apivalue_double, packetage_key, OKEY(enoceanDevice_key) },
    { "x-p44-rssi", apivalue_int64, rssi_key, OKEY(enoceanDevice_key) },
    { "x-p44-repeaterCount", apivalue_int64, repeaterCount_key, OKEY(enoceanDevice_key) },
  };
  if (!aParentDescriptor) {
    // root level - accessing properties on the Device level
    int n = inherited::numProps(aDomain, aParentDescriptor);
    if (aPropIndex<n)
      return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
    aPropIndex -= n; // rebase to 0 for my own first property
    return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
  }
  else {
    // other level
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  }
}


// access to all fields
bool EnoceanDevice::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(enoceanDevice_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case profileVariants_key:
          aPropValue->setType(apivalue_object); // make object (incoming object is NULL)
          return getProfileVariants(aPropValue);
        case profile_key:
          aPropValue->setInt32Value(getEEProfile()); return true;
        case packetage_key:
          // Note lastPacketTime is set to now at startup, so additionally check lastRSSI
          if (lastPacketTime==Never || lastRSSI<=-999)
            aPropValue->setNull();
          else
            aPropValue->setDoubleValue((double)(MainLoop::now()-lastPacketTime)/Second);
          return true;
        case rssi_key:
          if (lastRSSI<=-999)
            aPropValue->setNull();
          else
            aPropValue->setInt32Value(lastRSSI);
          return true;
        case repeaterCount_key:
          if (lastRSSI<=-999)
            aPropValue->setNull();
          else
            aPropValue->setUint8Value(lastRepeaterCount);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case profile_key:
          setProfileVariant(aPropValue->int32Value()); return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - device factory


EnoceanDevicePtr EnoceanDevice::newDevice(
  EnoceanDeviceContainer *aClassContainerP,
  EnoceanAddress aAddress,
  EnoceanSubDevice aSubDeviceIndex,
  EnoceanProfile aEEProfile, EnoceanManufacturer aEEManufacturer,
  bool aSendTeachInResponse
) {
  EnoceanDevicePtr newDev;
  RadioOrg rorg = EEP_RORG(aEEProfile);
  // dispatch to factory according to RORG
  switch ((int)rorg) {
    case rorg_RPS:
      newDev = EnoceanRpsHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
      break;
    case rorg_1BS:
      newDev = Enocean1bsHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
      break;
    case rorg_4BS:
      newDev = Enocean4bsHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
      break;
//    case rorg_VLD:
//      newDev = EnoceanVldHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
//      break;
    // pseudo RORGs (internal encoding of non-standard devices)
    case PSEUDO_RORG_REMOTECONTROL:
      newDev = EnoceanRemoteControlHandler::newDevice(aClassContainerP, aAddress, aSubDeviceIndex, aEEProfile, aEEManufacturer, aSendTeachInResponse);
    default:
      LOG(LOG_WARNING,"EnoceanDevice::newDevice: unknown RORG = 0x%02X\n", rorg);
      break;
  }
  // return device (or empty if none created)
  return newDev;
}


int EnoceanDevice::createDevicesFromEEP(EnoceanDeviceContainer *aClassContainerP, EnoceanAddress aAddress, EnoceanProfile aProfile, EnoceanManufacturer aManufacturer)
{
  EnoceanSubDevice subDeviceIndex = 0; // start at
  while (true) {
    // create devices until done
    EnoceanDevicePtr newDev = newDevice(
      aClassContainerP,
      aAddress,
      subDeviceIndex, // index to create a device for
      aProfile, aManufacturer,
      subDeviceIndex==0 // allow sending teach-in response for first subdevice only
    );
    if (!newDev) {
      // could not create a device for subDeviceIndex
      break; // -> done
    }
    // created device
    // - add it to the container
    aClassContainerP->addAndRemeberDevice(newDev);
    // - count it
    subDeviceIndex++;
  }
  // return number of devices created
  return subDeviceIndex;
}

