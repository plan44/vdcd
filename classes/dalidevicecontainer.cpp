//
//  dalidevicecontainer.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 17.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dalidevicecontainer.hpp"


/// device class name
const char *deviceClassIdentifier()
{
  return "DALI_Bus_Container";
}





class DaliDeviceCollector
{
  DaliComm *daliCommP;
  CompletedCB callback;
  DaliComm::DeviceListPtr deviceShortAddresses;
public:
  static void collectDevices(DaliComm *aDaliCommP, CompletedCB aCallback)
  {
    // create new instance, deletes itself when finished
    new DaliDeviceCollector(aDaliCommP, aCallback);
  };
private:
  DaliDeviceCollector(DaliComm *aDaliCommP, CompletedCB aCallback) :
    daliCommP(aDaliCommP),
    callback(aCallback)
  {
    daliCommP->daliScanBus(boost::bind(&DaliDeviceCollector::deviceListReceived, _2, _3));
  }

  void deviceListReceived(DaliComm::DeviceListPtr aDeviceListPtr, ErrorPtr aError)
  {
    // save list of short addresses
    deviceShortAddresses = aDeviceListPtr;
    // start collecting device info now
    
  }

};



/// collect devices from all device classes
/// @param aCompletedCB will be called when device scan for this device class has been completed
void DaliDeviceContainer::collectDevices(CompletedCB aCompletedCB)
{
  
}
