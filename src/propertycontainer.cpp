//
//  propertycontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 15.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "propertycontainer.hpp"

using namespace p44;


#pragma mark - property access API

ErrorPtr PropertyContainer::accessProperty(bool aForWrite, JsonObjectPtr &aJsonObject, const string &aName, int aIndex, int aElementCount)
{
  ErrorPtr err;
  // TODO: separate dot notation name?
  // all or single field in this container?
  if (aName=="*") {
    // all fields in this container
    if (aForWrite) {
      // write: write all fields of input object into properties of this container
      // - input JSON object must be a object
      if (!aJsonObject->isType(json_type_object))
        err = ErrorPtr(new JsonRpcError(415, "Value must be object"));
      else {
        // iterate over fields in object and access them one by one
        aJsonObject->resetKeyIteration();
        string key;
        JsonObjectPtr value;
        while (aJsonObject->nextKeyValue(key, value)) {
          // write single field
          err = accessProperty(true, value, key);
          if (!Error::isOK(err))
            break;
        }
      }
    }
    else {
      // read: collect all fields of this container into a JSON object
      JsonObjectPtr allFields = JsonObject::newObj();
      // - iterate over my own descriptors
      for (int propIndex = 0; propIndex<numProps(); propIndex++) {
        PropertyDescriptor *propDescP = getPropertyDescriptor(propIndex);
        if (!propDescP) break; // safety only
        JsonObjectPtr propField;
        err = accessPropertyByDescriptor(false, propField, *propDescP);
        if (Error::isOK(err)) {
          // add to resulting object
          allFields->add(propDescP->propertyName, propField);
        }
      }
    }
  }
  else {
    // single field from this container
    // - find descriptor
    PropertyDescriptor *propDescP = NULL;
    if (aName=="^") {
      // access first property (default property, internally used for ptype_proxy)
      propDescP = getPropertyDescriptor(0);
    }
    else {
      // search for descriptor by name
      for (int propIndex = 0; propIndex<numProps(); propIndex++) {
        PropertyDescriptor *p = getPropertyDescriptor(propIndex);
        if (aName==p->propertyName) {
          propDescP = p;
          break;
        }
      }
    }
    // - now use descriptor
    if (!propDescP) {
      // named property not found
      err = ErrorPtr(new JsonRpcError(501,"Unknown property name"));
    }
    else {
      // access the property
      err = accessPropertyByDescriptor(false, aJsonObject, *propDescP, aIndex, aElementCount);
    }
  }
  return err;
}



ErrorPtr PropertyContainer::accessPropertyByDescriptor(bool aForWrite, JsonObjectPtr &aJsonObject, PropertyDescriptor &aPropertyDescriptor, int aIndex, int aElementCount)
{
  ErrorPtr err;
  if (aPropertyDescriptor.isArray) {
    // array property
    // - size access is like a single value
    if (aIndex==PROP_ARRAY_SIZE) {
      // get array size
      accessField(aForWrite, aJsonObject, aPropertyDescriptor, PROP_ARRAY_SIZE);
    }
    else {
      // single element or range?
      if (aElementCount>0) {
        // Range of elements: only allowed for reading
        if (aForWrite)
          err = ErrorPtr(new JsonRpcError(403,"Arrays can only be written one element at a time"));
        else {
          // collect range of elements into JSON array
          aJsonObject = JsonObject::newArray();
          for (int n = 0; n<aElementCount || aElementCount==PROP_ARRAY_SIZE; n++) {
            JsonObjectPtr elementObj;
            // - collect single element
            err = accessPropertyByDescriptor(false, elementObj, aPropertyDescriptor, aIndex+n, 0);
            if (Error::isError(err, JsonRpcError::domain(), 204)) {
              // array exhausted
              err.reset(); // is not a real error
              break; // but stop collecting elements
            }
            else {
              // error, stop collecting elements
              break;
            }
            // - got array element, add it to result array
            aJsonObject->arrayAppend(elementObj);
          }
        }
      }
      else {
        // Single element of the array
        err = accessElementByDescriptor(aForWrite, aJsonObject, aPropertyDescriptor, aIndex);
      }
    }
  }
  else {
    // non-array property
    err = accessElementByDescriptor(aForWrite, aJsonObject, aPropertyDescriptor, 0);
  }
  return err;
}



ErrorPtr PropertyContainer::accessElementByDescriptor(bool aForWrite, JsonObjectPtr &aJsonObject, PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  ErrorPtr err;
  if (aPropertyDescriptor.propertyType==ptype_object || aPropertyDescriptor.propertyType==ptype_proxy) {
    // structured property with subproperties, get container
    PropertyContainer *containerP = getContainer(aPropertyDescriptor, aIndex);
    if (!containerP)
      err = ErrorPtr(new JsonRpcError(204,"Invalid array index")); // Note: must be array index problem, because there's no other reason for a object/proxy to return no container
    else {
      // access all fields of structured object (named "*"), or singe default field of proxied property (named "^")
      err = containerP->accessProperty(aForWrite, aJsonObject, aPropertyDescriptor.propertyType==ptype_object ? "*" : "^");
    }
  }
  else {
    // single value property
    if (!accessField(aForWrite, aJsonObject, aPropertyDescriptor, aIndex)) {
      err = ErrorPtr(new JsonRpcError(403,"Access denied"));
    }
  }
  return err;
}



#pragma mark - default implementation of direct struct field access by basepointer+offset


bool PropertyContainer::accessField(bool aForWrite, JsonObjectPtr aPropValue, PropertyDescriptor &aPropertyDescriptor, int aIndex)
{
  // get base pointer
  uint8_t *recordPtr = (uint8_t *)dataStructBasePtr(aIndex);
  if (recordPtr) {
    void *fieldPtr = (void *)(recordPtr+aPropertyDescriptor.accessKey);
    if (aForWrite) {
      // Write
      switch (aPropertyDescriptor.propertyType) {
        case ptype_bool:
          *((bool *)fieldPtr) = aPropValue->boolValue(); break;
        case ptype_int8:
          *((int8_t *)fieldPtr) = aPropValue->int32Value(); break;
        case ptype_int16:
          *((int16_t *)fieldPtr) = aPropValue->int32Value(); break;
        case ptype_int32:
          *((int32_t *)fieldPtr) = aPropValue->int32Value(); break;
        case ptype_int64:
          *((int64_t *)fieldPtr) = aPropValue->int64Value(); break;
        case ptype_double:
          *((double *)fieldPtr) = aPropValue->doubleValue(); break;
        case ptype_string:
          *((string *)fieldPtr) = aPropValue->stringValue(); break;
        default:
          return false;
      }
      return true;
    }
    else {
      // Read
      switch (aPropertyDescriptor.propertyType) {
        case ptype_bool:
          aPropValue = JsonObject::newBool(*((bool *)fieldPtr)); break;
        case ptype_int8:
          aPropValue = JsonObject::newInt32(*((int8_t *)fieldPtr)); break;
        case ptype_int16:
          aPropValue = JsonObject::newInt32(*((int16_t *)fieldPtr)); break;
        case ptype_int32:
          aPropValue = JsonObject::newInt32(*((int32_t *)fieldPtr)); break;
        case ptype_int64:
          aPropValue = JsonObject::newInt64(*((int64_t *)fieldPtr)); break;
        case ptype_double:
          aPropValue = JsonObject::newDouble(*((double *)fieldPtr)); break;
        case ptype_charptr:
          aPropValue = JsonObject::newString(*((const char **)fieldPtr)); break;
        case ptype_string:
          aPropValue = JsonObject::newString(*((string *)fieldPtr)); break;
        default:
          return false;
      }
      return true;
    }

  }
  // failed
  return false;
}







