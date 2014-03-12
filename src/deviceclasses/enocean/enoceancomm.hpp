//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __vdcd__enoceancomm__
#define __vdcd__enoceancomm__

#include "vdcd_common.hpp"

#include "serialqueue.hpp"
#include "digitalio.hpp"

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
  typedef uint8_t EepFunc;
  typedef uint8_t EepType;

  // unknown markers
  const EepFunc eep_func_unknown = 0xFF;
  const EepType eep_type_unknown = 0xFF;
  const EnoceanProfile eep_profile_unknown = (rorg_invalid<<16) + (eep_func_unknown<<8) + eep_type_unknown;
  const EnoceanProfile eep_ignore_type_mask = 0xFFFF00;

  // EEP access macros
  #define EEP_RORG(eep) ((RadioOrg)(((EnoceanProfile)eep>>16)&0xFF))
  #define EEP_FUNC(eep) ((EepFunc)(((EnoceanProfile)eep>>8)&0xFF))
  #define EEP_TYPE(eep) ((EepType)((EnoceanProfile)eep&0xFF))

  // learn bit
  #define LRN_BIT_MASK 0x08 // Bit 3, Byte 0 (4th data byte)

  // common commands
  #define CO_WR_SLEEP 0x01 // Order to enter in energy saving mode Order to reset the device
  #define CO_WR_RESET 0x02 // Reset
  #define CO_RD_VERSION 0x03 // Read the device (SW) version / (HW) version, chip ID etc.
  #define CO_RD_SYS_LOG 0x04 // Read system log from device databank
  #define CO_WR_SYS_LOG 0x05 // Reset System log from device databank
  #define CO_WR_BIST 0x06 // Perform Flash BIST operation
  #define CO_WR_IDBASE 0x07 // Write ID range base number
  #define CO_RD_IDBASE 0x08 // Read ID range base number
  #define CO_WR_REPEATER 0x09 // Write Repeater Level off,1,2
  #define CO_RD_REPEATER 0x0A // Read Repeater Level off,1,2
  #define CO_WR_FILTER_ADD 0x0B // Add filter to filter list
  #define CO_WR_FILTER_DEL 0x0C // Delete filter from filter list
  #define CO_WR_FILTER_DEL_ALL 0x0D // Delete all filter
  #define CO_WR_FILTER_ENABLE 0x0E // Enable/Disable supplied filters
  #define CO_RD_FILTER 0x0F // Read supplied filters
  #define CO_WR_WAIT_MATURITY 0x10 // Waiting till end of maturity time before received radio telegrams will transmitted
  #define CO_WR_SUBTEL 0x11 // Enable/Disable transmitting additional subtelegram info
  #define CO_WR_MEM 0x12 // Write x bytes of the Flash, XRAM, RAM0 ....
  #define CO_RD_MEM 0x13 // Read x bytes of the Flash, XRAM, RAM0 ....
  #define CO_RD_MEM_ADDRESS 0x14 // Feedback about the used address and length of the config area and the Smart Ack Table
  #define CO_RD_SECURITY 0x15 // Read security information (level, keys)
  #define CO_WR_SECURITY 0x16 // Write security information (level, keys)

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
    typedef P44Obj inherited;

    friend class EnoceanComm;

  public:
    typedef enum {
      ps_syncwait,
      ps_headerread,
      ps_dataread,
      ps_complete
    } PacketState;


  private:
    // packet contents
    uint8_t header[6]; ///< the ESP3 header
    uint8_t *payloadP; ///< the payload or NULL if none defined
    size_t payloadSize; ///< the payload size
    // scanner
    PacketState state; ///< scanning state
    size_t dataIndex; ///< data scanner index

    
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

    /// clear the packet so that we can re-start accepting bytes and looking for packet start or
    /// start filling in information for creating an outgoing packet
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


    /// finalize packet to make it ready for sending (complete header fields, calculate CRCs)
    void finalize();


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

    /// initialize packet for specific radio telegram
    /// @param aRadioOrg the radio organisation type for the packet
    /// @param aVLDsize only for variable length packets, length of the radio data
    /// @note this initializes payload storage such that it can be accessed at radio_userData()
    void initForRorg(RadioOrg aRadioOrg, size_t aVLDsize=0);

    /// @return subtelegram number
    uint8_t radioSubtelegrams();

    /// @return destination address
    EnoceanAddress radioDestination();

    /// @param aEnoceanAddress the destination address to set
    void setRadioDestination(EnoceanAddress aEnoceanAddress);

    /// @return RSSI in dBm (negative, higher (more near zero) values = better signal)
    int radioDBm();

    /// @return security level
    uint8_t radioSecurityLevel();

    /// @return security level
    void setRadioSecurityLevel(uint8_t aSecLevel);

    /// @return radio status byte
    uint8_t radioStatus();

    /// @return sender's address
    EnoceanAddress radioSender();

    /// @param radio sender's address to set
    /// @note enOcean modules will normally insert their native address here,
    ///   so usually there's no point in setting this
    void setRadioSender(EnoceanAddress aEnoceanAddress);

    /// @return the number of radio user data bytes
    size_t radioUserDataLength();

    /// @param the number of radio user data bytes
    void setRadioUserDataLength(size_t aSize);

    /// @return pointer to the radio user data
    uint8_t *radioUserData();

    /// @}


    /// @name Enocean Equipment Profile (EEP) information
    /// @{

    /// @return RORG (radio telegram organisation, valid for all telegrams)
    RadioOrg eepRorg();

    /// @param aMinLearnDBm if!=0, learn-in info must have at least aMinLearnDBm radio signal strength
    ///   for implicit learn-in information (RPS switches, window handle, key card)
    /// @param aMinDBmForAll if set, all learn-in is considered valid only when aMinLearnDBm signal strength is found
    /// @return true if at least eep_func() has some valid information that can be used for teach-in
    ///   (is the case for specific teach-in telegrams in 1BS, 4BS, VLD, as well as all RPS telegrams)
    bool eepHasTeachInfo(int aMinLearnDBm=0, bool aMinDBmForAll=false);

    /// @return EEP signature as 0x00rrfftt (rr=RORG, ff=FUNC, tt=TYPE)
    ///   ff and tt can be func_unknown or type_unknown if not extractable from telegram
    EnoceanProfile eepProfile();

    /// @return EEP manufacturer code (11 bit), or manufacturer_unknown if not known
    EnoceanManufacturer eepManufacturer();

    /// @}


    /// @name 4BS four byte data packet communication
    /// @{

    /// @return radioUserData()[0..3] as 32bit value
    uint32_t get4BSdata();

    /// @param a4BSdata radioUserData()[0..3] as 32bit value
    void set4BSdata(uint32_t a4BSdata);


    /// @param aEEProfile the EEP to represent in the 4BS telegram
    void set4BSTeachInEEP(EnoceanProfile aEEProfile);


    /// @}


    /// description
    string description();

  };


  typedef boost::function<void (EnoceanComm &aEnoceanComm, Esp3PacketPtr aEsp3PacketPtr, ErrorPtr aError)> RadioPacketCB;

  typedef boost::intrusive_ptr<EnoceanComm> EnoceanCommPtr;
	// Enocean communication
	class EnoceanComm : public SerialOperationQueue
	{
		typedef SerialOperationQueue inherited;
		
		Esp3PacketPtr currentIncomingPacket;
    RadioPacketCB radioPacketHandler;

    DigitalIoPtr enoceanResetPin;
    long aliveCheckTicket;
    long aliveTimeoutTicket;
		
	public:
		
		EnoceanComm(SyncIOMainLoop &aMainLoop);
		virtual ~EnoceanComm();
		
    /// set the connection parameters to connect to the enOcean TCM310 modem
    /// @param aConnectionSpec serial device path (/dev/...) or host name/address[:port] (1.2.3.4 or xxx.yy)
    /// @param aDefaultPort default port number for TCP connection (irrelevant for direct serial device connection)
    /// @param aEnoceanResetPinName name of a DigitalIO pin connected to an enOcean module's reset pin (active HI), or NULL if none
    void setConnectionSpecification(const char *aConnectionSpec, uint16_t aDefaultPort, const char *aEnoceanResetPinName);
		
    /// derived implementation: deliver bytes to the ESP3 parser
    /// @param aNumBytes number of bytes ready for accepting
    /// @param aBytes pointer to bytes buffer
    /// @return number of bytes parser could accept (normally, all)
    virtual size_t acceptBytes(size_t aNumBytes, uint8_t *aBytes);

    /// set callback to handle received radio packets 
    void setRadioPacketHandler(RadioPacketCB aRadioPacketCB);

    /// send a packet
    /// @param aPacket a Esp4Packet which must be ready for being finalize()d
    void sendPacket(Esp3PacketPtr aPacket);

    /// manufacturer name lookup
    /// @param aManufacturerCode EEP manufacturer code
    /// @return manufacturer name string
    static const char *manufacturerName(EnoceanManufacturer aManufacturerCode);


  protected:

    /// dispatch received Esp3 packets to approriate receiver
    void dispatchPacket(Esp3PacketPtr aPacket);

  private:

    void aliveCheck();
    void aliveCheckTimeout();
    void aliveCheckOK();
    void resetDone();

	};



} // namespace p44


#endif /* defined(__vdcd__enoceancomm__) */
