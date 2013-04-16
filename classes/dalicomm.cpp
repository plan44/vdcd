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


DaliComm::DaliComm(const char* aBridgeConnectionPath, uint16_t aPortNo) :
  bridgeConnectionPath(aBridgeConnectionPath),
  bridgeConnectionPort(aPortNo),
  bridgeConnectionOpen(false)
{
  setTransmitter(boost::bind(&DaliComm::transmitBytes, this, _1, _2));
}


DaliComm::~DaliComm()
{
  closeConnection();
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
      acceptBytes(1, &byte);
    }
  }
}


#pragma mark - serial connection to the DALI bridge


size_t DaliComm::transmitBytes(size_t aNumBytes, uint8_t *aBytes)
{
  size_t res = 0;
  if (establishConnection()) {
    res = write(bridgeFd,aBytes,aNumBytes);
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
        return false;
      }
      memcpy((char *)&conn_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
      if ((res = connect(bridgeFd, (struct sockaddr *)&conn_addr, sizeof(conn_addr))) < 0) {
        LOGERRNO(LOG_ERR);
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
  }
}


void DaliComm::reportDaliStatus(DaliStatus aStatus)
{
  if (aStatus!=DaliStatusOK && aStatus!=DaliStatusNoOrTimeout) {
    DBGLOG(LOG_ERR,"DaliComm: Not-OK DALI status 0x%02X reported\n", aStatus);
    // TODO: depending on type of error, initiate recovery (reconnect to bridge, reset bridge, etc.)
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

// pseudo response (generated when connection not working)
#define RESP_CODE_BRIDGECOMMERROR 0x58



class BridgeResponseHandler
{
  DaliComm::DaliBridgeResultCB callback;
  DaliComm *daliComm;
public:
  BridgeResponseHandler(DaliComm *aDaliCommP, DaliComm::DaliBridgeResultCB aResultCB) :
    daliComm(aDaliCommP),
    callback(aResultCB)
  {};
  void operator() (SerialOperation *aOpP, SerialOperationQueue *aQueueP) {
    SerialOperationReceive *ropP = dynamic_cast<SerialOperationReceive *>(aOpP);
    if (ropP) {
      // get received data
      if (ropP->getDataSize()>=2) {
        uint8_t resp1 = ropP->getDataP()[0];
        uint8_t resp2 = ropP->getDataP()[1];
        if (callback)
          callback(daliComm, resp1, resp2);
      }
    }
  };
};


void DaliComm::sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB)
{
  BridgeResponseHandler handler(this, aResultCB);
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
  DaliStatus statusFromBridgeResponse(uint8_t aResp1, uint8_t aResp2)
  {
    DBGLOG(LOG_DEBUG,"DALI bridge response: 0x%02X 0x%02X\n", aResp1, aResp2);
    switch(aResp1) {
      case RESP_CODE_ACK:
        // command acknowledged
        switch (aResp2) {
          case ACK_OK: return DaliStatusOK;
          case ACK_TIMEOUT: return DaliStatusNoOrTimeout;
          case ACK_FRAME_ERR: return DaliStatusFrameError;
          case ACK_INVALIDCMD: return DaliStatusFrameError;
          default: return DaliStatusBridgeUnknown;
        }
        break;
      case RESP_CODE_BRIDGECOMMERROR:
        return DaliStatusBridgeCommError;
      case RESP_CODE_DATA:
        return DaliStatusOK;
      default: return DaliStatusBridgeUnknown;
    }
    return DaliStatusBridgeUnknown;
  }
public:
  DaliCommandStatusHandler(DaliComm *aDaliCommP, DaliComm::DaliCommandStatusCB aResultCB) :
    daliComm(aDaliCommP),
    callback(aResultCB)
  { };
  void operator() (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2)
  {
    DaliStatus sta = statusFromBridgeResponse(aResp1, aResp2);
    // execute callback if any
    if (callback)
      callback(daliComm, sta);
    // anyway report status to queue object
    daliComm->reportDaliStatus(sta);
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

  void operator() (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2)
  {
    DaliStatus sta = statusFromBridgeResponse(aResp1, aResp2);
    // execute callback if any
    if (callback)
      callback(daliComm, sta, aResp2);
    // anyway report status to queue object
    daliComm->reportDaliStatus(sta);
  };
};


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
  void handleResponse(DaliStatus aStatus, uint8_t aResponse)
  {
    if (aStatus==DaliStatusOK && aResponse==DALIANSWER_YES) {
      activeDevicesPtr->push_back(busAddress);
    }
    busAddress++;
    if (busAddress<DALI_MAXDEVICES) {
      // more devices to scan
      queryNext();
    }
    else {
      // scan done, return list to callback
      callback(daliComm, activeDevicesPtr);
      // done, delete myself
      delete this;
    }
  };

  // query next device
  void queryNext()
  {
    daliComm->daliSendQuery(busAddress, DALICMD_QUERY_CONTROL_GEAR, boost::bind(&DaliBusScanner::handleResponse, this, _2, _3));
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
  void handleResponse(DaliStatus aStatus, uint8_t aResponse)
  {
    if (aStatus==DaliStatusOK) {
      // byte received, append to vector
      memory->push_back(aResponse);
      if (--bytesToRead>0) {
        // more bytes to read
        readNextByte();
        return;
      }
    }
    // all bytes read or read error
    if (bytesToRead>0) {
      // not all bytes received, read error
      memory->clear(); // consider entire read invalid
    }
    // read done, return memory to callback
    callback(daliComm, memory);
    // done, delete myself
    delete this;
  };

  void readNextByte()
  {
    daliComm->daliSendQuery(busAddress, DALICMD_READ_MEMORY_LOCATION, boost::bind(&DaliMemoryReader::handleResponse, this, _2, _3));
  }
};


void DaliComm::daliReadMemory(DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes)
{
  DaliMemoryReader::readMemory(this, aResultCB, aAddress, aBank, aOffset, aNumBytes);
}



// read device info of a DALI device

class DaliDeviceInfoReader
{
  // TODO: %%% implement
};



#pragma mark - DALI device info

string DaliDeviceInfo::description()
{
  // TODO: %%% real result
  return string("Not yet");
}




#pragma mark - testing %%%

void DaliComm::test()
{
  // first test
  test1();
}


void DaliComm::test1()
{
  sendBridgeCommand(0x10, 0x18, 0x00, boost::bind(&DaliComm::test1Ack, this, _1, _2, _3));
}

void DaliComm::test1Ack(DaliComm *aDaliComm, uint8_t aResp1, uint8_t aResp2)
{
  printf("test1Ack: Resp1 = 0x%02X, Resp2 = 0x%02X\n", aResp1, aResp2);
  // next test
  test2();
}




void DaliComm::test2()
{
  // Befehl 160: YAAA AAA1 1010 0000 „QUERY ACTUAL LEVEL“
  daliSendAndReceive(0x0B, 0xA0, boost::bind(&DaliComm::test2Ack, this, _2, _3));
}

void DaliComm::test2Ack(DaliStatus aStatus, uint8_t aResponse)
{
  printf("test2Ack: Status = %d, Response = 0x%02X\n", aStatus, aResponse);
  // next test
  test3();
}




void DaliComm::test3()
{
  daliScanBus(boost::bind(&DaliComm::test3Ack, this, _2));
}

void DaliComm::test3Ack(DeviceListPtr aDeviceListPtr)
{
  printf("test3Ack: %ld Devices found = ", aDeviceListPtr->size());
  for (list<DaliAddress>::iterator pos = aDeviceListPtr->begin(); pos!=aDeviceListPtr->end(); ++pos) {
    printf("%d ",*pos);
  }
  printf("\n");
  // next test
  test4();
}



void DaliComm::test4()
{
  daliReadMemory(boost::bind(&DaliComm::test4Ack, this, _2),12,0,0,15);
}

void DaliComm::test4Ack(MemoryVectorPtr aMemoryPtr)
{
  printf("test4Ack: %ld bytes read = ", aMemoryPtr->size());
  for (vector<uint8_t>::iterator pos = aMemoryPtr->begin(); pos!=aMemoryPtr->end(); ++pos) {
    printf("0x%02X ",*pos);
  }
  printf("\n");
}








