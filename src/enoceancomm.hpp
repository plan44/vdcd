//
//  enoceancomm.h
//  vdcd
//
//  Created by Lukas Zeller on 03.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__enoceancomm__
#define __vdcd__enoceancomm__

#include "vdcd_common.hpp"

#include "serialcomm.hpp"

using namespace std;

namespace p44 {

  typedef enum {
    pt_radio = 0x01, // Radio telegram
    pt_response = 0x02, // Response to any packet
    pt_radio_sub_tel = 0x03, // Radio subtelegram
    pt_event_message = 0x04, // Event message
    pt_common_cmd = 0x05, // Common command
    pt_smart_ack_command = 0x06, // Smart Ack command
    pt_remote_man_command = 0x07, // Remote management command
    pt_manufacturer_specific_cmd_first = 0x80, // first manufacturer specific command
    pt_manufacturer_specific_cmd_last = 0xFF // last manufacturer specific command
  } PacketType;

  typedef enum {
    rorg_invalid = 0, ///< pseudo-RORG = invalid
    rorg_RPS = 0xF6, ///< Repeated Switch Communication
    rorg_1BS = 0xD5, ///< 1 Byte Communication
    rorg_4BS = 0xA5, ///< 4 Byte Communication
    rorg_VLD = 0xD2, ///< Variable Length Data
    rorg_MSC = 0xD1, ///< Manufacturer specific communication
    rorg_ADT = 0xA6, ///< Adressing Destination Telegram
    rorg_SM_LRN_REQ = 0xC6, ///< Smart Ack Learn Request
    rorg_SM_LRN_ANS = 0xC7, ///< Smart Ack Learn Answer
    rorg_SM_REC = 0xA7, ///< Smart Ack Reclaim
    rorg_SYS_EX = 0xC5, ///< Remote Management
    rorg_SEC = 0x30, ///< Secure telegram
    rorg_SEC_ENCAPS = 0x31 ///< Secure telegram with R-ORG encapsulation
  } RadioOrg;


  // RPS bits
  typedef enum {
    status_mask = 0x30,
    status_T21 = 0x20,
    status_NU = 0x10 // set if N-Message, cleared if U-Message
  } StatusBits;


  /// Enocean EEP profile number (RORG/FUNC/TYPE)
  typedef uint32_t EnoceanProfile;
  // unknown markers
  const uint8_t eep_func_unknown = 0xFF;
  const uint8_t eep_type_unknown = 0xFF;
  const EnoceanProfile eep_profile_unknown = (rorg_invalid<<16) + (eep_func_unknown<<8) + eep_type_unknown;
  const EnoceanProfile eep_ignore_type_mask = 0xFFFF00;


  /// Enocean Manufacturer number (11 bits)
  typedef uint16_t EnoceanManufacturer;
  // unknown marker
  const EnoceanManufacturer manufacturer_unknown = 0xFFFF;

  /// EnOcean channel (for devices with multiple functions, or multiple instances of a function like multi-pushbuttons)
  typedef uint8_t EnoceanChannel;
  const EnoceanChannel EnoceanAllChannels = 0xFF; // all channels

  /// EnOcean addresses (IDs)
  typedef uint32_t EnoceanAddress;
  const EnoceanAddress EnoceanBroadcast = 0xFFFFFFFF; // broadcast

	class EnoceanComm;

  class Esp3Packet;
	typedef boost::intrusive_ptr<Esp3Packet> Esp3PacketPtr;
	/// ESP3 packet object with byte stream parser and generator
  class Esp3Packet : public P44Obj
  {
  public:
    typedef enum {
      ps_syncwait,
      ps_headerread,
      ps_dataread,
      ps_complete
    } PacketState;


  private:
    PacketState state;
    uint8_t header[6];
    size_t dataIndex;
    uint8_t *payloadP;
    size_t payloadSize;

    
  public:
    /// construct empty packet
    Esp3Packet();
    virtual ~Esp3Packet();

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

    /// clear the packet, re-start accepting bytes and looking for packet start
    void clear();
    /// clear only the payload data/optdata (implicitly happens at setDataLength() and setOptDataLength()
    void clearData();

    /// check if packet is complete
    bool isComplete();

    /// swallow bytes until packet is complete
    /// @param aNumBytes number of bytes ready for accepting
    /// @param aBytes pointer to bytes buffer
    /// @return number of bytes operation could accept, 0 if none (means that packet is already complete)
    size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);


    /// @name access to header fields
    /// @{

    /// data length
    size_t dataLength();
    void setDataLength(size_t aNumBytes);

    /// optional data length
    size_t optDataLength();
    void setOptDataLength(size_t aNumBytes);

    /// packet type
    PacketType packetType();
    void setPacketType(PacketType aPacketType);

    /// calculated CRC of header
    uint8_t headerCRC();

    /// calculated CRC of payload, 0 if no payload
    uint8_t payloadCRC();

    /// @}


    /// @name access to raw data
    /// @{

    /// @return pointer to payload buffer. If no buffer exists, or header size fields have been changed,
    ///   a new empty buffer is allocated.
    uint8_t *data();

    /// @return pointer to optional data part of payload
    uint8_t *optData();

    /// @}



    /// @name access to generic radio telegram fields
    /// @{

    /// @return subtelegram number
    uint8_t radio_subtelegrams();

    /// @return destination address
    EnoceanAddress radio_destination();

    /// @return RSSI in dBm (negative, higher (more near zero) values = better signal)
    int radio_dBm();

    /// @return security level
    uint8_t radio_security_level();

    /// @return radio status byte
    uint8_t radio_status();

    /// @return sender's address
    EnoceanAddress radio_sender();

    /// @return the number of radio user data bytes
    size_t radio_userDataLength();

    /// @return pointer to the radio user data
    uint8_t *radio_userData();

    /// @}


    /// @name Enocean Equipment Profile (EEP) information
    /// @{

    /// @return RORG (radio telegram organisation, valid for all telegrams)
    RadioOrg eep_rorg();

    /// @return true if at least eep_func() has some valid information that can be used for teach-in
    ///   (is the case for specific teach-in telegrams in 1BS, 4BS, VLD, as well as all RPS telegrams)
    bool eep_hasTeachInfo();

    /// @return EEP signature as 0x00rrfftt (rr=RORG, ff=FUNC, tt=TYPE)
    ///   ff and tt can be func_unknown or type_unknown if not extractable from telegram
    EnoceanProfile eep_profile();

    /// @return EEP manufacturer code (11 bit), or manufacturer_unknown if not known
    EnoceanManufacturer eep_manufacturer();

    /// @}


    /// description
    string description();

  };


  typedef boost::function<void (EnoceanComm *aEnoceanCommP, Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)> RadioPacketCB;

  typedef boost::intrusive_ptr<EnoceanComm> EnoceanCommPtr;
	// Enocean communication
	class EnoceanComm : public SerialComm
	{
		typedef SerialComm inherited;
		
		Esp3PacketPtr currentIncomingPacket;
    RadioPacketCB radioPacketHandler;
		
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

    /// set callback to handle received radio packets 
    void setRadioPacketHandler(RadioPacketCB aRadioPacketCB);

  protected:

    /// dispatch received Esp3 packets to approriate receiver
    void dispatchPacket(Esp3PacketPtr aPacket);

	};



} // namespace p44


#endif /* defined(__vdcd__enoceancomm__) */
