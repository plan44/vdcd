//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0


#include "serialqueue.hpp"

using namespace p44;


#define DEFAULT_RECEIVE_TIMEOUT (3*Second) // [uS] = 3 seconds


#pragma mark - SerialOperation


SerialOperation::SerialOperation(SerialOperationFinalizeCB aCallback) :
  callback(aCallback)
{
}

// set transmitter
void SerialOperation::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


// call to deliver received bytes
ssize_t SerialOperation::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  return 0; // accept none, expect none
}


OperationPtr SerialOperation::finalize(OperationQueue *aQueueP)
{
  if (callback) {
    callback(SerialOperationPtr(this), aQueueP, ErrorPtr());
    callback = NULL; // call once only
  }
  return OperationPtr(); // no operation to insert
}


void SerialOperation::abortOperation(ErrorPtr aError)
{
  if (callback && !aborted) {
    aborted = true;
    callback(SerialOperationPtr(this), NULL, aError);
    callback = NULL; // call once only
  }
}




#pragma mark - SerialOperationSend


SerialOperationSend::SerialOperationSend(size_t aNumBytes, uint8_t *aBytes, SerialOperationFinalizeCB aCallback) :
  inherited(aCallback),
  dataP(NULL)
{
  // copy data
  setDataSize(aNumBytes);
  appendData(aNumBytes, aBytes);
}


SerialOperationSend::~SerialOperationSend()
{
  clearData();
}




void SerialOperationSend::clearData()
{
  if (dataP) {
    delete [] dataP;
    dataP = NULL;
  }
  dataSize = 0;
  appendIndex = 0;
}


void SerialOperationSend::setDataSize(size_t aDataSize)
{
  clearData();
  if (aDataSize>0) {
    dataSize = aDataSize;
    dataP = new uint8_t[dataSize];
  }
}


void SerialOperationSend::appendData(size_t aNumBytes, uint8_t *aBytes)
{
  if (appendIndex+aNumBytes>dataSize)
    aNumBytes = dataSize-appendIndex;
  if (aNumBytes>0) {
    memcpy(dataP, aBytes, aNumBytes);
  }
}




bool SerialOperationSend::initiate()
{
  if (!canInitiate()) return false;
  FOCUSLOG("SerialOperationSend::initiate: sending %d bytes now\n", dataSize);
  size_t res;
  if (dataP && transmitter) {
    // transmit
    res = transmitter(dataSize,dataP);
    if (res!=dataSize) {
      // error
      abortOperation(ErrorPtr(new SQError(SQErrorTransmit)));
    }
    // early release
    clearData();
  }
  // executed
  return inherited::initiate();
}



#pragma mark - SerialOperationReceive


SerialOperationReceive::SerialOperationReceive(size_t aExpectedBytes, SerialOperationFinalizeCB aCallback) :
  inherited(aCallback)
{
  // allocate buffer
  expectedBytes = aExpectedBytes;
  dataP = new uint8_t[expectedBytes];
  dataIndex = 0;
  setTimeout(DEFAULT_RECEIVE_TIMEOUT);
};


SerialOperationReceive::~SerialOperationReceive()
{
  clearData();
}



void SerialOperationReceive::clearData()
{
  if (dataP) {
    delete [] dataP;
    dataP = NULL;
  }
  expectedBytes = 0;
  dataIndex = 0;
}


ssize_t SerialOperationReceive::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
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


bool SerialOperationReceive::hasCompleted()
{
  // completed if all expected bytes received
  return expectedBytes<=0;
}


void SerialOperationReceive::abortOperation(ErrorPtr aError)
{
  clearData(); // don't expect any more, early release
  inherited::abortOperation(aError);
}


#pragma mark - SerialOperationSendAndReceive


SerialOperationSendAndReceive::SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes, SerialOperationFinalizeCB aCallback) :
  inherited(aNumBytes, aBytes, aCallback),
  expectedBytes(aExpectedBytes),
  answersInSequence(true), // by default, answer must arrive until next send can be initiated
  receiveTimeoout(DEFAULT_RECEIVE_TIMEOUT)
{
};


OperationPtr SerialOperationSendAndReceive::finalize(OperationQueue *aQueueP)
{
  if (aQueueP) {
    // insert receive operation
    SerialOperationPtr op(new SerialOperationReceive(expectedBytes, callback)); // inherit completion callback
    callback = NULL; // prevent it to be called from this object!
    op->inSequence = answersInSequence; // if false, further sends might be started before answer received
    op->setTimeout(receiveTimeoout); // set receive timeout
    return op;
  }
  return inherited::finalize(aQueueP); // default
}


#pragma mark - SerialOperationQueue


// Link into mainloop
SerialOperationQueue::SerialOperationQueue(MainLoop &aMainLoop) :
  inherited(aMainLoop),
  acceptBufferP(NULL),
  acceptBufferSize(0),
  bufferedBytes(0)
{
  // Set handlers for FdComm
  serialComm = SerialCommPtr(new SerialComm(aMainLoop));
  serialComm->setReceiveHandler(boost::bind(&SerialOperationQueue::receiveHandler, this, _1));
  // TODO: once we implement buffered write, install the ready-for-transmission handler here
  //serialComm.setTransmitHandler(boost::bind(&SerialOperationQueue::transmitHandler, this, _1));
  // Set standard transmitter and receiver for operations
  setTransmitter(boost::bind(&SerialOperationQueue::standardTransmitter, this, _1, _2));
	setReceiver(boost::bind(&SerialOperationQueue::standardReceiver, this, _1, _2));
}


SerialOperationQueue::~SerialOperationQueue()
{
  serialComm->closeConnection();
  setAcceptBuffer(0);
}


// set transmitter
void SerialOperationQueue::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


// set receiver
void SerialOperationQueue::setReceiver(SerialOperationReceiver aReceiver)
{
  receiver = aReceiver;
}


#define RECBUFFER_SIZE 100


// handles incoming data from serial interface
void SerialOperationQueue::receiveHandler(ErrorPtr aError)
{
  if (receiver) {
    uint8_t buffer[RECBUFFER_SIZE];
    size_t numBytes = receiver(RECBUFFER_SIZE, buffer);
    FOCUSLOG("SerialOperationQueue::receiveHandler: got %d bytes to accept\n", numBytes);
    if (numBytes>0) {
      acceptBytes(numBytes, buffer);
    }
  }
}


// queue a new serial I/O operation
void SerialOperationQueue::queueSerialOperation(SerialOperationPtr aOperation)
{
  aOperation->setTransmitter(transmitter);
  inherited::queueOperation(aOperation);
}



void SerialOperationQueue::setAcceptBuffer(size_t aBufferSize)
{
  if (acceptBufferP) {
    delete [] acceptBufferP;
    acceptBufferP = NULL;
  }
  bufferedBytes = 0;
  acceptBufferSize = 0;
  if (aBufferSize>0) {
    acceptBufferSize = aBufferSize;
    acceptBufferP = new uint8_t[acceptBufferSize];
  }
}



// deliver bytes to the most recent waiting operation
size_t SerialOperationQueue::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  FOCUSLOG("Start of SerialOperationQueue::acceptBytes: received %d new bytes to accept\n", aNumBytes);
  // first check if some operations still need processing
  size_t acceptedBytes = 0;
  uint8_t *bytes;
  size_t numBytes;
  while (aNumBytes>0) {
    FOCUSLOG("- %d bytes left to process\n", aNumBytes);
    // buffered mode?
    if (acceptBufferSize>0) {
      // buffered mode - collect in buffer and then let operations process
      ssize_t by = acceptBufferSize - bufferedBytes;
      if (by>0) {
        // still room in the buffer, append to buffer
        if (aNumBytes<by) by = aNumBytes; // buffer at most aNumBytes
        memcpy(acceptBufferP+bufferedBytes, aBytes, by); // buffer
        bufferedBytes += by;
        aNumBytes -= by;
        aBytes += by;
        FOCUSLOG("- %d bytes buffered, %d total buffered, %d remaining\n", by, bufferedBytes, aNumBytes);
      }
      else {
        // buffer full, cannot store more
        LOG(LOG_DEBUG, "- %d received bytes could neither be processed nor buffered -> ignored\n", aNumBytes);
        break; // no point in iterating
      }
      // initiate processing on buffered data
      bytes = acceptBufferP;
      numBytes = bufferedBytes;
    }
    else {
      // unbuffered, directly process incoming data
      bytes = aBytes;
      numBytes = aNumBytes;
      aNumBytes = 0; // all must be consumed or are lost
    }
    // let operations process bytes now
    if (FOCUSLOGENABLED) {
      string s;
      for (size_t i=0; i<numBytes; i++) {
        string_format_append(s, " %02X", bytes[i]);
      }
      FOCUSLOG("- attempting to process %d bytes: %s\n", numBytes, s.c_str());
    }
    ssize_t consumed = 0;
    for (OperationList::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
      FOCUSLOG("- offering %d bytes to next operation to accept\n", numBytes);
      SerialOperationPtr sop = boost::dynamic_pointer_cast<SerialOperation>(*pos);
      if (sop) {
        consumed = sop->acceptBytes(numBytes, bytes);
        FOCUSLOG("- operation accepted %d bytes (-1: not enough to process anything)\n", consumed);
      }
      if (consumed==NOT_ENOUGH_BYTES) {
        FOCUSLOG("- operation will accept bytes, but needs more at a time -> don't process more\n");
        break; // this operation would accept bytes, but needs more of them at a time
      }
      bytes += consumed; // advance pointer
      numBytes -= consumed; // count
      acceptedBytes += consumed;
      if (numBytes<=0)
        break; // all bytes consumed
    }
    if (numBytes>0 && consumed!=NOT_ENOUGH_BYTES) {
      // Still bytes left to accept, give chance to process these now
      FOCUSLOG("- %d left after all pending operations asked, offer them to acceptExtraBytes()\n", numBytes);
      consumed = acceptExtraBytes(numBytes, bytes);
      FOCUSLOG("- acceptExtraBytes() accepted %d bytes (-1: not enough to process anything)\n", consumed);
      if (consumed!=NOT_ENOUGH_BYTES) {
        bytes += consumed; // advance pointer
        numBytes -= consumed; // count
        acceptedBytes += consumed;
      }
    }
    // buffered mode?
    if (acceptBufferSize>0) {
      // in buffered mode, remove accepted bytes and keep rest for next run
      bufferedBytes = numBytes;
      if (numBytes>0) {
        // still bytes left unprocessed, move them to the beginning of the buffer
        // Note: in buffered mode, numBytes is always less or equal acceptBufferSize, so we know we can move the rest
        memmove(acceptBufferP, bytes, numBytes);
      }
    }
    else {
      // unbuffered mode - bytes than cannot be processed are lost
      if (numBytes>0) {
        FOCUSLOG("SerialOperationQueue::acceptBytes - %d unprocessed bytes -> ignored\n", aNumBytes);
      }
      break;
    }
  } // while bytes to process
  // final check if some operations might be complete now
  processOperations();
  // return number of accepted bytes
  FOCUSLOG("End of SerialOperationQueue::acceptBytes: accepted %d bytes\n", acceptedBytes);
  return acceptedBytes;
};


#pragma mark - standard transmitter and receivers




size_t SerialOperationQueue::standardTransmitter(size_t aNumBytes, const uint8_t *aBytes)
{
  FOCUSLOG("SerialOperationQueue::standardTransmitter(%d bytes) called\n", aNumBytes);
  ssize_t res = 0;
  ErrorPtr err = serialComm->establishConnection();
  if (Error::isOK(err)) {
    res = serialComm->transmitBytes(aNumBytes, aBytes, err);
    if (!Error::isOK(err)) {
      FOCUSLOG("Error writing serial: %s\n", err->description().c_str());
      res = 0; // none written
    }
    else {
      if (FOCUSLOGENABLED) {
        std::string s;
        for (ssize_t i=0; i<res; i++) {
          string_format_append(s, "%02X ",aBytes[i]);
        }
        FOCUSLOG("Transmitted %d bytes: %s\n", res, s.c_str());
      }
      else {
        FOCUSLOG("Transmitted %d bytes\n", res);
      }
    }
  }
  else {
    LOG(LOG_DEBUG, "SerialOperationQueue::standardTransmitter error - connection could not be established!\n");
  }
  return res;
}



size_t SerialOperationQueue::standardReceiver(size_t aMaxBytes, uint8_t *aBytes)
{
  FOCUSLOG("SerialOperationQueue::standardReceiver(%d bytes) called\n", aMaxBytes);
  size_t gotBytes = 0;
  if (serialComm->connectionIsOpen()) {
		// get number of bytes available
    ErrorPtr err;
    gotBytes = serialComm->receiveBytes(aMaxBytes, aBytes, err);
    if (!Error::isOK(err)) {
      FOCUSLOG("- Error reading serial: %s\n", err->description().c_str());
      return 0;
    }
    else {
      if (FOCUSLOGENABLED) {
        if (gotBytes>0) {
          std::string s;
          for (size_t i=0; i<gotBytes; i++) {
            string_format_append(s, "%02X ",aBytes[i]);
          }
          FOCUSLOG("- Received %d bytes: %s\n", gotBytes, s.c_str());
        }
      }
      else {
        FOCUSLOG("Received %d bytes\n", gotBytes);
      }
    }
  }
  else {
    LOG(LOG_DEBUG, "SerialOperationQueue::standardReceiver error - connection is not open!\n");
  }
  return gotBytes;
}



