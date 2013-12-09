//
//  Copyright (c) 2013 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
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


  // Errors
  typedef enum json_tokener_error JsonErrors;

  class JsonError : public Error
  {
  public:
    static const char *domain() { return "JsonObject"; }
    virtual const char *getErrorDomain() const { return JsonError::domain(); };
    JsonError(JsonErrors aError) : Error(ErrorCode(aError), json_tokener_error_desc(aError)) {};
    JsonError(JsonErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };
  
  

  class JsonObject;

  /// shared pointer for JSON object
  typedef boost::intrusive_ptr<JsonObject> JsonObjectPtr;


  /// wrapper around json-c / libjson0 object
  class JsonObject : public P44Obj
  {
    struct json_object *json_obj; ///< the json-c object

    struct lh_entry *nextEntryP; ///< iterator pointer for resetKeyIteration()/nextKeyValue()

    /// construct object as wrapper of json-c json_object.
    /// @param obj json_object, ownership is passed into this JsonObject, caller looses ownership!
    JsonObject(struct json_object *aObjPassingOwnership);

    /// construct empty object
    JsonObject();

  public:

    /// destructor, releases internally kept json_object (which is possibly owned by other objects)
    virtual ~JsonObject();

    /// factory to return smart pointer to new wrapper of a newly created json_object
    /// @param obj json_object, ownership is passed into this JsonObject, caller looses ownership!
    static JsonObjectPtr newObj(struct json_object *aObjPassingOwnership);

    /// get type
    /// @return type code
    json_type type();

    /// check type
    /// @param aRefType type to check for
    /// @return true if object matches given type
    bool isType(json_type aRefType);

    /// string representation of object.
    const char *json_c_str(int aFlags=0);
    string json_str(int aFlags=0);


    /// add object for key
    void add(const char* aKey, JsonObjectPtr aObj);

    /// get object by key
    /// @param aKey key of object
    /// @return the value of the object
    /// @note to distinguish between having no such key and having the key with
    ///   a NULL object, use get(aKey,aJsonObject) instead
    JsonObjectPtr get(const char *aKey);

    /// get object by key
    /// @param aKey key of object
    /// @param aJsonObject will be set to the value of the key when return value is true
    /// @return true if key exists (but aJsonObject might still be empty in case of a NULL object)
    bool get(const char *aKey, JsonObjectPtr &aJsonObject);

    /// get object's string value by key
    /// @return NULL if key does not exists or actually has NULL value, string object otherwise
    /// @note the returned C string pointer is valid only as long as the object is not deleted
    const char *getCString(const char *aKey);

    /// delete object by key
    void del(const char *aKey);


    /// get array length
    /// @return length of array. Returns 0 for empty arrays and all non-array objects
    int arrayLength();

    /// append to array
    /// @param aObj object to append to the array
    void arrayAppend(JsonObjectPtr aObj);

    /// get from a specific position in the array
    /// @param aAtIndex index position to return value for
    /// @return NULL pointer if element does not exist, value otherwise
    JsonObjectPtr arrayGet(int aAtIndex);

    /// put at specific position in array
    /// @param aAtIndex index position to put value to (overwriting existing value at that position)
    /// @param aObj object to store in the array
    void arrayPut(int aAtIndex, JsonObjectPtr aObj);


    /// reset object iterator
    /// @return false if object cannot be iterated
    bool resetKeyIteration();

    /// reset object iterator
    /// @return false if no more key/values
    bool nextKeyValue(string &aKey, JsonObjectPtr &aValue);


    /// create new empty object
    static JsonObjectPtr newObj();

    /// create new NULL object (does not embed a real JSON-C object, just a NULL pointer)
    static JsonObjectPtr newNull();


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
    static JsonObjectPtr newString(const string &aString, bool aEmptyIsNull = false);
    /// get string value
    const char *c_strValue();
    size_t stringLength();
    string stringValue();
    string lowercaseStringValue();

  };

} // namespace

#endif /* defined(__p44utils__jsonobject__) */
