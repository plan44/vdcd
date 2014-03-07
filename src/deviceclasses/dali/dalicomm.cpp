/*
 * dalicomm.cpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include "dalicomm.hpp"

using namespace p44;


// pseudo baudrate for dali bridge must be 9600bd
#define DALIBRIDGE_BAUDRATE 9600


DaliComm::DaliComm(SyncIOMainLoop &aMainLoop) :
	inherited(aMainLoop),
  runningProcedures(0),
  closeAfterIdleTime(Never),
  connectionTimeoutTicket(0)
{
}


DaliComm::~DaliComm()
{
}


#pragma mark - procedure management

void DaliComm::startProcedure()
{
  ++runningProcedures;
}

void DaliComm::endProcedure()
{
  if (runningProcedures>0)
    --runningProcedures;
}


bool DaliComm::isBusy()
{
  return runningProcedures>0;
}




#pragma mark - DALI bridge low level communication

// DALI Bridge commands and responses
// ==================================
//
// bridge commands
// - one byte commands 0..7
#define CMD_CODE_RESET 0  // reset

// - three byte commands 8..n
#define CMD_CODE_SEND16 0x10 // send 16-bit DALI sequence, return RESP_ACK when done
#define CMD_CODE_2SEND16 0x11 // double send 16-bit DALI sequence with 10mS gap in between, return RESP_ACK when done
#define CMD_CODE_SEND16_REC8 0x12 // send 16-bit command, receive one 8-bit DALI response, return RESP_DATA when data received, RESP_ACK with error code
// - 3 byte debug commands
#define CMD_CODE_ECHO_DATA1 0x41 // returns RESP_DATA echoing DATA1
#define CMD_CODE_ECHO_DATA2 0x42 // returns RESP_DATA echoing DATA2

// bridge responses
#define RESP_CODE_ACK 0x2A // reponse for all commands that do not return data, second byte is status
#define RESP_CODE_DATA 0x3D // response for commands that return data, second byte is data
// - ACK status codes
#define ACK_OK 0x30 // ok status
#define ACK_TIMEOUT 0x31 // timeout receiving from DALI
#define ACK_FRAME_ERR 0x32 // rx frame error
#define ACK_INVALIDCMD 0x39 // invalid command



void DaliComm::setConnectionSpecification(const char *aConnectionSpec, uint16_t aDefaultPort, MLMicroSeconds aCloseAfterIdleTime)
{
  serialComm.setConnectionSpecification(aConnectionSpec, aDefaultPort, DALIBRIDGE_BAUDRATE);
}


#warning "// TODO: add dali bridge bus short circuit testing"

class BridgeResponseHandler
{
  DaliComm::DaliBridgeResultCB callback;
  DaliComm &daliComm;
public:
  BridgeResponseHandler(DaliComm &aDaliComm, DaliComm::DaliBridgeResultCB aResultCB) :
    daliComm(aDaliComm),
    callback(aResultCB)
  {};
  void operator() (Operation &aOperation, OperationQueue *aQueueP, ErrorPtr aError) {
    SerialOperationReceive *ropP = dynamic_cast<SerialOperationReceive *>(&aOperation);
    if (ropP) {
      // get received data
      if (!aError && ropP->getDataSize()>=2) {
        uint8_t resp1 = ropP->getDataP()[0];
        uint8_t resp2 = ropP->getDataP()[1];
        if (callback)
          callback(daliComm, resp1, resp2, aError);
      }
      else {
        // error
        if (!aError)
          aError = ErrorPtr(new DaliCommError(DaliCommErrorMissingData));
        if (callback)
          callback(daliComm, 0, 0, aError);
      }
    }
  };
};


void DaliComm::sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB, int aWithDelay)
{
  // reset connection closing timeout
  MainLoop::currentMainLoop().cancelExecutionTicket(connectionTimeoutTicket);
  if (closeAfterIdleTime!=Never) {
    MainLoop::currentMainLoop().executeOnce(boost::bind(&DaliComm::connectionTimeout, this), closeAfterIdleTime);
  }
  // deliver unhandled error
  SerialOperation *opP = NULL;
  if (aCmd<8) {
    // single byte command
    opP = new SerialOperationSendAndReceive(1, &aCmd, 2, BridgeResponseHandler(*this, aResultCB));
  }
  else {
    // 3 byte command
    uint8_t cmd3[3];
    cmd3[0] = aCmd;
    cmd3[1] = aDali1;
    cmd3[2] = aDali2;
    opP = new SerialOperationSendAndReceive(3, cmd3, 2, BridgeResponseHandler(*this, aResultCB));
  }
  if (opP) {
    SerialOperationPtr op(opP);
    if (aWithDelay>0)
      op->setInitiationDelay(aWithDelay);
    queueSerialOperation(op);
  }
  // process operations
  processOperations();
}


void DaliComm::connectionTimeout()
{
  serialComm.closeConnection();
}

#pragma mark - DALI bus communication basics


class DaliCommandStatusHandler
{
  DaliComm::DaliCommandStatusCB callback;
protected:
  DaliComm &daliComm;

public:
  static ErrorPtr checkBridgeResponse(uint8_t aResp1, uint8_t aResp2, ErrorPtr aError, bool &aNoOrTimeout)
  {
    aNoOrTimeout = false;
    if (aError) {
      return aError;
    }
    switch(aResp1) {
      case RESP_CODE_ACK:
        // command acknowledged
        switch (aResp2) {
          case ACK_TIMEOUT:
            aNoOrTimeout = true;  // only DALI timeout, which is no real error
            // otherwise like OK
          case ACK_OK:
            return ErrorPtr(); // no error
          case ACK_FRAME_ERR:
            return ErrorPtr(new DaliCommError(DaliCommErrorDALIFrame));
          case ACK_INVALIDCMD:
            return ErrorPtr(new DaliCommError(DaliCommErrorBridgeCmd));
        }
        break;
      case RESP_CODE_DATA:
        return ErrorPtr(); // no error
    }
    // other, uncatched error
    return ErrorPtr(new DaliCommError(DaliCommErrorBridgeUnknown));
  }

  DaliCommandStatusHandler(DaliComm &aDaliComm, DaliComm::DaliCommandStatusCB aResultCB) :
    daliComm(aDaliComm),
    callback(aResultCB)
  { };

  void operator() (DaliComm &aDaliComm, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)
  {
    bool noOrTimeout;
    aError = checkBridgeResponse(aResp1, aResp2, aError, noOrTimeout);
    if (!aError && noOrTimeout) {
      // timeout for a send-only command -> out of sync, bridge communication error
      aError = ErrorPtr(new DaliCommError(DaliCommErrorBridgeComm));
    }
    // execute callback if any
    if (callback)
      callback(daliComm, aError);
  }
};


class DaliQueryResponseHandler : DaliCommandStatusHandler
{
  DaliComm::DaliQueryResultCB callback;
public:
  DaliQueryResponseHandler(DaliComm &aDaliComm, DaliComm::DaliQueryResultCB aResultCB) :
    DaliCommandStatusHandler(aDaliComm, NULL),
    callback(aResultCB)
  { };

  void operator() (DaliComm &aDaliComm, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)
  {
    bool noOrTimeout;
    aError = checkBridgeResponse(aResp1, aResp2, aError, noOrTimeout);
    // execute callback if any
    if (callback)
      callback(daliComm, noOrTimeout, aResp2, aError);
  };
};



// reset the bridge

void DaliComm::reset(DaliCommandStatusCB aStatusCB)
{
  sendBridgeCommand(CMD_CODE_RESET, 0, 0, NULL);
  sendBridgeCommand(CMD_CODE_RESET, 0, 0, NULL);
  sendBridgeCommand(CMD_CODE_RESET, 0, 0, NULL); // 3 reset commands in row will terminate any out-of-sync commands
  daliSend(DALICMD_TERMINATE, 0, aStatusCB); // terminate any special commands on the DALI bus
}



// Regular DALI bus commands

void DaliComm::daliSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  sendBridgeCommand(CMD_CODE_SEND16, aDali1, aDali2, DaliCommandStatusHandler(*this, aStatusCB), aWithDelay);
}

void DaliComm::daliSendDirectPower(uint8_t aAddress, uint8_t aPower, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  daliSend(dali1FromAddress(aAddress), aPower, aStatusCB, aWithDelay);
}

void DaliComm::daliSendCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  daliSend(dali1FromAddress(aAddress)+1, aCommand, aStatusCB, aWithDelay);
}


void DaliComm::daliSendDtrAndCommand(DaliAddress aAddress, uint8_t aCommand, uint8_t aDTRValue, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  daliSend(DALICMD_SET_DTR, aDTRValue);
  daliSendCommand(aAddress, aCommand, aStatusCB, aWithDelay);
}





// DALI config commands (send twice within 100ms)

void DaliComm::daliSendTwice(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  sendBridgeCommand(CMD_CODE_2SEND16, aDali1, aDali2, DaliCommandStatusHandler(*this, aStatusCB), aWithDelay);
}

void DaliComm::daliSendConfigCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  daliSendTwice(dali1FromAddress(aAddress)+1, aCommand, aStatusCB, aWithDelay);
}


void DaliComm::daliSendDtrAndConfigCommand(DaliAddress aAddress, uint8_t aCommand, uint8_t aDTRValue, DaliCommandStatusCB aStatusCB, int aWithDelay)
{
  daliSend(DALICMD_SET_DTR, aDTRValue);
  daliSendConfigCommand(aAddress, aCommand, aStatusCB, aWithDelay);
}



// DALI Query commands (expect answer byte)

void DaliComm::daliSendAndReceive(uint8_t aDali1, uint8_t aDali2, DaliQueryResultCB aResultCB, int aWithDelay)
{
  sendBridgeCommand(CMD_CODE_SEND16_REC8, aDali1, aDali2, DaliQueryResponseHandler(*this, aResultCB), aWithDelay);
}


void DaliComm::daliSendQuery(DaliAddress aAddress, uint8_t aQueryCommand, DaliQueryResultCB aResultCB, int aWithDelay)
{
  daliSendAndReceive(dali1FromAddress(aAddress)+1, aQueryCommand, aResultCB, aWithDelay);
}



bool DaliComm::isYes(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr &aError, bool aCollisionIsYes)
{
  bool isYes = !aNoOrTimeout;
  if (aError && aCollisionIsYes && aError->isError(DaliCommError::domain(), DaliCommErrorDALIFrame)) {
    // framing error -> consider this a YES
    isYes = true;
    aError.reset(); // not considered an error when aCollisionIsYes is set
  }
  else if (isYes && !aCollisionIsYes) {
    // regular answer, must be DALIANSWER_YES to be a regular YES
    if (aResponse!=DALIANSWER_YES) {
      // invalid YES response
      aError.reset(new DaliCommError(DaliCommErrorInvalidAnswer));
    }
  }
  if (aError)
    return false; // real error, consider NO
  // return YES/NO
  return isYes;
}



// DALI address byte:
// 0AAA AAAS : device short address (0..63)
// 100A AAAS : group address (0..15)
// 1111 111S : broadcast
// S : 0=direct arc power, 1=command

uint8_t DaliComm::dali1FromAddress(DaliAddress aAddress)
{
  if (aAddress==DaliBroadcast) {
    return 0xFE; // broadcast
  }
  else if (aAddress & DaliGroup) {
    return 0x80 + ((aAddress & DaliAddressMask)<<1); // group address
  }
  else {
    return ((aAddress & DaliAddressMask)<<1); // device short address
  }
}


DaliAddress DaliComm::addressFromDaliResponse(uint8_t aResponse)
{
  aResponse &= 0xFE;
  if (aResponse==0xFE) {
    return DaliBroadcast; // broadcast
  }
  else if (aResponse & 0x80) {
    return ((aResponse>>1) & DaliGroupMask) + DaliGroup;
  }
  else {
    return (aResponse>>1) & DaliAddressMask; // device short address
  }
}




#pragma mark - DALI bus scanning

// Scan bus for active devices (returns list of short addresses)

class DaliBusScanner : public P44Obj
{
  DaliComm &daliComm;
  DaliComm::DaliBusScanCB callback;
  DaliAddress shortAddress;
  DaliComm::ShortAddressListPtr activeDevicesPtr;
  bool probablyCollision;
  bool unconfiguredDevices;
public:
  static void scanBus(DaliComm &aDaliComm, DaliComm::DaliBusScanCB aResultCB)
  {
    // create new instance, deletes itself when finished
    new DaliBusScanner(aDaliComm, aResultCB);
  };
private:
  DaliBusScanner(DaliComm &aDaliComm, DaliComm::DaliBusScanCB aResultCB) :
    callback(aResultCB),
    daliComm(aDaliComm),
    probablyCollision(false),
    unconfiguredDevices(false),
    activeDevicesPtr(new std::list<DaliAddress>)
  {
    daliComm.startProcedure();
    LOG(LOG_INFO, "DaliComm: starting quick bus scan (short address poll)\n");
    // reset the bus first
    daliComm.reset(boost::bind(&DaliBusScanner::resetComplete, this, _2));
  }

  void resetComplete(ErrorPtr aError)
  {
    if (aError)
      return completed(aError);
    // check if there are devices without short address
    daliComm.daliSendQuery(DaliBroadcast, DALICMD_QUERY_MISSING_SHORT_ADDRESS, boost::bind(&DaliBusScanner::handleMissingShortAddressResponse, this, _2, _3, _4));
  }


  void handleMissingShortAddressResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    if (DaliComm::isYes(aNoOrTimeout, aResponse, aError, true)) {
      // we have devices without short addresses
      unconfiguredDevices = true;
    }
    // start the scan
    shortAddress = 0;
    queryNext();
  };

  // handle scan result
  void handleScanResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    bool isYes = false;
    if (aError && aError->isError(DaliCommError::domain(), DaliCommErrorDALIFrame)) {
      // framing error, indicates that we might have duplicates
      LOG(LOG_INFO, "Detected framing error for response from short address %d - probably short address collision\n", shortAddress);
      probablyCollision = true;
      isYes = true; // still count as YES
      aError.reset(); // do not count as error aborting the search
    }
    else if (!aError && !aNoOrTimeout) {
      // no error, no timeout
      isYes = true;
      if (aResponse!=DALIANSWER_YES) {
        // not entirely correct answer, also indicates collision
        LOG(LOG_INFO, "Detected incorrect YES answer 0x%02X from short address %d - probably short address collision\n", aResponse, shortAddress);
        probablyCollision = true;
      }
    }
    if (isYes) {
      activeDevicesPtr->push_back(shortAddress);
    }
    shortAddress++;
    if (shortAddress<DALI_MAXDEVICES && !aError) {
      // more devices to scan
      queryNext();
    }
    else {
      return completed(aError);
    }
  };

  // query next device
  void queryNext()
  {
    daliComm.daliSendQuery(shortAddress, DALICMD_QUERY_CONTROL_GEAR, boost::bind(&DaliBusScanner::handleScanResponse, this, _2, _3, _4));
  }

  void completed(ErrorPtr aError)
  {
    // scan done or error, return list to callback
    if (!aError && (probablyCollision || unconfiguredDevices)) {
      aError = ErrorPtr(new DaliCommError(DaliCommErrorNeedFullScan,"Need full bus scan"));
    }
    daliComm.endProcedure();
    callback(daliComm, activeDevicesPtr, aError);
    // done, delete myself
    delete this;
  }

};


void DaliComm::daliBusScan(DaliBusScanCB aResultCB)
{
  if (isBusy()) { aResultCB(*this, ShortAddressListPtr(), DaliComm::busyError()); return; }
  DaliBusScanner::scanBus(*this, aResultCB);
}




// Scan DALI bus by random address

#define MAX_RESTARTS 3

class DaliFullBusScanner : public P44Obj
{
  DaliComm &daliComm;
  DaliComm::DaliBusScanCB callback;
  bool fullScanOnlyIfNeeded;
  uint32_t searchMax;
  uint32_t searchMin;
  uint32_t searchAddr;
  uint8_t searchL, searchM, searchH;
  uint32_t lastSearchMin; // for re-starting
  int restarts;
  bool setLMH;
  DaliComm::ShortAddressListPtr foundDevicesPtr;
  DaliComm::ShortAddressListPtr usedShortAddrsPtr;
  DaliAddress newAddress;
public:
  static void fullBusScan(DaliComm &aDaliComm, DaliComm::DaliBusScanCB aResultCB, bool aFullScanOnlyIfNeeded)
  {
    // create new instance, deletes itself when finished
    new DaliFullBusScanner(aDaliComm, aResultCB, aFullScanOnlyIfNeeded);
  };
private:
  DaliFullBusScanner(DaliComm &aDaliComm, DaliComm::DaliBusScanCB aResultCB, bool aFullScanOnlyIfNeeded) :
    daliComm(aDaliComm),
    callback(aResultCB),
    fullScanOnlyIfNeeded(aFullScanOnlyIfNeeded),
    foundDevicesPtr(new DaliComm::ShortAddressList)
  {
    daliComm.startProcedure();
    // first scan for used short addresses
    DaliBusScanner::scanBus(daliComm,boost::bind(&DaliFullBusScanner::shortAddrListReceived, this, _2, _3));
  }

  void shortAddrListReceived(DaliComm::ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError)
  {
    bool fullScanNeeded = aError && aError->isError(DaliCommError::domain(), DaliCommErrorNeedFullScan);
    if (aError && !fullScanNeeded)
      return completed(aError);
    // exit now if no full scan needed and short address scan is ok
    if (!fullScanNeeded && fullScanOnlyIfNeeded) {
      // just use the short address scan result
      foundDevicesPtr = aShortAddressListPtr;
      completed(ErrorPtr()); return;
    }
    // save the short address list
    usedShortAddrsPtr = aShortAddressListPtr;
    LOG(LOG_NOTICE, "DaliComm: starting full bus scan (random address binary search)\n");
    // Terminate any special modes first
    daliComm.daliSend(DALICMD_TERMINATE, 0x00);
    // initialize entire system for random address selection process
    daliComm.daliSendTwice(DALICMD_INITIALISE, 0x00);
    daliComm.daliSendTwice(DALICMD_RANDOMISE, 0x00);
    // start search at lowest address
    newSearchUpFrom(0);
  };


  bool isShortAddressInList(DaliAddress aShortAddress, DaliComm::ShortAddressListPtr aShortAddressList)
  {
    if (!usedShortAddrsPtr)
      return true; // no info, consider all used as we don't know
    for (DaliComm::ShortAddressList::iterator pos = aShortAddressList->begin(); pos!=aShortAddressList->end(); ++pos) {
      if (aShortAddress==(*pos))
        return true;
    }
    return false;
  }

  // get new unused short address
  DaliAddress newShortAddress()
  {
    DaliAddress newAddr = DALI_MAXDEVICES;
    while (newAddr>0) {
      newAddr--;
      if (!isShortAddressInList(newAddr,usedShortAddrsPtr)) {
        // this one is free, use it
        usedShortAddrsPtr->push_back(newAddr);
        return newAddr;
      }
    }
    // return broadcast if none available
    return DaliBroadcast;
  }


  void newSearchUpFrom(uint32_t aMinSearch)
  {
    // init search range
    searchMax = 0xFFFFFF;
    searchMin = aMinSearch;
    lastSearchMin = aMinSearch;
    restarts = 0;
    searchAddr = (searchMax-aMinSearch)/2+aMinSearch; // start in the middle
    // no search address currently set
    setLMH = true;
    compareNext();
  }


  void compareNext()
  {
    // issue next compare command
    // - update address bytes as needed (only those that have changed)
    uint8_t by = (searchAddr>>16) & 0xFF;
    if (by!=searchH || setLMH) {
      searchH = by;
      daliComm.daliSend(DALICMD_SEARCHADDRH, searchH);
    }
    // - searchM
    by = (searchAddr>>8) & 0xFF;
    if (by!=searchM || setLMH) {
      searchM = by;
      daliComm.daliSend(DALICMD_SEARCHADDRM, searchM);
    }
    // - searchL
    by = (searchAddr) & 0xFF;
    if (by!=searchL || setLMH) {
      searchL = by;
      daliComm.daliSend(DALICMD_SEARCHADDRL, searchL);
    }
    setLMH = false; // incremental from now on until flag is set again
    // - issue the compare command
    daliComm.daliSendAndReceive(DALICMD_COMPARE, 0x00, boost::bind(&DaliFullBusScanner::handleCompareResult, this, _2, _3, _4));
  }

  void handleCompareResult(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    // Anything received but timeout is considered a yes
    bool isYes = DaliComm::isYes(aNoOrTimeout, aResponse, aError, true);
    if (aError) {
      completed(aError); // other error, abort
      return;
    }
    DBGLOG(LOG_DEBUG, "DALICMD_COMPARE result = %s, search=0x%06X, searchMin=0x%06X, searchMax=0x%06X\n", isYes ? "Yes" : "No ", searchAddr, searchMin, searchMax);
    // any ballast has smaller or equal random address?
    if (isYes) {
      // yes, there is at least one, max address is what we searched so far
      searchMax = searchAddr;
    }
    else {
      // none at or below current search
      if (searchMin==0xFFFFFF) {
        // already at max possible -> no more devices found
        LOG(LOG_INFO, "No more devices\n");
        completed(ErrorPtr()); return;
      }
      searchMin = searchAddr+1; // new min
    }
    if (searchMin==searchMax && searchAddr==searchMin) {
      // found!
      LOG(LOG_NOTICE, "- Found device at 0x%06X\n", searchAddr);
      // read current short address
      daliComm.daliSendAndReceive(DALICMD_QUERY_SHORT_ADDRESS, 0x00, boost::bind(&DaliFullBusScanner::handleShortAddressQuery, this, _2, _3, _4));
    }
    else {
      // not yet - continue
      searchAddr = searchMin + (searchMax-searchMin)/2;
      DBGLOG(LOG_DEBUG, "                         Next search=0x%06X, searchMin=0x%06X, searchMax=0x%06X\n", searchAddr, searchMin, searchMax);
      if (searchAddr>0xFFFFFF) {
        LOG(LOG_WARNING, "- failed search\n");
        if (restarts<MAX_RESTARTS) {
          LOG(LOG_NOTICE, "- restarting search at address of last found device + 1\n");
          restarts++;
          newSearchUpFrom(lastSearchMin);
          return;
        }
        else {
          return completed(ErrorPtr(new DaliCommError(DaliCommErrorDeviceSearch, "Binary search got out of range")));
        }
      }
      // issue next compare
      compareNext();
    }
  }

  void handleShortAddressQuery(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    if (aError)
      return completed(aError);
    if (aNoOrTimeout) {
      LOG(LOG_WARNING, "- Device at 0x%06X does not respond to DALICMD_QUERY_SHORT_ADDRESS\n", searchAddr);
      // TODO: should not happen, probably bus error led to false device detection, restart search
    }
    else {
      // response is short address in 0AAAAAA1 format or DALIVALUE_MASK (no adress)
      newAddress = DaliBroadcast; // none
      DaliAddress shortAddress = newAddress; // none
      if (aResponse==DALIVALUE_MASK) {
        // device has no short address yet, assign one
        newAddress = newShortAddress();
        LOG(LOG_NOTICE, "- Device at 0x%06X has NO short address -> assigning new short address = %d\n", searchAddr, newAddress);
      }
      else {
        shortAddress = DaliComm::addressFromDaliResponse(aResponse);
        DBGLOG(LOG_INFO, "- Device at 0x%06X has short address: %d\n", searchAddr, shortAddress);
        // check for collisions
        if (isShortAddressInList(shortAddress, foundDevicesPtr)) {
          newAddress = newShortAddress();
          LOG(LOG_NOTICE, "- Collision on short address %d -> assigning new short address = %d\n", shortAddress, newAddress);
        }
      }
      // check if we need to re-assign the short address
      if (newAddress!=DaliBroadcast) {
        // new address must be assigned
        daliComm.daliSend(DALICMD_PROGRAM_SHORT_ADDRESS, DaliComm::dali1FromAddress(newAddress)+1);
        daliComm.daliSendAndReceive(
          DALICMD_VERIFY_SHORT_ADDRESS, DaliComm::dali1FromAddress(newAddress)+1,
          boost::bind(&DaliFullBusScanner::handleNewShortAddressVerify, this, _2, _3, _4),
          1000 // delay one second before querying for new short address
        );
      }
      else {
        // short address is ok as-is
        deviceFound(shortAddress);
      }
    }
  }

  void handleNewShortAddressVerify(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    if (DaliComm::isYes(aNoOrTimeout, aResponse, aError, false)) {
      // real clean YES - new short address verified
      deviceFound(newAddress);
    }
    else {
      // short address verification failed
      LOG(LOG_ERR, "Error - could not assign new short address %d\n", newAddress);
      return completed(ErrorPtr(new DaliCommError(DaliCommErrorSetShortAddress, "Failed setting short address")));
    }
  }

  void deviceFound(DaliAddress aShortAddress)
  {
    // store short address
    foundDevicesPtr->push_back(aShortAddress);
    // withdraw this device from further searches
    daliComm.daliSend(DALICMD_WITHDRAW, 0x00);
    // continue searching devices
    newSearchUpFrom(searchAddr+1);
  }

  void completed(ErrorPtr aError)
  {
    // terminate
    daliComm.daliSend(DALICMD_TERMINATE, 0x00);
    // callback
    daliComm.endProcedure();
    callback(daliComm, foundDevicesPtr, aError);
    // done, delete myself
    delete this;
  }
};


void DaliComm::daliFullBusScan(DaliBusScanCB aResultCB, bool aFullScanOnlyIfNeeded)
{
  if (isBusy()) { aResultCB(*this, ShortAddressListPtr(), DaliComm::busyError()); return; }
  DaliFullBusScanner::fullBusScan(*this, aResultCB, aFullScanOnlyIfNeeded);
}



#pragma mark - DALI memory access / device info reading

class DaliMemoryReader : public P44Obj
{
  DaliComm &daliComm;
  DaliComm::DaliReadMemoryCB callback;
  DaliAddress busAddress;
  DaliComm::MemoryVectorPtr memory;
  int bytesToRead;
  typedef std::vector<uint8_t> MemoryVector;
public:
  static void readMemory(DaliComm &aDaliComm, DaliComm::DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes)
  {
    // create new instance, deletes itself when finished
    new DaliMemoryReader(aDaliComm, aResultCB, aAddress, aBank, aOffset, aNumBytes);
  };
private:
  DaliMemoryReader(DaliComm &aDaliComm, DaliComm::DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes) :
    daliComm(aDaliComm),
    callback(aResultCB),
    busAddress(aAddress),
    memory(new MemoryVector)
  {
    daliComm.startProcedure();
    LOG(LOG_INFO, "DALI - reading %d bytes from bank %d at offset %d:\n", aNumBytes, aBank, aOffset);
    // set DTR1 = bank
    daliComm.daliSend(DALICMD_SET_DTR1, aBank);
    // set DTR = offset within bank
    daliComm.daliSend(DALICMD_SET_DTR, aOffset);
    // start reading
    bytesToRead = aNumBytes;
    readNextByte();
  };

  // handle scan result
  void handleResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    if (!aError && !aNoOrTimeout) {
      // byte received, append to vector
      memory->push_back(aResponse);
      if (--bytesToRead>0) {
        // more bytes to read
        readNextByte();
        return;
      }
    }
    // read done, timeout or error, return memory to callback
    daliComm.endProcedure();
    if (LOGENABLED(LOG_INFO)) {
      // dump data
      int o=0;
      for (MemoryVector::iterator pos = memory->begin(); pos!=memory->end(); ++pos, ++o) {
        LOG(LOG_INFO, "- %03d/0x%02X : 0x%02X/%03d\n", o, o, *pos, *pos);
      }
    }
    callback(daliComm, memory, aError);
    // done, delete myself
    delete this;
  };

  void readNextByte()
  {
    daliComm.daliSendQuery(busAddress, DALICMD_READ_MEMORY_LOCATION, boost::bind(&DaliMemoryReader::handleResponse, this, _2, _3, _4));
  }
};


void DaliComm::daliReadMemory(DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes)
{
  if (isBusy()) { aResultCB(*this, MemoryVectorPtr(), DaliComm::busyError()); return; }
  DaliMemoryReader::readMemory(*this, aResultCB, aAddress, aBank, aOffset, aNumBytes);
}



// read device info of a DALI device

class DaliDeviceInfoReader : public P44Obj
{
  DaliComm &daliComm;
  DaliComm::DaliDeviceInfoCB callback;
  DaliAddress busAddress;
  DaliComm::DaliDeviceInfoPtr deviceInfo;
  uint8_t bankChecksum;
public:
  static void readDeviceInfo(DaliComm &aDaliComm, DaliComm::DaliDeviceInfoCB aResultCB, DaliAddress aAddress)
  {
    // create new instance, deletes itself when finished
    new DaliDeviceInfoReader(aDaliComm, aResultCB, aAddress);
  };
private:
  DaliDeviceInfoReader(DaliComm &aDaliComm, DaliComm::DaliDeviceInfoCB aResultCB, DaliAddress aAddress) :
    daliComm(aDaliComm),
    callback(aResultCB),
    busAddress(aAddress)
  {
    daliComm.startProcedure();
    // read the memory
    bankChecksum = 0; // !(sum(byte2..byteLast). Check with sum(byte1..byteLast)==0xFF
    DaliMemoryReader::readMemory(daliComm, boost::bind(&DaliDeviceInfoReader::handleBank0Data, this, _2, _3), busAddress, 0, 0, DALIMEM_BANK0_MINBYTES);
  };

  void handleBank0Data(DaliComm::MemoryVectorPtr aBank0Data, ErrorPtr aError)
  {
    deviceInfo.reset(new DaliDeviceInfo);
    deviceInfo->shortAddress = busAddress;
    if (aError)
      return complete(aError);
    if (aBank0Data->size()==DALIMEM_BANK0_MINBYTES) {
      // sum up starting with checksum itself, result must be 0xFF in the end
      for (int i=0x01; i<DALIMEM_BANK0_MINBYTES; i++) {
        bankChecksum += (*aBank0Data)[i];
      }
      // GTIN: bytes 0x03..0x08, MSB first
      deviceInfo->gtin = 0;
      for (int i=0x03; i<=0x08; i++) {
        deviceInfo->gtin = (deviceInfo->gtin << 8) + (*aBank0Data)[i];
      }
      // Firmware version
      deviceInfo->fw_version_major = (*aBank0Data)[0x09];
      deviceInfo->fw_version_minor = (*aBank0Data)[0x0A];
      // Serial: bytes 0x0B..0x0E
      deviceInfo->serialNo = 0;
      for (int i=0x0B; i<=0x0E; i++) {
        deviceInfo->serialNo = (deviceInfo->serialNo << 8) + (*aBank0Data)[i];
      }
      // check for extra data device may have
      int extraBytes = (*aBank0Data)[0]-DALIMEM_BANK0_MINBYTES;
      if (extraBytes>0) {
        // issue read of extra bytes
        DaliMemoryReader::readMemory(daliComm, boost::bind(&DaliDeviceInfoReader::handleBank0ExtraData, this, _2, _3), busAddress, 0, DALIMEM_BANK0_MINBYTES, extraBytes);
      }
      else {
        // directly continue by reading bank1
        bank0readcomplete();
      }
    }
    else {
      // not enough bytes
      return complete(ErrorPtr(new DaliCommError(DaliCommErrorMissingData,string_format("Not enough bytes read from bank0 at shortAddress %d", busAddress))));
    }
  };

  void handleBank0ExtraData(DaliComm::MemoryVectorPtr aBank0Data, ErrorPtr aError)
  {
    if (aError)
      return complete(aError);
    else {
      // add extra bytes to checksum, result must be 0xFF in the end
      for (int i=0; i<aBank0Data->size(); i++) {
        bankChecksum += (*aBank0Data)[i];
      }
      // TODO: look at that data
      // now get OEM info
      bank0readcomplete();
    }
  };


  void bank0readcomplete()
  {
    // verify checksum of bank0 data first
    if (bankChecksum!=0xFF) {
      // checksum error
      // - invalidate gtin, serial and fw version
      deviceInfo->gtin = 0;
      deviceInfo->fw_version_major = 0;
      deviceInfo->fw_version_minor = 0;
      deviceInfo->serialNo = 0;
      // - report error
      LOG(LOG_ERR, "DALI shortaddress %d Bank 0 checksum is wrong - should sum up to 0xFF, actual sum is 0x%02X\n", busAddress, bankChecksum);
      return complete(ErrorPtr(new DaliCommError(DaliCommErrorBadChecksum,string_format("bad DALI memeory bank 0 checksum at shortAddress %d", busAddress))));
    }
    // now read OEM info from bank1
    bankChecksum = 0;
    DaliMemoryReader::readMemory(daliComm, boost::bind(&DaliDeviceInfoReader::handleBank1Data, this, _2, _3), busAddress, 1, 0, DALIMEM_BANK1_MINBYTES);
  };


  void handleBank1Data(DaliComm::MemoryVectorPtr aBank1Data, ErrorPtr aError)
  {
    if (aError)
      return complete(aError);
    if (aBank1Data->size()==DALIMEM_BANK1_MINBYTES) {
      // sum up starting with checksum itself, result must be 0xFF in the end
      for (int i=0x01; i<DALIMEM_BANK1_MINBYTES; i++) {
        bankChecksum += (*aBank1Data)[i];
      }
      // OEM GTIN: bytes 0x03..0x08, MSB first
      deviceInfo->gtin = 0;
      for (int i=0x03; i<=0x08; i++) {
        deviceInfo->oem_gtin = (deviceInfo->oem_gtin << 8) + (*aBank1Data)[i];
      }
      // Serial: bytes 0x09..0x0C
      deviceInfo->serialNo = 0;
      for (int i=0x09; i<=0x0C; i++) {
        deviceInfo->oem_serialNo = (deviceInfo->oem_serialNo << 8) + (*aBank1Data)[i];
      }
      // check for extra data device may have
      int extraBytes = (*aBank1Data)[0]-DALIMEM_BANK1_MINBYTES;
      if (extraBytes>0) {
        // issue read of extra bytes
        DaliMemoryReader::readMemory(daliComm, boost::bind(&DaliDeviceInfoReader::handleBank1ExtraData, this, _2, _3), busAddress, 0, DALIMEM_BANK1_MINBYTES, extraBytes);
      }
      else {
        // No extra bytes: device info is complete already
        return bank1readcomplete(aError);
      }
    }
    else {
      // No bank1 OEM info: device info is complete already (is not an error)
      return complete(aError);
    }
  };


  void handleBank1ExtraData(DaliComm::MemoryVectorPtr aBank1Data, ErrorPtr aError)
  {
    if (aError)
      return complete(aError);
    else {
      // add extra bytes to checksum, result must be 0xFF in the end
      for (int i=0; i<aBank1Data->size(); i++) {
        bankChecksum += (*aBank1Data)[i];
      }
      // TODO: look at that data
      // now get OEM info
      bank1readcomplete(aError);
    }
  };


  void bank1readcomplete(ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      // test checksum
      if (bankChecksum!=0xFF) {
        // checksum error
        // - invalidate gtin, serial and fw version
        deviceInfo->oem_gtin = 0;
        deviceInfo->oem_serialNo = 0;
        // - report error
        LOG(LOG_ERR, "DALI shortaddress %d Bank 1 checksum is wrong - should sum up to 0xFF, actual sum is 0x%02X\n", busAddress, bankChecksum);
        aError = ErrorPtr(new DaliCommError(DaliCommErrorBadChecksum,string_format("bad DALI memeory bank 1 checksum at shortAddress %d", busAddress)));
      }
    }
    complete(aError);
  }


  void complete(ErrorPtr aError)
  {
    daliComm.endProcedure();
    callback(daliComm, deviceInfo, aError);
    // done, delete myself
    delete this;
  }
};


void DaliComm::daliReadDeviceInfo(DaliDeviceInfoCB aResultCB, DaliAddress aAddress)
{
  if (isBusy()) { aResultCB(*this, DaliDeviceInfoPtr(), DaliComm::busyError()); return; }
  DaliDeviceInfoReader::readDeviceInfo(*this, aResultCB, aAddress);
}


#pragma mark - DALI device info

DaliDeviceInfo::DaliDeviceInfo()
{
  gtin = 0;
  fw_version_major = 0;
  fw_version_minor = 0;
  serialNo = 0;
  oem_gtin = 0;
  oem_serialNo = 0;
}


string DaliDeviceInfo::description()
{
  string s = string_format("- DaliDeviceInfo for shortAddress %d\n", shortAddress);
  string_format_append(s, "  - is %suniquely defining the device\n", uniquelyIdentifying() ? "" : "NOT ");
  string_format_append(s, "  - GTIN       : %lld\n", gtin);
  string_format_append(s, "  - Serial     : %lld\n", serialNo);
  string_format_append(s, "  - OEM GTIN   : %lld\n", oem_gtin);
  string_format_append(s, "  - OEM Serial : %lld\n", oem_serialNo);
  string_format_append(s, "  - Firmware   : %d.%d\n", fw_version_major, fw_version_minor);
  return s;
}


bool DaliDeviceInfo::uniquelyIdentifying()
{
  return gtin!=0 && serialNo!=0;
}


#pragma mark - testing %%%



void DaliComm::testReset()
{
  reset(boost::bind(&DaliComm::testResetAck, this, _2));
}


void DaliComm::testResetAck(ErrorPtr aError)
{
  if (aError)
    printf("resetAck: error = %s\n", aError->description().c_str());
  else
    printf("resetAck: DALI bridge and bus reset OK\n");
}



void DaliComm::testBusScan()
{
  daliBusScan(boost::bind(&DaliComm::testBusScanAck, this, _2, _3));
}

void DaliComm::testBusScanAck(ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError)
{
  if (aError)
    printf("testBusScanAck: error = %s\n", aError->description().c_str());
  if (aShortAddressListPtr) {
    printf("testBusScanAck: %ld Devices found = ", aShortAddressListPtr->size());
    for (list<DaliAddress>::iterator pos = aShortAddressListPtr->begin(); pos!=aShortAddressListPtr->end(); ++pos) {
      printf("%d ",*pos);
    }
    printf("\n");
  }
}



void DaliComm::testFullBusScan()
{
  daliFullBusScan(boost::bind(&DaliComm::testFullBusScanAck, this, _2, _3), true); // full scan only if needed
}

void DaliComm::testFullBusScanAck(ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError)
{
  if (aError)
    printf("testFullBusScanAck: error = %s\n", aError->description().c_str());
  else {
    printf("testFullBusScanAck: %ld Devices found = ", aShortAddressListPtr->size());
    for (list<DaliAddress>::iterator pos = aShortAddressListPtr->begin(); pos!=aShortAddressListPtr->end(); ++pos) {
      printf("%d ",*pos);
    }
    printf("\n");
  }
}



void DaliComm::testReadBytes(DaliAddress aShortAddress)
{
  daliReadMemory(boost::bind(&DaliComm::testReadBytesAck, this, _2, _3), aShortAddress, 0, 0, 15);
}

void DaliComm::testReadBytesAck(MemoryVectorPtr aMemoryPtr, ErrorPtr aError)
{
  if (aError)
    printf("testReadBytesAck: error = %s\n", aError->description().c_str());
  else {
    printf("testReadBytesAck: %ld bytes read = ", aMemoryPtr->size());
    for (vector<uint8_t>::iterator pos = aMemoryPtr->begin(); pos!=aMemoryPtr->end(); ++pos) {
      printf("0x%02X ",*pos);
    }
    printf("\n");
  }
}


void DaliComm::testReadDeviceInfo(DaliAddress aShortAddress)
{
  daliReadDeviceInfo(boost::bind(&DaliComm::testReadDeviceInfoAck, this, _2, _3), aShortAddress);
}

void DaliComm::testReadDeviceInfoAck(DaliDeviceInfoPtr aDeviceInfo, ErrorPtr aError)
{
  if (aError)
    printf("testReadDeviceInfoAck: error = %s\n", aError->description().c_str());
  else
    printf("testReadDeviceInfoAck: %s", aDeviceInfo->description().c_str());
}









