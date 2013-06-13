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


static const FieldDefinition defaultkeys[] = {
  { "parentID", SQLITE_TEXT},
  { NULL, 0 },
};


const FieldDefinition *PersistentParams::getKeyDefs()
{
  return defaultkeys;
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
    sql = string_format("CREATE INDEX parentIndex ON %s (%s)", tableName(), getKeyDefs()->fieldName);
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


ErrorPtr PersistentParams::loadFromStore(const char *aParentIdentifier)
{
  ErrorPtr err;
  sqlite3pp::query qry(paramStore);
  string sql = "SELECT ROWID";
  // key fields
  appendfieldList(sql, getKeyDefs(), true, false);
  // other fields
  appendfieldList(sql, getFieldDefs(), true, false);
  string_format_append(sql, " WHERE %s='%s'", getKeyDefs()->fieldName, aParentIdentifier);
  // now execute query
  if (qry.prepare(sql.c_str())!=SQLITE_OK) {
    // - error could mean schema is not up to date
    qry.reset();
    checkAndUpdateSchema();
    if (qry.prepare(sql.c_str())!=SQLITE_OK) {
      // error now means something is really wrong
      err = paramStore.error();
    }
  }
  if (Error::isOK(err)) {
    sqlite3pp::query::iterator i = qry.begin();
    if (i!=qry.end()) {
      // got record
      // - load ROWID which is always there
      rowid = i->get<long long>(0);
      // - let subclass load the other params
      loadFromRow(i, 1);
    }
  }
  if (Error::isOK(err)) {
    err = loadChildren();
  }
  return err;
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
      // bind the values
      // - the parent identifier is the first
      cmd.bind(0, aParentIdentifier);
      // - let subclass bind the other params
      bindToStatement(cmd, 1);
      // now execute command
      if (cmd.execute()!=SQLITE_OK) {
        // error on update is always a real error - if we loaded the params from the DB, schema IS ok!
        err = paramStore.error();
      }
      else {
        dirty = false;
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
        // - the parent identifier is the first
        cmd.bind(0, aParentIdentifier);
        // - let subclass bind the other params
        bindToStatement(cmd, 1);
        // now execute command
        if (cmd.execute()!=SQLITE_OK) {
          err = paramStore.error();
        }
        else {
          // get the new ROWID
          rowid = paramStore.last_insert_rowid();
          dirty = false;
        }
      }
    }
  }
  // anyway, have childrend checked
  if (Error::isOK(err)) {
    err = saveChildren();
  }
  return err;
}

