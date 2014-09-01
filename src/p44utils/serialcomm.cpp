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

#include "serialcomm.hpp"

#include <sys/ioctl.h>

using namespace p44;


SerialComm::SerialComm(MainLoop &aMainLoop) :
	inherited(aMainLoop),
  connectionPort(0),
  baudRate(9600),
  connectionOpen(false),
  reconnecting(false)
{
}


SerialComm::~SerialComm()
{
  closeConnection();
}



void SerialComm::setConnectionSpecification(const char* aConnectionSpec, uint16_t aDefaultPort, int aDefaultBaudRate)
{
  // device or IP host?
  string path;
  if (aConnectionSpec && *aConnectionSpec) {
    if (aConnectionSpec[0]=='/') {
      // serial device
      path = aConnectionSpec;
      size_t n = path.find_first_of(':');
      if (n!=string::npos) {
        // explicit specification of baudrate
        string opt = path.substr(n+1,string::npos);
        path.erase(n,string::npos);
        // get baud rate
        sscanf(opt.c_str(), "%d", &aDefaultBaudRate);
      }
    }
    else {
      // IP host
      splitHost(aConnectionSpec, &path, &aDefaultPort);
    }
  }
  setConnectionParameters(path.c_str(), aDefaultPort, aDefaultBaudRate);
}



void SerialComm::setConnectionParameters(const char* aConnectionPath, uint16_t aPortNo, int aBaudRate)
{
  closeConnection();
	connectionPath = nonNullCStr(aConnectionPath);
  connectionPort = aPortNo;
  baudRate = aBaudRate;
}


ErrorPtr SerialComm::establishConnection()
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
        return ErrorPtr(new SerialCommError(SerialCommErrorUnknownBaudrate));
      }
      // assume it's a serial port
      connectionFd = open(connectionPath.c_str(), O_RDWR | O_NOCTTY);
      if (connectionFd<0) {
        return SysError::errNo("Cannot open serial port: ");
      }
      tcgetattr(connectionFd,&oldTermIO); // save current port settings
      // see "man termios" for details
      memset(&newtio, 0, sizeof(newtio));
      // - 8-N-1, no modem control lines (local), reading enabled
      newtio.c_cflag = CS8 | CLOCAL | CREAD;
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
      // - set speed (as this ors into c_cflag, this must be after setting c_cflag initial value)
      cfsetspeed(&newtio, baudRateCode);
      // - set new params
      tcflush(connectionFd, TCIFLUSH);
      tcsetattr(connectionFd,TCSANOW,&newtio);
    }
    else {
      // assume it's an IP address or hostname
      struct sockaddr_in conn_addr;
      if ((connectionFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return SysError::errNo("Cannot create socket: ");
      }
      // prepare IP address
      memset(&conn_addr, '0', sizeof(conn_addr));
      conn_addr.sin_family = AF_INET;
      conn_addr.sin_port = htons(connectionPort);
      struct hostent *server;
      server = gethostbyname(connectionPath.c_str());
      if (server == NULL) {
        return ErrorPtr(new SerialCommError(SerialCommErrorInvalidHost));
      }
      memcpy((char *)&conn_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
      if ((res = connect(connectionFd, (struct sockaddr *)&conn_addr, sizeof(conn_addr))) < 0) {
        return SysError::errNo("Cannot open socket: ");
      }
    }
    // successfully opened
    connectionOpen = true;
		// now set FD for FdComm to monitor
		setFd(connectionFd);
  }
  reconnecting = false; // successfully opened, don't try to reconnect any more
  return ErrorPtr(); // ok
}


bool SerialComm::requestConnection()
{
  ErrorPtr err = establishConnection();
  if (!Error::isOK(err)) {
    if (!reconnecting) {
      LOG(LOG_ERR, "SerialComm: requestConnection() could not open connection now: %s -> entering background retry mode\n", err->description().c_str());
      reconnecting = true;
      MainLoop::currentMainLoop().executeOnce(boost::bind(&SerialComm::reconnectHandler, this), 5*Second);
    }
    return false;
  }
  return true;
}




void SerialComm::closeConnection()
{
  reconnecting = false; // explicit close, don't try to reconnect any more
  if (connectionOpen) {
		// stop monitoring
		setFd(-1);
    // restore IO settings
    if (serialConnection) {
      tcsetattr(connectionFd,TCSANOW,&oldTermIO);
    }
    // close
    close(connectionFd);
    // closed
    connectionOpen = false;
  }
}


bool SerialComm::connectionIsOpen()
{
  return connectionOpen;
}



#pragma mark - handling data exception


void SerialComm::dataExceptionHandler(int aFd, int aPollFlags)
{
  DBGLOG(LOG_DEBUG, "SerialComm::dataExceptionHandler(fd==%d, pollflags==0x%X)\n", aFd, aPollFlags);
  bool reEstablish = false;
  if (aPollFlags & POLLHUP) {
    // other end has closed connection
    LOG(LOG_ERR, "SerialComm: serial connection was hung up unexpectely\n");
    reEstablish = true;
  }
  else if (aPollFlags & POLLIN) {
    // Note: on linux a socket closed server side does not return POLLHUP, but POLLIN with no data
    // alerted for read, but nothing to read any more: assume connection closed
    LOG(LOG_ERR, "SerialComm: serial connection returns POLLIN with no data: assuming connection broken\n");
    reEstablish = true;
  }
  else if (aPollFlags & POLLERR) {
    // error
    LOG(LOG_ERR, "SerialComm: error on serial connection: assuming connection broken\n");
    reEstablish = true;
  }
  // in case of error, close and re-open connection
  if (reEstablish && !reconnecting) {
    LOG(LOG_ERR, "SerialComm: closing and re-opening connection in attempt to re-establish it after error\n");
    closeConnection();
    // try re-opening right now
    reconnecting = true;
    reconnectHandler();
  }
}


void SerialComm::reconnectHandler()
{
  if (reconnecting) {
    ErrorPtr err = establishConnection();
    if (!Error::isOK(err)) {
      LOG(LOG_ERR, "SerialComm: re-connect failed: %s -> retry again later\n", err->description().c_str());
      reconnecting = true;
      MainLoop::currentMainLoop().executeOnce(boost::bind(&SerialComm::reconnectHandler, this), 15*Second);
    }
    else {
      LOG(LOG_NOTICE, "SerialComm: successfully reconnected to %s\n", connectionPath.c_str());
    }
  }
}


