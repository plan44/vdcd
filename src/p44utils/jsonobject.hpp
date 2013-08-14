//
//  jsonobject.hpp
//  p44utils
//
//  Created by Lukas Zeller on 25.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__jsonobject__
#define __p44utils__jsonobject__

#include "p44_common.hpp"

#if DIGI_ESP
#include "json.h"
#else
#include <json/json.h>
#endif

#include <boost/intrusive_ptr.hpp>



using namespace std;

namespace p44 {

  class JsonObject;

  /// shared pointer for JSON object
  typedef boost::shared_ptr<JsonObject> JsonObjectPtr;


  /// wrapper around json-c / libjson0 object
  class JsonObject
  {
    friend class JsonComm;

    struct json_object *json_obj;

    /// construct object as wrapper of json-c json_object.
    /// @param obj json_object, ownership is passed into this JsonObject, caller looses ownership!
    JsonObject(struct json_object *aObjPassingOwnership);

    /// factory to return smart pointer to new wrapper of a newly created json_object
    /// @param obj json_object, ownership is passed into this JsonObject, caller looses ownership!
    static JsonObjectPtr newObj(struct json_object *aObjPassingOwnership);

    /// construct empty object
    JsonObject();

  public:

    /// destructor, releases internally kept json_object (which is possibly owned by other objects)
    virtual ~JsonObject();

    /// get type
    json_type type();

    /// check type
    bool isType(json_type aRefType);

    /// string representation of object.
    const char *json_c_str(int aFlags=0);
    string json_str(int aFlags=0);


    /// add object for key
    void add(const char* aKey, JsonObjectPtr aObj);

    /// get object by key
    /// @note to distingusih between having no such key and having the key with
    ///   a NULL object, use get(aKey,aJsonObject) instead
    JsonObjectPtr get(const char *aKey);

    /// get object by key
    /// @return true if key exists (but aJsonObject might still be empty in case of a NULL object)
    bool get(const char *aKey, JsonObjectPtr &aJsonObject);

    /// get object's string value by key
    /// @return NULL if key does not exists or actually has NULL value, string object otherwise
    /// @note the returned C string pointer is valid only as long as the object is not deleted
    const char *getCString(const char *aKey);

    /// delete object by key
    void del(const char *aKey);


    /// get array length
    int arrayLength();

    /// append to array
    void arrayAppend(JsonObjectPtr aObj);

    /// get from a specific position in the array
    JsonObjectPtr arrayGet(int aAtIndex);

    /// put at specific position in array
    void arrayPut(int aAtIndex, JsonObjectPtr aObj);


    /// create new empty object
    static JsonObjectPtr newObj();

    /// create new object from text
    static JsonObjectPtr objFromText(const char *aJsonText, ssize_t aMaxChars = -1);

    /// create new array object
    static JsonObjectPtr newArray();


    /// create new boolean object
    static JsonObjectPtr newBool(bool aBool);
    /// get boolean value
    bool boolValue();

    /// create int objects
    static JsonObjectPtr newInt32(int32_t aInt32);
    static JsonObjectPtr newInt64(int64_t aInt64);
    /// get int values
    int32_t int32Value();
    int64_t int64Value();

    /// create double object
    static JsonObjectPtr newDouble(double aDouble);
    /// get double value
    double doubleValue();

    /// create new string object
    static JsonObjectPtr newString(const char *aCStr);
    static JsonObjectPtr newString(const char *aCStr, size_t aLen);
    static JsonObjectPtr newString(const string &aString);
    /// get string value
    const char *c_strValue();
    size_t stringLength();
    string stringValue();
    string lowercaseStringValue();

  };

} // namespace

#endif /* defined(__p44utils__jsonobject__) */
