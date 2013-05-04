//
//  serialcomm.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__serialcomm__
#define __p44bridged__serialcomm__

#include "p44bridged_common.hpp"

#include "serialqueue.hpp"

// unix I/O and network
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>


using namespace std;

namespace p44 {

  // Errors
  typedef enum {
    SerialCommErrorOK,
    SerialCommErrorSocketOpen,
    SerialCommErrorInvalidHost,
    SerialCommErrorSerialOpen,
    SerialCommErrorUnknownBaudrate,
  } SerialCommErrors;

  class SerialCommError : public Error
  {
  public:
    static const char *domain() { return "SerialComm"; }
    SerialCommError(SerialCommErrors aError) : Error(ErrorCode(aError)) {};
    SerialCommError(SerialCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
    virtual const char *getErrorDomain() const { return SerialCommError::domain(); };
  };



  class SerialComm;
  typedef boost::shared_ptr<SerialComm> SerialCommPtr;

  /// A class providing low level access to the DALI bus
  class SerialComm : public SerialOperationQueue
  {
    typedef SerialOperationQueue inherited;

    // connection to the bridge
    string connectionPath;
    uint16_t connectionPort;
    int baudRate;
    bool connectionOpen;
    int connectionFd;
    struct termios oldTermIO;
    bool serialConnection;
  protected:
    ErrorPtr unhandledError;
  public:

    SerialComm(SyncIOMainLoop *aMainLoopP);
    virtual ~SerialComm();

    /// Set the serial connection parameters
    /// @param aConnectionPath serial device path (/dev/...) or host name/address (1.2.3.4 or xxx.yy)
    /// @param aPortNo port number for TCP connection (irrelevant for direct serial device connection)
    /// @param aBaudRate baud rate for serial connection (irrelevant for TCP connection)
    void setConnectionParameters(const char* aConnectionPath, uint16_t aPortNo, int aBaudRate);

    /// transmit data
    size_t transmitBytes(size_t aNumBytes, const uint8_t *aBytes);

    /// receive data
    size_t receiveBytes(size_t aMaxBytes, uint8_t *aBytes);


    /// establish the connection to the DALI bridge
    /// @note can be called multiple times, opens connection only if not already open
    bool establishConnection();

    /// close the current connection, if any
    void closeConnection();

    /// set DALI bus error not handled by callback
    void setUnhandledError(ErrorPtr aError);

    /// get last unhandled error and clear it
    ErrorPtr getLastUnhandledError();

  };

} // namespace p44

#endif /* defined(__p44bridged__serialcomm__) */
