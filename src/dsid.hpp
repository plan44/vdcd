//
//  dsid.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__dsid__
#define __p44bridged__dsid__

#include "vdcd_common.hpp"

#define DSID_OBJECTCLASS_DSDEVICE 0x000000
#define DSID_OBJECTCLASS_DSMETER 0x000001
#define DSID_OBJECTCLASS_MACADDRESS 0xFF0000


using namespace std;

namespace p44 {

  class dSID
  {
  public:
    static const uint8_t GID96Header = 0x35; ///< GID96 8bit header byte
    static const uint32_t ManagerNo = 0x04175FE; ///< 28bit Manager number (for Aizo GmbH)
    typedef uint32_t ObjectClass; ///< 24bit object class
    typedef uint64_t SerialNo; ///< 36bit serial no (up to 52bits for certain object classes such as MAC-address)
    static const uint8_t dsidBytes = 12;
    typedef union {
      uint8_t bytes[dsidBytes];
    } RawID;
  private:
    RawID raw; ///< the raw 96bit (12*8) dsid
    void internalInit();
  public:
    /// create empty dSID (GID96Header and ManagerNo set, rest zeroed)
    dSID();

    /// create dsID from string
    /// @param aString 24 digit hex string, dashed allowed (ignored) everywhere, any non-hex character stops parsing
    dSID(string &aString);

    // comparison
    bool operator== (const dSID &aDSID) const;
    bool operator< (const dSID &aDSID) const;

    /// set object class
    /// @param aObjectClass 24 bit object class. If 0xFF0000, aSerialNo can be up to 52 bits
    void setObjectClass(ObjectClass aObjectClass);

    /// set serial number
    /// @param aSerialNo 36 bit serial number except if object class is already set 0xFFxxxx, in this case
    ///   aSerialNo can be up to 52 bits.
    ///   dS defines only 48 bits for MAC address (bits 48..51 in aSerialNo zero)
    ///   This method maps bits 48..51 of aSerial into bits 32..35 of the serial number field
    void setSerialNo(SerialNo aSerialNo);


    /// set as string, group separating dashes are allowed
    /// @param aString 24 digit hex string, dashed allowed (ignored) everywhere, any non-hex character stops parsing
    /// @return true if 24 digits read, false otherwise
    bool setAsString(string &aString);


    /// get raw dSID
    /// @return RawID union consisting of 12 bytes
    RawID getRawId() const;

    /// get pointer to raw dSID bytes
    /// @return pointer to array of 12 dsid bytes
    const uint8_t *getBytesP() const;

    /// get dSID as hex string
    /// @return string representation of dsid as 24 hex digits
    string getString() const;

    /// get object class
    /// @return 24bit object class. For object class 0xFFxxxx, xxxx is
    //    returned as 0 (these bits are mapped into result of getSerialNo()
    ObjectClass getObjectClass() const;

    /// get upper 8 bit of object class
    /// @return upper 8bit of object class
    uint8_t getObjectClassMSB() const;

    /// get serial number
    /// @return 36bit serial number, except for object class 0xFFxxxx,
    ///   where up to 52bits can be returned (16 bits from xxxx in object class)
    SerialNo getSerialNo() const;

  };

} // namespace p44

#endif /* defined(__p44bridged__dsid__) */
