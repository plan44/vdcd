//
//  propertycontainer.h
//  vdcd
//
//  Created by Lukas Zeller on 15.08.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__propertycontainer__
#define __vdcd__propertycontainer__

#include "jsonrpccomm.hpp"

using namespace std;

namespace p44 {

  typedef enum {
    ptype_bool,
    ptype_int8,
    ptype_int16,
    ptype_int32,
    ptype_int64,
    ptype_double,
    ptype_charptr, // const char *, read only
    ptype_string, // std::string
    ptype_object, // structured value represented by a container
    ptype_proxy, // field not in this container, get container and ask it for value again
  } PropertyType;

  #define PROP_ARRAY_SIZE -1

  class PropertyContainer;

  struct PropertyDescriptor;

  typedef struct PropertyDescriptor {
    const char *propertyName; ///< name of the property
    PropertyType propertyType; ///< type of the property
    bool isArray; ///< property is an array
    size_t accessKey; ///< key or offset to access the property within its container struct or object
    void *objectKey; ///< identifier for object this property belongs to (for properties spread over sublcasses)
  } PropertyDescriptor;


  typedef boost::shared_ptr<PropertyContainer> PropertyContainerPtr;

  class PropertyContainer
  {

  public:

    /// @name property access API
    /// @{

    /// read or write property
    /// @param aForWrite false for reading, true for writing
    /// @param aJsonObject for read, will be set to the resulting property. For write, this is the object to be written
    /// @param aName name of the property to return. "*" can be passed to return an object listing all properties in this container,
    ///   "^" to return the default property value (internally used for ptype_proxy).
    /// @param aIndex in case of array, the array element to access, -1 to get size of array as integer number
    ///   for non-array properties, aIndex is ignored
    /// @param aElementCount in case of array, the number of elements to collect starting at aIndex
    ///   and return as JSON array. 0 = single element only. PROP_ARRAY_SIZE = all elements up to end of array.
    /// @return Error 501 if property is unknown, 204 if aIndex addresses a non-existing element,
    ///   403 if property exists but cannot be accessed, 415 if value type is incompatible with the property
    ErrorPtr accessProperty(bool aForWrite, JsonObjectPtr &aJsonObject, const string &aName, int aDomain, int aIndex, int aElementCount);

    /// @}

  protected:

    /// @name methods that to be overriden in concrete subclasses to access properties
    /// @{

    /// @return the number of properties in this container
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    virtual int numProps(int aDomain) { return 0; }

    /// get property descriptor by index
    /// @param aPropIndex property index, 0..numProps()-1
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @return pointer to property descriptor or NULL if aPropIndex is out of range
    /// @note base class always returns NULL, which means no properties
    /// @note implementation does not need to check aPropIndex, it will always be within the range set by numProps()
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain) { return NULL; }

    /// access single field in this container
    /// @param aForWrite false for reading, true for writing
    /// @param aPropertyDescriptor decriptor for a single value field/array in this container
    /// @param aPropValue JsonObject with a single value
    /// @param aIndex in case of array, the index of the element to access
    /// @return false if value could not be accessed
    /// @note base class implements pointer+offset access to fields, by using dataStructBasePtr()+accessKey
    virtual bool accessField(bool aForWrite, JsonObjectPtr &aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

    /// get subcontainer for a ptype_object or ptype_proxy property
    /// @param aPropertyDescriptor decriptor for a structured (object) property or a ptype_proxy property
    /// @param aDomain the domain for which to access properties. Call might modify the domain such that it fits
    ///   to the accessed container. For example, one container might support different sets of properties
    ///   (like description/settings/states for DsBehaviours)
    /// @param aIndex, for array properties the element to access (0..size)
    /// @return PropertyContainer representing the property or property array element
    /// @note base class always returns NULL, which means no structured or proxy properties
    virtual PropertyContainer *getContainer(const PropertyDescriptor &aPropertyDescriptor, int &aDomain, int aIndex = 0) { return NULL; };

    /// get base pointer for accessing scalar fields in a struct by adding accessKey from the descriptor to it
    virtual void *dataStructBasePtr(int aIndex = 0) { return NULL; }

    /// @}

  private:

    ErrorPtr accessPropertyByDescriptor(bool aForWrite, JsonObjectPtr &aJsonObject, const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex, int aElementCount);

    ErrorPtr accessElementByDescriptor(bool aForWrite, JsonObjectPtr &aJsonObject, const PropertyDescriptor &aPropertyDescriptor, int aDomain, int aIndex);



  };
  
} // namespace p44

#endif /* defined(__vdcd__propertycontainer__) */
