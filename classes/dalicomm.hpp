/*
 * dalicomm.hpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */


#ifndef DALICOMM_H_
#define DALICOMM_H_

#include "p44bridged_common.hpp"

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

class DaliComm;
typedef boost::shared_ptr<DaliComm> DaliCommPtr;

/// A class providing low level access to the DALI bus
class DaliComm
{
  // connection to the bridge
  string bridgeConnectionPath;
  uint16_t bridgeConnectionPort;
  bool bridgeConnectionOpen;
  int bridgeFd;
  struct termios oldTermIO;
  bool serialConnection;
public:

  /// Initialize the DALI bus communication object
  /// @param aBridgeConnectionPath serial device path (/dev/...) or host name/address (1.2.3.4 or xxx.yy) to connect DALI bridge
  /// @param aPortNo port number for TCP connection (irrelevant for serial device)
  DaliComm(const char* aBridgeConnectionPath, uint16_t aPortNo);
  /// destructor
  ~DaliComm();

  /// Get the file descriptor to be monitored in daemon main loop
  /// @return <0 if nothing to be monitored (no connection open)
  int toBeMonitoredFD();
  /// Must be called from main loop when monitored FD has data to process
  void dataReadyOnMonitoredFD();
  /// process pending events
  /// @note must be called in regular intervals from mainloop
  /// @return false if no more events pending (caller may go to sleep)
  void processPendingEvents();

  /// transmit data
  void transmitBytes(size_t aNumBytes, uint8_t *aBytes);

  /// Send DALI command to bridge
  /// @param aCmd bridge command byte
  /// @param aDali1 DALI byte 1
  /// @param aDali2 DALI byte 2
  /// @param aResultCB callback executed when bridge response arrives
  void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2);

  /// establish the connection to the DALI bridge
  /// @note can be called multiple times, opens connection only if not already open
  bool establishConnection();

  /// close the current connection, if any
  void closeConnection();


  // %%% test
  void allOn();
  void ackAllOn(DaliComm *aDaliComm, uint8_t aResp1, uint8_t aResp2);
};

#endif /* DALICOMM_H_ */
