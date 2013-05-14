//
//  dalidevicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dalidevicecontainer.hpp"

#include "dalidevice.hpp"

using namespace p44;


DaliDeviceContainer::DaliDeviceContainer(int aInstanceNumber) :
  DeviceClassContainer(aInstanceNumber),
	daliComm(SyncIOMainLoop::currentMainLoop())
{
}



// device class name
const char *DaliDeviceContainer::deviceClassIdentifier() const
{
  return "DALI_Bus_Container";
}



class DaliDeviceCollector
{
  DaliComm *daliCommP;
  CompletedCB callback;
  DaliComm::ShortAddressListPtr deviceShortAddresses;
  DaliComm::ShortAddressList::iterator nextDev;
  DaliDeviceContainer *daliDeviceContainerP;
public:
  static void collectDevices(DaliDeviceContainer *aDaliDeviceContainerP, DaliComm *aDaliCommP, CompletedCB aCallback, bool aForceFullScan)
  {
    // create new instance, deletes itself when finished
    new DaliDeviceCollector(aDaliDeviceContainerP, aDaliCommP, aCallback, aForceFullScan);
  };
private:
  DaliDeviceCollector(DaliDeviceContainer *aDaliDeviceContainerP, DaliComm *aDaliCommP, CompletedCB aCallback, bool aForceFullScan) :
    daliCommP(aDaliCommP),
    callback(aCallback),
    daliDeviceContainerP(aDaliDeviceContainerP)
  {
    daliCommP->daliFullBusScan(boost::bind(&DaliDeviceCollector::deviceListReceived, this, _2, _3), !aForceFullScan); // allow quick scan when not forced
  }

  void deviceListReceived(DaliComm::ShortAddressListPtr aDeviceListPtr, ErrorPtr aError)
  {
    // save list of short addresses
    deviceShortAddresses = aDeviceListPtr;
    // check if any devices
    if (aError || deviceShortAddresses->size()==0)
      return completed(aError); // no devices to query, completed
    // start collecting device info now
    nextDev = deviceShortAddresses->begin();
    queryNextDev(ErrorPtr());
  }

  void queryNextDev(ErrorPtr aError)
  {
    if (!aError && nextDev!=deviceShortAddresses->end())
      daliCommP->daliReadDeviceInfo(boost::bind(&DaliDeviceCollector::deviceInfoReceived, this, _2, _3), *nextDev);
    else
      return completed(aError);
  }

  void deviceInfoReceived(DaliComm::DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)
  {
    bool missingData = aError && aError->isError(DaliCommError::domain(), DaliCommErrorMissingData);
    if (!aError || missingData) {
      if (missingData) { DBGLOG(LOG_INFO,"Device at shortAddress %d does not have device info: %d\n",aDaliDeviceInfoPtr->shortAddress); }
      // - create device
      DaliDevicePtr daliDevice(new DaliDevice(daliDeviceContainerP));
      // - give it device info (such that it can calculate its dsid)
      //   Note: device info might be empty except for short address
      daliDevice->setDeviceInfo(*aDaliDeviceInfoPtr);
      // - add it to our collection
      daliDeviceContainerP->addDevice(daliDevice);
    }
    else {
      DBGLOG(LOG_INFO,"Error reading device info: %s\n",aError->description().c_str());
      return completed(aError);
    }
    // check next
    ++nextDev;
    queryNextDev(ErrorPtr());
  }

  void completed(ErrorPtr aError)
  {
    // completed
    callback(aError);
    // done, delete myself
    delete this;
  }

};



/// collect devices from this device class
/// @param aCompletedCB will be called when device scan for this device class has been completed
void DaliDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  forgetDevices();
  DaliDeviceCollector::collectDevices(this, &daliComm, aCompletedCB, aExhaustive);
}

