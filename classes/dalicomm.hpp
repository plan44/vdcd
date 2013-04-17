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

#include "dalidefs.h"

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

// Errors
typedef enum {
  DaliCommErrorOK,
  DaliCommErrorSocketOpen,
  DaliCommErrorInvalidHost,
  DaliCommErrorSerialOpen,
  DaliCommErrorBridgeComm,
  DaliCommErrorBridgeCmd,
  DaliCommErrorBridgeUnknown,
  DaliCommErrorDALIFrame,
  DaliCommErrorMissingData,
} DaliCommErrors;

class DaliCommError : public Error
{
public:
  DaliCommError(DaliCommErrors aError) : Error(ErrorCode(aError)) {};
  DaliCommError(DaliCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  virtual const char *getErrorDomain() { return "DaliComm"; }
};



class DaliComm;

//typedef enum {
//  DaliStatusOK, // ok
//  DaliStatusNoOrTimeout, // response timeout (also means NO in some queries)
//  DaliStatusFrameError, // DALI bus framing error
//  DaliStatusBridgeCmdError, // invalid bridge command
//  DaliStatusBridgeCommError, // pseudo error - problem communicating with bridge
//  DaliStatusBridgeUnknown // unknown status/error
//} DaliStatus;


/// abstracted DALI bus address
typedef uint8_t DaliAddress;
const DaliAddress DaliGroup = 0x80; // marks group address
const DaliAddress DaliBroadcast = 0xFF; // all devices on the bus
const DaliAddress DaliAddressMask = 0x3F; // address mask

/// DALI device information record
class DaliDeviceInfo
{
public:
  DaliDeviceInfo();
  // short address
  DaliAddress shortAddress;
  // DALI device information
  long long gtin; /// < global trade identification number (GTIN / EAN)
  uint8_t fw_version_major; /// < major firmware version
  uint8_t fw_version_minor; /// < minor firmware version
  long long serialNo; /// < unique serial number
  // OEM product information
  long long oem_gtin; /// < global trade identification number of OEM product (GTIN / EAN)
  long long oem_serialNo; /// < unique serial number
  /// text description
  string description();
};



typedef boost::shared_ptr<DaliComm> DaliCommPtr;

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
  ErrorPtr lastError;
public:

  DaliComm();
  ~DaliComm();

  /// Set the connection parameters for the DALI bus bridge
  /// @param aBridgeConnectionPath serial device path (/dev/...) or host name/address (1.2.3.4 or xxx.yy) to connect DALI bridge
  /// @param aPortNo port number for TCP connection (irrelevant for serial device)
  void setConnectionParameters(const char* aBridgeConnectionPath, uint16_t aPortNo);

  /// @name Main loop integration
  /// @{

  /// Get the file descriptor to be monitored in daemon main loop
  /// @return <0 if nothing to be monitored (no connection open)
  int toBeMonitoredFD();

  /// Must be called from main loop when monitored FD has data to process
  void dataReadyOnMonitoredFD();

  /// Should be called in regular intervals to trigger timed operations (such as timeouts)
  void process();

  /// @}


  /// @name Connection
  /// @{

  /// transmit data
  size_t transmitBytes(size_t aNumBytes, uint8_t *aBytes);

  /// establish the connection to the DALI bridge
  /// @note can be called multiple times, opens connection only if not already open
  bool establishConnection();

  /// close the current connection, if any
  void closeConnection();

  /// set DALI bus global error
  void setError(ErrorPtr aError);

  /// @}


  /// @name low level DALI bus communication
  /// @{

  /// Send DALI command to bridge
  /// @param aCmd bridge command byte
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aResultCB callback executed when bridge response arrives
  typedef boost::function<void (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)> DaliBridgeResultCB;
  void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB);


  typedef boost::function<void (DaliComm *aDaliCommP, ErrorPtr aError)> DaliCommandStatusCB;

  /// reset the communication with the bridge
  void reset(DaliCommandStatusCB aStatusCB);

  /// Send two byte DALI bus command
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aStatusCB status callback
  void daliSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aPower Arc power
  /// @param aStatusCB status callback
  void daliSendDirectPower(uint8_t aAddress, uint8_t aPower, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aStatusCB status callback
  void daliSendCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);

  /// Send two byte DALI bus command twice within 100ms
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aStatusCB status callback
  void daliSendTwice(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);
  /// Send DALI config command (send twice within 100ms)
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aStatusCB status callback
  void daliSendConfigCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB = NULL, int aMinTimeToNextCmd = -1);

  typedef boost::function<void (DaliComm *aDaliCommP, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)> DaliQueryResultCB;
  /// Send DALI command and expect answer byte
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aResultCB result callback
  void daliSendAndReceive(uint8_t aDali1, uint8_t aDali2, DaliQueryResultCB aResultCB);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aResultCB result callback
  void daliSendQuery(DaliAddress aAddress, uint8_t aQueryCommand, DaliQueryResultCB aResultCB);

  /// utility function to create address byte
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @return first DALI byte for use in daliSend/daliSendTwice
  uint8_t dali1FromAddress(DaliAddress aAddress);

  /// @}

  /// @name high level DALI bus services
  /// @{

  typedef shared_ptr<std::list<DaliAddress>> DeviceListPtr;
  typedef boost::function<void (DaliComm *aDaliCommP, DeviceListPtr aDeviceListPtr, ErrorPtr aError)> DaliBusScanCB;
  /// Scan the bus for active devices (short address)
  /// @param aResultCB callback receiving a list<int> of available short addresses on the bus
  void daliScanBus(DaliBusScanCB aResultCB);

  typedef shared_ptr<std::vector<uint8_t>> MemoryVectorPtr;
  typedef boost::function<void (DaliComm *aDaliCommP, MemoryVectorPtr aMemoryVectorPtr, ErrorPtr aError)> DaliReadMemoryCB;
  /// Read DALI memory
  /// @param aResultCB callback receiving the data read as a vector<uint8_t>
  /// @param aAddress short address of device to read
  /// @param aBank memory bank to read
  /// @param aOffset offset to start reading
  /// @param aNumBytes number of bytes to read
  /// @note reading none or less data than requested is not considered an error - aMemoryVectorPtr param in callback will
  ///   just return the number of bytes that could be read; check its size to make sure expected result was returned
  void daliReadMemory(DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes);

  typedef shared_ptr<DaliDeviceInfo> DaliDeviceInfoPtr;
  typedef boost::function<void (DaliComm *aDaliCommP, DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)> DaliDeviceInfoCB;
  /// Read DALI device info
  /// @param aResultCB callback receiving the device info record
  /// @param aAddress short address of device to read device info from
  void daliReadDeviceInfo(DaliDeviceInfoCB aResultCB, DaliAddress aAddress);

  /// @}


public:
  // %%% test
  void test();
private:
  void resetAck(ErrorPtr aError);
  void test1();
  void test1Ack(uint8_t aResp1, uint8_t aResp2, ErrorPtr aError);
  void test2();
  void test2Ack(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError);
  void test3();
  void test3Ack(DeviceListPtr aDeviceListPtr, ErrorPtr aError);
  void test4();
  void test4Ack(MemoryVectorPtr aMemoryPtr, ErrorPtr aError);
  void test5();
  void test5Ack(DaliDeviceInfoPtr aDeviceInfo, ErrorPtr aError);
};

#endif /* DALICOMM_H_ */
