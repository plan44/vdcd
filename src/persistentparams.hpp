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
    
  };


  class PersistentParams
  {
    ParamStore &paramStore; ///< the associated parameter store
    bool dirty; ///< if set, means that values need to be saved
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
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int aFirstFieldIndex) = 0;

    /// bind values to passed statement
    /// @param aStatement statement to bind parameter values to
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int aFirstFieldIndex) = 0;

    /// load child parameters (if any)
    virtual ErrorPtr loadChildren() { return ErrorPtr(); };

    /// save child parameters (if any)
    virtual ErrorPtr saveChildren() { return ErrorPtr(); };


    /// @}

    /// get parameter set from persistent storage
    /// @param aPersistence the sqlite3persistence to load from
    /// @param aKey the record key
    /// @param aParentKey the parent key or NULL if no parent
    ErrorPtr loadFromStore(const char *aParentIdentifier);

    /// save parameter set to persistent storage if dirty
    /// @param aPersistence the sqlite3persistence to load from
    /// @param aParentIdentifier identifies the parent of this parameter set (the dsid or the ROWID of a parent parameter set)
    ErrorPtr saveToStore(const char *aParentIdentifier);

  private:
    /// check and update schema to hold the parameters
    void checkAndUpdateSchema();

  };
  
  
} // namespace


#endif /* defined(__p44bridged__PersistentParams__) */
