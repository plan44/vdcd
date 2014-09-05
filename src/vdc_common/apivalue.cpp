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

#include "apivalue.hpp"


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
  if (aType!=objectType) {
    objectType = aType;
    // type has changed, make sure internals are cleared
    clear();
  }
}


int ApiValue::arrayLength()
{
  return 0;
}


void ApiValue::clear()
{
  switch (objectType) {
    // "Zero" simple values
    case apivalue_bool:
      setBoolValue(false);
      break;
    case apivalue_int64:
    case apivalue_uint64:
      setUint64Value(0);
      break;
    case apivalue_double:
      setDoubleValue(0);
      break;
    case apivalue_string:
      setStringValue("");
      break;
    // stuctured values need to be handled in derived class
    default:
      break;
  }
}



// getting and setting as string (for all basic types)

string ApiValue::stringValue()
{
  switch (objectType) {
    case apivalue_bool:
      return boolValue() ? "true" : "false";
    case apivalue_int64:
      return string_format("%lld", int64Value());
    case apivalue_uint64:
      return string_format("%llu", uint64Value());
    case apivalue_double:
      return string_format("%f", doubleValue());
    case apivalue_binary:
      return "<binary>";
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


bool ApiValue::setStringValue(const string &aString)
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


string ApiValue::description()
{
  string s;
  bool firstElem = true;
  if (objectType==apivalue_object) {
    string k;
    ApiValuePtr v;
    resetKeyIteration();
    s = "{ ";
    while (nextKeyValue(k,v)) {
      if (!firstElem) s += ", ";
      firstElem = false;
      // add key
      s += k;
      s += ":";
      // add value
      s += v->description();
    }
    s += " }";
  }
  else if(objectType==apivalue_array) {
    ApiValuePtr v;
    int i = 0;
    s = "[ ";
    while (i<arrayLength()) {
      if (!firstElem) s += ", ";
      firstElem = false;
      // add value
      v = arrayGet(i);
      s += v->description();
      i++;
    }
    s += " ]";
  }
  else if (objectType==apivalue_string) {
    s = shellQuote(stringValue());
  }
  else if (objectType==apivalue_binary) {
    string b = binaryValue();
    if (b.size()>20) {
      b.erase(20); // truncate to 20
      s = binaryToHexString(b);
      string_format_append(s, "... (%ld bytes)", b.size());
    }
    else {
      s = binaryToHexString(b);
    }
  }
  else {
    // must be simple type
    s = stringValue();
  }
  return s;
}



// factory methods
ApiValuePtr ApiValue::newInt64(int64_t aInt64)
{
  ApiValuePtr newVal = newValue(apivalue_int64);
  newVal->setInt64Value(aInt64);
  return newVal;
}


ApiValuePtr ApiValue::newUint64(uint64_t aUint64)
{
  ApiValuePtr newVal = newValue(apivalue_uint64);
  newVal->setUint64Value(aUint64);
  return newVal;
}


ApiValuePtr ApiValue::newDouble(double aDouble)
{
  ApiValuePtr newVal = newValue(apivalue_double);
  newVal->setDoubleValue(aDouble);
  return newVal;
}


ApiValuePtr ApiValue::newBool(bool aBool)
{
  ApiValuePtr newVal = newValue(apivalue_bool);
  newVal->setBoolValue(aBool);
  return newVal;
}


ApiValuePtr ApiValue::newBinary(const string &aBinary)
{
  ApiValuePtr newVal = newValue(apivalue_binary);
  newVal->setBinaryValue(aBinary);
  return newVal;
}


ApiValuePtr ApiValue::newString(const char *aString)
{
  if (!aString) aString = "";
  return newString(string(aString));
}


ApiValuePtr ApiValue::newString(const string &aString)
{
  ApiValuePtr newVal = newValue(apivalue_string);
  newVal->setStringValue(aString);
  return newVal;
}


ApiValuePtr ApiValue::newObject()
{
  return newValue(apivalue_object);
}


ApiValuePtr ApiValue::newArray()
{
  return newValue(apivalue_array);
}


ApiValuePtr ApiValue::newNull()
{
  return newValue(apivalue_null);
}




// get in different int types

uint8_t ApiValue::uint8Value()
{
  return uint64Value() & 0xFF;
}

uint16_t ApiValue::uint16Value()
{
  return uint64Value() & 0xFFFF;
}

uint32_t ApiValue::uint32Value()
{
  return uint64Value() & 0xFFFFFFFF;
}


int8_t ApiValue::int8Value()
{
  return (int8_t)int64Value();
}


int16_t ApiValue::int16Value()
{
  return (int16_t)int64Value();
}


int32_t ApiValue::int32Value()
{
  return (int32_t)int64Value();
}

// set in different int types

void ApiValue::setUint8Value(uint8_t aUint8)
{
  setUint64Value(aUint8);
}

void ApiValue::setUint16Value(uint16_t aUint16)
{
  setUint64Value(aUint16);
}

void ApiValue::setUint32Value(uint32_t aUint32)
{
  setUint64Value(aUint32);
}

void ApiValue::setInt8Value(int8_t aInt8)
{
  setInt64Value(aInt8);
}

void ApiValue::setInt16Value(int16_t aInt16)
{
  setInt64Value(aInt16);
}

void ApiValue::setInt32Value(int32_t aInt32)
{
  setInt64Value(aInt32);
}




// convenience utilities

size_t ApiValue::stringLength()
{
  return stringValue().length();
}


bool ApiValue::setStringValue(const char *aCString)
{
  return setStringValue(aCString ? string(aCString) : "");
}


bool ApiValue::setStringValue(const char *aCStr, size_t aLen)
{
  string s;
  if (aCStr) s.assign(aCStr, aLen);
  return setStringValue(s);
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



#pragma mark - StandaloneApiValue


