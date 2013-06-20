//
//  persistentparams.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 13.06.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "persistentparams.hpp"

using namespace p44;


PersistentParams::PersistentParams(ParamStore &aParamStore) :
  paramStore(aParamStore),
  dirty(false),
  rowid(false)
{
}




const FieldDefinition *PersistentParams::getKeyDefs()
{
  static const FieldDefinition keyDefs[] = {
    { "parentID", SQLITE_TEXT },
    { NULL, 0 },
  };
  return keyDefs;
}



static string fieldDeclaration(const FieldDefinition *aFieldDefP)
{
  const char *t = NULL;
  switch (aFieldDefP->dataTypeCode) {
    case SQLITE_INTEGER : t = "INTEGER"; break;
    case SQLITE_FLOAT : t = "FLOAT"; break;
    case SQLITE_TEXT : t = "TEXT"; break;
    case SQLITE_BLOB : t = "BLOB"; break;
  }
  return string_format("%s %s", aFieldDefP->fieldName, t);
}


void PersistentParams::checkAndUpdateSchema()
{
  // check for table
  string sql = string_format("SELECT name FROM sqlite_master WHERE name ='%s' and type='table'", tableName());
  sqlite3pp::query qry(paramStore, sql.c_str());
  sqlite3pp::query::iterator i = qry.begin();
  if (i==qry.end()) {
    // table does not yet exist
    // - new table
    sql = string_format("CREATE TABLE %s (", tableName());
    bool firstfield = true;
    // - key fields
    for (const FieldDefinition *fd = getKeyDefs(); fd->fieldName!=NULL; ++fd) {
      if (!firstfield)
        sql += ", ";
      sql += fieldDeclaration(fd);
      firstfield = false;
    }
    // - data fields
    for (const FieldDefinition *fd = getFieldDefs(); fd->fieldName!=NULL; ++fd) {
      sql += ", ";
      sql += fieldDeclaration(fd);
    }
    sql += ")";
    // - create it
    sqlite3pp::command cmd(paramStore, sql.c_str());
    cmd.execute();
    // create index for parentID (first field on getKeyDefs()
    sql = string_format("CREATE INDEX %s_parentIndex ON %s (%s)", tableName(), tableName(), getKeyDefs()->fieldName);
    cmd.prepare(sql.c_str());
    cmd.execute();
  }
  else {
    // table exists, but maybe not all fields
    // - just try to add each of them. SQLite will not accept duplicates anyway
    for (const FieldDefinition *fd = getFieldDefs(); fd->fieldName!=NULL; ++fd) {
      sql = string_format("ALTER TABLE %s ADD ", tableName());
      sql += fieldDeclaration(fd);
      sqlite3pp::command cmd(paramStore, sql.c_str());
      cmd.execute();
    }
  }
}



static int appendfieldList(string &sql, const FieldDefinition *aDefinitions, bool aAppendFields, bool aWithParamAssignment)
{
  int numfields = 0;
  while(aDefinitions->fieldName!=NULL) {
    if (aAppendFields)
    sql += ", ";
    sql += aDefinitions->fieldName;
    if (aWithParamAssignment)
      sql += "=?";
    aAppendFields = true; // from second field on, always append
    aDefinitions++; // next
    numfields++; // count the field
  }
  return numfields;
}


// helper for implementation of loadChildren()
sqlite3pp::query *PersistentParams::newLoadAllQuery(const char *aParentIdentifier)
{
  sqlite3pp::query * queryP = new sqlite3pp::query(paramStore);
  string sql = "SELECT ROWID";
  // key fields
  appendfieldList(sql, getKeyDefs(), true, false);
  // other fields
  appendfieldList(sql, getFieldDefs(), true, false);
  string_format_append(sql, " FROM %s WHERE %s='%s'", tableName(), getKeyDefs()->fieldName, aParentIdentifier);
  // now execute query
  if (queryP->prepare(sql.c_str())!=SQLITE_OK) {
    // - error could mean schema is not up to date
    queryP->reset();
    checkAndUpdateSchema();
    if (queryP->prepare(sql.c_str())!=SQLITE_OK) {
      // error now means something is really wrong
      delete queryP;
      return NULL;
    }
  }
  return queryP;
}



/// load values from passed row
void PersistentParams::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  // - load ROWID which is always there
  rowid = aRow->get<long long>(aIndex++);
  // - skip the row that identifies the parent (because that's the fetch criteria)
  aIndex++;
}


ErrorPtr PersistentParams::loadFromStore(const char *aParentIdentifier)
{
  ErrorPtr err;
  rowid = 0; // loading means that we'll get the rowid from the DB, so forget any previous one
  sqlite3pp::query * queryP = newLoadAllQuery(aParentIdentifier);
  if (queryP==NULL) {
    // real error preparing query
    err = paramStore.error();
  }
  else {
    sqlite3pp::query::iterator row = queryP->begin();
    // Note: it might be OK to not find any stored params in the DB. If so, values are left untouched
    if (row!=queryP->end()) {
      // got record
      int index = 0;
      loadFromRow(row, index);
      dirty = false; // just loaded: clean
    }
  }
  if (Error::isOK(err)) {
    err = loadChildren();
  }
  return err;
}



void PersistentParams::markDirty()
{
  dirty = true;
}



/// bind values to passed statement
void PersistentParams::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  // the parent identifier is the first column to bind
  aStatement.bind(aIndex++, aParentIdentifier, false); // text not static
}


ErrorPtr PersistentParams::saveToStore(const char *aParentIdentifier)
{
  ErrorPtr err;
  if (dirty) {
    sqlite3pp::command cmd(paramStore);
    string sql;
    if (rowid!=0) {
      // already exists in the DB, just update
      sql = string_format("UPDATE %s SET ", tableName());
      // - update all fields, even key fields may change (as long as they don't collide with another entry)
      appendfieldList(sql, getKeyDefs(), false, true);
      appendfieldList(sql, getFieldDefs(), true, true);
      string_format_append(sql, " WHERE ROWID=%lld", rowid);
      // now execute command
      if (cmd.prepare(sql.c_str())!=SQLITE_OK) {
        // error on update is always a real error - if we loaded the params from the DB, schema IS ok!
        err = paramStore.error();
      }
      if (Error::isOK(err)) {
        // bind the values
        int index = 1; // SQLite parameter indexes are 1-based!
        bindToStatement(cmd, index, aParentIdentifier);
        // now execute command
        if (cmd.execute()==SQLITE_OK) {
          // ok, updated ok
          dirty = false;
        }
        else {
          // failed
          err = paramStore.error();
        }
      }
    }
    else {
      // seems new, insert. But use INSERT OR REPLACE to make sure key constraints are enforced
      sql = string_format("INSERT OR REPLACE INTO %s (", tableName());;
      int numFields = appendfieldList(sql, getKeyDefs(), false, false);
      numFields += appendfieldList(sql, getFieldDefs(), true, false);
      sql += ") VALUES (";
      bool first = true;
      for (int i=0; i<numFields; i++) {
        if (!first) sql += ", ";
        sql += "?";
        first = false;
      }
      sql += ")";
      // prepare
      if (cmd.prepare(sql.c_str())!=SQLITE_OK) {
        // - error on INSERT could mean schema is not up to date
        cmd.reset();
        checkAndUpdateSchema();
        if (cmd.prepare(sql.c_str())!=SQLITE_OK) {
          // error now means something is really wrong
          err = paramStore.error();
        }
      }
      if (Error::isOK(err)) {
        // bind the values
        int index = 1; // SQLite parameter indexes are 1-based!
        bindToStatement(cmd, index, aParentIdentifier);
        // now execute command
        if (cmd.execute()==SQLITE_OK) {
          // get the new ROWID
          rowid = paramStore.last_insert_rowid();
          dirty = false;
        }
        else {
          // failed
          err = paramStore.error();
        }
      }
    }
  }
  // anyway, have children checked
  if (Error::isOK(err)) {
    err = saveChildren();
  }
  return err;
}


ErrorPtr PersistentParams::deleteFromStore()
{
  ErrorPtr err;
  dirty = false; // forget any unstored changes
  if (rowid!=0) {
    if (paramStore.executef("DELETE FROM %s WHERE ROWID=%d", tableName(), rowid) != SQLITE_OK) {
      err = paramStore.error();
    }
    // deleted, forget
    rowid = 0;
  }
  // anyway, remove children
  if (Error::isOK(err)) {
    err = deleteChildren();
  }
  return err;
}


