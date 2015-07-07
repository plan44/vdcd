//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//


#ifndef DALICOMM_H_
#define DALICOMM_H_

#include "vdcd_common.hpp"

#include "serialqueue.hpp"

#include "dalidefs.h"

using namespace std;

// Note: before 2015-02-27, we had a bug which caused the last extra byte not being read, so the checksum reached zero
// only if the last byte was 0. We also passed the if checksum was 0xFF, because our reference devices always had 0x01 in
// the last byte, and I assumed missing by 1 was the result of not precise enough specs or a bug in the device.
// If OLD_BUGGY_CHKSUM_COMPATIBLE is defined, extra checks will be made to keep already-in-use devices identified by
// shortaddr to avoid messing up existing installations
#define OLD_BUGGY_CHKSUM_COMPATIBLE 1

namespace p44 {

  // Errors
  typedef enum {
    DaliCommErrorOK,
    DaliCommErrorBusy,
    DaliCommErrorBridgeComm,
    DaliCommErrorBridgeCmd,
    DaliCommErrorBridgeUnknown,
    DaliCommErrorDALIFrame,
    DaliCommErrorMissingData,
    DaliCommErrorBadChecksum,
    DaliCommErrorBadDeviceInfo,
    DaliCommErrorInvalidAnswer,
    DaliCommErrorNeedFullScan,
    DaliCommErrorDeviceSearch,
    DaliCommErrorSetShortAddress,
    DaliCommErrorBusOverload,
    DaliCommErrorDataUnreliable
  } DaliCommErrors;

  class DaliCommError : public Error
  {
  public:
    static const char *domain() { return "DaliComm"; }
    virtual const char *getErrorDomain() const { return DaliCommError::domain(); };
    DaliCommError(DaliCommErrors aError) : Error(ErrorCode(aError)) {};
    DaliCommError(DaliCommErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };



  class DaliComm;


  /// abstracted DALI bus address
  typedef uint8_t DaliAddress;
  const DaliAddress DaliGroup = 0x80; // marks group address
  const DaliAddress DaliBroadcast = 0xFF; // all devices on the bus
  const DaliAddress DaliAddressMask = 0x3F; // address mask
  const DaliAddress DaliGroupMask = 0x0F; // address mask

  /// DALI device information record
  class DaliDeviceInfo : public P44Obj
  {
  public:
    typedef enum {
      devinf_none,
      devinf_maybe,
      devinf_solid,
      devinf_notForID
    } DaliDevInfStatus;

    DaliDeviceInfo();
    /// clear everything except short address
    void clear();
    /// short address
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
    /// status of device info
    DaliDevInfStatus devInfStatus;
  };



  typedef boost::intrusive_ptr<DaliComm> DaliCommPtr;

  /// A class providing low level access to the DALI bus
  class DaliComm : public SerialOperationQueue
  {
    typedef SerialOperationQueue inherited;

    int runningProcedures;

    bool isBusy();
    static ErrorPtr busyError() { return ErrorPtr(new DaliCommError(DaliCommErrorBusy)); };

    MLMicroSeconds closeAfterIdleTime;
    long connectionTimeoutTicket;

    int expectedBridgeResponses; ///< not yet received bridge responses
    bool responsesInSequence; ///< set when repsonses need to be in sequence with requests

    uint8_t sendEdgeAdj; ///< adjustment for sending rising edge - first param to CMD_CODE_EDGEADJ
    uint8_t samplePointAdj; ///< adjustment for sampling point - second param to CMD_CODE_EDGEADJ

  public:

    DaliComm(MainLoop &aMainLoop);
    virtual ~DaliComm();

    void startProcedure();
    void endProcedure();

    /// @name low level DALI bus communication
    /// @{

    /// set the connection parameters to connect to the DALI bridge
    /// @param aConnectionSpec serial device path (/dev/...) or host name/address[:port] (1.2.3.4 or xxx.yy)
    /// @param aDefaultPort default port number for TCP connection (irrelevant for direct serial device connection)
    /// @param aCloseAfterIdleTime if not Never, serial port will be closed after being idle for the specified time
    void setConnectionSpecification(const char *aConnectionSpec, uint16_t aDefaultPort, MLMicroSeconds aCloseAfterIdleTime);

    /// set DALI edge adjustment
    /// @param how much (in 1/256th DALI bit time units) to delay the going inactive edge of the sending signal, to compensate for slow falling (going active) edge on the bus
    void setDaliSendAdj(uint8_t aSendEdgeDelay) { sendEdgeAdj = aSendEdgeDelay; };

    /// @param how much (in 1/256th DALI bit time units) to delay or advance the sample point when receiving DALI data
    void setDaliSampleAdj(int8_t aSamplePointDelay) { samplePointAdj = (uint8_t)aSamplePointDelay; };


    /// callback function for sendBridgeCommand
    typedef boost::function<void (uint8_t aResp1, uint8_t aResp2, ErrorPtr aError)> DaliBridgeResultCB;

    /// Send DALI command to bridge
    /// @param aCmd bridge command byte
    /// @param aDali1 first DALI byte
    /// @param aDali2 second DALI byte
    /// @param aResultCB callback executed when bridge response arrives
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB, int aWithDelay = -1);


    /// callback function for daliSendXXX methods
    typedef boost::function<void (ErrorPtr aError)> DaliCommandStatusCB;

    /// reset the communication with the bridge
    void reset(DaliCommandStatusCB aStatusCB);

    /// Send two byte DALI bus command
    /// @param aDali1 first DALI byte
    /// @param aDali2 second DALI byte
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSend(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);

    /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
    /// @param aPower Arc power
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendDirectPower(DaliAddress aAddress, uint8_t aPower, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);

    /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
    /// @param aCommand command
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);

    /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
    /// @param aCommand command
    /// @param aDTRValue the value to be sent to DTR before executing aCommand
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendDtrAndCommand(DaliAddress aAddress, uint8_t aCommand, uint8_t aDTRValue, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);


    /// Send two byte DALI bus command twice within 100ms
    /// @param aDali1 first DALI byte
    /// @param aDali2 second DALI byte
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendTwice(uint8_t aDali1, uint8_t aDali2, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);

    /// Send DALI config command (send twice within 100ms)
    /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
    /// @param aCommand command
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendConfigCommand(DaliAddress aAddress, uint8_t aCommand, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);

    /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
    /// @param aCommand command
    /// @param aDTRValue the value to be sent to DTR before executing aCommand
    /// @param aStatusCB status callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendDtrAndConfigCommand(DaliAddress aAddress, uint8_t aCommand, uint8_t aDTRValue, DaliCommandStatusCB aStatusCB = NULL, int aWithDelay = -1);

    /// callback function for daliSendXXX methods returning data
    typedef boost::function<void (bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)> DaliQueryResultCB;

    /// Send DALI command and expect answer byte
    /// @param aDali1 first DALI byte
    /// @param aDali2 second DALI byte
    /// @param aResultCB result callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendAndReceive(uint8_t aDali1, uint8_t aDali2, DaliQueryResultCB aResultCB, int aWithDelay = -1);

    /// @param aAddress DALI address (device short address, or group address + DaliGroup, or DaliBroadcast)
    /// @param aCommand command
    /// @param aResultCB result callback
    /// @param aWithDelay if>0, time (in microseconds) to delay BEFORE sending the command
    void daliSendQuery(DaliAddress aAddress, uint8_t aQueryCommand, DaliQueryResultCB aResultCB, int aWithDelay = -1);

    /// helper to check daliSendQuery() callback response for a DALI YES answer
    static bool isYes(bool aNoOrTimeout, uint8_t aResponse, ErrorPtr &aError, bool aCollisionIsYes);

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
    typedef boost::function<void (ShortAddressListPtr aShortAddressListPtr, ShortAddressListPtr aUnreliableShortAddressListPtr, ErrorPtr aError)> DaliBusScanCB;

    /// Scan the bus for active devices (short address)
    /// @param aResultCB callback receiving a list<int> of available short addresses on the bus
    void daliBusScan(DaliBusScanCB aResultCB);

    /// Scan the bus for devices by random address search
    /// @param aResultCB callback receiving a list<int> of available short addresses on the bus
    /// @param aFullScanOnlyIfNeeded
    /// @note detects short address conflicts and devices without short address, assigns new short addresses as needed
    void daliFullBusScan(DaliBusScanCB aResultCB, bool aFullScanOnlyIfNeeded);


    /// Test the reliability of writing and reading back data from a specific device
    /// @param aResultCB callback receiving ok or error
    /// @param aAddress short address of device to test
    /// @param aNumCycles number of R/W cycles to perform (on DTR of the device)
    /// @note does not check running procedure, so make sure no other procedure is running before calling this
    void daliBusTestData(StatusCB aResultCB, DaliAddress aAddress, uint8_t aNumCycles);


    typedef boost::shared_ptr<std::vector<uint8_t> > MemoryVectorPtr;

    /// callback function for daliReadMemory

    typedef boost::function<void (MemoryVectorPtr aMemoryVectorPtr, ErrorPtr aError)> DaliReadMemoryCB;
    /// Read DALI memory
    /// @param aResultCB callback receiving the data read as a vector<uint8_t>
    /// @param aAddress short address of device to read
    /// @param aBank memory bank to read
    /// @param aOffset offset to start reading
    /// @param aNumBytes number of bytes to read
    /// @note reading none or less data than requested is not considered an error - aMemoryVectorPtr param in callback will
    ///   just return the number of bytes that could be read; check its size to make sure expected result was returned
    void daliReadMemory(DaliReadMemoryCB aResultCB, DaliAddress aAddress, uint8_t aBank, uint8_t aOffset, uint8_t aNumBytes);


    typedef boost::intrusive_ptr<DaliDeviceInfo> DaliDeviceInfoPtr;

    /// callback function for daliReadDeviceInfo
    typedef boost::function<void (DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)> DaliDeviceInfoCB;

    /// Read DALI device info
    /// @param aResultCB callback receiving the device info record
    /// @param aAddress short address of device to read device info from
    void daliReadDeviceInfo(DaliDeviceInfoCB aResultCB, DaliAddress aAddress);

    /// @}

  private:

    void bridgeResponseHandler(DaliBridgeResultCB aBridgeResultHandler, SerialOperationPtr aOperation, OperationQueuePtr aQueueP, ErrorPtr aError);
    void daliCommandStatusHandler(DaliCommandStatusCB aResultCB, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError);
    void daliQueryResponseHandler(DaliQueryResultCB aResultCB, uint8_t aResp1, uint8_t aResp2, ErrorPtr aError);
    void connectionTimeout();

  };

} // namespace p44


#endif /* DALICOMM_H_ */
