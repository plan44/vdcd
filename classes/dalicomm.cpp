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
class SerialOperationQueue;

/// SerialOperation completion callback
typedef boost::function<void (SerialOperation *, SerialOperationQueue *)> SerialOperationFinalizeCB;

/// SerialOperation transmitter
typedef boost::function<size_t(size_t aNumBytes, uint8_t *aBytes)> SerialOperationTransmitter;


/// Serial operation
typedef boost::shared_ptr<SerialOperation> SerialOperationPtr;
class SerialOperation
{
protected:
  SerialOperationFinalizeCB finalizeCallback;
  SerialOperationTransmitter transmitter;
  bool initiated;
public:
  /// if this flag is set, no operation queued after this operation will execute
  bool inSequence;
  /// constructor
  SerialOperation() { initiated = false; inSequence = true; };
  /// set transmitter
  void setTransmitter(SerialOperationTransmitter aTransmitter) { transmitter = aTransmitter; }
  /// set callback to execute when operation completes
  void setSerialOperationCB(SerialOperationFinalizeCB aCallBack) { finalizeCallback = aCallBack; };
  /// call to initiate operation
  /// @return false if cannot be initiated now and must be retried
  virtual bool initiate() { initiated=true; return initiated; }; // NOP
  /// check if already initiated
  bool isInitiated() { return initiated; }
  /// call to deliver received bytes
  /// @return number of bytes operation could accept, 0 if none
  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes) { return 0; };  
  /// call to check if operation has completed
  /// @return true if completed
  virtual bool hasCompleted() { return true; };
  /// call to execute after completion
  virtual void finalize(SerialOperationQueue *aQueueP = NULL) {
    if (finalizeCallback) {
      finalizeCallback(this,aQueueP);
    }
  }
};




/// Send operation
class SerialOperationSend : public SerialOperation
{
  typedef SerialOperation inherited;

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

  virtual ~SerialOperationSend() {
    if (dataP) {
      free(dataP);
    }
  }

  virtual bool initiate() {
    size_t res;
    if (dataP && transmitter) {
      // transmit
      res = transmitter(dataSize,dataP);
      // early release
      free(dataP);
      dataP = NULL;
    }
    // executed
    return inherited::initiate();
  }
};
typedef boost::shared_ptr<SerialOperationSend> SerialOperationSendPtr;


/// receive operation
class SerialOperationReceive : public SerialOperation
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

  uint8_t *getDataP() { return dataP; };
  size_t getDataSize() { return dataIndex; };

  virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes)
  {
    // append bytes into buffer
    if (!initiated)
      return 0; // cannot accept bytes when not yet initiated
    if (aNumBytes>expectedBytes)
      aNumBytes = expectedBytes;
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
    // completed if all expected bytes received
    return expectedBytes<=0;
  };

};
typedef boost::shared_ptr<SerialOperationReceive> SerialOperationReceivePtr;





#pragma mark - serial operation queue


class SerialOperationQueue
{
  typedef list<SerialOperationPtr> operationQueue_t;
  operationQueue_t operationQueue;
  SerialOperationTransmitter transmitter;

public:

  SerialOperationQueue(SerialOperationTransmitter aTransmitter) : transmitter(aTransmitter) {};

  /// queue a new operation
  /// @param aOperation the operation to queue
  void queueOperation(SerialOperationPtr aOperation)
  {
    aOperation->setTransmitter(transmitter);
    operationQueue.push_back(aOperation);
  }

  /// insert a new operation before other pending operations
  /// @param aOperation the operation to insert
  void insertOperation(SerialOperationPtr aOperation)
  {
    aOperation->setTransmitter(transmitter);
    // TODO: we might need checks for inserting in the right place (after already initiated)
    operationQueue.push_front(aOperation);
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
    bool processed = false;
    while (!processed) {
      operationQueue_t::iterator pos;
      // (re)start with first element in queue
      for (pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
        SerialOperationPtr op = *pos;
        if (!op->isInitiated()) {
          // initiate now
          if (!op->initiate()) {
            // cannot initiate this one now, continue further down into queue if sequence is not important
            if (op->inSequence) {
              // this op needs to be initiated before others can be checked
              processed = true; // something must happen outside this routine to change the state of the op, so done for now
              break;
            }
          }
        }
        else {
          // initiated, check if already completed
          if (op->hasCompleted()) {
            // operation has completed
            // - remove from list
            operationQueue.pop_front();
            // - finalize. This might push new operations in front or back of the queue
            op->finalize(this);
            // restart with start of (modified) queue
            break;
          }
          else {
            // operation has not yet completed
            if (op->inSequence) {
              // this op needs to be complete before others can be checked
              processed = true; // something must happen outside this routine to change the state of the op, so done for now
              break;
            }
          }
        }
      } // for all ops in queue
      if (pos==operationQueue.end()) processed = true; // if seen all, we're done for now as well
    } // while not processed
  };

};



/// send operation which automatically inserts a receive operation after completion
class SerialOperationSendAndReceive : public SerialOperationSend
{
  typedef SerialOperationSend inherited;

  size_t expectedBytes;

public:

  SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes) :
  inherited(aNumBytes, aBytes),
  expectedBytes(aExpectedBytes)
  { };

  virtual void finalize(SerialOperationQueue *aQueueP = NULL)
  {
    if (aQueueP) {
      // insert receive operation
      SerialOperationPtr op(new SerialOperationReceive(expectedBytes));
      op->setSerialOperationCB(finalizeCallback); // inherit completion callback
      aQueueP->insertOperation(op);
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


#pragma mark - transmitter

void DaliComm::transmitBytes(size_t aNumBytes, uint8_t *aBytes)
{
  size_t res = 0;
  if (establishConnection()) {
    res = write(bridgeFd,aBytes,aNumBytes);
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



class BridgeResponseHandler
{
  DaliBridgeResultCB callback;
  DaliComm *daliComm;
public:
  BridgeResponseHandler(DaliBridgeResultCB aResultCB, DaliComm *aDaliCommP) { callback = aResultCB; daliComm = aDaliCommP; };
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
  BridgeResponseHandler handler(aResultCB,this);
  if (aCmd<8) {
    // single byte command
    // %%% tbd
  }
  else {
    // 3 byte command
    uint8_t cmd3[3];
    cmd3[0] = aCmd;
    cmd3[1] = aDali1;
    cmd3[2] = aDali2;
    // %%% tbd
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














