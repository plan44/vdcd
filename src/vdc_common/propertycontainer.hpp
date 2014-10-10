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

#ifndef __vdcd__propertycontainer__
#define __vdcd__propertycontainer__

#include "vdcapi.hpp"

using namespace std;

namespace p44 {

  class PropertyContainer;

  struct PropertyDescriptor;


  #define OKEY(x) ((intptr_t)&x) ///< macro to define unique object keys by using address of a variable

  #define PROPINDEX_NONE -1 ///< special value to signal "no next descriptor" for getDescriptorByName

  /// type for const tables describing static properties
  typedef struct PropertyDescription {
    const char *propertyName; ///< name of the property
    uint16_t propertyType; ///< type of the property value
    size_t fieldKey; ///< key for accessing the property within its container. (size_t to allow using offset into struct)
    intptr_t objectKey; ///< identifier for object this property belongs to (for properties spread over sublcasses)
  } PropertyDescription;

  typedef enum {
    access_read,
    access_write,
    access_write_preload
  } PropertyAccessMode;

  typedef enum {
    proptype_mask = 0x7F,
    propflag_container = 0x80
  } PropertyFlags;


  typedef boost::intrusive_ptr<PropertyDescriptor> PropertyDescriptorPtr;

  /// description of a property
  class PropertyDescriptor : public P44Obj
  {
  public:
    /// constructor
    PropertyDescriptor(PropertyDescriptorPtr aParentDescriptor) : parentDescriptor(aParentDescriptor) {};
    /// the parent descriptor (NULL at root level of DsAdressables)
    PropertyDescriptorPtr parentDescriptor;
    /// name of the property
    virtual const char *name() const = 0;
    /// type of the property
    virtual ApiValueType type() const = 0;
    /// access index/key of the property
    virtual size_t fieldKey() const = 0;
    /// extra identification for the object from the API perspective (allows having more than one API object within one C++ object
    virtual intptr_t objectKey() const = 0;
    /// is array container
    virtual bool isArrayContainer() const = 0;
    /// checks
    bool hasObjectKey(char &aMemAddrObjectKey) { return (objectKey()==(intptr_t)&aMemAddrObjectKey); };
    bool hasObjectKey(intptr_t aIntObjectKey) { return (objectKey()==aIntObjectKey); };
    bool isStructured() { return type()==apivalue_object || isArrayContainer(); };
  };


  /// description of a static property (usually a named field described via a PropertyDescription in a const table)
  class StaticPropertyDescriptor : public PropertyDescriptor
  {
    typedef PropertyDescriptor inherited;
    const PropertyDescription *descP;

  public:
    /// create from const table entry
    StaticPropertyDescriptor(const PropertyDescription *aDescP, PropertyDescriptorPtr aParentDescriptor) :
      inherited(aParentDescriptor),
      descP(aDescP)
    {};

    virtual const char *name() const { return descP->propertyName; }
    virtual ApiValueType type() const { return (ApiValueType)((descP->propertyType) & proptype_mask); }
    virtual size_t fieldKey() const { return descP->fieldKey; }
    virtual intptr_t objectKey() const { return descP->objectKey; }
    virtual bool isArrayContainer() const { return descP->propertyType & propflag_container; };
  };


  /// description of a dynamic property (such as an element of a container, created on the fly when accessed)
  class DynamicPropertyDescriptor : public PropertyDescriptor
  {
    typedef PropertyDescriptor inherited;
  public:
    DynamicPropertyDescriptor(PropertyDescriptorPtr aParentDescriptor) :
      inherited(aParentDescriptor),
      arrayContainer(false)
    {};
    string propertyName; ///< name of the property
    ApiValueType propertyType; ///< type of the property value
    size_t propertyFieldKey; ///< key for accessing the property within its container. (size_t to allow using offset into struct)
    intptr_t propertyObjectKey; ///< identifier for object this property belongs to (for properties spread over sublcasses)
    bool arrayContainer;

    virtual const char *name() const { return propertyName.c_str(); }
    virtual ApiValueType type() const { return propertyType; }
    virtual size_t fieldKey() const { return propertyFieldKey; }
    virtual intptr_t objectKey() const { return propertyObjectKey; }
    virtual bool isArrayContainer() const { return arrayContainer; };
  };




  typedef boost::intrusive_ptr<PropertyContainer> PropertyContainerPtr;

  /// Base class for objects providing API properties
  /// Implements generic mechanisms to handle accessing elements and subtrees of named propeties.
  /// There is no strict relation between C++ classes of the framework and the property tree;
  /// a single C++ class can implement multiple levels of the property tree.
  /// PropertyContainer is also designed to allow subclasses adding property fields to the fields
  /// provided by base classes, without modifications of the base class.
  class PropertyContainer : public P44Obj
  {

  public:

    /// @name property access API
    /// @{

    /// read or write property
    /// @param aMode access mode (see PropertyAccessMode: read, write or write preload)
    /// @param aQueryObject the object defining the read or write query
    /// @param aResultObject for read, must be an object
    /// @param aParentDescriptor the descriptor of the parent property, can be NULL at root level
    /// @return Error 501 if property is unknown, 403 if property exists but cannot be accessed, 415 if value type is incompatible with the property
    ErrorPtr accessProperty(PropertyAccessMode aMode, ApiValuePtr aQueryObject, ApiValuePtr aResultObject, int aDomain, PropertyDescriptorPtr aParentDescriptor);

    /// @}

  protected:

    /// @name methods that should be overriden in concrete subclasses to access properties
    /// @{

    /// @return the number of properties in this container
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @param aParentDescriptor the descriptor of the parent property, is NULL at root level of a DsAdressable
    ///   Allows single C++ class to handle multiple levels of nested property tree objects
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor) { return 0; }

    /// get property descriptor by index.
    /// @param aPropIndex property index, 0..numProps()-1
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @param aParentDescriptor the descriptor of the parent property, is NULL at root level of a DsAdressable
    /// @return pointer to property descriptor or NULL if aPropIndex is out of range
    /// @note base class always returns NULL, which means no properties
    /// @note implementation does not need to check aPropIndex, it will always be within the range set by numProps()
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor) { return NULL; }

    /// get next property descriptor by name
    /// @param aPropMatch a plain property name, or a *-terminated wildcard, or a indexed access specifier #n, or empty for matching all
    /// @param aStartIndex on input: the property index to start searching, on exit: the next PropertyDescriptor to check.
    ///   When the search is exhausted, aStartIndex is set to PROPINDEX_NONE to signal there is no next property to check
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @param aParentDescriptor the descriptor of the parent property, is NULL at root level of a DsAdressable
    /// @return pointer to property descriptor or NULL if aPropIndex is out of range
    /// @note base class provides a default implementation which uses numProps/getDescriptorByIndex and compares names.
    ///   Subclasses may override this to more efficiently access array-like containers where aPropMatch can directly be used
    ///   to find an element (without iterating through all indices).
    virtual PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);

    /// get subcontainer for a apivalue_object property
    /// @param aPropertyDescriptor descriptor for a structured (object) property. Call might modify this pointer such as setting it to
    ///   NULL when new container is the root of another DsAdressable. The resulting pointer will be passed to the container's access
    ///   methods as aParentDescriptor.
    /// @param aDomain the domain for which to access properties. Call might modify the domain such that it fits
    ///   to the accessed container. For example, one container might support different sets of properties
    ///   (like description/settings/states for DsBehaviours)
    /// @return PropertyContainer representing the property or property array element
    /// @note base class always returns NULL, which means no structured or proxy properties
    virtual PropertyContainerPtr getContainer(PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain) { return NULL; };

    /// access single field in this container
    /// @param aMode access mode (see PropertyAccessMode: read, write or write preload)
    /// @param aPropValue JsonObject with a single value
    /// @param aPropertyDescriptor decriptor for a single value field in this container
    /// @return false if value could not be accessed
    /// @note this base class always returns false, as it does not have any properties implemented
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor) { return false; };

    /// post-process written properties in subcontainers. This is called after a property write access has
    /// compleded successfully in a subcontainer (as returned by this object's getContainer()), and can be used to commit container
    /// wide transactions etc.
    /// @param aMode the property access mode (write or write_preload - for the latter, container might want to prevent committing, such as for MOC channel updates)
    /// @param aPropertyDescriptor decriptor for a structured (object) property
    /// @param aDomain the domain in which the write access happened
    /// @param aInContainer the container object that was accessed
    virtual ErrorPtr writtenProperty(PropertyAccessMode aMode, PropertyDescriptorPtr aPropertyDescriptor, int aDomain, PropertyContainerPtr aInContainer) { return ErrorPtr(); }

    /// @}

    /// @name utility methods
    /// @{

    /// parse aPropmatch for numeric index (both plain number and #n are allowed, plus empty and "*" wildcards)
    /// @param aPropMatch property name to match
    /// @param aStartIndex current index, will be set to next matching index or PROPINDEX_NONE
    ///   if no next index available for this aPropMatch
    /// @return true if aPropMatch actually specifies a numeric name, false if aPropMatch is a wildcard
    ///   (The #n notation is not considered a numeric name!)
    bool getNextPropIndex(string aPropMatch, int &aStartIndex);

    /// @param aPropMatch property name to match
    /// @return true if aPropMatch specifies a name (vs. "*"/"" or "#n")
    bool isNamedPropSpec(string &aPropMatch);

    /// @param aPropMatch property name to match
    /// @return true if aPropMatch specifies a match-all wildcard ("*" or "")
    bool isMatchAll(string &aPropMatch);

    /// utility method to get next property descriptor in numerically addressed containers by numeric name
    /// @param aPropMatch a match-all wildcard (* or empty), a numeric name or indexed access specifier #n
    /// @param aStartIndex on input: the property index to start searching, on exit: the next PropertyDescriptor to check.
    ///   When the search is exhausted, aStartIndex is set to PROPINDEX_NONE to signal there is no next property to check
    /// @param aDomain the domain for which to access properties (different APIs might have different properties for the same PropertyContainer)
    /// @param aParentDescriptor the descriptor of the parent property, can be NULL at root level
    /// @param aObjectKey the object key the resulting descriptor should have
    /// @return pointer to property descriptor or NULL if aPropIndex is out of range
    /// @note the returned descriptor will have its fieldKey set to the index position of the to-be-accessed element,
    ///   and its type inherited from the parent descriptor
    PropertyDescriptorPtr getDescriptorByNumericName(
      string aPropMatch, int &aStartIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor,
      intptr_t aObjectKey
    );



    /// @}

  };
  
} // namespace p44

#endif /* defined(__vdcd__propertycontainer__) */
