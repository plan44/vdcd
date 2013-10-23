//
//  dsid.hpp
//  vdcd
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__dsid__
#define __vdcd__dsid__

#include "vdcd_common.hpp"


// UUID based dsids
// - enOcean device namespace
#define DSID_ENOCEAN_NAMESPACE_UUID "0ba94a7b-7c92-4dab-b8e3-5fe09e83d0f3"
// - plan44 vDC implementation namespace (used for generating vDC-implementation specific dsids)
#define DSID_P44VDC_NAMESPACE_UUID "441A1FED-F449-4058-BEBA-13B1C4AB6A93"

// classic dsids
#define DSID_OBJECTCLASS_DSDEVICE 0x000000
#define DSID_OBJECTCLASS_DSMETER 0x000001
#define DSID_OBJECTCLASS_MACADDRESS 0xFF0000



using namespace std;

namespace p44 {

  class dSID : public P44Obj
  {
  public:
    /// type of ID
    typedef enum {
      dsidtype_undefined,
      dsidtype_classic,
      dsidtype_sgtin,
      dsidtype_uuid
    } DsIdType;

    // new SGTIN96 or UUID(128) dsid
    static const uint8_t sgtinBytes = 12;
    static const uint8_t uuidBytes = 16;
    static const uint8_t rawBytes = 16; // must be max of the above

    // old 96 bit dsid
    static const uint8_t GID96Header = 0x35; ///< GID96 8bit header byte
    static const uint32_t ManagerNo = 0x04175FE; ///< 28bit Manager number (for Aizo GmbH)
    typedef uint32_t ObjectClass; ///< 24bit object class
    typedef uint64_t DsSerialNo; ///< 36bit serial no (up to 52bits for certain object classes such as MAC-address)
    static const uint8_t classicBytes = 12;
    typedef union {
      uint8_t classic[classicBytes];
      uint8_t sgtin[sgtinBytes];
      uint8_t uuid[uuidBytes];
      uint8_t raw[uuidBytes]; // largest
    } RawID;

  private:

    DsIdType idType; ///< the type of ID
    uint8_t idBytes; ///< the length of the ID in bytes
    RawID raw; ///< the raw dsid

    void internalInit();

    void setIdType(DsIdType aIdType);

    bool setAsSGTIN(const string &aString);
    bool setAsUUID(const string &aString);
    bool setAsClassic(const string &aString);
    bool setAsHex(const string &aString);


  public:

    /// @name generic dsid operations
    /// @{

    /// create empty dSID
    dSID();

    /// create dsID from string
    /// @param aString string representing a dsid. Must be in one of the following formats
    /// - a 24 digit hex string without any dashes for classic dsids
    /// - a UUID in standard notation with dashes, i.e. 4by-2by-2by-2by-6by
    /// - a sgtin in standard notation, i.e. ???? %%%% TODO: specify!
    dSID(const string &aString);
    dSID(const char *aString);

    /// set as string, group separating dashes are allowed
    /// @param aString string representing a dsid. Must be in one of the following formats
    /// - a 24 digit hex string without any dashes for classic dsids
    /// - a UUID in standard notation with dashes, i.e. 4by-2by-2by-2by-6by
    /// - a sgtin in standard notation, i.e. ???? %%%% TODO: specify!
    /// @return true if 24 digits read, false otherwise
    bool setAsString(const string &aString);

    /// get dSID in official string representation
    /// @return string representation of dsid, depending on the type
    /// - empty string for dsidtype_undefined
    /// - a 24 digit hex string without any dashes for classic dsids
    /// - a UUID in standard notation with dashes, i.e. 4by-2by-2by-2by-6by
    /// - a sgtin in standard notation, i.e. ???? %%%% TODO: specify!
    string getString() const;

    // comparison
    bool operator== (const dSID &aDSID) const;
    bool operator< (const dSID &aDSID) const;

    /// @}


    /// @name SGTIN96 based dsids
    /// @{

    /// set the GTIN part of the dsid
    /// @param aGTIN a GTIN (global trade indentifier Number, GS1 number consisting of GCP + product identifier)
    /// @param aPartition the partition value (encoding the length of the CGP in the GTIN)
    void setGTIN(uint64_t aGTIN, uint8_t aPartition);

    /// set the serial part of the dsid
    /// @param aSerial a maximally 38bit long serial number
    void setSerial(uint64_t aSerial);

    /// @}


    /// @name UUID based dsids
    /// @{

    /// create UUIDv5 from namespace ID + name
    /// @param aName the name part (unique identifier within the name space)
    /// @param aNameSpace a UUID-type dsid representing the name space
    void setNameInSpace(const string &aName, const dSID &aNameSpace);

    /// @}


    /// @name classic dsids
    /// @{

    /// set object class
    /// @param aObjectClass 24 bit object class. If 0xFF0000, aSerialNo can be up to 52 bits
    void setObjectClass(ObjectClass aObjectClass);

    /// set serial number
    /// @param aSerialNo 36 bit serial number except if object class is already set 0xFFxxxx, in this case
    ///   aSerialNo can be up to 52 bits.
    ///   dS defines only 48 bits for MAC address (bits 48..51 in aSerialNo zero)
    ///   This method maps bits 48..51 of aSerial into bits 32..35 of the serial number field
    void setDsSerialNo(DsSerialNo aSerialNo);

    /// get upper 8 bit of object class
    /// @return upper 8bit of object class
    uint8_t getObjectClassMSB() const;

    /// @}

  };

} // namespace p44

#endif /* defined(__vdcd__dsid__) */
