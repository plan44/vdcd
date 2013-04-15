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

class DaliComm;

typedef enum {
  DaliStatusOK, // ok
  DaliStatusNoOrTimeout, // response timeout (also means NO in some queries)
  DaliStatusFrameError, // DALI bus framing error
  DaliStatusBridgeCmdError, // invalid bridge command
  DaliStatusBridgeCommError, // pseudo error - problem communicating with bridge
  DaliStatusBridgeUnknown // unknown status/error
} DaliStatus;



typedef boost::shared_ptr<DaliComm> DaliCommPtr;

typedef boost::function<void (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2)> DaliBridgeResultCB;

typedef boost::function<void (DaliComm *aDaliCommP, DaliStatus aStatus)> DaliCommandStatusCB;
typedef boost::function<void (DaliComm *aDaliCommP, DaliStatus aStatus, uint8_t aResponse)> DaliQueryResultCB;


/// A class providing low level access to the DALI bus
class DaliComm : SerialOperationQueue
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

  /// transmit data
  size_t transmitBytes(size_t aNumBytes, uint8_t *aBytes);
  /// establish the connection to the DALI bridge
  /// @note can be called multiple times, opens connection only if not already open
  bool establishConnection();
  /// close the current connection, if any
  void closeConnection();

  /// Send DALI command to bridge
  /// @param aCmd bridge command byte
  /// @param aDali1 DALI byte 1
  /// @param aDali2 DALI byte 2
  /// @param aResultCB callback executed when bridge response arrives
  void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB);


  /// Send regular DALI bus command
  void daliSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);
  /// Send DALI config command (send twice within 100ms) 
  void daliConfigSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);
  /// Send DALI Query command (expect answer byte)
  void daliQuerySend(uint8_t aDali1, uint8_t aDali2, DaliQueryResultCB aResultCB);


  // %%% test
  void test1();
  void test1Ack(DaliComm *aDaliComm, uint8_t aResp1, uint8_t aResp2);
  void test2();
  void test2Ack(DaliStatus aStatus, uint8_t aResponse);
};

#endif /* DALICOMM_H_ */
