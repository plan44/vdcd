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
  DaliCommErrorInvalidAnswer,
  DaliCommErrorNeedFullScan,
  DaliCommErrorDeviceSearch,
  DaliCommErrorSetShortAddress,
} DaliCommErrors;

class DaliCommError : public Error
{
public:
  static const char *domain() { return "DaliComm"; }
  DaliCommError(DaliCommErrors aError) : Error(ErrorCode(aError)) {};
  DaliCommError(DaliCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  virtual const char *getErrorDomain() const { return DaliCommError::domain(); };
};



class DaliComm;


/// abstracted DALI bus address
typedef uint8_t DaliAddress;
const DaliAddress DaliGroup = 0x80; // marks group address
const DaliAddress DaliBroadcast = 0xFF; // all devices on the bus
const DaliAddress DaliAddressMask = 0x3F; // address mask
const DaliAddress DaliGroupMask = 0x0F; // address mask

/// DALI device information record
class DaliDeviceInfo
{
public:
  DaliDeviceInfo();
  // short address
  DaliAddress shortAddress;
  // DALI device information
  long long gtin; /// < 48 bit global trade identification number (GTIN / EAN)
  uint8_t fw_version_major; /// < major firmware version
  uint8_t fw_version_minor; /// < minor firmware version
  long long serialNo; /// < unique serial number
  // OEM product information
  long long oem_gtin; /// < 48 bit global trade identification number of OEM product (GTIN / EAN)
  long long oem_serialNo; /// < unique serial number
  /// text description
  string description();
  /// returns true if uniquely identifying the device (real GTIN + serial found)
  bool uniquelyIdentifiing();
};



typedef boost::shared_ptr<DaliComm> DaliCommPtr;

/// A class providing low level access to the DALI bus
class DaliComm : SerialOperationQueue
{
	typedef SerialOperationQueue inherited;
	
  // connection to the bridge
  string bridgeConnectionPath;
  uint16_t bridgeConnectionPort;
  bool bridgeConnectionOpen;
  int bridgeFd;
  struct termios oldTermIO;
  bool serialConnection;
  ErrorPtr unhandledError;
public:

  DaliComm(SyncIOMainLoop *aMainLoopP);
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

  /// @}


  /// @name low level DALI bus communication
  /// @{


  /// callback function for sendBridgeCommand
  typedef boost::function<void (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)> DaliBridgeResultCB;
  /// Send DALI command to bridge
  /// @param aCmd bridge command byte
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aResultCB callback executed when bridge response arrives
  void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB, int aWithDelay = -1);


  /// callback function for daliSendXXX methods
  typedef boost::function<void (DaliComm *aDaliCommP, ErrorPtr aError)> DaliCommandStatusCB;

  /// reset the communication with the bridge
  void reset(DaliCommandStatusCB aStatusCB);

  /// Send two byte DALI bus command
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aStatusCB status callback
  void daliSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aPower Arc power
  /// @param aStatusCB status callback
  void daliSendDirectPower(uint8_t aAddress, uint8_t aPower, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aStatusCB status callback
  void daliSendCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aDTRValue the value to be sent to DTR before executing aCommand
  /// @param aStatusCB status callback
  void daliSendDtrAndCommand(DaliAddress aAddress, uint8_t aCommand, uint8_t aDTRValue, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);


  /// Send two byte DALI bus command twice within 100ms
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aStatusCB status callback
  void daliSendTwice(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);
  /// Send DALI config command (send twice within 100ms)
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aStatusCB status callback
  void daliSendConfigCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);


  /// callback function for daliSendXXX methods returning data
  typedef boost::function<void (DaliComm *aDaliCommP, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)> DaliQueryResultCB;
  /// Send DALI command and expect answer byte
  /// @param aDali1 first DALI byte
  /// @param aDali2 second DALI byte
  /// @param aResultCB result callback
  void daliSendAndReceive(uint8_t aDali1, uint8_t aDali2, DaliQueryResultCB aResultCB, int aWithDelay = -1);
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @param aCommand command
  /// @param aResultCB result callback
  void daliSendQuery(DaliAddress aAddress, uint8_t aQueryCommand, DaliQueryResultCB aResultCB, int aWithDelay = -1);

  /// utility function to create address byte
  /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  /// @return first DALI byte for use in daliSend/daliSendTwice
  static uint8_t dali1FromAddress(DaliAddress aAddress);
  /// utility function to decode address byte
  /// @param DALI-style bAAAAAAx address byte, as returned by some query commands
  /// @return DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
  static DaliAddress addressFromDaliResponse(uint8_t aAnswer);

  /// @}

  /// @name high level DALI bus services
  /// @{

  typedef std::list<DaliAddress> ShortAddressList;
  typedef boost::shared_ptr<ShortAddressList> ShortAddressListPtr;
  /// callback function for daliScanBus
  typedef boost::function<void (DaliComm *aDaliCommP, ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError)> DaliBusScanCB;
  /// Scan the bus for active devices (short address)
  /// @param aResultCB callback receiving a list<int> of available short addresses on the bus
  void daliBusScan(DaliBusScanCB aResultCB);
  /// Scan the bus for devices by random address search
  /// @param aResultCB callback receiving a list<int> of available short addresses on the bus
  /// @param aFullScanOnlyIfNeeded
  /// @note detects short address conflicts and devices without short address, assigns new short addresses as needed
  void daliFullBusScan(DaliBusScanCB aResultCB, bool aFullScanOnlyIfNeeded);


  typedef boost::shared_ptr<std::vector<uint8_t> > MemoryVectorPtr;
  /// callback function for daliReadMemory
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


  typedef boost::shared_ptr<DaliDeviceInfo> DaliDeviceInfoPtr;
  /// callback function for daliReadDeviceInfo
  typedef boost::function<void (DaliComm *aDaliCommP, DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)> DaliDeviceInfoCB;
  /// Read DALI device info
  /// @param aResultCB callback receiving the device info record
  /// @param aAddress short address of device to read device info from
  void daliReadDeviceInfo(DaliDeviceInfoCB aResultCB, DaliAddress aAddress);

  /// @}


public:
  // %%% test
  void testReset();
  void testResetAck(ErrorPtr aError);

  void testBusScan();
  void testBusScanAck(ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError);

  void testFullBusScan();
  void testFullBusScanAck(ShortAddressListPtr aShortAddressListPtr, ErrorPtr aError);

  void testReadBytes(DaliAddress aShortAddress);
  void testReadBytesAck(MemoryVectorPtr aMemoryPtr, ErrorPtr aError);

  void testReadDeviceInfo(DaliAddress aShortAddress);
  void testReadDeviceInfoAck(DaliDeviceInfoPtr aDeviceInfo, ErrorPtr aError);
};

#endif /* DALICOMM_H_ */
