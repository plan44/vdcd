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

#include "pbufvdcapi.hpp"


using namespace p44;



#pragma mark - PbufApiValue

PbufApiValue::PbufApiValue() :
  allocatedType(apivalue_null)
{
}


PbufApiValue::~PbufApiValue()
{
  clear();
}


ApiValuePtr PbufApiValue::newValue(ApiValueType aObjectType)
{
  ApiValuePtr newVal = ApiValuePtr(new PbufApiValue);
  newVal->setType(aObjectType);
  return newVal;
}


void PbufApiValue::operator=(ApiValue &aApiValue)
{
  setNull(); // forget old content
  PbufApiValue *pavP = dynamic_cast<PbufApiValue *>(&aApiValue);
  if (pavP) {
    setType(aApiValue.getType());
    allocate();
    switch (allocatedType) {
      case apivalue_string:
      case apivalue_binary:
        objectValue.stringP = new string(*(pavP->objectValue.stringP));
        break;
      case apivalue_object:
        objectValue.objectMapP = new ApiValueFieldMap(*(pavP->objectValue.objectMapP));
        break;
      case apivalue_array:
        objectValue.arrayVectorP = new ApiValueArray(*(pavP->objectValue.arrayVectorP));
        break;
      default:
        objectValue = pavP->objectValue; // copy union containing a scalar value
        break;
    }
  }
  else
    setNull(); // not assignable
}




void PbufApiValue::clear()
{
  // forget allocated type
  if (allocatedType!=apivalue_null) {
    switch (allocatedType) {
      case apivalue_string:
      case apivalue_binary:
        if (objectValue.stringP) delete objectValue.stringP;
        break;
      case apivalue_object:
        if (objectValue.objectMapP) delete objectValue.objectMapP;
        break;
      case apivalue_array:
        if (objectValue.arrayVectorP) delete objectValue.arrayVectorP;
        break;
      default:
        break;
    }
    // zero out union
    memset(&objectValue, 0, sizeof(objectValue));
    // is now a NULL object
    allocatedType = apivalue_null;
  }
}


void PbufApiValue::allocate()
{
  if (allocatedType!=getType() && allocatedType==apivalue_null) {
    allocatedType = getType();
    switch (allocatedType) {
      case apivalue_string:
      case apivalue_binary:
        objectValue.stringP = new string;
        break;
      case apivalue_object:
        objectValue.objectMapP = new ApiValueFieldMap;
        break;
      case apivalue_array:
        objectValue.arrayVectorP = new ApiValueArray;
        break;
      default:
        break;
    }
  }
}


bool PbufApiValue::allocateIf(ApiValueType aIsType)
{
  if (getType()==aIsType) {
    allocate();
    return true;
  }
  return false;
}



void PbufApiValue::add(const string &aKey, ApiValuePtr aObj)
{
  PbufApiValuePtr val = boost::dynamic_pointer_cast<PbufApiValue>(aObj);
  if (val && allocateIf(apivalue_object)) {
    (*(objectValue.objectMapP))[aKey] = val;
  }
}


ApiValuePtr PbufApiValue::get(const string &aKey)
{
  if (allocatedType==apivalue_object) {
    ApiValueFieldMap::iterator pos = objectValue.objectMapP->find(aKey);
    if (pos!=objectValue.objectMapP->end())
      return pos->second;
  }
  return ApiValuePtr();
}


void PbufApiValue::del(const string &aKey)
{
  if (allocatedType==apivalue_object) {
    objectValue.objectMapP->erase(aKey);
  }
}


int PbufApiValue::arrayLength()
{
  if (allocatedType==apivalue_array) {
    return (int)objectValue.arrayVectorP->size();
  }
  return 0;
}


void PbufApiValue::arrayAppend(ApiValuePtr aObj)
{
  PbufApiValuePtr val = boost::dynamic_pointer_cast<PbufApiValue>(aObj);
  if (val && allocateIf(apivalue_array)) {
    objectValue.arrayVectorP->push_back(val);
  }
}


ApiValuePtr PbufApiValue::arrayGet(int aAtIndex)
{
  if (allocatedType==apivalue_array) {
    if (aAtIndex<objectValue.arrayVectorP->size()) {
      return objectValue.arrayVectorP->at(aAtIndex);
    }
  }
  return ApiValuePtr();
}


void PbufApiValue::arrayPut(int aAtIndex, ApiValuePtr aObj)
{
  PbufApiValuePtr val = boost::dynamic_pointer_cast<PbufApiValue>(aObj);
  if (val && allocateIf(apivalue_array)) {
    if (aAtIndex<objectValue.arrayVectorP->size()) {
      ApiValueArray::iterator i = objectValue.arrayVectorP->begin() + aAtIndex;
      objectValue.arrayVectorP->erase(i);
      objectValue.arrayVectorP->insert(i, val);
    }
  }
}



size_t PbufApiValue::numObjectFields()
{
  if (allocatedType==apivalue_object) {
    return objectValue.objectMapP->size();
  }
  return 0;
}


bool PbufApiValue::resetKeyIteration()
{
  if (allocatedType==apivalue_object) {
    keyIterator = objectValue.objectMapP->begin();
  }
  return false; // cannot be iterated
}


bool PbufApiValue::nextKeyValue(string &aKey, ApiValuePtr &aValue)
{
  if (allocatedType==apivalue_object) {
    if (keyIterator!=objectValue.objectMapP->end()) {
      aKey = keyIterator->first;
      aValue = keyIterator->second;
      keyIterator++;
      return true;
    }
  }
  return false;
}



uint64_t PbufApiValue::uint64Value()
{
  if (allocatedType==apivalue_uint64) {
    return objectValue.uint64Val;
  }
  else if (allocatedType==apivalue_int64 && objectValue.int64Val>=0) {
    return objectValue.int64Val; // only return positive values
  }
  return 0;
}



int64_t PbufApiValue::int64Value()
{
  if (allocatedType==apivalue_int64) {
    return objectValue.int64Val;
  }
  else if (allocatedType==apivalue_uint64) {
    return objectValue.uint64Val & 0x7FFFFFFFFFFFFFFFll; // prevent returning sign
  }
  return 0;
}


double PbufApiValue::doubleValue()
{
  if (allocatedType==apivalue_double) {
    return objectValue.doubleVal;
  }
  else {
    return int64Value(); // int (or int derived from uint) can also be read as double
  }
}


bool PbufApiValue::boolValue()
{
  if (allocatedType==apivalue_bool) {
    return objectValue.boolVal;
  }
  else {
    return int64Value()!=0; // non-zero int is also true
  }
}


string PbufApiValue::binaryValue()
{
  if (allocatedType==apivalue_binary) {
    return *(objectValue.stringP);
  }
  else if (allocatedType==apivalue_string) {
    return hexToBinaryString(objectValue.stringP->c_str());
  }
  else {
    return ""; // not binary
  }
}


string PbufApiValue::stringValue()
{
  if (allocatedType==apivalue_string) {
    return *(objectValue.stringP);
  }
  else if (allocatedType==apivalue_binary) {
    // render as hex string
    return binaryToHexString(*(objectValue.stringP));
  }
  // let base class render the contents as string
  return inherited::stringValue();
}


void PbufApiValue::setUint64Value(uint64_t aUint64)
{
  if (allocateIf(apivalue_uint64)) {
    objectValue.uint64Val = aUint64;
  }
}


void PbufApiValue::setInt64Value(int64_t aInt64)
{
  if (allocateIf(apivalue_int64)) {
    objectValue.int64Val = aInt64;
  }
}


void PbufApiValue::setDoubleValue(double aDouble)
{
  if (allocateIf(apivalue_double)) {
    objectValue.doubleVal = aDouble;
  }
}


void PbufApiValue::setBoolValue(bool aBool)
{
  if (allocateIf(apivalue_bool)) {
    objectValue.boolVal = aBool;
  }
}


void PbufApiValue::setBinaryValue(const string &aBinary)
{
  if (allocateIf(apivalue_binary)) {
    objectValue.stringP->assign(aBinary);
  }
}


bool PbufApiValue::setStringValue(const string &aString)
{
  if (allocateIf(apivalue_string)) {
    objectValue.stringP->assign(aString);
    return true;
  }
  else if (allocateIf(apivalue_binary)) {
    // parse string as hex
    objectValue.stringP->assign(hexToBinaryString(aString.c_str()));
    return true;
  }
  else {
    // let base class try to convert to type of object
    return inherited::setStringValue(aString);
  }
}


void PbufApiValue::setNull()
{
  clear();
}



void PbufApiValue::getValueFromMessageField(const ProtobufCFieldDescriptor &aFieldDescriptor, const ProtobufCMessage &aMessage)
{
  const uint8_t *baseP = (const uint8_t *)(&aMessage);
  const void *fieldBaseP = baseP+aFieldDescriptor.offset;
  // check quantifier
  if (aFieldDescriptor.label==PROTOBUF_C_LABEL_REPEATED) {
    // repeated field
    size_t arraySize = *((size_t *)(baseP+aFieldDescriptor.quantifier_offset));
    // - check for special processing of PropertyElement arrays
    if (aFieldDescriptor.descriptor==&vdcapi__property_element__descriptor) {
      // add elements as key/val to myself
      if (arraySize==0)
        setNull(); // avoid empty object, return simple NULL value instead
      else {
        for (int i = 0; i<arraySize; i++) {
          // is an array, dereference the data pointer once to get to elements
          const Vdcapi__PropertyElement **elements = (const Vdcapi__PropertyElement **)(*((void **)fieldBaseP));
          addKeyValFromPropertyElementField(elements[i]);
        }
      }
    }
    else {
      // pack into array
      setType(apivalue_array);
      for (int i = 0; i<arraySize; i++) {
        PbufApiValuePtr element = PbufApiValuePtr(new PbufApiValue);
        element->setValueFromField(aFieldDescriptor, fieldBaseP, i, arraySize);
        arrayAppend(element);
      }
    }
  }
  else {
    // not repeated (array), but single field
    bool hasField = false;
    if (aFieldDescriptor.label==PROTOBUF_C_LABEL_OPTIONAL) {
      if (aFieldDescriptor.quantifier_offset) {
        // scalar that needs quantifier
        hasField = *((protobuf_c_boolean *)(baseP+aFieldDescriptor.quantifier_offset));
      }
      else {
        // value is a pointer, exists if not NULL
        hasField = *((const void **)(baseP+aFieldDescriptor.offset))!=NULL;
      }
    }
    else {
      // must be mandatory
      hasField = true;
    }
    // get value, if available
    if (!hasField) {
      // optional field is not present
      setNull();
    }
    else {
      // get value
      // - check special case of single PropertyElement
      if (aFieldDescriptor.descriptor==&vdcapi__property_element__descriptor) {
        // add element as single key/val to myself
        addKeyValFromPropertyElementField(*((const Vdcapi__PropertyElement **)fieldBaseP));
      }
      setValueFromField(aFieldDescriptor, fieldBaseP, 0, -1); // not array
    }
  }
}



void PbufApiValue::putValueIntoMessageField(const ProtobufCFieldDescriptor &aFieldDescriptor, const ProtobufCMessage &aMessage)
{
  uint8_t *baseP = (uint8_t *)(&aMessage);
  uint8_t *fieldBaseP = baseP+aFieldDescriptor.offset;
  // check quantifier
  if (aFieldDescriptor.label==PROTOBUF_C_LABEL_REPEATED) {
    // repeated field
    // - check special case of repeated Property Element
    if (aFieldDescriptor.descriptor==&vdcapi__property_element__descriptor && getType()==apivalue_object) {
      // - set size
      size_t numElems = numObjectFields();
      *((size_t *)(baseP+aFieldDescriptor.quantifier_offset)) = numElems;
      // - set contents
      Vdcapi__PropertyElement **elems = NULL;
      if (numElems>0) {
        elems = new Vdcapi__PropertyElement *[numElems];
        Vdcapi__PropertyElement **elemP = elems;
        // fill in fields
        resetKeyIteration();
        string key;
        ApiValuePtr val;
        while (nextKeyValue(key, val)) {
          PbufApiValuePtr pval = boost::dynamic_pointer_cast<PbufApiValue>(val);
          pval->storeKeyValIntoPropertyElementField(key, *(elemP++));
        }
      }
      *((Vdcapi__PropertyElement ***)fieldBaseP) = elems;
    }
    else if (getType()==apivalue_array) {
      // value is an array, just assign elements
      // - set size
      *((size_t *)(baseP+aFieldDescriptor.quantifier_offset)) = arrayLength();
      // iterate over existing elements
      for (int i = 0; i<arrayLength(); i++) {
        PbufApiValuePtr element = boost::dynamic_pointer_cast<PbufApiValue>(arrayGet(i));
        element->putValueIntoField(aFieldDescriptor, fieldBaseP, i, arrayLength());
      }
    }
    else {
      // non array value into repeated field - store as single repetition
      *((size_t *)(baseP+aFieldDescriptor.quantifier_offset)) = 1; // single element
      // put value into that single element
      putValueIntoField(aFieldDescriptor, fieldBaseP, 0, 1);
    }
  }
  else {
    // not repeated (array), but single field
    bool hasField;
    if (aFieldDescriptor.label==PROTOBUF_C_LABEL_OPTIONAL) {
      hasField = !isNull();
      if (aFieldDescriptor.quantifier_offset) {
        // scalar that needs quantifier
        *((protobuf_c_boolean *)(baseP+aFieldDescriptor.quantifier_offset)) = hasField;
      }
    }
    else {
      // must be mandatory
      hasField = true;
    }
    // put value, if available
    if (hasField) {
      // - check special case of single Property Element
      if (aFieldDescriptor.descriptor==&vdcapi__property_element__descriptor && getType()==apivalue_object) {
        // store first element of object (rest will be ignored)
        resetKeyIteration();
        string key;
        ApiValuePtr val;
        nextKeyValue(key, val);
        PbufApiValuePtr pval = boost::dynamic_pointer_cast<PbufApiValue>(val);
        pval->storeKeyValIntoPropertyElementField(key, *((Vdcapi__PropertyElement **)fieldBaseP));
      }
      else {
        putValueIntoField(aFieldDescriptor, fieldBaseP, 0, -1); // not array
      }
    }
  }
}




void PbufApiValue::getObjectFromMessageFields(const ProtobufCMessage &aMessage)
{
  // must be an object
  setType(apivalue_object);
  // iterate over fields
  const ProtobufCFieldDescriptor *fieldDescP = aMessage.descriptor->fields;
  for (unsigned f = 0; f<aMessage.descriptor->n_fields; f++) {
    PbufApiValuePtr field = PbufApiValuePtr(new PbufApiValue);
    field->getValueFromMessageField(*fieldDescP, aMessage);
    if (field->getType()!=apivalue_null) {
      // do not add NULL fields
      add(fieldDescP->name, field);
    }
    fieldDescP++; // next field descriptor
  }
}



void PbufApiValue::putObjectIntoMessageFields(ProtobufCMessage &aMessage)
{
  // only if we actually have any object data
  if (allocatedType==apivalue_object) {
    // iterate over fields
    const ProtobufCFieldDescriptor *fieldDescP = aMessage.descriptor->fields;
    for (unsigned f = 0; f<aMessage.descriptor->n_fields; f++) {
      // see if value object has a key for this field
      PbufApiValuePtr val = boost::dynamic_pointer_cast<PbufApiValue>(get(fieldDescP->name));
      if (val) {
        val->putValueIntoMessageField(*fieldDescP, aMessage);
      }
      fieldDescP++; // next field descriptor
    }
  }
}



void PbufApiValue::addObjectFieldFromMessage(const ProtobufCMessage &aMessage, const char* aFieldName)
{
  // must be an object
  setType(apivalue_object);
  const ProtobufCFieldDescriptor *fieldDescP = protobuf_c_message_descriptor_get_field_by_name(aMessage.descriptor, aFieldName);
  if (fieldDescP) {
    PbufApiValuePtr val = PbufApiValuePtr(new PbufApiValue);
    val->getValueFromMessageField(*fieldDescP, aMessage);
    if (!val->isNull()) {
      // don't add NULL values, because this means field was not set in message, which means no value, not NULL value
      add(aFieldName, val); // add with specified name
    }
  }
}



void PbufApiValue::putObjectFieldIntoMessage(ProtobufCMessage &aMessage, const char* aFieldName)
{
  if (isType(apivalue_object)) {
    const ProtobufCFieldDescriptor *fieldDescP = protobuf_c_message_descriptor_get_field_by_name(aMessage.descriptor, aFieldName);
    if (fieldDescP) {
      PbufApiValuePtr val = boost::dynamic_pointer_cast<PbufApiValue>(get(aFieldName));
      if (val) {
        val->putValueIntoMessageField(*fieldDescP, aMessage);
      }
    }
  }
}




void PbufApiValue::setValueFromField(const ProtobufCFieldDescriptor &aFieldDescriptor, const void *aData, size_t aIndex, ssize_t aArraySize)
{
  if (aArraySize>=0) {
    // is an array, dereference the data pointer once to get to elements
    aData = *((void **)aData);
  }
  switch (aFieldDescriptor.type) {
    case PROTOBUF_C_TYPE_BOOL:
      setType(apivalue_bool);
      setBoolValue(*((protobuf_c_boolean *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      setType(apivalue_int64);
      setInt32Value(*((int32_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      setType(apivalue_int64);
      setInt64Value(*((int64_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      setType(apivalue_uint64);
      setUint32Value(*((uint32_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      setType(apivalue_uint64);
      setUint64Value(*((uint64_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_FLOAT:
      setType(apivalue_double);
      setDoubleValue(*((float *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_DOUBLE:
      setType(apivalue_double);
      setDoubleValue(*((double *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_ENUM:
      setType(apivalue_int64);
      setInt32Value(*((int *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_STRING:
      setType(apivalue_string);
      setStringValue(*((const char **)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_BYTES: {
      setType(apivalue_binary);
      ProtobufCBinaryData *p = (ProtobufCBinaryData *)aData+aIndex;
      string b = string((const char *)p->data, p->len);
      setBinaryValue(b);
      break;
    }
    case PROTOBUF_C_TYPE_MESSAGE: {
      // submessage, pack into object value
      const ProtobufCMessage *subMessageP = *((const ProtobufCMessage **)aData+aIndex);
      setType(apivalue_object);
      getObjectFromMessageFields(*subMessageP);
      break;
    }
    default:
      setType(apivalue_null);
      break;
  }
}



void PbufApiValue::putValueIntoField(const ProtobufCFieldDescriptor &aFieldDescriptor, void *aData, size_t aIndex, ssize_t aArraySize)
{
  // check array case
  bool allocArray = false;
  void *dataP = aData; // default to in-place data (no array)
  if (aArraySize>=0) {
    // check if array is allocated already
    if (*((void **)aData)==NULL) {
      // array does not yet exist
      allocArray = true;
      dataP = NULL;
    }
    else {
      // array does exist, data pointer can be used as-is
      dataP = *((void **)aData); // dereference once
    }
  }
  switch (aFieldDescriptor.type) {
    case PROTOBUF_C_TYPE_BOOL:
      if (allocatedType==apivalue_bool) {
        if (allocArray) dataP = new protobuf_c_boolean[aArraySize];
        *((protobuf_c_boolean *)dataP+aIndex) = boolValue();
      }
      break;
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      if (allocatedType==apivalue_int64 || allocatedType==apivalue_uint64) {
        if (allocArray) dataP = new int32_t[aArraySize];
        *((int32_t *)dataP+aIndex) = int32Value();
      }
      break;
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      if (allocatedType==apivalue_int64 || allocatedType==apivalue_uint64) {
        if (allocArray) dataP = new int64_t[aArraySize];
        *((int64_t *)dataP+aIndex) = int64Value();
      }
      break;
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      if (allocatedType==apivalue_uint64 || allocatedType==apivalue_int64) {
        if (allocArray) dataP = new uint32_t[aArraySize];
        *((uint32_t *)dataP+aIndex) = uint32Value();
      }
      break;
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      if (allocatedType==apivalue_uint64 || allocatedType==apivalue_int64) {
        if (allocArray) dataP = new uint64_t[aArraySize];
        *((uint64_t *)dataP+aIndex) = uint64Value();
      }
      break;
    case PROTOBUF_C_TYPE_FLOAT:
      if (allocatedType==apivalue_double) {
        if (allocArray) dataP = new float[aArraySize];
        *((float *)dataP+aIndex) = doubleValue();
      }
      break;
    case PROTOBUF_C_TYPE_DOUBLE:
      if (allocatedType==apivalue_double) {
        if (allocArray) dataP = new double[aArraySize];
        *((double *)dataP+aIndex) = doubleValue();
      }
      break;
    case PROTOBUF_C_TYPE_ENUM:
      if (allocatedType==apivalue_uint64) {
        if (allocArray) dataP = new int[aArraySize];
        *((int *)dataP+aIndex) = int32Value();
      }
      break;
    case PROTOBUF_C_TYPE_STRING:
      if (allocatedType==apivalue_string || allocatedType==apivalue_binary) {
        if (allocArray) {
          dataP = new char*[aArraySize];
          memset(dataP, 0, aArraySize*sizeof(char *)); // null the array
        }
        string s = stringValue(); // might also be binary converted to hex string
        char * p = new char [s.size()+1];
        strcpy(p, s.c_str());
        *((char **)dataP+aIndex) = p;
      }
      break;
    case PROTOBUF_C_TYPE_BYTES:
      if (allocatedType==apivalue_binary) {
        if (allocArray) {
          dataP = new ProtobufCBinaryData[aArraySize];
          memset(dataP, 0, aArraySize*sizeof(ProtobufCBinaryData)); // null the array
        }
        string b = binaryValue();
        uint8_t *p = new uint8_t[b.size()];
        memcpy(p, b.c_str(), b.size());
        ((ProtobufCBinaryData *)dataP+aIndex)->data = p;
        ((ProtobufCBinaryData *)dataP+aIndex)->len = b.size();
        break;
      }
    case PROTOBUF_C_TYPE_MESSAGE: {
      // submessage
      // - field is a message, we might need to allocate the pointer array
      if (allocArray) {
        dataP = new void *[aArraySize];
        memset(dataP, 0, aArraySize*sizeof(void *)); // null the array
      }
      if (allocatedType==apivalue_object && !allocArray) {
        ProtobufCMessage *aSubMessageP = *((ProtobufCMessage **)dataP+aIndex);
        if (aSubMessageP) {
          // submessage exists, have it filled in
          putObjectIntoMessageFields(*aSubMessageP);
        }
      }
      break;
    }
    default:
      // do nothing
      break;
  }
  if (allocArray) {
    // array was allocated new, link into message
    *((void **)aData) = dataP;
  }
}


//  struct  _Vdcapi__PropertyElement
//  {
//    ProtobufCMessage base;
//    char *name;
//    Vdcapi__PropertyValue *value;
//    size_t n_elements;
//    Vdcapi__PropertyElement **elements;
//  };

void PbufApiValue::addKeyValFromPropertyElementField(const Vdcapi__PropertyElement *aPropertyElementP)
{
  // add the property element as a key/val to myself (and make me object)
  // in a key/val, there must always be a value
  PbufApiValuePtr val = PbufApiValuePtr(new PbufApiValue);
  if (aPropertyElementP->value) {
    // simple value
    val->getValueFromPropVal(*aPropertyElementP->value);
  }
  else if (aPropertyElementP->n_elements) {
    // nested object, "elements" is field #2
    val->getValueFromMessageField(aPropertyElementP->base.descriptor->fields[2], aPropertyElementP->base);
  }
  else {
    // neither value nor object -> NULL
    val->setNull();
  }
  // get the name
  const char *name = aPropertyElementP->name;
  if (!name) name="<none>";
  // add it now
  setType(apivalue_object);
  add(name,val);
}



void PbufApiValue::storeKeyValIntoPropertyElementField(string aKey, Vdcapi__PropertyElement *&aPropertyElementP)
{
  // create a PropertyElement and store my value plus specified key into
  // - create the element
  aPropertyElementP = new Vdcapi__PropertyElement;
  vdcapi__property_element__init(aPropertyElementP);
  // - store the value/subvalues
  if (isType(apivalue_object)) {
    // create nested value, "elements" is field #2
    putValueIntoMessageField(aPropertyElementP->base.descriptor->fields[2], aPropertyElementP->base);
  }
  else if (!isNull()) {
    // create the value
    aPropertyElementP->value = new Vdcapi__PropertyValue;
    vdcapi__property_value__init(aPropertyElementP->value);
    // store value
    putValueIntoPropVal(*aPropertyElementP->value);
  }
  // store name
  aPropertyElementP->name = new char[aKey.size()+1];
  strcpy(aPropertyElementP->name, aKey.c_str());
}






void PbufApiValue::getValueFromPropVal(Vdcapi__PropertyValue &aPropVal)
{
  if (aPropVal.has_v_bool) {
    setType(apivalue_bool);
    setBoolValue(aPropVal.v_bool);
  }
  else if (aPropVal.has_v_int64) {
    setType(apivalue_int64);
    setInt64Value(aPropVal.v_int64);
  }
  else if (aPropVal.has_v_uint64) {
    setType(apivalue_uint64);
    setUint64Value(aPropVal.v_uint64);
  }
  else if (aPropVal.has_v_double) {
    setType(apivalue_double);
    setDoubleValue(aPropVal.v_double);
  }
  else if (aPropVal.v_string) {
    setType(apivalue_string);
    setStringValue(aPropVal.v_string);
  }
  else if (aPropVal.has_v_bytes) {
    setType(apivalue_binary);
    setBinaryValue(string((const char *)aPropVal.v_bytes.data, aPropVal.v_bytes.len));
  }
  else {
    // null value
    setType(apivalue_null);
  }
}


void PbufApiValue::putValueIntoPropVal(Vdcapi__PropertyValue &aPropVal)
{
  switch (allocatedType) {
    case apivalue_bool:
      aPropVal.has_v_bool = true;
      aPropVal.v_bool = boolValue();
      break;
    case apivalue_int64:
      aPropVal.has_v_int64 = true;
      aPropVal.v_int64 = int64Value();
      break;
    case apivalue_uint64:
      aPropVal.has_v_uint64 = true;
      aPropVal.v_uint64 = uint64Value();
      break;
    case apivalue_double:
      aPropVal.has_v_double = true;
      aPropVal.v_double = doubleValue();
      break;
    case apivalue_string: {
      string s = stringValue();
      char *p = new char [s.size()+1];
      strcpy(p, s.c_str());
      aPropVal.v_string = p;
      break;
    }
    case apivalue_binary: {
      aPropVal.has_v_bytes = true;
      string b = binaryValue();
      aPropVal.v_bytes.len = b.size();
      uint8_t *p = new uint8_t [b.size()];
      memcpy(p, b.c_str(),b.size());
      aPropVal.v_bytes.data = p;
      break;
    }
    default:
      // null value, do nothing
      break;
  }
}



#pragma mark - VdcPbufApiServer


VdcApiConnectionPtr VdcPbufApiServer::newConnection()
{
  // create the right kind of API connection
  return VdcApiConnectionPtr(static_cast<VdcApiConnection *>(new VdcPbufApiConnection()));
}



#pragma mark - VdcPbufApiRequest


VdcPbufApiRequest::VdcPbufApiRequest(VdcPbufApiConnectionPtr aConnection, uint32_t aRequestId)
{
  pbufConnection = aConnection;
  reqId = aRequestId;
  responseType = VDCAPI__TYPE__GENERIC_RESPONSE;
}


VdcApiConnectionPtr VdcPbufApiRequest::connection()
{
  return pbufConnection;
}



ErrorPtr VdcPbufApiRequest::sendResult(ApiValuePtr aResult)
{
  ErrorPtr err;
  if (!aResult || aResult->isNull()) {
    // empty result is like sending no error
    err = sendError(0);
    LOG(LOG_INFO,"vdSM <- vDC (pbuf) result sent: requestid='%d', result=NULL\n", reqId);
  }
  else {
    // we might have a specific result
    PbufApiValuePtr result = boost::dynamic_pointer_cast<PbufApiValue>(aResult);
    ProtobufCMessage *subMessageP = NULL;
    // create a message
    Vdcapi__Message msg = VDCAPI__MESSAGE__INIT;
    msg.has_message_id = true; // is response to a previous method call message
    msg.message_id = reqId; // use same message id as in method call
    // set correct type and generate appropriate submessage
    msg.type = responseType;
    switch (responseType) {
      case VDCAPI__TYPE__VDC_RESPONSE_HELLO:
        msg.vdc_response_hello = new Vdcapi__VdcResponseHello;
        vdcapi__vdc__response_hello__init(msg.vdc_response_hello);
        subMessageP = &(msg.vdc_response_hello->base);
        // pbuf API structure and field names are different, we need to map them
        if (result) {
          result->putObjectFieldIntoMessage(*subMessageP, "vdcdSUID");
        }
        break;
      case VDCAPI__TYPE__VDC_RESPONSE_GET_PROPERTY:
        msg.vdc_response_get_property = new Vdcapi__VdcResponseGetProperty;
        vdcapi__vdc__response_get_property__init(msg.vdc_response_get_property);
        subMessageP = &(msg.vdc_response_get_property->base);
        // result object is property value(s)
        // and only field in VdcResponseGetProperty is the "properties" repeating field
        if (result) {
          result->putValueIntoMessageField(subMessageP->descriptor->fields[0], *subMessageP);
        }
        break;
      default:
        LOG(LOG_INFO,"vdSM <- vDC (pbuf) response '%s' cannot be sent because no message is implemented for it at the pbuf level\n", aResult->description().c_str());
        return ErrorPtr(new VdcApiError(500,"Error: Method is not implemented in the pbuf API"));
    }
    // send
    err = pbufConnection->sendMessage(&msg);
    // dispose allocated submessage
    protobuf_c_message_free_unpacked(subMessageP, NULL);
    // log
    LOG(LOG_INFO,"vdSM <- vDC (pbuf) result sent: requestid='%d', result=%s\n", reqId, aResult ? aResult->description().c_str() : "<none>");
  }
  return err;
}


ErrorPtr VdcPbufApiRequest::sendError(uint32_t aErrorCode, string aErrorMessage, ApiValuePtr aErrorData)
{
  ErrorPtr err;
  // create a message
  Vdcapi__Message msg = VDCAPI__MESSAGE__INIT;
  Vdcapi__GenericResponse resp = VDCAPI__GENERIC_RESPONSE__INIT;
  // error response is a generic message
  msg.generic_response = &resp;
  msg.has_message_id = true; // is response to a previous method call message
  msg.message_id = reqId; // use same message id as in method call
  resp.code = VdcPbufApiConnection::internalToPbufError(aErrorCode);
  resp.description = (char *)(aErrorMessage.size()>0 ? aErrorMessage.c_str() : NULL);
  err = pbufConnection->sendMessage(&msg);
  // log (if not just OK)
  if (aErrorCode!=ErrorOK)
    LOG(LOG_INFO,"vdSM <- vDC (pbuf) error sent: requestid='%d', error=%d (%s)\n", reqId, aErrorCode, aErrorMessage.c_str());
  // done
  return err;
}



#pragma mark - VdcPbufApiConnection


VdcPbufApiConnection::VdcPbufApiConnection() :
  closeWhenSent(false),
  expectedMsgBytes(0),
  requestIdCounter(0)
{
  socketComm = SocketCommPtr(new SocketComm(SyncIOMainLoop::currentMainLoop()));
  // install data handler
  socketComm->setReceiveHandler(boost::bind(&VdcPbufApiConnection::gotData, this, _1));
}


// max message size accepted - everything bigger must be an error
#define MAX_DATA_SIZE 16384


void VdcPbufApiConnection::gotData(ErrorPtr aError)
{
  // got data
  if (Error::isOK(aError)) {
    // no error
    size_t dataSz = socketComm->numBytesReady();
    DBGLOG(LOG_DEBUG, "gotData: numBytesReady()=%d\n", dataSz);
    // read data we've got so far
    if (dataSz>0) {
      // temporary buffer
      uint8_t *buf = new uint8_t[dataSz];
      size_t receivedBytes = socketComm->receiveBytes(dataSz, buf, aError);
      DBGLOG(LOG_DEBUG, "gotData: receiveBytes(%d)=%d\n", dataSz, receivedBytes);
      DBGLOG(LOG_DEBUG, "gotData: before appending: receivedMessage.size()=%d\n", receivedMessage.size());
      if (Error::isOK(aError)) {
        // append to receive buffer
        receivedMessage.append((const char *)buf, receivedBytes);
        DBGLOG(LOG_DEBUG, "gotData: after appending: receivedMessage.size()=%d\n", receivedMessage.size());
        // single message extraction
        while(true) {
          DBGLOG(LOG_DEBUG, "gotData: processing loop beginning, expectedMsgBytes=%d\n", expectedMsgBytes);
          if(expectedMsgBytes==0 && receivedMessage.size()>=2) {
            // got 2-byte length header, decode it
            const uint8_t *sz = (const uint8_t *)receivedMessage.c_str();
            expectedMsgBytes =
              (sz[0]<<8) +
              sz[1];
            receivedMessage.erase(0,2);
            DBGLOG(LOG_DEBUG, "gotData: parsed new header, now expectedMsgBytes=%d\n", expectedMsgBytes);
            DBGLOG(LOG_DEBUG, "gotData: after removing header: receivedMessage.size()=%d\n", receivedMessage.size());
            if (expectedMsgBytes>MAX_DATA_SIZE) {
              aError = ErrorPtr(new VdcApiError(413, "message exceeds maximum length of 16kB"));
              break;
            }
          }
          // check for complete message
          if (expectedMsgBytes && (receivedMessage.size()>=expectedMsgBytes)) {
            DBGLOG(LOG_DEBUG, "gotData: receivedMessage.size()=%d >= expectedMsgBytes=%d -> process\n", receivedMessage.size(), expectedMsgBytes);
            // process message
            aError = processMessage((uint8_t *)receivedMessage.c_str(),expectedMsgBytes);
            // erase processed message
            receivedMessage.erase(0,expectedMsgBytes);
            DBGLOG(LOG_DEBUG, "gotData: after removing message: receivedMessage.size()=%d\n", receivedMessage.size());
            expectedMsgBytes = 0; // reset to unknown
            // repeat evaluation with remaining bytes (could be another message)
          }
          else {
            // no complete message yet, done for now
            break;
          }
        }
        DBGLOG(LOG_DEBUG, "gotData: end of processing loop: receivedMessage.size()=%d\n", receivedMessage.size());
      }
      // forget buffer
      delete[] buf; buf = NULL;
    } // some data seems to be ready
  } // no connection error
  if (!Error::isOK(aError)) {
    // error occurred
    // pbuf API cannot resynchronize, close connection
    LOG(LOG_WARNING,"Error occurred on protobuf connection - cannot be re-synced, closing: %s\n", aError->description().c_str());
    closeConnection();
  }
}


ErrorPtr VdcPbufApiConnection::sendMessage(const Vdcapi__Message *aVdcApiMessage)
{
  ErrorPtr err;
  #if defined(DEBUG) || ALWAYS_DEBUG
  if (DBGLOGENABLED(LOG_DEBUG)) {
    protobufMessagePrint(stdout, &aVdcApiMessage->base, 0);
  }
  #endif
  // generate the binary message
  size_t packedSize = vdcapi__message__get_packed_size(aVdcApiMessage);
  uint8_t *packedMsg = new uint8_t[packedSize+2]; // leave room for header
  // - add the header
  packedMsg[0] = (packedSize>>8) & 0xFF;
  packedMsg[1] = packedSize & 0xFF;
  // - add the message data
  vdcapi__message__pack(aVdcApiMessage, packedMsg+2);
  // - adjust the total message length
  packedSize += 2;
  // send the message
  if (transmitBuffer.size()>0) {
    // other messages are already waiting, append entire message
    transmitBuffer.append((const char *)packedMsg, packedSize);
  }
  else {
    // nothing in buffer yet, start new send
    size_t sentBytes = socketComm->transmitBytes(packedSize, packedMsg, err);
    if (Error::isOK(err)) {
      // check if all could be sent
      if (sentBytes<packedSize) {
        // Not everything (or maybe nothing, transmitBytes() can return 0) was sent
        // - enable callback for ready-for-send
        socketComm->setTransmitHandler(boost::bind(&VdcPbufApiConnection::canSendData, this, _1));
        // buffer the rest, canSendData handler will take care of writing it out
        transmitBuffer.assign((char *)packedMsg+sentBytes, packedSize-sentBytes);
      }
			else {
				// all sent
				// - disable transmit handler
        socketComm->setTransmitHandler(NULL);
			}
    }
  }
  // return the buffer
  delete[] packedMsg;
  // done
  return err;
}


void VdcPbufApiConnection::canSendData(ErrorPtr aError)
{
  size_t bytesToSend = transmitBuffer.size();
  if (bytesToSend>0 && Error::isOK(aError)) {
    // send data from transmit buffer
    size_t sentBytes = socketComm->transmitBytes(bytesToSend, (const uint8_t *)transmitBuffer.c_str(), aError);
    if (Error::isOK(aError)) {
      if (sentBytes==bytesToSend) {
        // all sent
        transmitBuffer.erase();
				// - disable transmit handler
        socketComm->setTransmitHandler(NULL);
      }
      else {
        // partially sent, remove sent bytes
        transmitBuffer.erase(0, sentBytes);
      }
      // check for closing connection when no data pending to be sent any more
      if (closeWhenSent && transmitBuffer.size()==0) {
        closeWhenSent = false; // done
        closeConnection();
      }
    }
  }
}





ErrorCode VdcPbufApiConnection::pbufToInternalError(Vdcapi__ResultCode aVdcApiResultCode)
{
  ErrorCode errorCode;
  switch (aVdcApiResultCode) {
    case VDCAPI__RESULT_CODE__ERR_OK: errorCode = 0; break;
    case VDCAPI__RESULT_CODE__ERR_MESSAGE_UNKNOWN: errorCode = 405; break;
    case VDCAPI__RESULT_CODE__ERR_NOT_FOUND: errorCode = 404; break;
    case VDCAPI__RESULT_CODE__ERR_INCOMPATIBLE_API: errorCode = 505; break;
    case VDCAPI__RESULT_CODE__ERR_SERVICE_NOT_AVAILABLE: errorCode = 503; break;
    case VDCAPI__RESULT_CODE__ERR_INSUFFICIENT_STORAGE: errorCode = 507; break;
    case VDCAPI__RESULT_CODE__ERR_FORBIDDEN: errorCode = 403; break;
    case VDCAPI__RESULT_CODE__ERR_NOT_AUTHORIZED: errorCode = 401; break;
    case VDCAPI__RESULT_CODE__ERR_NOT_IMPLEMENTED: errorCode = 501; break;
    case VDCAPI__RESULT_CODE__ERR_NO_CONTENT_FOR_ARRAY: errorCode = 204; break;
    case VDCAPI__RESULT_CODE__ERR_INVALID_VALUE_TYPE: errorCode = 415; break;
    default: errorCode = 500; break; // general error
  }
  return errorCode;
}


Vdcapi__ResultCode VdcPbufApiConnection::internalToPbufError(ErrorCode aErrorCode)
{
  Vdcapi__ResultCode res;
  switch (aErrorCode) {
    case 0:
    case 200: res = VDCAPI__RESULT_CODE__ERR_OK; break;
    case 405: res = VDCAPI__RESULT_CODE__ERR_MESSAGE_UNKNOWN; break;
    case 404: res = VDCAPI__RESULT_CODE__ERR_NOT_FOUND; break;
    case 505: res = VDCAPI__RESULT_CODE__ERR_INCOMPATIBLE_API; break;
    case 503: res = VDCAPI__RESULT_CODE__ERR_SERVICE_NOT_AVAILABLE; break;
    case 507: res = VDCAPI__RESULT_CODE__ERR_INSUFFICIENT_STORAGE; break;
    case 403: res = VDCAPI__RESULT_CODE__ERR_FORBIDDEN; break;
    case 401: res = VDCAPI__RESULT_CODE__ERR_NOT_AUTHORIZED; break;
    case 501: res = VDCAPI__RESULT_CODE__ERR_NOT_IMPLEMENTED; break;
    case 204: res = VDCAPI__RESULT_CODE__ERR_NO_CONTENT_FOR_ARRAY; break;
    case 415: res = VDCAPI__RESULT_CODE__ERR_INVALID_VALUE_TYPE; break;
    default: res = VDCAPI__RESULT_CODE__ERR_NOT_IMPLEMENTED; break; // something is obviously not implemented...
  }
  return res;
}



ErrorPtr VdcPbufApiConnection::processMessage(const uint8_t *aPackedMessageP, size_t aPackedMessageSize)
{
  Vdcapi__Message *decodedMsg;
  ProtobufCMessage *paramsMsg = NULL;
  PbufApiValuePtr msgFieldsObj = PbufApiValuePtr(new PbufApiValue);

  ErrorPtr err;

  decodedMsg = vdcapi__message__unpack(NULL, aPackedMessageSize, aPackedMessageP); // Deserialize the serialized input
  if (decodedMsg == NULL) {
    err = ErrorPtr(new VdcApiError(400,"error unpacking incoming message"));
  }
  else {
    // print it
    #if defined(DEBUG) || ALWAYS_DEBUG
    if (DBGLOGENABLED(LOG_DEBUG)) {
      protobufMessagePrint(stdout, &decodedMsg->base, 0);
    }
    #endif
    // successful message decoding
    string method;
    int responseType = 0; // none
    int32_t responseForId = -1; // none
    bool emptyResult = false;
    switch (decodedMsg->type) {
      // incoming method calls
      case VDCAPI__TYPE__VDSM_REQUEST_HELLO: {
        method = "hello";
        paramsMsg = &(decodedMsg->vdsm_request_hello->base);
        responseType = VDCAPI__TYPE__VDC_RESPONSE_HELLO;
        // pbuf API field names match, we can use generic decoding
        break;
//        // pbuf API structure and field names are different, we need to map them
//        msgFieldsObj->addObjectFieldFromMessage(*paramsMsg, "APIVersion");
//        goto getDsUid;
      }
      case VDCAPI__TYPE__VDSM_REQUEST_GET_PROPERTY: {
        method = "getProperty";
        paramsMsg = &(decodedMsg->vdsm_request_get_property->base);
        responseType = VDCAPI__TYPE__VDC_RESPONSE_GET_PROPERTY;
        // pbuf API field names match, we can use generic decoding
        break;
//        // pbuf API structure and field names are different, we need to map them
//        msgFieldsObj->addObjectFieldFromMessage(*paramsMsg, "query");
//        goto getDsUid;
      }
      case VDCAPI__TYPE__VDSM_REQUEST_SET_PROPERTY: {
        method = "setProperty";
        paramsMsg = &(decodedMsg->vdsm_request_set_property->base);
        responseType = VDCAPI__TYPE__GENERIC_RESPONSE;
        // pbuf API field names match, we can use generic decoding
        break;
//        msgFieldsObj->addObjectFieldFromMessage(*paramsMsg, "properties");
//        goto getDsUid;
      }
      case VDCAPI__TYPE__VDSM_SEND_REMOVE: {
        method = "remove";
        paramsMsg = &(decodedMsg->vdsm_send_remove->base);
        responseType = VDCAPI__TYPE__GENERIC_RESPONSE;
        goto getDsUid;
      }
      case VDCAPI__TYPE__VDSM_SEND_BYE: {
        method = "bye";
        paramsMsg = NULL; // no params (altough there is a message with no fields)
        responseType = VDCAPI__TYPE__GENERIC_RESPONSE;
        break;
      }
      // Notifications
      case VDCAPI__TYPE__VDSM_SEND_PING: {
        method = "ping";
        paramsMsg = &(decodedMsg->vdsm_send_ping->base);
        goto getDsUid;
      }
      case VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_SCENE:
        method = "callScene";
        paramsMsg = &(decodedMsg->vdsm_send_call_scene->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_SAVE_SCENE:
        method = "saveScene";
        paramsMsg = &(decodedMsg->vdsm_send_save_scene->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_UNDO_SCENE:
        method = "undoScene";
        paramsMsg = &(decodedMsg->vdsm_send_undo_scene->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_SET_LOCAL_PRIO:
        method = "setLocalPriority";
        paramsMsg = &(decodedMsg->vdsm_send_set_local_prio->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_SET_CONTROL_VALUE:
        method = "setControlValue";
        paramsMsg = &(decodedMsg->vdsm_send_set_control_value->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_CALL_MIN_SCENE:
        method = "callSceneMin";
        paramsMsg = &(decodedMsg->vdsm_send_call_min_scene->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_IDENTIFY:
        method = "identify";
        paramsMsg = &(decodedMsg->vdsm_send_identify->base);
        // pbuf API field names match, we can use generic decoding
        break;
      case VDCAPI__TYPE__VDSM_NOTIFICATION_DIM_CHANNEL:
        method = "dimChannel";
        paramsMsg = &(decodedMsg->vdsm_send_dim_channel->base);
        // pbuf API field names match, we can use generic decoding
        break;
      // incoming responses
      case VDCAPI__TYPE__GENERIC_RESPONSE: {
        // error or NULL response
        if (decodedMsg->generic_response->code!=VDCAPI__RESULT_CODE__ERR_OK) {
          // convert to HTTP-style internal codes
          ErrorCode errorCode = pbufToInternalError(decodedMsg->generic_response->code);
          err = ErrorPtr(new VdcApiError(errorCode, nonNullCStr(decodedMsg->generic_response->description)));
        }
        responseForId = decodedMsg->message_id;
        emptyResult = true; // do not convert message fields to result
        break;
      }
      // common case for many messages: message with parameters that were explicitly mapped has also a dSUID parameter
      getDsUid:
        msgFieldsObj->addObjectFieldFromMessage(*paramsMsg, "dSUID"); // get the dSUID
        paramsMsg = NULL; // prevent generic parameter mapping, mapping was done explicitly before
        break;
      // unknown message type
      default:
        method = string_format("unknownMethod_%d", decodedMsg->type);
        responseType = VDCAPI__TYPE__GENERIC_RESPONSE;
        break;
    }
    // dispatch between incoming method calls/notifications and responses for outgoing calls
    if (responseForId>=0) {
      // This is a response
      if (Error::isOK(err) && !emptyResult) {
        // convert fields of response to API object
        msgFieldsObj->getObjectFromMessageFields(decodedMsg->base);
      }
      // find the originating request
      PendingAnswerMap::iterator pos = pendingAnswers.find(responseForId);
      if (pos==pendingAnswers.end()) {
        // errors without ID cannot be associated with calls made earlier, so just log the error
        LOG(LOG_WARNING,"vdSM -> vDC (pbuf) error: Received response with unknown 'id'=%d, error=%s\n", responseForId, Error::isOK(err) ? "<none>" : err->description().c_str());
      }
      else {
        // found callback
        VdcApiResponseCB cb = pos->second;
        pendingAnswers.erase(pos); // erase
        // create request object just to hold the response ID
        VdcPbufApiRequestPtr request = VdcPbufApiRequestPtr(new VdcPbufApiRequest(VdcPbufApiConnectionPtr(this), responseForId));
        if (Error::isOK(err)) {
          LOG(LOG_INFO,"vdSM -> vDC (pbuf) result received: id='%s', result=%s\n", request->requestId().c_str(), msgFieldsObj ? msgFieldsObj->description().c_str() : "<none>");
        }
        else {
          LOG(LOG_INFO,"vdSM -> vDC (pbuf) error received: id='%s', error=%s, errordata=%s\n", request->requestId().c_str(), err->description().c_str(), msgFieldsObj ? msgFieldsObj->description().c_str() : "<none>");
        }
        cb(this, request, err, msgFieldsObj); // call handler
      }
      err.reset(); // delivered
    }
    else {
      // this is a method call or notification
      VdcPbufApiRequestPtr request;
      // - get the params
      if (paramsMsg) {
        msgFieldsObj->getObjectFromMessageFields(*paramsMsg);
      }
      // - dispatch between methods and notifications
      if (decodedMsg->has_message_id) {
        // method call, we need a request reference object
        request = VdcPbufApiRequestPtr(new VdcPbufApiRequest(VdcPbufApiConnectionPtr(this), decodedMsg->message_id));
        request->responseType = (Vdcapi__Type)responseType; // save the response type for sending answers later
        LOG(LOG_INFO,"vdSM -> vDC (pbuf) method call received: requestid='%d', method='%s', params=%s\n", request->reqId, method.c_str(), msgFieldsObj ? msgFieldsObj->description().c_str() : "<none>");
      }
      else {
        LOG(LOG_INFO,"vdSM -> vDC (pbuf) notification received: method='%s', params=%s\n", method.c_str(), msgFieldsObj ? msgFieldsObj->description().c_str() : "<none>");
      }
      // call handler
      apiRequestHandler(VdcPbufApiConnectionPtr(this), request, method, msgFieldsObj);
    }
  }
  // free the unpacked message
  vdcapi__message__free_unpacked(decodedMsg, NULL); // Free the message from unpack()
  // return error, in case one is left
  return err;
}


void VdcPbufApiConnection::closeAfterSend()
{
  closeWhenSent = true;
}


ApiValuePtr VdcPbufApiConnection::newApiValue()
{
  return ApiValuePtr(new PbufApiValue);
}


ErrorPtr VdcPbufApiConnection::sendRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  PbufApiValuePtr params = boost::dynamic_pointer_cast<PbufApiValue>(aParams);
  ErrorPtr err;

  // create a message
  Vdcapi__Message msg = VDCAPI__MESSAGE__INIT;
  // find out which type and which submessage applies
  ProtobufCMessage *subMessageP = NULL;
  if (aMethod=="pong") {
    msg.type = VDCAPI__TYPE__VDC_SEND_PONG;
    msg.vdc_send_pong = new Vdcapi__VdcSendPong;
    vdcapi__vdc__send_pong__init(msg.vdc_send_pong);
    subMessageP = &(msg.vdc_send_pong->base);
  }
  else if (aMethod=="announce") {
    msg.type = VDCAPI__TYPE__VDC_SEND_ANNOUNCE;
    msg.vdc_send_announce = new Vdcapi__VdcSendAnnounce;
    vdcapi__vdc__send_announce__init(msg.vdc_send_announce);
    subMessageP = &(msg.vdc_send_announce->base);
  }
  else if (aMethod=="announcevdc") {
    msg.type = VDCAPI__TYPE__VDC_SEND_ANNOUNCEVDC;
    msg.vdc_send_announce_vdc = new Vdcapi__VdcSendAnnounceVdc;
    vdcapi__vdc__send_announce_vdc__init(msg.vdc_send_announce_vdc);
    subMessageP = &(msg.vdc_send_announce_vdc->base);
  }
  else if (aMethod=="vanish") {
    msg.type = VDCAPI__TYPE__VDC_SEND_VANISH;
    msg.vdc_send_vanish = new Vdcapi__VdcSendVanish;
    vdcapi__vdc__send_vanish__init(msg.vdc_send_vanish);
    subMessageP = &(msg.vdc_send_vanish->base);
  }
  else if (aMethod=="pushProperty") {
    msg.type = VDCAPI__TYPE__VDC_SEND_PUSH_PROPERTY;
    msg.vdc_send_push_property = new Vdcapi__VdcSendPushProperty;
    vdcapi__vdc__send_push_property__init(msg.vdc_send_push_property);
    subMessageP = &(msg.vdc_send_push_property->base);
  }
  else if (aMethod=="identify") {
    // Note: this method has the same (JSON) name as the method from the vdsm used to identify (blink) a device.
    //   In protobuf API however this is a different message type
    msg.type = VDCAPI__TYPE__VDC_SEND_IDENTIFY;
    msg.vdc_send_identify = new Vdcapi__VdcSendIdentify;
    vdcapi__vdc__send_identify__init(msg.vdc_send_identify);
    subMessageP = &(msg.vdc_send_identify->base);
  }
  else {
    // no suitable submessage, cannot send
    LOG(LOG_INFO,"vdSM <- vDC (pbuf) method '%s' cannot be sent because no message is implemented for it at the pbuf level\n", aMethod.c_str());
    return ErrorPtr(new VdcApiError(500,"Error: Method is not implemented in the pbuf API"));
  }
  if (Error::isOK(err)) {
    if (aResponseHandler) {
      // method call expecting response
      msg.has_message_id = true; // has a messageID
      msg.message_id = ++requestIdCounter; // use new ID
      // save response handler into our map so that it can be called later when answer arrives
      pendingAnswers[requestIdCounter] = aResponseHandler;
    }
    // now generically fill parameters into submessage (if any, and if not handled above explicitly)
    if (params) {
      params->putObjectIntoMessageFields(*subMessageP);
    }
    // send
    err = sendMessage(&msg);
    // dispose allocated submessage
    protobuf_c_message_free_unpacked(subMessageP, NULL);
    // log
    if (aResponseHandler) {
      LOG(LOG_INFO,"vdSM <- vDC (pbuf) method call sent: requestid='%d', method='%s', params=%s\n", requestIdCounter, aMethod.c_str(), aParams ? aParams->description().c_str() : "<none>");
    }
    else {
      LOG(LOG_INFO,"vdSM <- vDC (pbuf) notification sent: method='%s', params=%s\n", aMethod.c_str(), aParams ? aParams->description().c_str() : "<none>");
    }
  }
  // done
  return err;
}


#pragma mark - generic protobuf-C message printing

#if defined(DEBUG) || ALWAYS_DEBUG


static void printLfAndIndent(FILE *aOutFile, int aIndent)
{
  int i;
  fputc('\n', aOutFile);
  for (i = 0; i<aIndent; i++) fputc(' ', aOutFile);
}


void protobufMessagePrintInternal(FILE *aOutFile, const ProtobufCMessage *aMessageP, int aIndent);

static void protobufFieldPrint(FILE *aOutFile, const ProtobufCFieldDescriptor *aFieldDescriptorP, const void *aData, size_t aIndex, int aIndent)
{
  switch (aFieldDescriptorP->type) {
    case PROTOBUF_C_TYPE_BOOL:
      fprintf(aOutFile, "(bool)%s", *((protobuf_c_boolean *)aData+aIndex) ? "true" : "false");
      break;
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      fprintf(aOutFile, "(int32)%d", *((int32_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      fprintf(aOutFile, "(int64)%lld", *((int64_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      fprintf(aOutFile, "(uint32)%u", *((uint32_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      fprintf(aOutFile, "(uint64)%llu", *((uint64_t *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_FLOAT:
      fprintf(aOutFile, "(float)%f", *((float *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_DOUBLE:
      fprintf(aOutFile, "(double)%f", *((double *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_ENUM:
      fprintf(aOutFile, "(enum)%d", *((int *)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_STRING:
      fprintf(aOutFile, "(string)\"%s\"", *((const char **)aData+aIndex));
      break;
    case PROTOBUF_C_TYPE_BYTES: {
      fprintf(aOutFile, "(bytes)");
      ProtobufCBinaryData *bd = ((ProtobufCBinaryData *)aData+aIndex);
      for (int i=0; i<bd->len; i++) {
        fprintf(aOutFile, "%02X", bd->data[i]);
      }
      break;
    }
    case PROTOBUF_C_TYPE_MESSAGE: {
      // submessage, pack into object value
      const ProtobufCMessage *subMessageP = *((const ProtobufCMessage **)aData+aIndex);
      protobufMessagePrintInternal(aOutFile, subMessageP, aIndent-2);
      break;
    }
    default:
      fprintf(aOutFile, "(unknown field type)");
      break;
  }
}

void protobufMessagePrint(FILE *aOutFile, const ProtobufCMessage *aMessageP, int aIndent)
{
  // Start and end with a LF
  printLfAndIndent(aOutFile, aIndent);
  protobufMessagePrintInternal(aOutFile, aMessageP, aIndent);
  fprintf(aOutFile, "\n\n");
}


void protobufMessagePrintInternal(FILE *aOutFile, const ProtobufCMessage *aMessageP, int aIndent)
{
  const ProtobufCFieldDescriptor *fieldDescP;
  unsigned f;

  fprintf(aOutFile, "(%s) {", aMessageP->descriptor->name);
  aIndent += 2; // contents of message always indented
  // Fields
  fieldDescP = aMessageP->descriptor->fields;
  for (f = 0; f<aMessageP->descriptor->n_fields; f++) {
    const uint8_t *baseP = (const uint8_t *)(aMessageP);
    const uint8_t *fieldBaseP = baseP+fieldDescP->offset;
    if (fieldDescP->label==PROTOBUF_C_LABEL_REPEATED) {
      // repeated field, show all elements
      size_t arraySize = *((size_t *)(baseP+fieldDescP->quantifier_offset));
      for (int i = 0; i<arraySize; i++) {
        printLfAndIndent(aOutFile, aIndent);
        fprintf(aOutFile, "%s[%d]: ", fieldDescP->name, i);
        protobufFieldPrint(aOutFile, fieldDescP, *((void **)fieldBaseP), i, aIndent+2); // every subsequent line of content indented
      }
    }
    else {
      // not repeated (array), but single field
      bool hasField = false;
      if (fieldDescP->label==PROTOBUF_C_LABEL_OPTIONAL) {
        if (fieldDescP->quantifier_offset) {
          // scalar that needs quantifier
          hasField = *((protobuf_c_boolean *)(baseP+fieldDescP->quantifier_offset));
        }
        else {
          // value is a pointer, exists if not NULL
          hasField = *((const void **)(baseP+fieldDescP->offset))!=NULL;
        }
      }
      else {
        // must be mandatory
        hasField = true;
      }
      // get value, if available
      if (hasField) {
        printLfAndIndent(aOutFile, aIndent);
        fprintf(aOutFile, "%s: ", fieldDescP->name);
        // get value
        protobufFieldPrint(aOutFile, fieldDescP, fieldBaseP, 0, aIndent+2); // every subsequent line of content indented
      }
    }
    fieldDescP++; // next field descriptor
  }
  aIndent -= 2; // end of message bracket unindented
  printLfAndIndent(aOutFile, aIndent);
  fprintf(aOutFile, "}");
}


#endif // DEBUG | ALWAYS_DEBUG

