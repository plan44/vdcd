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

#include "propertycontainer.hpp"

using namespace p44;


#pragma mark - property access API

ErrorPtr PropertyContainer::accessProperty(PropertyAccessMode aMode, ApiValuePtr aApiObject, const string &aName, int aDomain, int aIndex, int aElementCount, int aNestLevel)
{
  ErrorPtr err;
  // TODO: separate dot notation name?
  // all or single field in this container?
  if (aName=="*" || aName=="#") {
    // all fields in this container
    if (aMode!=access_read) {
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
          err = accessProperty(aMode, value, key, aDomain, PROP_ARRAY_SIZE, 0, aNestLevel+1); // if array, write size (writing array is not supported)
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
        ApiValuePtr propField;
        if (propDescP->isArray && aName=="#") {
          // for arrays (at any level), only report size, not entire contents
          propField = aApiObject->newValue(apivalue_int64); // size of array as int
          err = accessPropertyByDescriptor(access_read, propField, *propDescP, aDomain, -1, 1); // only size of array
        }
        else if (propDescP->isArray && aNestLevel>0) {
          // second level array -> render existing elements as fields with index integrated into name
          // - get size
          ApiValuePtr sz = aApiObject->newValue(apivalue_int64); // size of array as int
          err = accessPropertyByDescriptor(access_read, sz, *propDescP, aDomain, -1, 1); // only size of array
          size_t maxIndex = sz->int32Value();
          for (size_t index=0; index<maxIndex; index++) {
            propField = aApiObject->newValue(propDescP->propertyType); // create field value of correct type
            err = accessPropertyByDescriptor(access_read, propField, *propDescP, aDomain, 0, 0); // only one element
            if (Error::isOK(err) && !propField->isNull()) {
              // this array element exists, add it as a field with index appended to name
              string elementName = string_format("%s%d", propDescP->propertyName, index);
              aApiObject->add(elementName, propField);
            }
          }
          // elements (if any) added, don't add property itself again
          continue;
        }
        else {
          // value or complete contents of array
          propField = aApiObject->newValue(propDescP->propertyType); // create field value of correct type
          err = accessPropertyByDescriptor(access_read, propField, *propDescP, aDomain, 0, PROP_ARRAY_SIZE); // if array, entire array
        }
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
        size_t propNameLen=strlen(p->propertyName);
        if (strncmp(aName.c_str(),p->propertyName,propNameLen)==0) {
          // specified name matches up to length of property name
          if (aName.size()==propNameLen) {
            // exact match, single property field
            propDescP = p;
            break;
          }
          else if (p->isArray && aNestLevel>0) {
            // beginning matches, but specfied name is longer and property is a second level array -> could be array index embedded in name
            if (sscanf(aName.c_str()+propNameLen, "%d", &aIndex)==1) {
              // array access using index appended to property name
              propDescP = p;
              break;
            }
          }
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
      err = accessPropertyByDescriptor(aMode, aApiObject, *propDescP, aDomain, aIndex, aElementCount);
    }
  }
  return err;
}



ErrorPtr PropertyContainer::accessPropertyByDescriptor(PropertyAccessMode aMode, ApiValuePtr aApiObject, const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex, int aElementCount)
{
  ErrorPtr err;
  if (aPropertyDescriptor.isArray) {
    // array property
    // - size access is like a single value
    if (aIndex==PROP_ARRAY_SIZE) {
      // get array size
      aApiObject->setType(apivalue_uint64); // force integer value
      accessField(aMode, aApiObject, aPropertyDescriptor, PROP_ARRAY_SIZE);
    }
    else {
      // get size of array
      ApiValuePtr o = aApiObject->newValue(apivalue_uint64); // size is an integer value
      accessField(access_read, o, aPropertyDescriptor, PROP_ARRAY_SIZE); // query size
      int arrSz = o->int32Value();
      // single element or range?
      if (aElementCount!=0) {
        // Range of elements: only allowed for reading
        if (aMode!=access_read)
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
            err = accessPropertyByDescriptor(access_read, elementObj, aPropertyDescriptor, aDomain, aIndex+n, 0);
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
          err = accessElementByDescriptor(aMode, aApiObject, aPropertyDescriptor, aDomain, aIndex);
      }
    }
  }
  else {
    // non-array property
    err = accessElementByDescriptor(aMode, aApiObject, aPropertyDescriptor, aDomain, 0);
  }
  return err;
}



ErrorPtr PropertyContainer::accessElementByDescriptor(PropertyAccessMode aMode, ApiValuePtr aApiObject, const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex)
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
      err = container->accessProperty(aMode, aApiObject, aPropertyDescriptor.propertyType==apivalue_object ? "*" : "^", containerDomain, PROP_ARRAY_SIZE, 0, 1 /* second level */);
      if ((aMode!=access_read) && Error::isOK(err)) {
        // give this container a chance to post-process write access
        err = writtenProperty(aPropertyDescriptor, aDomain, aIndex, container);
      }
    }
  }
  else {
    // single value property
    if (aMode==access_read) aApiObject->setType(aPropertyDescriptor.propertyType); // for read, set correct type for value (for write, type should match already)
    if (!accessField(aMode, aApiObject, aPropertyDescriptor, aIndex)) {
      err = ErrorPtr(new VdcApiError(403,"Access denied"));
    }
  }
  return err;
}






