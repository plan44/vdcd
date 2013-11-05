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
// - GS1-128 identified device namespace
#define DSID_GS128_NAMESPACE_UUID "8ca838d5-4c40-47cc-bafa-37ac89658962"
// - vDC namespace (to create a UUIDv5 for a vDC from the MAC address of the hardware)
#define DSID_VDC_NAMESPACE_UUID "9888dd3d-b345-4109-b088-2673306d0c65"
// - vdSM namespace (to create a UUIDv5 for a vdSM from the MAC address of the hardware)
#define DSID_VDSM_NAMESPACE_UUID "195de5c0-902f-4b71-a706-b43b80765e3d"

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
      dsidtype_classic, // 12-byte classic dsid
      dsidtype_gid, // classic dsid, but encoded as GID96 within dsUID
      dsidtype_sgtin, // dsUID based on SGTIN96
      dsidtype_uuid, // dsUID based on UUID
      dsidtype_other, // dsUID of not (yet) identifiable type
    } DsIdType;

    // new dsUID (SGTIN96, GID96 or UUID based)
    static const uint8_t SGTIN96Header = 0x30; ///< SGTIN96 8bit header byte
    static const uint8_t dsuidBytes = 17; ///< total bytes in a dsUID
    static const uint8_t uuidBytes = 16; ///< actual ID bytes (UUID or EPC96 mapped into UUID)

    // old 96 bit dsid
    static const uint8_t GID96Header = 0x35; ///< GID96 8bit header byte
    static const uint32_t ManagerNo = 0x04175FE; ///< 28bit Manager number (for Aizo GmbH)
    typedef uint32_t ObjectClass; ///< 24bit object class
    typedef uint64_t DsSerialNo; ///< 36bit serial no (up to 52bits for certain object classes such as MAC-address)
    static const uint8_t dsidBytes = 12; ///< total bytes in a (classic) dsid

    typedef uint8_t RawID[dsuidBytes];

  private:

    DsIdType idType; ///< the type of ID
    uint8_t idBytes; ///< the length of the ID in bytes
    RawID raw; ///< the raw dsid

    void internalInit();

    void setIdType(DsIdType aIdType);

  public:

    /// @name generic dsid operations
    /// @{

    /// create empty dsUID
    dSID();

    /// create dsUID from string
    /// @param aString string representing a dsid. Must be in one of the following formats
    /// - a 24 digit hex string for classic dsids
    /// - a 34 digit hex string for dsUIDs
    dSID(const string &aString);
    dSID(const char *aString);

    /// set as string, group separating dashes are allowed (but usually not needed)
    /// @param aString string representing a dsUID. Must be in one of the following formats
    /// - a 24 digit hex string for classic dsids
    /// - a 34 digit hex string for dsUIDs
    /// @return true if valid dsUID could be read
    bool setAsString(const string &aString);

    /// get dsUID in official string representation
    /// @return string representation of dsid, depending on the type
    /// - empty string for dsidtype_undefined
    /// - a 24 digit hex string for classic dsids
    /// - a 34 digit hex string for dsUIDs
    string getString() const;

    // comparison
    bool operator== (const dSID &aDSID) const;
    bool operator< (const dSID &aDSID) const;

    /// @}


    /// set the function/subdevice index of the dsUID
    /// @param aSubDeviceIndex a subdevice index. Devices containing multiple, logically independent subdevices
    ///   or functionality (like 2 or 4 buttons in one enOcean device) must use this index to differentiate
    ///   the subdevices.
    void setSubdeviceIndex(uint8_t aSubDeviceIndex);


    /// @name SGTIN96 based dsUIDs
    /// @{

    /// set the GTIN part of the dsid
    /// @param aGCP the global company prefix
    /// @param aItemRef the item reference
    /// @param aPartition the partition value (encoding the length of the CGP in the GTIN)
    void setGTIN(uint64_t aGCP, uint64_t aItemRef, uint8_t aPartition);

    /// set the serial part of the dsid
    /// @param aSerial a maximally 38bit long serial number
    void setSerial(uint64_t aSerial);

    /// @}


    /// @name UUID based dsUIDs
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
  typedef boost::intrusive_ptr<dSID> dSIDPtr;


} // namespace p44

#endif /* defined(__vdcd__dsid__) */
