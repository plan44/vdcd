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

ErrorPtr PropertyContainer::accessProperty(bool aForWrite, ApiValuePtr aApiObject, const string &aName, int aDomain, int aIndex, int aElementCount)
{
  ErrorPtr err;
  // TODO: separate dot notation name?
  // all or single field in this container?
  if (aName=="*") {
    // all fields in this container
    if (aForWrite) {
      // write: write all fields of input object into properties of this container
      // - input JSON object must be a object
      if (!aApiObject->isType(apivalue_object))
        err = ErrorPtr(new VdcApiError(415, "Value must be object"));
      else {
        // iterate over fields in object and access them one by one
        aApiObject->resetKeyIteration();
        string key;
        ApiValuePtr value;
        while (aApiObject->nextKeyValue(key, value)) {
          // write single field
          err = accessProperty(true, value, key, aDomain, PROP_ARRAY_SIZE, 0); // if array, write size (writing array is not supported)
          if (!Error::isOK(err))
            break;
        }
      }
    }
    else {
      // read: collect all fields of this container into a object type API value
      aApiObject->setType(apivalue_object);
      // - iterate over my own descriptors
      for (int propIndex = 0; propIndex<numProps(aDomain); propIndex++) {
        const PropertyDescriptor *propDescP = getPropertyDescriptor(propIndex, aDomain);
        if (!propDescP) break; // safety only, propIndex should never be invalid
        ApiValuePtr propField = aApiObject->newValue(propDescP->propertyType); // create field value of correct type
        err = accessPropertyByDescriptor(false, propField, *propDescP, aDomain, 0, PROP_ARRAY_SIZE); // if array, entire array
        if (Error::isOK(err)) {
          // add to resulting object, if not no object returned at all (explicit JsonObject::newNull()) will be returned!)
          aApiObject->add(propDescP->propertyName, propField);
        }
      }
    }
  }
  else {
    // single field from this container
    // - find descriptor
    const PropertyDescriptor *propDescP = NULL;
    if (aName=="^") {
      // access first property (default property, internally used for apivalue_proxy)
      if (numProps(aDomain)>0)
        propDescP = getPropertyDescriptor(0, aDomain);
    }
    else {
      // search for descriptor by name
      for (int propIndex = 0; propIndex<numProps(aDomain); propIndex++) {
        const PropertyDescriptor *p = getPropertyDescriptor(propIndex, aDomain);
        if (aName==p->propertyName) {
          propDescP = p;
          break;
        }
      }
    }
    // - now use descriptor
    if (!propDescP) {
      // named property not found
      err = ErrorPtr(new VdcApiError(501,"Unknown property name"));
    }
    else {
      // access the property
      err = accessPropertyByDescriptor(aForWrite, aApiObject, *propDescP, aDomain, aIndex, aElementCount);
    }
  }
  return err;
}



ErrorPtr PropertyContainer::accessPropertyByDescriptor(bool aForWrite, ApiValuePtr aApiObject, const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex, int aElementCount)
{
  ErrorPtr err;
  if (aPropertyDescriptor.isArray) {
    // array property
    // - size access is like a single value
    if (aIndex==PROP_ARRAY_SIZE) {
      // get array size
      aApiObject->setType(apivalue_uint64); // force integer value
      accessField(aForWrite, aApiObject, aPropertyDescriptor, PROP_ARRAY_SIZE);
    }
    else {
      // get size of array
      ApiValuePtr o = aApiObject->newValue(apivalue_uint64); // size is an integer value
      accessField(false, o, aPropertyDescriptor, PROP_ARRAY_SIZE); // query size
      int arrSz = o->int32Value();
      // single element or range?
      if (aElementCount!=0) {
        // Range of elements: only allowed for reading
        if (aForWrite)
          err = ErrorPtr(new VdcApiError(403,"Arrays can only be written one element at a time"));
        else {
          // return array
          aApiObject->setType(apivalue_array);
          // limit range to actual array size
          if (aIndex>arrSz)
            aElementCount = 0; // invalid start index, return empty array
          else if (aElementCount==PROP_ARRAY_SIZE || aIndex+aElementCount>arrSz)
            aElementCount = arrSz-aIndex; // limit to number of elements from current index to end of array
          // collect range of elements into JSON array
          for (int n = 0; n<aElementCount; n++) {
            // - create element of appropriate type
            ApiValuePtr elementObj = aApiObject->newValue(aPropertyDescriptor.propertyType);
            // - collect single element
            err = accessPropertyByDescriptor(false, elementObj, aPropertyDescriptor, aDomain, aIndex+n, 0);
            if (Error::isError(err, VdcApiError::domain(), 204)) {
              // array exhausted
              err.reset(); // is not a real error
              break; // but stop collecting elements
            }
            else if(!Error::isOK(err)) {
              // other error, stop collecting elements
              break;
            }
            // - got array element, add it to result array
            aApiObject->arrayAppend(elementObj);
          }
        }
      }
      else {
        // Single element of the array
        // - check index
        if (aIndex>=arrSz)
          err = ErrorPtr(new VdcApiError(204,"Invalid array index"));
        else
          err = accessElementByDescriptor(aForWrite, aApiObject, aPropertyDescriptor, aDomain, aIndex);
      }
    }
  }
  else {
    // non-array property
    err = accessElementByDescriptor(aForWrite, aApiObject, aPropertyDescriptor, aDomain, 0);
  }
  return err;
}



ErrorPtr PropertyContainer::accessElementByDescriptor(bool aForWrite, ApiValuePtr aApiObject, const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex)
{
  ErrorPtr err;
  if (aPropertyDescriptor.propertyType==apivalue_object || aPropertyDescriptor.propertyType==apivalue_proxy) {
    // structured property with subproperties, get container
    int containerDomain = aDomain;
    PropertyContainerPtr container = getContainer(aPropertyDescriptor, containerDomain, aIndex);
    if (!container) {
      if (aPropertyDescriptor.isArray)
        err = ErrorPtr(new VdcApiError(204,"Invalid array index")); // Note: must be array index problem, because there's no other reason for a array object/proxy to return no container
      else
        aApiObject->setNull(); // NULL value
    }
    else {
      // access all fields of structured object (named "*"), or single default field of proxied property (named "^")
      err = container->accessProperty(aForWrite, aApiObject, aPropertyDescriptor.propertyType==apivalue_object ? "*" : "^", containerDomain, PROP_ARRAY_SIZE, 0);
      if (aForWrite && Error::isOK(err)) {
        // give this container a chance to post-process write access
        err = writtenProperty(aPropertyDescriptor, aDomain, aIndex, container);
      }
    }
  }
  else {
    // single value property
    aApiObject->setType(aPropertyDescriptor.propertyType);
    if (!accessField(aForWrite, aApiObject, aPropertyDescriptor, aIndex)) {
      err = ErrorPtr(new VdcApiError(403,"Access denied"));
    }
  }
  return err;
}






