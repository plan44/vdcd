//
//  persistentparams.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 13.06.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__PersistentParams__
#define __p44bridged__PersistentParams__

#include "sqlite3persistence.hpp"

using namespace std;

namespace p44 {

  typedef struct {
    const char *fieldName;
    int dataTypeCode;
  } FieldDefinition;


  class ParamStore : public SQLite3Persistence
  {
    typedef SQLite3Persistence inherited;
  };


  class PersistentParams
  {
    bool dirty; ///< if set, means that values need to be saved
  protected:
    ParamStore &paramStore; ///< the associated parameter store
  public:
    PersistentParams(ParamStore &aParamStore);
    uint64_t rowid; ///< ROWID of the persisted data, 0 if not yet persisted

    /// @name interface to be implemented for specific parameter sets in subclasses
    /// @{

    /// return table name
    /// @return table name
    virtual const char *tableName() = 0;

    /// get primary key field definitions
    /// @return array of FieldDefinition structs, terminated by entry with fieldName==NULL
    ///   these fields together build the primary key, which must be unique (only one record with a given
    ///   combination of key values may exist in the DB)
    /// @note the first field must be the one which identifies the parent.
    //    Other key fields may be needed if parent can have more than one child
    /// @note for the base class, this returns a single string field named "parentID",
    ///   which is usually the dsid of the device for which this is the parameter set
    virtual const FieldDefinition *getKeyDefs();

    /// get data field definitions
    /// @return array of FieldDefinition structs, terminated by entry with fieldName==NULL
    virtual const FieldDefinition *getFieldDefs() = 0;

    /// load values from passed row
    /// @param aRow result row to get parameter values from
    /// @param aIndex index of first column to load
    /// @note the base class loads ROWID and the parent identifier (first item in keyDefs) automatically.
    ///   subclasses should always call inherited loadFromRow() FIRST
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex);

    /// bind values to passed statement
    /// @param aStatement statement to bind parameter values to
    /// @param aIndex index of first column to bind, will be incremented past the last bound column
    /// @note the base class binds ROWID and the parent identifier (first item in keyDefs) automatically.
    ///   subclasses should always call inherited bindToStatement() FIRST
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier);

    /// load child parameters (if any)
    virtual ErrorPtr loadChildren() { return ErrorPtr(); };

    /// save child parameters (if any)
    virtual ErrorPtr saveChildren() { return ErrorPtr(); };

    /// delete child parameters (if any)
    virtual ErrorPtr deleteChildren() { return ErrorPtr(); };

    /// @}

    /// mark the parameter set dirty (so it will be saved to DB next time saveToStore is called
    virtual void markDirty();


    /// get parameter set from persistent storage
    /// @param aParentIdentifier identifies the parent of this parameter set (the dsid or the ROWID of a parent parameter set)
    ErrorPtr loadFromStore(const char *aParentIdentifier);

    /// save parameter set to persistent storage if dirty
    /// @param aParentIdentifier identifies the parent of this parameter set (the dsid or the ROWID of a parent parameter set)
    ErrorPtr saveToStore(const char *aParentIdentifier);

    /// delete this parameter set from the store
    ErrorPtr deleteFromStore();

    /// helper for implementation of loadChildren()
    /// @return a prepared query set up to iterate through all records with a given parent identifier, or NULL on error
    /// @param aParentIdentifier identifies the parent of this parameter set (the dsid or the ROWID of a parent parameter set)
    sqlite3pp::query *newLoadAllQuery(const char *aParentIdentifier);


  private:
    /// check and update schema to hold the parameters
    void checkAndUpdateSchema();

  };
  
  
} // namespace


#endif /* defined(__p44bridged__PersistentParams__) */