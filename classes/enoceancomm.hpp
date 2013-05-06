//
//  enoceancomm.h
//  p44bridged
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__enoceancomm__
#define __p44bridged__enoceancomm__

#include "p44bridged_common.hpp"

#include "serialcomm.hpp"

using namespace std;

namespace p44 {

	class EnoceanComm;

  class Esp3Telegram;
	typedef boost::shared_ptr<Esp3Telegram> Esp3TelegramPtr;
	/// ESP3 telegram object with byte stream parser and generator
  class Esp3Telegram
  {
  public:
    typedef enum {
      ts_syncwait,
      ts_headerread,
      ts_dataread,
      ts_complete
    } TelegramState;

  private:
    TelegramState state;
    uint8_t header[6];
    size_t dataIndex;
    uint8_t *payloadP;
    size_t payloadSize;

    
  public:
    /// construct empty telegram
    Esp3Telegram();
    ~Esp3Telegram();

    /// add one byte to a ESP3 CRC8
    /// @param aByte the byte to add
    /// @param aCRCValue the current CRC
    /// @return updated CRC
    static uint8_t addToCrc8(uint8_t aByte, uint8_t aCRCValue);
    /// calculate ESP3 CRC8 over a range of bytes
    /// @param aDataP data buffer
    /// @param aNumBytes number of bytes
    /// @param aCRCValue start value, feed in existing CRC to continue adding bytes. Defaults to 0.
    /// @return updated CRC
    static uint8_t crc8(uint8_t *aDataP, size_t aNumBytes, uint8_t aCRCValue = 0);

    /// clear the telegram, now re-start accepting bytes and looking for telegram start
    void clear();

    /// data length
    size_t dataLength();
    void setDataLength(size_t aNumBytes);
    /// optional data length
    size_t optDataLength();
    void setOptDataLength(size_t aNumBytes);
    /// packet type
    uint8_t packetType();
    void setPacketType(uint8_t aPacketType);
    /// calculated CRC of header
    uint8_t headerCRC();
    /// calculated CRC of payload, 0 if no payload
    uint8_t payloadCRC();

    /// check if telegram is complete
    bool isComplete();

    /// swallow bytes until telegram is complete
    /// @param aNumBytes number of bytes ready for accepting
    /// @param aBytes pointer to bytes buffer
    /// @return number of bytes operation could accept, 0 if none (means that telegram is already complete)
    size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

    /// description
    string description();

  };
	
	
  typedef boost::shared_ptr<EnoceanComm> EnoceanCommPtr;
	// Enocean communication
	class EnoceanComm : public SerialComm
	{
		typedef SerialComm inherited;
		
		Esp3TelegramPtr currentIncomingTelegram;
		
	public:
		
		EnoceanComm(SyncIOMainLoop *aMainLoopP);
		virtual ~EnoceanComm();
		
    /// set the connection parameters to connect to the enOcean TCM310 modem
    /// @param aConnectionPath serial device path (/dev/...) or host name/address (1.2.3.4 or xxx.yy)
    /// @param aPortNo port number for TCP connection (irrelevant for direct serial device connection)
    void setConnectionParameters(const char* aConnectionPath, uint16_t aPortNo);
		
    /// derived implementation: deliver bytes to the ESP3 parser
    /// @param aNumBytes number of bytes ready for accepting
    /// @param aBytes pointer to bytes buffer
    /// @return number of bytes parser could accept (normally, all)
    virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);
		
	};



} // namespace p44


#endif /* defined(__p44bridged__enoceancomm__) */
