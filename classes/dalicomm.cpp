/*
 * dalicomm.cpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include "dalicomm.hpp"

#include <list>

#pragma mark - serial operations


class SerialOperation;

/// SerialOperation completion callback
typedef boost::function<void (SerialOperation *)> SerialOperationCompletedCB;


/// Serial operation
typedef boost::shared_ptr<SerialOperation> SerialOperationPtr;
class SerialOperation
{
  SerialOperationCompletedCB completedCallback;
public:
  /// if this flag is set, no operation queued after this operation will execute
  bool inSequence;
  /// set callback to execute when operation completes
  void setSerialOperationCB(SerialOperationCompletedCB aCallBack) { completedCallback = aCallBack; };
  /// call to initiate operation
  /// @return false if cannot be initiated now and must be retried
  virtual bool initiate() { return true; }; // NOP
  /// call to deliver received bytes
  /// @return number of bytes operation could accept, 0 if none
  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes) { return 0; };  
  /// call to check if operation has completed
  /// @return true if completed
  virtual bool hasCompleted() { return 0; };
};



/// SerialOperation completion callback
typedef boost::function<size_t(size_t aNumBytes, uint8_t *aBytes)> SerialOperationTransmitter;

/// Send operation
class SerialOperationSend : SerialOperation
{
  typedef SerialOperation inherited;

  SerialOperationTransmitter transmitter;
  size_t dataSize;
  uint8_t *dataP;
public:

  SerialOperationSend(size_t aNumBytes, uint8_t *aBytes) {
    // copy data
    dataP = NULL;
    dataSize = aNumBytes;
    if (dataSize>0) {
      dataP = (uint8_t *)malloc(dataSize);
      memcpy(dataP, aBytes, dataSize);
    }
  };

  void setTransmitter(SerialOperationTransmitter aTransmitter)
  {
    // remember transmitter
    transmitter = aTransmitter;
  }

  virtual ~SerialOperationSend() {
    if (dataP) {
      free(dataP);
    }
  }

  virtual bool execute() {
    size_t res;
    if (dataP && transmitter) {
      // transmit
      res = transmitter(dataSize,dataP);
      // early release
      free(dataP);
      dataP = NULL;
    }
    // executed
    return true;
  }
};


/// receive operation
class SerialOperationReceive : SerialOperation
{
  typedef SerialOperation inherited;

  size_t expectedBytes;
  uint8_t *dataP;
  size_t dataIndex;

public:

  SerialOperationReceive(size_t aExpectedBytes)
  {
    // allocate buffer
    expectedBytes = aExpectedBytes;
    dataP = (uint8_t *)malloc(expectedBytes);
    dataIndex = 0;
  };

  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes)
  {
    // append bytes into buffer
    if (aNumBytes>expectedBytes) aNumBytes = expectedBytes;
    if (aNumBytes>0) {
      memcpy(dataP+dataIndex, aBytes, aNumBytes);
      dataIndex += aNumBytes;
      expectedBytes -= aNumBytes;
    }
    // return number of bytes actually accepted
    return aNumBytes;
  }

  virtual bool hasCompleted()
  {
    // %%%
  };

};


#pragma mark - serial operation queue


class SerialOperationQueue
{
  typedef list<SerialOperationPtr> operationQueue_t;
  operationQueue_t operationQueue;

public:

  /// queue a new operation
  /// @param aOperation the operation to execute
  void queueOperation(SerialOperationPtr aOperation)
  {
    operationQueue.push_back(aOperation);
  }

  /// deliver bytes to the most recent waiting operation
  size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes)
  {
    // let operations receive bytes
    size_t acceptedBytes = 0;
    for (operationQueue_t::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
      size_t consumed = (*pos)->acceptBytes(aNumBytes, aBytes);
      aBytes += consumed; // advance pointer
      aNumBytes -= consumed; // count
      acceptedBytes += consumed;
      if (aNumBytes<=0)
        break; // all bytes consumed
    }
    if (aNumBytes>0) {
      // Still bytes left to accept
      // TODO: possibly create "unexpected receive" operation
    }
    // check if some operations might be complete now
    processOperations();
    // return number of accepted bytes
    return acceptedBytes;
  };

  /// process operations now
  void processOperations()
  {
    for (operationQueue_t::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
      

    }
  }


};





#pragma mark - DaliComm

// pseudo baudrate for dali bridge must be 9600bd
#define BAUDRATE B9600


DaliComm::DaliComm(const char* aBridgeConnectionPath, uint16_t aPortNo) :
  bridgeConnectionPath(aBridgeConnectionPath),
  bridgeConnectionPort(aPortNo),
  bridgeConnectionOpen(false)
{
  bridgeConnectionPath = aBridgeConnectionPath;
  bridgeConnectionPort = aPortNo;
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
    // 
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
#define ACK_OK EQU 0x30 // ok status
#define ACK_TIMEOUT EQU 0x31 // timeout receiving from DALI
#define ACK_FRAME_ERR EQU 0x32 // rx frame error
#define ACK_INVALIDCMD EQU 0x39 // invalid command

// pseudo response (generated when connection not working)
#define RESP_CODE_BRIDGECOMMERROR 0x58



void DaliComm::sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB)
{
  size_t res = 0;
  if (establishConnection()) {
    if (aCmd<8) {
      // single byte command
      res = write(bridgeFd,&aCmd,1);
    }
    else {
      // 3 byte command
      uint8_t cmd3[3];
      cmd3[0] = aCmd;
      cmd3[1] = aDali1;
      cmd3[2] = aDali2;
      res = write(bridgeFd,&cmd3,3);
    }
    // save callback for execution when response arrives
    pendingBridgeResultCallback = aResultCB;
  }
  else {
    // error, generate pseudo result immediately
    aResultCB(this,RESP_CODE_BRIDGECOMMERROR,0);
  }
}


void DaliComm::ackAllOn(DaliComm *aDaliComm, uint8_t aResp1, uint8_t aResp2)
{
  printf("ackAllOn: Resp1 = 0x%02X, Resp2 = 0x%02X\n", aResp1, aResp2);
}


void DaliComm::allOn()
{
  sendBridgeCommand(0x10, 0xFE, 0xFE, boost::bind(&DaliComm::ackAllOn, this, _1, _2, _3));
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














