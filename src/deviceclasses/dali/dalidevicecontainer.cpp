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

#include "dalidevicecontainer.hpp"

#include "dalidevice.hpp"

using namespace p44;


DaliDeviceContainer::DaliDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag) :
  DeviceClassContainer(aInstanceNumber, aDeviceContainerP, aTag)
{
  daliComm = DaliCommPtr(new 	DaliComm(SyncIOMainLoop::currentMainLoop()));
}



// device class name
const char *DaliDeviceContainer::deviceClassIdentifier() const
{
  return "DALI_Bus_Container";
}


class DaliDeviceCollector
{
  DaliCommPtr daliComm;
  CompletedCB callback;
  DaliComm::ShortAddressListPtr deviceShortAddresses;
  DaliComm::ShortAddressList::iterator nextDev;
  DaliDeviceContainer *daliDeviceContainerP;
  bool incremental;
public:
  static void collectDevices(DaliDeviceContainer *aDaliDeviceContainerP, DaliCommPtr aDaliComm, CompletedCB aCallback, bool aIncremental, bool aForceFullScan)
  {
    // create new instance, deletes itself when finished
    new DaliDeviceCollector(aDaliDeviceContainerP, aDaliComm, aCallback, aIncremental, aForceFullScan);
  };
private:
  DaliDeviceCollector(DaliDeviceContainer *aDaliDeviceContainerP, DaliCommPtr aDaliComm, CompletedCB aCallback, bool aIncremental, bool aForceFullScan) :
    daliComm(aDaliComm),
    callback(aCallback),
    incremental(aIncremental),
    daliDeviceContainerP(aDaliDeviceContainerP)
  {
    daliComm->daliFullBusScan(boost::bind(&DaliDeviceCollector::deviceListReceived, this, _1, _2), !aForceFullScan); // allow quick scan when not forced
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
      daliComm->daliReadDeviceInfo(boost::bind(&DaliDeviceCollector::deviceInfoReceived, this, _1, _2), *nextDev);
    else
      return completed(aError);
  }

  void deviceInfoReceived(DaliComm::DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)
  {
    bool missingData = aError && aError->isError(DaliCommError::domain(), DaliCommErrorMissingData);
    bool badData = aError && aError->isError(DaliCommError::domain(), DaliCommErrorBadChecksum);
    if (!aError || missingData || badData) {
      if (missingData) { LOG(LOG_INFO,"Device at shortAddress %d does not have device info\n",aDaliDeviceInfoPtr->shortAddress); }
      if (badData) { LOG(LOG_INFO,"Device at shortAddress %d does not have invalid device info (checksum error)\n",aDaliDeviceInfoPtr->shortAddress); }
      // - create device
      DaliDevicePtr daliDevice(new DaliDevice(daliDeviceContainerP));
      // - give it device info (such that it can calculate its dSUID)
      //   Note: device info might be empty except for short address
      daliDevice->setDeviceInfo(*aDaliDeviceInfoPtr);
      // - make it 
      // - add it to our collection (if not already there)
      daliDeviceContainerP->addDevice(daliDevice);
    }
    else {
      LOG(LOG_ERR,"Error reading device info: %s\n",aError->description().c_str());
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
void DaliDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive)
{
  if (!aIncremental) {
    removeDevices(false);
  }
  DaliDeviceCollector::collectDevices(this, daliComm, aCompletedCB, aIncremental, aExhaustive);
}


#pragma mark - Self test

void DaliDeviceContainer::selfTest(CompletedCB aCompletedCB)
{
  // do bus short address scan
  daliComm->daliBusScan(boost::bind(&DaliDeviceContainer::testScanDone, this, aCompletedCB, _1, _2));
}


void DaliDeviceContainer::testScanDone(CompletedCB aCompletedCB, DaliComm::ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError)
{
  if (Error::isOK(aError) && aShortAddressListPtr && aShortAddressListPtr->size()>0) {
    // found at least one device, do a R/W test using the DTR
    DaliAddress testAddr = aShortAddressListPtr->front();
    LOG(LOG_NOTICE,"- DALI self test: switch all lights on, then do R/W tests with DTR of device short address %d\n",testAddr);
    daliComm->daliSendDirectPower(DaliBroadcast, 0, NULL); // off
    daliComm->daliSendDirectPower(DaliBroadcast, 254, NULL, 2*Second); // max
    testRW(aCompletedCB, testAddr, 0x55); // use first found device
  }
  else {
    // return error
    if (Error::isOK(aError)) aError = ErrorPtr(new DaliCommError(DaliCommErrorDeviceSearch)); // no devices is also an error
    aCompletedCB(aError);
  }
}


void DaliDeviceContainer::testRW(CompletedCB aCompletedCB, DaliAddress aShortAddr, uint8_t aTestByte)
{
  // set DTR
  daliComm->daliSend(DALICMD_SET_DTR, aTestByte);
  // query DTR again, with 200mS delay
  daliComm->daliSendQuery(aShortAddr, DALICMD_QUERY_CONTENT_DTR, boost::bind(&DaliDeviceContainer::testRWResponse, this, aCompletedCB, aShortAddr, aTestByte, _1, _2, _3), 200*MilliSecond);
}


void DaliDeviceContainer::testRWResponse(CompletedCB aCompletedCB, DaliAddress aShortAddr, uint8_t aTestByte, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && !aNoOrTimeout && aResponse==aTestByte) {
    LOG(LOG_NOTICE,"  - sent 0x%02X, received 0x%02X\n",aTestByte, aResponse, aNoOrTimeout);
    // successfully read back same value from DTR as sent before
    // - check if more tests
    switch (aTestByte) {
      case 0x55: aTestByte = 0xAA; break; // next test: inverse
      case 0xAA: aTestByte = 0x00; break; // next test: all 0
      case 0x00: aTestByte = 0xFF; break; // next test: all 1
      case 0xFF: aTestByte = 0xF0; break; // next test: half / half
      case 0xF0: aTestByte = 0x0F; break; // next test: half / half inverse
      default:
        // all tests done
        aCompletedCB(aError);
        // turn off lights
        daliComm->daliSendDirectPower(DaliBroadcast, 0); // off
        return;
    }
    // launch next test
    testRW(aCompletedCB, aShortAddr, aTestByte);
  }
  else {
    // not ok
    if (Error::isOK(aError) && aNoOrTimeout) aError = ErrorPtr(new DaliCommError(DaliCommErrorMissingData));
    // report
    LOG(LOG_ERR,"DALI self test error: sent 0x%02X, error: %s\n",aTestByte, aError->description().c_str());
    aCompletedCB(aError);
  }
}
