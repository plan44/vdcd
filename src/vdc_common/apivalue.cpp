//
//  apivalue.cpp
//  vdcd
//
//  Created by Lukas Zeller on 27.11.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "apivalue.h"


using namespace p44;


#pragma mark - ApiValue


ApiValue::ApiValue() :
  objectType(apivalue_null)
{
}


bool ApiValue::isType(ApiValueType aObjectType)
{
  return (objectType==aObjectType);
}


ApiValueType ApiValue::getType()
{
  return objectType;
}


void ApiValue::setType(ApiValueType aType)
{
  // base class: just set type
  objectType = aType;
}


int ApiValue::arrayLength()
{
  return 0;
}


// getting and setting as string (for all basic types)

string ApiValue::stringValue()
{
  switch (objectType) {
    case apivalue_bool:
      return boolValue() ? "true" : "false";
    case apivalue_int64:
      return string_format("%ld", int64Value());
    case apivalue_uint64:
      return string_format("%ud", uint64Value());
    case apivalue_double:
      return string_format("%f", doubleValue());
    case apivalue_object:
      return "<object>";
    case apivalue_array:
      return "<array>";
    case apivalue_null:
    case apivalue_string: // if actual type is string, derived class should have delivered it
    default:
      return "";
  }
}


bool ApiValue::setStringValue(const string &aString, bool aEmptyIsNull)
{
  int n;
  switch (objectType) {
    case apivalue_bool: {
      string s = lowerCase(aString);
      setBoolValue(s.length()>0 && s!="false" && s!="0" && s!="no");
      return true;
    }
    case apivalue_int64: {
      int64_t intVal;
      n = sscanf(aString.c_str(), "%lld", &intVal);
      if (n==1) setInt64Value(intVal);
      return n==1;
    }
    case apivalue_uint64: {
      uint64_t uintVal;
      n = sscanf(aString.c_str(), "%llu", &uintVal);
      if (n==1) setUint64Value(uintVal);
      return n==1;
    }
    case apivalue_double: {
      double doubleVal;
      n = sscanf(aString.c_str(), "%lf", &doubleVal);
      if (n==1) setDoubleValue(doubleVal);
      return n==1;
    }
    default:
      break;
  }
  // cannot set as string
  return false;
}


// convenience utilities

size_t ApiValue::stringLength()
{
  return stringValue().length();
}


bool ApiValue::setStringValue(const char *aCString)
{
  return setStringValue(aCString ? string(aCString) : "", false);
}


bool ApiValue::setStringValue(const char *aCStr, size_t aLen)
{
  string s;
  if (aCStr) s.assign(aCStr, aLen);
  return setStringValue(s, false);
}


const char *ApiValue::c_strValue()
{
  return stringValue().c_str();
}


bool ApiValue::isNull()
{
  return getType()==apivalue_null;
}


void ApiValue::setNull()
{
  setType(apivalue_null);
}


string ApiValue::lowercaseStringValue()
{
  return lowerCase(stringValue());
}



#pragma mark - JsonApiValue

JsonApiValue::JsonApiValue()
{
}


JsonApiValue::JsonApiValue(JsonObjectPtr aWithObject)
{
  jsonObj = aWithObject;
  // derive type
  switch (jsonObj->type()) {
    case json_type_boolean: setType(apivalue_bool); break;
    case json_type_double: setType(apivalue_double); break;
    case json_type_int: setType(apivalue_int64); break;
    case json_type_object: setType(apivalue_object); break;
    case json_type_array: setType(apivalue_array); break;
    case json_type_string: setType(apivalue_string); break;
    case json_type_null:
    default:
      setType(apivalue_null);
      break;
  }
}


ApiValuePtr JsonApiValue::newApiObject(JsonObjectPtr aWithObject)
{
  return ApiValuePtr(new JsonApiValue(aWithObject));
}


void JsonApiValue::setType(ApiValueType aType)
{
  if (aType!=getType()) {
    inherited::setType(aType);
    jsonObj.reset(); // no value yet
  }
}




#pragma mark - StandaloneApiValue


