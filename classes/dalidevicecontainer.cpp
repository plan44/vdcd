//
//  dalidevicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dalidevicecontainer.hpp"

#include "dalidevice.hpp"


/// device class name
const char *DaliDeviceContainer::deviceClassIdentifier() const
{
  return "DALI_Bus_Container";
}





class DaliDeviceCollector
{
  DaliComm *daliCommP;
  CompletedCB callback;
  DaliComm::DeviceListPtr deviceShortAddresses;
  DaliComm::DeviceList::iterator nextDev;
  DaliDeviceContainer *daliDeviceContainerP;
public:
  static void collectDevices(DaliDeviceContainer *aDaliDeviceContainerP, DaliComm *aDaliCommP, CompletedCB aCallback)
  {
    // create new instance, deletes itself when finished
    new DaliDeviceCollector(aDaliDeviceContainerP, aDaliCommP, aCallback);
  };
private:
  DaliDeviceCollector(DaliDeviceContainer *aDaliDeviceContainerP, DaliComm *aDaliCommP, CompletedCB aCallback) :
    daliCommP(aDaliCommP),
    callback(aCallback),
    daliDeviceContainerP(aDaliDeviceContainerP)
  {
    daliCommP->daliScanBus(boost::bind(&DaliDeviceCollector::deviceListReceived, this, _2, _3));
  }

  void deviceListReceived(DaliComm::DeviceListPtr aDeviceListPtr, ErrorPtr aError)
  {
    // save list of short addresses
    deviceShortAddresses = aDeviceListPtr;
    // check if any devices
    if (aError || deviceShortAddresses->size()==0) {
      // no devices to query, completed
      completed(aError);
      return;
    }
    // start collecting device info now
    nextDev = deviceShortAddresses->begin();
    queryNextDev(NULL);
  }

  void queryNextDev(ErrorPtr aError)
  {
    if (!aError && nextDev!=deviceShortAddresses->end())
      daliCommP->daliReadDeviceInfo(boost::bind(&DaliDeviceCollector::deviceInfoReceived, this, _2, _3), *nextDev);
    else
      completed(aError);
  }

  void deviceInfoReceived(DaliComm::DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)
  {
    if (!aError) {
      // got device info, add it to our collection
      // - create device
      DaliDevicePtr daliDevice(new DaliDevice(daliDeviceContainerP));
      // - give it device info (such that it can calculate it's dsid)
      daliDevice->setDeviceInfo(*aDaliDeviceInfoPtr);
      // - add it to our collection
      daliDeviceContainerP->addCollectedDevice(daliDevice);
    }
    else {
      #error might be old device w/o device info
    }
    // check next
    ++nextDev;
    queryNextDev(aError);
  }

  void completed(ErrorPtr aError)
  {
    // completed
    callback(aError);
    // done, delete myself
    delete this;
  }

};



/// collect devices from all device classes
/// @param aCompletedCB will be called when device scan for this device class has been completed
void DaliDeviceContainer::collectDevices(CompletedCB aCompletedCB)
{
  DaliDeviceCollector::collectDevices(this, &daliComm, aCompletedCB);
}

