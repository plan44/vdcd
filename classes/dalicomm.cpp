/*
 * dalicomm.cpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include "dalicomm.hpp"

// pseudo baudrate for dali bridge must be 9600bd
#define BAUDRATE B9600


DaliComm::DaliComm() :
  bridgeConnectionPort(0),
  bridgeConnectionOpen(false)
{
  setTransmitter(boost::bind(&DaliComm::transmitBytes, this, _1, _2));
}


DaliComm::~DaliComm()
{
  closeConnection();
}


void DaliComm::setConnectionParameters(const char* aBridgeConnectionPath, uint16_t aPortNo)
{
  closeConnection();
  bridgeConnectionPath = aBridgeConnectionPath;
  bridgeConnectionPort = aPortNo;
}



#pragma mark - main loop integration

int DaliComm::toBeMonitoredFD()
{
  if (bridgeConnectionOpen) {
    return bridgeFd;
  }
  else {
    return -1;
  }
}


void DaliComm::dataReadyOnMonitoredFD()
{
  if (bridgeConnectionOpen) {
    uint8_t byte;
    size_t res = read(bridgeFd,&byte,1); // read single byte
    if (res==1) {
      // deliver to queue
      DBGLOG(LOG_DEBUG,"Received byte: %02X\n", byte);
      acceptBytes(1, &byte);
    }
  }
}


void DaliComm::process()
{
  processOperations();
}



#pragma mark - serial connection to the DALI bridge


size_t DaliComm::transmitBytes(size_t aNumBytes, uint8_t *aBytes)
{
  size_t res = 0;
  if (establishConnection()) {
    res = write(bridgeFd,aBytes,aNumBytes);
    #ifdef DEBUG
    std::string s;
    for (size_t i=0; i<aNumBytes; i++) {
      string_format_append(s, "%02X ",aBytes[i]);
    }
    DBGLOG(LOG_DEBUG,"Transmitted bytes: %s\n", s.c_str());
    #endif
  }
  return res;
}


bool DaliComm::establishConnection()
{
  if (!bridgeConnectionOpen) {
    // Open connection to bridge
    bridgeFd = 0;
    int res;
    struct termios newtio;
    serialConnection = bridgeConnectionPath[0]=='/';
    // check type of input
    if (serialConnection) {
      // assume it's a serial port
      bridgeFd = open(bridgeConnectionPath.c_str(), O_RDWR | O_NOCTTY);
      if (bridgeFd<0) {
        LOGERRNO(LOG_ERR);
        setError(ErrorPtr(new DaliCommError(DaliCommErrorSerialOpen)));
        return false;
      }
      tcgetattr(bridgeFd,&oldTermIO); // save current port settings
      // see "man termios" for details
      memset(&newtio, 0, sizeof(newtio));
      // - baudrate, 8-N-1, no modem control lines (local), reading enabled
      newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
      // - ignore parity errors
      newtio.c_iflag = IGNPAR;
      // - no output control
      newtio.c_oflag = 0;
      // - no input control (non-canonical)
      newtio.c_lflag = 0;
      // - no inter-char time
      newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
      // - receive every single char seperately
      newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 chars received */
      // - set new params
      tcflush(bridgeFd, TCIFLUSH);
      tcsetattr(bridgeFd,TCSANOW,&newtio);
    }
    else {
      // assume it's an IP address or hostname
      struct sockaddr_in conn_addr;
      if ((bridgeFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(LOG_ERR,"Error: Could not create socket\n");
        setError(ErrorPtr(new DaliCommError(DaliCommErrorSocketOpen)));
        return false;
      }
      // prepare IP address
      memset(&conn_addr, '0', sizeof(conn_addr));
      conn_addr.sin_family = AF_INET;
      conn_addr.sin_port = htons(bridgeConnectionPort);
      struct hostent *server;
      server = gethostbyname(bridgeConnectionPath.c_str());
      if (server == NULL) {
        LOG(LOG_ERR,"Error: no such host\n");
        setError(ErrorPtr(new DaliCommError(DaliCommErrorInvalidHost)));
        return false;
      }
      memcpy((char *)&conn_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
      if ((res = connect(bridgeFd, (struct sockaddr *)&conn_addr, sizeof(conn_addr))) < 0) {
        LOGERRNO(LOG_ERR);
        setError(ErrorPtr(new DaliCommError(DaliCommErrorSocketOpen)));
        return false;
      }
    }
    // successfully opened
    bridgeConnectionOpen = true;
  }
  return bridgeConnectionOpen;
}


void DaliComm::closeConnection()
{
  if (bridgeConnectionOpen) {
    // restore IO settings
    if (serialConnection) {
      tcsetattr(bridgeFd,TCSANOW,&oldTermIO);
    }
    // close
    close(bridgeFd);
    // closed
    bridgeConnectionOpen = false;
    // abort all pending operations
    abortOperations();
  }
}


void DaliComm::setError(ErrorPtr aError)
{
  if (aError) {
    lastError = aError;
    LOG(LOG_ERR,"DaliComm global error set: %s\n",aError->description().c_str());
  }
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



class BridgeResponseHandler
{
  DaliComm::DaliBridgeResultCB callback;
  DaliComm *daliComm;
public:
  BridgeResponseHandler(DaliComm *aDaliCommP, DaliComm::DaliBridgeResultCB aResultCB) :
    daliComm(aDaliCommP),
    callback(aResultCB)
  {};
  void operator() (SerialOperation *aOpP, SerialOperationQueue *aQueueP, ErrorPtr aError) {
    SerialOperationReceive *ropP = dynamic_cast<SerialOperationReceive *>(aOpP);
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


void DaliComm::sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB)
{
  SerialOperation *opP = NULL;
  if (aCmd<8) {
    // single byte command
    opP = new SerialOperationSendAndReceive(1,&aCmd,2);
  }
  else {
    // 3 byte command
    uint8_t cmd3[3];
    cmd3[0] = aCmd;
    cmd3[1] = aDali1;
    cmd3[2] = aDali2;
    opP = new SerialOperationSendAndReceive(3,cmd3,2);
  }
  if (opP) {
    SerialOperationPtr op(opP);
    op->setSerialOperationCB(BridgeResponseHandler(this, aResultCB));
    queueOperation(op);
  }
  // process operations
  processOperations();
}


#pragma mark - DALI bus communication basics


class DaliCommandStatusHandler
{
  DaliComm::DaliCommandStatusCB callback;
protected:
  DaliComm *daliComm;
  ErrorPtr checkBridgeResponse(uint8_t aResp1, uint8_t aResp2, ErrorPtr aError, bool &aNoOrTimeout)
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
public:
  DaliCommandStatusHandler(DaliComm *aDaliCommP, DaliComm::DaliCommandStatusCB aResultCB) :
    daliComm(aDaliCommP),
    callback(aResultCB)
  { };
  void operator() (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)
  {
    bool noOrTimeout;
    aError = checkBridgeResponse(aResp1, aResp2, aError, noOrTimeout);
    // execute callback if any
    if (callback)
      callback(daliComm, aError);
    // anyway, report any real errors to DaliComm
    daliComm->setError(aError);
  }
};


class DaliQueryResponseHandler : DaliCommandStatusHandler
{
  DaliComm::DaliQueryResultCB callback;
public:
  DaliQueryResponseHandler(DaliComm *aDaliCommP, DaliComm::DaliQueryResultCB aResultCB) :
    DaliCommandStatusHandler(aDaliCommP, NULL),
    callback(aResultCB)
  { };

  void operator() (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)
  {
    bool noOrTimeout;
    aError = checkBridgeResponse(aResp1, aResp2, aError, noOrTimeout);
    // execute callback if any
    if (callback)
      callback(daliComm, noOrTimeout, aResp2, aError);
    // anyway, report any real errors to DaliComm
    daliComm->setError(aError);
  };
};
  


/// reset the bridge

void DaliComm::reset(DaliCommandStatusCB aStatusCB)
{
  sendBridgeCommand(CMD_CODE_RESET, 0, 0, NULL);
  sendBridgeCommand(CMD_CODE_RESET, 0, 0, NULL);
  sendBridgeCommand(CMD_CODE_RESET, 0, 0, NULL); // 3 reset commands in row will terminate any out-of-sync commands
  daliSend(DALICMD_TERMINATE, 0, aStatusCB); // terminate any special commands on the DALI bus
}



// Regular DALI bus commands

void DaliComm::daliSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB, int aMinTimeToNextCmd)
{
  sendBridgeCommand(CMD_CODE_SEND16, aDali1, aDali2, DaliCommandStatusHandler(this, aStatusCB));
}

void DaliComm::daliSendDirectPower(uint8_t aAddress, uint8_t aPower, DaliCommandStatusCB aStatusCB, int aMinTimeToNextCmd)
{
  daliSend(dali1FromAddress(aAddress), aPower, aStatusCB, aMinTimeToNextCmd);
}

void DaliComm::daliSendCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB, int aMinTimeToNextCmd)
{
  daliSend(dali1FromAddress(aAddress)+1, aCommand, aStatusCB, aMinTimeToNextCmd);
}



// DALI config commands (send twice within 100ms)

void DaliComm::daliSendTwice(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB, int aMinTimeToNextCmd)
{
  sendBridgeCommand(CMD_CODE_2SEND16, aDali1, aDali2, DaliCommandStatusHandler(this, aStatusCB));
}

void DaliComm::daliSendConfigCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB, int aMinTimeToNextCmd)
{
  daliSendTwice(dali1FromAddress(aAddress)+1, aCommand, aStatusCB, aMinTimeToNextCmd);
}



// DALI Query commands (expect answer byte)

void DaliComm::daliSendAndReceive(uint8_t aDali1, uint8_t aDali2, DaliQueryResultCB aResultCB)
{
  sendBridgeCommand(CMD_CODE_SEND16_REC8, aDali1, aDali2, DaliQueryResponseHandler(this, aResultCB));
}


void DaliComm::daliSendQuery(DaliAddress aAddress, uint8_t aQueryCommand, DaliQueryResultCB aResultCB)
{
  daliSendAndReceive(dali1FromAddress(aAddress)+1, aQueryCommand, aResultCB);
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
    return 0x80 + (aAddress & DaliAddressMask)<<1; // group address
  }
  else {
    return (aAddress & DaliAddressMask)<<1; // device short address
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




#pragma mark - DALI functionality

// Scan bus for active devices (returns list of short addresses)

class DaliBusScanner
{
  DaliComm *daliComm;
  DaliComm::DaliBusScanCB callback;
  DaliAddress busAddress;
  DaliComm::DeviceListPtr activeDevicesPtr;
public:
  static void scanBus(DaliComm *aDaliCommP, DaliComm::DaliBusScanCB aResultCB)
  {
    // create new instance, deletes itself when finished
    new DaliBusScanner(aDaliCommP, aResultCB);
  };
private:
  DaliBusScanner(DaliComm *aDaliCommP, DaliComm::DaliBusScanCB aResultCB) :
    callback(aResultCB),
    daliComm(aDaliCommP),
    activeDevicesPtr(new std::list<DaliAddress>)
  {
    busAddress = 0;
    queryNext();
  };

  // handle scan result
  void handleResponse(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    if (!aError && !aNoOrTimeout && aResponse==DALIANSWER_YES) {
      activeDevicesPtr->push_back(busAddress);
    }
    busAddress++;
    if (busAddress<DALI_MAXDEVICES && !aError) {
      // more devices to scan
      queryNext();
    }
    else {
      // scan done or error, return list to callback
      callback(daliComm, activeDevicesPtr, aError);
      // done, delete myself
      delete this;
    }
  };

  // query next device
  void queryNext()
  {
    daliComm->daliSendQuery(busAddress, DALICMD_QUERY_CONTROL_GEAR, boost::bind(&DaliBusScanner::handleResponse, this, _2, _3, _4));
  }

};


void DaliComm::daliScanBus(DaliBusScanCB aResultCB)
{
  DaliBusScanner::scanBus(this, aResultCB);
}



class DaliMemoryReader
{
  DaliComm *daliComm;
  DaliComm::DaliReadMemoryCB callback;
  DaliAddress busAddress;
  DaliComm::MemoryVectorPtr memory;
  int bytesToRead;
public:
  static void readMemory(DaliComm *aDaliCommP, DaliComm::DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes)
  {
    // create new instance, deletes itself when finished
    new DaliMemoryReader(aDaliCommP, aResultCB, aAddress, aBank, aOffset, aNumBytes);
  };
private:
  DaliMemoryReader(DaliComm *aDaliCommP, DaliComm::DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes) :
    daliComm(aDaliCommP),
    callback(aResultCB),
    busAddress(aAddress),
    memory(new std::vector<uint8_t>)
  {
    // set DTR1 = bank
    daliComm->daliSend(DALICMD_SET_DTR1, aBank);
    // set DTR = offset within bank
    daliComm->daliSend(DALICMD_SET_DTR, aOffset);
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
    callback(daliComm, memory, aError);
    // done, delete myself
    delete this;
  };

  void readNextByte()
  {
    daliComm->daliSendQuery(busAddress, DALICMD_READ_MEMORY_LOCATION, boost::bind(&DaliMemoryReader::handleResponse, this, _2, _3, _4));
  }
};


void DaliComm::daliReadMemory(DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes)
{
  DaliMemoryReader::readMemory(this, aResultCB, aAddress, aBank, aOffset, aNumBytes);
}



// read device info of a DALI device

class DaliDeviceInfoReader
{
  DaliComm *daliComm;
  DaliComm::DaliDeviceInfoCB callback;
  DaliAddress busAddress;
  DaliComm::DaliDeviceInfoPtr deviceInfo;
public:
  static void readDeviceInfo(DaliComm *aDaliCommP, DaliComm::DaliDeviceInfoCB aResultCB, DaliAddress aAddress)
  {
    // create new instance, deletes itself when finished
    new DaliDeviceInfoReader(aDaliCommP, aResultCB, aAddress);
  };
private:
  DaliDeviceInfoReader(DaliComm *aDaliCommP, DaliComm::DaliDeviceInfoCB aResultCB, DaliAddress aAddress) :
    daliComm(aDaliCommP),
    callback(aResultCB),
    busAddress(aAddress)
  {
    // read the memory
    daliComm->daliReadMemory(boost::bind(&DaliDeviceInfoReader::handleBank0Data, this, _2, _3), busAddress, 0, 0, DALIMEM_BANK0_MINBYTES);
  };

  void handleBank0Data(DaliComm::MemoryVectorPtr aBank0Data, ErrorPtr aError)
  {
    deviceInfo.reset(new DaliDeviceInfo);
    deviceInfo->shortAddress = busAddress;
    if (aError)
      deviceInfoComplete(aError);
    else if (aBank0Data->size()==DALIMEM_BANK0_MINBYTES) {
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
        daliComm->daliReadMemory(boost::bind(&DaliDeviceInfoReader::handleBank0ExtraData, this, _2, _3), busAddress, 0, DALIMEM_BANK0_MINBYTES, extraBytes);
      }
      else {
        // directly continue by reading bank1
        readOEMInfo();
      }
    }
    else {
      // not enough bytes
      deviceInfoComplete(ErrorPtr(new DaliCommError(DaliCommErrorMissingData,string_format("Not enough bytes read from bank0 at shortAddress %d", busAddress))));
    }
  };

  void handleBank0ExtraData(DaliComm::MemoryVectorPtr aBank0Data, ErrorPtr aError)
  {
    if (aError)
      deviceInfoComplete(aError);
    else {
      // TODO: look at that data
      // now get OEM info
      readOEMInfo();
    }
  };


  void readOEMInfo()
  {
    daliComm->daliReadMemory(boost::bind(&DaliDeviceInfoReader::handleBank1Data, this, _2, _3), busAddress, 1, 0, DALIMEM_BANK1_MINBYTES);
  };

  void handleBank1Data(DaliComm::MemoryVectorPtr aBank1Data, ErrorPtr aError)
  {
    if (aError)
      deviceInfoComplete(aError);
    else if (aBank1Data->size()==DALIMEM_BANK1_MINBYTES) {
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
        daliComm->daliReadMemory(boost::bind(&DaliDeviceInfoReader::handleBank1ExtraData, this, _2, _3), busAddress, 0, DALIMEM_BANK1_MINBYTES, extraBytes);
      }
      else {
        // No extra bytes: device info is complete already
        deviceInfoComplete(aError);
      }
    }
    else {
      // No bank1 OEM info: device info is complete already (is not an error)
      deviceInfoComplete(aError);
    }
  };

  void handleBank1ExtraData(DaliComm::MemoryVectorPtr aBank1Data, ErrorPtr aError)
  {
    // TODO: look at that data
    // device info is complete
    deviceInfoComplete(aError);
  };


  void deviceInfoComplete(ErrorPtr aError)
  {
    callback(daliComm, deviceInfo, aError);
    // done, delete myself
    delete this;
  }
};


void DaliComm::daliReadDeviceInfo(DaliDeviceInfoCB aResultCB, DaliAddress aAddress)
{
  DaliDeviceInfoReader::readDeviceInfo(this, aResultCB, aAddress);
}



#pragma mark - Scan DALI bus by random address


class DaliFullBusScanner
{
  DaliComm *daliComm;
  DaliComm::DaliBusScanCB callback;
  uint32_t searchMax;
  uint32_t searchMin;
  uint32_t searchAddr;
  uint8_t searchL, searchM, searchH;
  bool setLMH;
  DaliComm::DeviceListPtr foundDevicesPtr;
public:
  static void fullScanBus(DaliComm *aDaliCommP, DaliComm::DaliBusScanCB aResultCB)
  {
    // create new instance, deletes itself when finished
    new DaliFullBusScanner(aDaliCommP, aResultCB);
  };
private:
  DaliFullBusScanner(DaliComm *aDaliCommP, DaliComm::DaliBusScanCB aResultCB) :
    daliComm(aDaliCommP),
    callback(aResultCB),
    foundDevicesPtr(new DaliComm::DeviceList)
  {
    // Terminate any special modes first
    daliComm->daliSend(DALICMD_TERMINATE, 0x00);
    // initialize entire system for random address selection process
    daliComm->daliSendTwice(DALICMD_INITIALISE, 0x00);
    daliComm->daliSendTwice(DALICMD_RANDOMISE, 0x00);
    // start search at lowest address
    newSearchUpFrom(0);
  };


  void newSearchUpFrom(uint32_t aMinSearch)
  {
    // init search range
    searchMax = 0xFFFFFF;
    searchMin = aMinSearch;
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
      daliComm->daliSend(DALICMD_SEARCHADDRH, searchH);
    }
    // - searchM
    by = (searchAddr>>8) & 0xFF;
    if (by!=searchM || setLMH) {
      searchM = by;
      daliComm->daliSend(DALICMD_SEARCHADDRM, searchM);
    }
    // - searchL
    by = (searchAddr) & 0xFF;
    if (by!=searchL || setLMH) {
      searchL = by;
      daliComm->daliSend(DALICMD_SEARCHADDRL, searchL);
    }
    setLMH = false; // incremental from now on until flag is set again
    // - issue the compare command
    daliComm->daliSendAndReceive(DALICMD_COMPARE, 0x00, boost::bind(&DaliFullBusScanner::handleCompareResult, this, _2, _3, _4));
  }


  void handleCompareResult(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    // Anything received but timeout is considered a yes
    bool isYes = !aNoOrTimeout;
    if (aError) {
      if (aError->isError(DaliCommError::domain(), DaliCommErrorDALIFrame)) {
        // framing error, can occur when multiple devices try to answer with YES with slightly different timing
        // -> consider this a YES
        isYes = true;
      }
      else {
        // other error, abort
        completed(aError);
        return;
      }
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
        DBGLOG(LOG_DEBUG, "No more devices\n");
        completed(NULL);
        return;
      }
      searchMin = searchAddr+1; // new min
    }
    if (searchMin==searchMax && searchAddr==searchMin) {
      // found!
      DBGLOG(LOG_DEBUG, "- Found device at 0x%06X\n", searchAddr);
      // read current short address
      daliComm->daliSendAndReceive(DALICMD_QUERY_SHORT_ADDRESS, 0x00, boost::bind(&DaliFullBusScanner::handleShortAddressQuery, this, _2, _3, _4));
    }
    else {
      // not yet - continue
      searchAddr = searchMin + (searchMax-searchMin)/2;
      DBGLOG(LOG_DEBUG, "-                                          New searchMin=0x%06X, searchMax=0x%06X\n", searchMin, searchMax);
      // issue next compare
      compareNext();
    }
  }


  void handleShortAddressQuery(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
  {
    if (aError) {
      completed(aError);
      return;
    }
    if (aNoOrTimeout) {
      DBGLOG(LOG_ERR, "- Device at 0x%06X does not respond to DALICMD_QUERY_SHORT_ADDRESS\n", searchAddr);
      // TODO: should not happen, probably bus error led to false device detection, restart search
    }
    else {
      // response is short address in 0AAAAAA1 format or DALIVALUE_MASK (no adress)
      if (aResponse==DALIVALUE_MASK) {
        // device has no short address yet
        DBGLOG(LOG_INFO, "- Device at 0x%06X has NO short address yet\n", searchAddr);
        // represent by 0xFF
        foundDevicesPtr->push_back(DALIVALUE_MASK);
        // TODO: assign new short address
      }
      else {
        DaliAddress shortAddress = DaliComm::addressFromDaliResponse(aResponse);
        DBGLOG(LOG_INFO, "- Device at 0x%06X has short address: %d\n", searchAddr, shortAddress);
        foundDevicesPtr->push_back(shortAddress);
        // TODO: check for duplicates and re-assign short address when needed to make unique
      }
    }
    // withdraw this device from further searches
    daliComm->daliSend(DALICMD_WITHDRAW, 0x00);
    // continue searching devices
    newSearchUpFrom(searchAddr+1);
  }



  void completed(ErrorPtr aError)
  {
    // terminate
    daliComm->daliSend(DALICMD_TERMINATE, 0x00);
    // callback
    callback(daliComm, foundDevicesPtr, aError);
    // done, delete myself
    delete this;
  }
};


void DaliComm::daliFullScanBus(DaliBusScanCB aResultCB)
{
  DaliFullBusScanner::fullScanBus(this, aResultCB);
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
  string s = string_format("DaliDeviceInfo for shortAddress %d\n", shortAddress);
  string_format_append(s, "- is %suniquely defining the device\n", uniquelyIdentifiing() ? "" : "NOT ");
  string_format_append(s, "- GTIN       : %lld\n", gtin);
  string_format_append(s, "- Firmware   : %d.%d\n", fw_version_major, fw_version_minor);
  string_format_append(s, "- Serial     : %lld\n", serialNo);
  string_format_append(s, "- OEM GTIN   : %lld\n", oem_gtin);
  string_format_append(s, "- OEM Serial : %lld\n", oem_serialNo);
  return s;
}


bool DaliDeviceInfo::uniquelyIdentifiing()
{
  return gtin!=0 && serialNo!=0;
}


#pragma mark - testing %%%



void DaliComm::test()
{
  reset(boost::bind(&DaliComm::resetAck, this, _2));
}


void DaliComm::resetAck(ErrorPtr aError)
{
  if (aError)
    printf("resetAck: error = %s\n", aError->description().c_str());
  else
    printf("resetAck: DALI bridge and bus reset OK\n");
  // next test
  test1();
}



void DaliComm::test1()
{
  sendBridgeCommand(0x10, 0x18, 0x00, boost::bind(&DaliComm::test1Ack, this, _2, _3, _4));
}

void DaliComm::test1Ack(uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)
{
  if (aError)
    printf("test1Ack: error = %s\n", aError->description().c_str());
  else
    printf("test1Ack: Resp1 = 0x%02X, Resp2 = 0x%02X\n", aResp1, aResp2);
  // next test
  test2();
}




void DaliComm::test2()
{
  // Befehl 160: YAAA AAA1 1010 0000 „QUERY ACTUAL LEVEL“
  daliSendAndReceive(0x0B, 0xA0, boost::bind(&DaliComm::test2Ack, this, _2, _3, _4));
}

void DaliComm::test2Ack(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  if (aError)
    printf("test2Ack: error = %s\n", aError->description().c_str());
  else
    printf("test2Ack: Timeout = %d, Response = 0x%02X\n", aNoOrTimeout, aResponse);
  // next test
  test3();
}




void DaliComm::test3()
{
  daliScanBus(boost::bind(&DaliComm::test3Ack, this, _2, _3));
}

void DaliComm::test3Ack(DeviceListPtr aDeviceListPtr, ErrorPtr aError)
{
  if (aError)
    printf("test3Ack: error = %s\n", aError->description().c_str());
  else {
    printf("test3Ack: %ld Devices found = ", aDeviceListPtr->size());
    for (list<DaliAddress>::iterator pos = aDeviceListPtr->begin(); pos!=aDeviceListPtr->end(); ++pos) {
      printf("%d ",*pos);
    }
    printf("\n");
  }
  // next test
  test4();
}



void DaliComm::test4()
{
  daliReadMemory(boost::bind(&DaliComm::test4Ack, this, _2, _3), 12, 0, 0, 15);
}

void DaliComm::test4Ack(MemoryVectorPtr aMemoryPtr, ErrorPtr aError)
{
  if (aError)
    printf("test4Ack: error = %s\n", aError->description().c_str());
  else {
    printf("test4Ack: %ld bytes read = ", aMemoryPtr->size());
    for (vector<uint8_t>::iterator pos = aMemoryPtr->begin(); pos!=aMemoryPtr->end(); ++pos) {
      printf("0x%02X ",*pos);
    }
    printf("\n");
  }
  // next test
  test5();
}


void DaliComm::test5()
{
  daliReadDeviceInfo(boost::bind(&DaliComm::test5Ack, this, _2, _3), 10);
}

void DaliComm::test5Ack(DaliDeviceInfoPtr aDeviceInfo, ErrorPtr aError)
{
  if (aError)
    printf("test5Ack: error = %s\n", aError->description().c_str());
  else
    printf("test5Ack: %s", aDeviceInfo->description().c_str());
}



void DaliComm::test6()
{
  daliFullScanBus(boost::bind(&DaliComm::test6Ack, this, _2, _3));
}

void DaliComm::test6Ack(DeviceListPtr aDeviceListPtr, ErrorPtr aError)
{
  if (aError)
    printf("test6Ack: error = %s\n", aError->description().c_str());
  else {
    printf("test6Ack: %ld Devices found = ", aDeviceListPtr->size());
    for (list<DaliAddress>::iterator pos = aDeviceListPtr->begin(); pos!=aDeviceListPtr->end(); ++pos) {
      printf("%d ",*pos);
    }
    printf("\n");
  }
}






