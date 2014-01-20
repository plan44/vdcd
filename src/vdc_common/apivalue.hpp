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

#ifndef __vdcd__apivalue__
#define __vdcd__apivalue__

#include "p44_common.hpp"

using namespace std;

namespace p44 {


  typedef enum {
    apivalue_null,
    apivalue_bool,
    apivalue_int64,
    apivalue_uint64,
    apivalue_double,
    apivalue_string, // std::string
    apivalue_object, // object containing multiple named ApiValues
    apivalue_array, // array of multiple ApiValues
    apivalue_proxy, // not used in actual ApiValues, but reserved to describe properties
  } ApiValueType;



  class ApiValue;

  typedef boost::intrusive_ptr<ApiValue> ApiValuePtr;

  /// wrapper around json-c / libjson0 object
  class ApiValue : public P44Obj
  {
  protected:

    ApiValueType objectType;

  public:

    /// construct empty object
    ApiValue();

    /// create new API value of same implementation variant as this object
    virtual ApiValuePtr newValue(ApiValueType aObjectType) = 0;

    /// check if object is of given type
    /// @param aObjectType type to check for
    /// @return true if object matches given type
    bool isType(ApiValueType aObjectType);

    /// get the current type
    /// @return type code
    ApiValueType getType();

    /// set a new type
    /// @param type to convert object into
    /// @note existing data will be discarded (not converted)!
    virtual void setType(ApiValueType aType);

    /// clear object to "empty" or "zero" value of its type
    /// @note does not change the type (unlike setNull)
    virtual void clear();


    /// add object for key
    /// @param aKey key of object
    virtual void add(const string &aKey, ApiValuePtr aObj) = 0;

    /// get object by key
    /// @param aKey key of object
    /// @return NULL pointer of key does not exists, value otherwise
    /// @note to distinguish between having no such key and having the key with
    ///   a NULL object, use get(aKey,aObj) instead
    virtual ApiValuePtr get(const string &aKey) = 0;

    /// delete object by key
    /// @param aKey key of object
    virtual void del(const string &aKey) = 0;

    /// get array length
    /// @return length of array. Returns 0 for empty arrays and all non-array objects
    virtual int arrayLength();

    /// append to array
    /// @param aObj object to append to the array
    virtual void arrayAppend(ApiValuePtr aObj) = 0;

    /// get from a specific position in the array
    /// @param aAtIndex index position to return value for
    /// @return NULL pointer if element does not exist, value otherwise
    virtual ApiValuePtr arrayGet(int aAtIndex) = 0;

    /// put at specific position in array
    /// @param aAtIndex index position to put value to (overwriting existing value at that position)
    /// @param aObj object to store in the array
    /// @note aAtIndex must point to an existing element (method does not extend the array)
    virtual void arrayPut(int aAtIndex, ApiValuePtr aObj) = 0;

    /// reset object iterator
    /// @return false if object cannot be iterated
    virtual bool resetKeyIteration() = 0;

    /// get next key/value pair from object
    /// @param aKey will be set to the next key
    /// @param aValue will be set to the next value
    /// @return false if no more key/values
    virtual bool nextKeyValue(string &aKey, ApiValuePtr &aValue) = 0;

    /// @name simple value accessors
    /// @{

    virtual uint64_t uint64Value() = 0;
    virtual int64_t int64Value() = 0;
    virtual double doubleValue() = 0;
    virtual bool boolValue() = 0;

    virtual void setUint64Value(uint64_t aUint64) = 0;
    virtual void setInt64Value(int64_t aInt64) = 0;
    virtual void setDoubleValue(double aDouble) = 0;
    virtual void setBoolValue(bool aBool) = 0;

    /// @}


    /// @name factory methods
    /// @{

    ApiValuePtr newInt64(int64_t aInt64);
    ApiValuePtr newUint64(uint64_t aUint64);
    ApiValuePtr newDouble(double aDouble);
    ApiValuePtr newBool(bool aBool);
    ApiValuePtr newString(const char *aString);
    ApiValuePtr newString(const string &aString);
    ApiValuePtr newObject();
    ApiValuePtr newArray();

    /// @}


    /// generic string value (works for all types)
    /// @return value as string
    virtual string stringValue();

    /// set string value (works for all types)
    /// @param aString value as string to be set. Must match type of object for successful assignment
    /// @return true if assignment was successful, false otherwise
    virtual bool setStringValue(const string &aString);


    /// get in different int types
    uint8_t uint8Value();
    uint16_t uint16Value();
    uint32_t uint32Value();
    int8_t int8Value();
    int16_t int16Value();
    int32_t int32Value();

    /// set in different int types
    void setUint8Value(uint8_t aUint8);
    void setUint16Value(uint16_t aUint16);
    void setUint32Value(uint32_t aUint32);
    void setInt8Value(int8_t aInt8);
    void setInt16Value(int16_t aInt16);
    void setInt32Value(int32_t aInt32);



    /// utilities
    virtual size_t stringLength();
    bool setStringValue(const char *aCString);
    bool setStringValue(const char *aCStr, size_t aLen);
    bool isNull();
    void setNull();
    string lowercaseStringValue();

    /// human readable content of the value
    string description();

  };


}


#endif /* defined(__vdcd__apivalue__) */
