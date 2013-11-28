//
//  apivalue.h
//  vdcd
//
//  Created by Lukas Zeller on 27.11.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__apivalue__
#define __vdcd__apivalue__

#include "p44_common.hpp"

#ifndef VDC_API_NO_JSON
#include "jsonobject.hpp"
#endif

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
  } ApiValueType;



  class ApiValue;

  typedef boost::intrusive_ptr<ApiValue> ApiValuePtr;

  typedef map<string, ApiValuePtr> ApiValueFieldMap;
  typedef vector<ApiValuePtr> ApiValueArray;

  /// wrapper around json-c / libjson0 object
  class ApiValue : public P44Obj
  {
    ApiValueType objectType;

  public:

    /// construct empty object
    ApiValue();

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

    /// generic string value (works for all types)
    /// @return value as string
    virtual string stringValue();

    /// set string value (works for all types)
    /// @param aString value as string to be set. Must match type of object for successful assignment
    /// @return true if assignment was successful, false otherwise
    bool setStringValue(const string &aString, bool aEmptyIsNull = false);


    /// utilities
    virtual size_t stringLength();
    bool setStringValue(const char *aCString);
    bool setStringValue(const char *aCStr, size_t aLen);
    virtual const char *c_strValue();
    bool isNull();
    void setNull();
    string lowercaseStringValue();


    #ifndef VDC_API_NO_JSON
    JsonObjectPtr jsonObject() { return JsonObjectPtr(); };
    #endif

  };



  #ifndef VDC_API_NO_JSON

  class JsonApiValue : public ApiValue
  {
    typedef ApiValue inherited;

    // using an embedded Json Object
    JsonObjectPtr jsonObj;

    JsonApiValue(JsonObjectPtr aWithObject);

  public:

    JsonApiValue();

    static ApiValuePtr newApiObject(JsonObjectPtr aWithObject);

    virtual void setType(ApiValueType aType);

    virtual void add(const string &aKey, ApiValuePtr aObj) { if (jsonObj) jsonObj->add(aKey.c_str(), aObj->jsonObject()); };
    virtual ApiValuePtr get(const string &aKey)  { JsonObjectPtr o; if (jsonObj && jsonObj->get(aKey.c_str(), o)) return newApiObject(o); else return ApiValuePtr(); };
    virtual void del(const string &aKey) { if (jsonObj) jsonObj->del(aKey.c_str()); };
    virtual int arrayLength() { return jsonObj ? jsonObj->arrayLength() : 0; };
    virtual void arrayAppend(ApiValuePtr aObj) { if (jsonObj) jsonObj->arrayAppend(aObj->jsonObject()); };
    virtual ApiValuePtr arrayGet(int aAtIndex) { if (jsonObj) { JsonObjectPtr o = jsonObj->arrayGet(aAtIndex); return newApiObject(o); } else return ApiValuePtr(); };
    virtual void arrayPut(int aAtIndex, ApiValuePtr aObj) { if (jsonObj) jsonObj->arrayPut(aAtIndex, aObj->jsonObject()); };
    virtual bool resetKeyIteration() { if (jsonObj) return jsonObj->resetKeyIteration(); else return false; };
    virtual bool nextKeyValue(string &aKey, ApiValuePtr &aValue) { if (jsonObj) { JsonObjectPtr o; bool gotone = jsonObj->nextKeyValue(aKey, o); aValue = newApiObject(o); return gotone; } else return false; };

    virtual uint64_t uint64Value() { return jsonObj ? (uint64_t)jsonObj->int64Value() : 0; };
    virtual int64_t int64Value() { return jsonObj ? jsonObj->int64Value() : 0; };
    virtual double doubleValue() { return jsonObj ? jsonObj->doubleValue() : 0; };
    virtual bool boolValue() { return jsonObj ? jsonObj->boolValue() : false; };
    virtual string stringValue() { if (getType()==apivalue_string) { return jsonObj ? jsonObj->stringValue() : ""; } else return inherited::stringValue(); };

    virtual void setUint64Value(uint64_t aUint64) { jsonObj = JsonObject::newInt64(aUint64); }
    virtual void setInt64Value(int64_t aInt64) { jsonObj = JsonObject::newInt64(aInt64); };
    virtual void setDoubleValue(double aDouble) { jsonObj = JsonObject::newDouble(aDouble); };
    virtual void setBoolValue(bool aBool) { jsonObj = JsonObject::newBool(aBool); };
    virtual bool setStringValue(const string &aString, bool aEmptyIsNull = false) { if (getType()==apivalue_string) { jsonObj = JsonObject::newString(aString, aEmptyIsNull); return true; } else return inherited::setStringValue(aString, aEmptyIsNull); };
    virtual void setNull() { jsonObj.reset(); }

    JsonObjectPtr jsonObject() { return jsonObj; };

  protected:


  };

  #endif // not VDC_API_NO_JSON


  class StandaloneApiValue : public ApiValue
  {
    typedef ApiValue inherited;

    // the actual storage
    union {
      bool boolVal;
      uint32_t uint64Val;
      int64_t int64Val;
      double doubleVal;
      string *stringP;
      ApiValueFieldMap *objectMapP;
      ApiValueArray *objectArrayP;
    } objectValue;
  };


}


#endif /* defined(__vdcd__apivalue__) */
