//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "jsonobject.hpp"

#if DIGI_ESP
#include "json_object_private.h"
#else
#include <json/json_object_private.h> // needed for _ref_count
#endif

using namespace p44;


#pragma mark - private constructors / destructor


// construct from raw json_object, passing ownership
JsonObject::JsonObject(struct json_object *aObjPassingOwnership) :
  nextEntryP(NULL)
{
  json_obj = aObjPassingOwnership;
}


// construct empty
JsonObject::JsonObject()
{
  json_obj = json_object_new_object();
}


JsonObject::~JsonObject()
{
  if (json_obj) {
    json_object_put(json_obj);
    json_obj = NULL;
  }
}


#pragma mark - type


json_type JsonObject::type()
{
  return json_object_get_type(json_obj);
}


bool JsonObject::isType(json_type aRefType)
{
  return json_object_is_type(json_obj, aRefType);
}



#pragma mark - conversion to string

const char *JsonObject::json_c_str(int aFlags)
{
  return json_object_to_json_string_ext(json_obj, aFlags);
}


string JsonObject::json_str(int aFlags)
{
  return string(json_c_str(aFlags));
}


#pragma mark - add, get and delete by key

void JsonObject::add(const char* aKey, JsonObjectPtr aObj)
{
  // json_object_object_add assumes caller relinquishing ownership,
  // so we must compensate this by retaining (getting) the object
  // as the object still belongs to us
  // Except if a NULL (no object) is passed
  json_object_object_add(json_obj, aKey, aObj ? json_object_get(aObj->json_obj) : NULL);
}


bool JsonObject::get(const char *aKey, JsonObjectPtr &aJsonObject)
{
  json_object *weakObjRef = NULL;
  if (json_object_object_get_ex(json_obj, aKey, &weakObjRef)) {
    // found object, but can be the NULL object (which will return no JsonObjectPtr
    if (weakObjRef==NULL) {
      aJsonObject = JsonObjectPtr(); // no object
    }
    else {
      // - claim ownership as json_object_object_get_ex does not do that automatically
      json_object_get(weakObjRef);
      // - create wrapper
      aJsonObject = newObj(weakObjRef);
    }
    return true; // key exists, but returned object might still be NULL
  }
  return false; // key does not exist, aJsonObject unchanged
}



JsonObjectPtr JsonObject::get(const char *aKey)
{
  JsonObjectPtr p;
  get(aKey, p);
  return p;
}


const char *JsonObject::getCString(const char *aKey)
{
  JsonObjectPtr p = get(aKey);
  if (p)
    return p->c_strValue();
  return NULL;
}



void JsonObject::del(const char *aKey)
{
  json_object_object_del(json_obj, aKey);
}


#pragma mark - arrays


int JsonObject::arrayLength()
{
  if (type()!=json_type_array)
    return 0; // normal objects don't have a length
  else
    return json_object_array_length(json_obj);
}


void JsonObject::arrayAppend(JsonObjectPtr aObj)
{
  if (type()==json_type_array) {
    // - claim ownership as json_object_array_add does not do that automatically
    json_object_get(aObj->json_obj);
    json_object_array_add(json_obj, aObj->json_obj);
  }
}


JsonObjectPtr JsonObject::arrayGet(int aAtIndex)
{
  JsonObjectPtr p;
  json_object *weakObjRef = json_object_array_get_idx(json_obj, aAtIndex);
  if (weakObjRef) {
    // found object
    // - claim ownership as json_object_object_get_ex does not do that automatically
    json_object_get(weakObjRef);
    // - return wrapper
    p = newObj(weakObjRef);
  }
  return p;
}


void JsonObject::arrayPut(int aAtIndex, JsonObjectPtr aObj)
{
  if (type()==json_type_array) {
    // - claim ownership as json_object_array_put_idx does not do that automatically
    json_object_get(aObj->json_obj);
    json_object_array_put_idx(json_obj, aAtIndex, aObj->json_obj);
  }
}


#pragma mark - object key/value iteration


bool JsonObject::resetKeyIteration()
{
  if (isType(json_type_object)) {
    nextEntryP = json_object_get_object(json_obj)->head;
    return true; // can be iterated (but might still have zero key/values)
  }
  return false; // cannot be iterated
}


bool JsonObject::nextKeyValue(string &aKey, JsonObjectPtr &aValue)
{
  if (nextEntryP) {
    // get key
    aKey = (char*)nextEntryP->k;
    // get value
    json_object *weakObjRef = (struct json_object*)nextEntryP->v;
    if (weakObjRef) {
      // claim ownership
      json_object_get(weakObjRef);
      // - return wrapper
      aValue = newObj(weakObjRef);
    }
    else {
      aValue = JsonObjectPtr(); // NULL
    }
    // advance to next
    nextEntryP = nextEntryP->next;
    return true;
  }
  // no more entries
  return false;
}


#pragma mark - factories and value getters

// private wrapper factory from newly created json_object (ownership passed in)
JsonObjectPtr JsonObject::newObj(struct json_object *aObjPassingOwnership)
{
  return JsonObjectPtr(new JsonObject(aObjPassingOwnership));
}



JsonObjectPtr JsonObject::newObj()
{
  return JsonObjectPtr(new JsonObject());
}


JsonObjectPtr JsonObject::newNull()
{
  // create wrapper with no embedded object (as a plain C NULL pointer represents NULL in JSON-C)
  return JsonObjectPtr(new JsonObject(NULL));
}


JsonObjectPtr JsonObject::objFromText(const char *aJsonText, ssize_t aMaxChars)
{
  JsonObjectPtr obj;
  if (aMaxChars<0) aMaxChars = strlen(aJsonText);
  struct json_tokener* tokener = json_tokener_new();
  struct json_object *o = json_tokener_parse_ex(tokener, aJsonText, (int)aMaxChars);
  if (o) {
    obj = JsonObject::newObj(o);
  }
  json_tokener_free(tokener);
  return obj;
}



JsonObjectPtr JsonObject::newArray()
{
  return JsonObjectPtr(new JsonObject(json_object_new_array()));
}




JsonObjectPtr JsonObject::newBool(bool aBool)
{
  return newObj(json_object_new_boolean(aBool));
}

bool JsonObject::boolValue()
{
  return json_object_get_boolean(json_obj);
}


JsonObjectPtr JsonObject::newInt32(int32_t aInt32)
{
  return newObj(json_object_new_int(aInt32));
}

JsonObjectPtr JsonObject::newInt64(int64_t aInt64)
{
  return newObj(json_object_new_int64(aInt64));
}

int32_t JsonObject::int32Value()
{
  return json_object_get_int(json_obj);
}

int64_t JsonObject::int64Value()
{
  return json_object_get_int64(json_obj);
}


JsonObjectPtr JsonObject::newDouble(double aDouble)
{
  return newObj(json_object_new_double(aDouble));
}

double JsonObject::doubleValue()
{
  return json_object_get_double(json_obj);
}



JsonObjectPtr JsonObject::newString(const char *aCStr)
{
  if (!aCStr) return JsonObjectPtr();
  return newObj(json_object_new_string(aCStr));
}

JsonObjectPtr JsonObject::newString(const char *aCStr, size_t aLen)
{
  if (!aCStr) return JsonObjectPtr();
  return newObj(json_object_new_string_len(aCStr, (int)aLen));
}

JsonObjectPtr JsonObject::newString(const string &aString, bool aEmptyIsNull)
{
  if (aEmptyIsNull & aString.empty()) return JsonObjectPtr();
  return JsonObject::newString(aString.c_str());
}

const char *JsonObject::c_strValue()
{
  return json_object_get_string(json_obj);
}

size_t JsonObject::stringLength()
{
  return (size_t)json_object_get_string_len(json_obj);
}

string JsonObject::stringValue()
{
  return string(c_strValue());
}

string JsonObject::lowercaseStringValue()
{
  return lowerCase(c_strValue());
}






