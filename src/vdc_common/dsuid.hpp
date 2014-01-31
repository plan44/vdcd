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

#ifndef __vdcd__dsid__
#define __vdcd__dsid__

#include "vdcd_common.hpp"


// Name spaces for UUID based dSUIDs
// - enOcean device namespace
#define DSUID_ENOCEAN_NAMESPACE_UUID "0ba94a7b-7c92-4dab-b8e3-5fe09e83d0f3"
// - GS1-128 identified device namespace
#define DSUID_GS128_NAMESPACE_UUID "8ca838d5-4c40-47cc-bafa-37ac89658962"
// - vDC namespace (to create a UUIDv5 for a vDC from the MAC address of the hardware)
#define DSUID_VDC_NAMESPACE_UUID "9888dd3d-b345-4109-b088-2673306d0c65"
// - vdSM namespace (to create a UUIDv5 for a vdSM from the MAC address of the hardware)
#define DSUID_VDSM_NAMESPACE_UUID "195de5c0-902f-4b71-a706-b43b80765e3d"

// - plan44 vDC implementation namespace (used for generating vDC-implementation specific dsids)
#define DSUID_P44VDC_NAMESPACE_UUID "441A1FED-F449-4058-BEBA-13B1C4AB6A93"

// components for classic dsids
#define DSID_OBJECTCLASS_DSDEVICE 0x000000
#define DSID_OBJECTCLASS_DSMETER 0x000001
#define DSID_OBJECTCLASS_MACADDRESS 0xFF0000



using namespace std;

namespace p44 {

  class DsUid : public P44Obj
  {
  public:
    /// type of ID
    typedef enum {
      idtype_undefined,
      idtype_classic, // 12-byte classic dSUID
      idtype_gid, // classic dSUID, but encoded as GID96 within dSUID
      idtype_sgtin, // dSUID based on SGTIN96
      idtype_uuid, // dSUID based on UUID
      idtype_other, // dSUID of not (yet) identifiable type
    } DsUidType;

    // new dSUID (SGTIN96, GID96 or UUID based)
    static const uint8_t SGTIN96Header = 0x30; ///< SGTIN96 8bit header byte
    static const uint8_t dsuidBytes = 17; ///< total bytes in a dSUID
    static const uint8_t uuidBytes = 16; ///< actual ID bytes (UUID or EPC96 mapped into UUID)

    // old 96 bit dSUID
    static const uint8_t GID96Header = 0x35; ///< GID96 8bit header byte
    static const uint32_t ManagerNo = 0x04175FE; ///< 28bit Manager number (for Aizo GmbH)
    typedef uint32_t ObjectClass; ///< 24bit object class
    typedef uint64_t DsSerialNo; ///< 36bit serial no (up to 52bits for certain object classes such as MAC-address)
    static const uint8_t dsidBytes = 12; ///< total bytes in a (classic) dSUID

    typedef uint8_t RawID[dsuidBytes];

  private:

    DsUidType idType; ///< the type of ID
    uint8_t idBytes; ///< the length of the ID in bytes
    RawID raw; ///< the raw dSUID

    void internalInit();

    void setIdType(DsUidType aIdType);

  public:

    /// @name generic dSUID operations
    /// @{

    /// create empty dSUID
    DsUid();

    /// create dSUID from string
    /// @param aString string representing a dSUID. Must be in one of the following formats
    /// - a 24 digit hex string for classic dsids
    /// - a 34 digit hex string for dsUIDs
    DsUid(const string &aString);
    DsUid(const char *aString);

    /// set as string, group separating dashes are allowed (but usually not needed)
    /// @param aString string representing a dSUID. Must be in one of the following formats
    /// - a 24 digit hex string for classic dsids
    /// - a 34 digit hex string for dsUIDs
    /// - a 32 digit UUID, containing at least one dash (position does not matter)
    /// @return true if valid dSUID could be read
    bool setAsString(const string &aString);

    /// get dSUID in official string representation
    /// @return string representation of dSUID, depending on the type
    /// - empty string for idtype_undefined
    /// - a 24 digit hex string for classic dsids
    /// - a 34 digit hex string for dsUIDs
    string getString() const;

    // comparison
    bool operator== (const DsUid &aDsUid) const;
    bool operator< (const DsUid &aDsUid) const;

    // test
    // @return true if empty (no value assigned)
    bool empty() const;

    // clear, make empty()==true
    void clear();

    /// @}


    /// set the function/subdevice index of the dSUID
    /// @param aSubDeviceIndex a subdevice index. Devices containing multiple, logically independent subdevices
    ///   or functionality (like 2 or 4 buttons in one enOcean device) must use this index to differentiate
    ///   the subdevices.
    void setSubdeviceIndex(uint8_t aSubDeviceIndex);


    /// @name SGTIN96 based dsUIDs
    /// @{

    /// set the GTIN part of the dSUID
    /// @param aGCP the global company prefix
    /// @param aItemRef the item reference
    /// @param aPartition the partition value (encoding the length of the CGP in the GTIN)
    void setGTIN(uint64_t aGCP, uint64_t aItemRef, uint8_t aPartition);

    /// set the serial part of the dSUID
    /// @param aSerial a maximally 38bit long serial number
    void setSerial(uint64_t aSerial);

    /// @}


    /// @name UUID based dsUIDs
    /// @{

    /// create UUIDv5 from namespace ID + name
    /// @param aName the name part (unique identifier within the name space)
    /// @param aNameSpace a UUID-type dSUID representing the name space
    void setNameInSpace(const string &aName, const DsUid &aNameSpace);

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

    /// get classic dsid derived via standard hashing procedure from dSUID
    /// @return a classic 12-byte dsid in the terminal block domain (32 bit serial) with same
    ///   subdeviceindex as the original one (only for 0..7 range) and MSB (bit31) set to
    ///   distinguish it from any real terminal block dsid (those have smaller serial numbers)
    DsUid getDerivedClassicId() const;


    /// @}

  };
  typedef boost::intrusive_ptr<DsUid> DsUidPtr;


} // namespace p44

#endif /* defined(__vdcd__dsid__) */
