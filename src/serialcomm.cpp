//
//  serialcomm.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "serialcomm.hpp"

#include <sys/ioctl.h>

using namespace p44;


SerialComm::SerialComm(SyncIOMainLoop *aMainLoopP) :
	inherited(aMainLoopP),
  connectionPort(0),
  baudRate(9600),
  connectionOpen(false)
{
  setTransmitter(boost::bind(&SerialComm::transmitBytes, this, _1, _2));
	setReceiver(boost::bind(&SerialComm::receiveBytes, this, _1, _2));
}


SerialComm::~SerialComm()
{
  closeConnection();
}



void SerialComm::setConnectionParameters(const char* aConnectionPath, uint16_t aPortNo, int aBaudRate)
{
  closeConnection();
	connectionPath = nonNullCStr(aConnectionPath);
  connectionPort = aPortNo;
  baudRate = aBaudRate;
}




size_t SerialComm::transmitBytes(size_t aNumBytes, const uint8_t *aBytes)
{
  size_t res = 0;
  if (establishConnection()) {
    res = write(connectionFd,aBytes,aNumBytes);
    if (DBGLOGENABLED(LOG_DEBUG)) {
      std::string s;
      for (size_t i=0; i<aNumBytes; i++) {
        string_format_append(s, "%02X ",aBytes[i]);
      }
      DBGLOG(LOG_DEBUG,"Transmitted bytes: %s\n", s.c_str());
    }
  }
  return res;
}



size_t SerialComm::receiveBytes(size_t aMaxBytes, uint8_t *aBytes)
{
  if (connectionOpen) {
		// get number of bytes available
		size_t numBytes;
		ioctl(connectionFd, FIONREAD, &numBytes);
		// limit to max buffer size
		if (numBytes>aMaxBytes)
			numBytes = aMaxBytes;
		// read
    size_t gotBytes = 0;
		if (numBytes>0)
			gotBytes = read(connectionFd,aBytes,numBytes); // read available bytes
    if (DBGLOGENABLED(LOG_DEBUG)) {
      if (gotBytes>0) {
        std::string s;
        for (size_t i=0; i<gotBytes; i++) {
          string_format_append(s, "%02X ",aBytes[i]);
        }
        DBGLOG(LOG_DEBUG,"   Received bytes: %s\n", s.c_str());
      }
    }
		return gotBytes;
  }
	return 0;
}



bool SerialComm::establishConnection()
{
  if (!connectionOpen) {
    // Open connection to bridge
    connectionFd = 0;
    int res;
    struct termios newtio;
    serialConnection = connectionPath[0]=='/';
    // check type of input
    if (serialConnection) {
      // convert the baudrate
      int baudRateCode = 0;
      switch (baudRate) {
        case 50 : baudRateCode = B50; break;
        case 75 : baudRateCode = B75; break;
        case 134 : baudRateCode = B134; break;
        case 150 : baudRateCode = B150; break;
        case 200 : baudRateCode = B200; break;
        case 300 : baudRateCode = B300; break;
        case 600 : baudRateCode = B600; break;
        case 1200 : baudRateCode = B1200; break;
        case 1800 : baudRateCode = B1800; break;
        case 2400 : baudRateCode = B2400; break;
        case 4800 : baudRateCode = B4800; break;
        case 9600 : baudRateCode = B9600; break;
        case 19200 : baudRateCode = B19200; break;
        case 38400 : baudRateCode = B38400; break;
        case 57600 : baudRateCode = B57600; break;
        case 115200 : baudRateCode = B115200; break;
        case 230400 : baudRateCode = B230400; break;
      }
      if (baudRateCode==0) {
        setUnhandledError(ErrorPtr(new SerialCommError(SerialCommErrorUnknownBaudrate)));
        return false;
      }
      // assume it's a serial port
      connectionFd = open(connectionPath.c_str(), O_RDWR | O_NOCTTY);
      if (connectionFd<0) {
        LOGERRNO(LOG_ERR);
        setUnhandledError(ErrorPtr(new SerialCommError(SerialCommErrorSerialOpen)));
        return false;
      }
      tcgetattr(connectionFd,&oldTermIO); // save current port settings
      // see "man termios" for details
      memset(&newtio, 0, sizeof(newtio));
      // - baudrate, 8-N-1, no modem control lines (local), reading enabled
      newtio.c_cflag = baudRateCode | CRTSCTS | CS8 | CLOCAL | CREAD;
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
      tcflush(connectionFd, TCIFLUSH);
      tcsetattr(connectionFd,TCSANOW,&newtio);
    }
    else {
      // assume it's an IP address or hostname
      struct sockaddr_in conn_addr;
      if ((connectionFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(LOG_ERR,"Error: Could not create socket\n");
        setUnhandledError(ErrorPtr(new SerialCommError(SerialCommErrorSocketOpen)));
        return false;
      }
      // prepare IP address
      memset(&conn_addr, '0', sizeof(conn_addr));
      conn_addr.sin_family = AF_INET;
      conn_addr.sin_port = htons(connectionPort);
      struct hostent *server;
      server = gethostbyname(connectionPath.c_str());
      if (server == NULL) {
        LOG(LOG_ERR,"Error: no such host\n");
        setUnhandledError(ErrorPtr(new SerialCommError(SerialCommErrorInvalidHost)));
        return false;
      }
      memcpy((char *)&conn_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
      if ((res = connect(connectionFd, (struct sockaddr *)&conn_addr, sizeof(conn_addr))) < 0) {
        LOGERRNO(LOG_ERR);
        setUnhandledError(ErrorPtr(new SerialCommError(SerialCommErrorSocketOpen)));
        return false;
      }
    }
    // successfully opened
    connectionOpen = true;
		// now set FD for serialqueue to monitor
		setFDtoMonitor(connectionFd);
  }
  return connectionOpen;
}


void SerialComm::closeConnection()
{
  if (connectionOpen) {
		// stop monitoring
		setFDtoMonitor();
    // restore IO settings
    if (serialConnection) {
      tcsetattr(connectionFd,TCSANOW,&oldTermIO);
    }
    // close
    close(connectionFd);
    // closed
    connectionOpen = false;
    // abort all pending operations
    abortOperations();
  }
}


void SerialComm::setUnhandledError(ErrorPtr aError)
{
  if (aError) {
    unhandledError = aError;
    LOG(LOG_ERR,"Unhandled error set: %s\n",aError->description().c_str());
  }
}



ErrorPtr SerialComm::getLastUnhandledError()
{
  ErrorPtr err = unhandledError;
  unhandledError.reset(); // no error
  return err;
}
