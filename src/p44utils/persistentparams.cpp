//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0

#include "persistentparams.hpp"

using namespace p44;


PersistentParams::PersistentParams(ParamStore &aParamStore) :
  paramStore(aParamStore),
  dirty(false),
  rowid(false)
{
}



static const size_t numKeys = 1;

size_t PersistentParams::numKeyDefs()
{
  return numKeys;
}

const FieldDefinition *PersistentParams::getKeyDef(size_t aIndex)
{
  static const FieldDefinition keyDefs[numKeys] = {
    { "parentID", SQLITE_TEXT }
  };
  if (aIndex<numKeys)
    return &keyDefs[aIndex];
  return NULL;
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
    for (size_t i=0; i<numKeyDefs(); ++i) {
      const FieldDefinition *fd = getKeyDef(i);
      if (!firstfield)
        sql += ", ";
      sql += fieldDeclaration(fd);
      firstfield = false;
    }
    // - data fields
    for (size_t i=0; i<numFieldDefs(); ++i) {
      const FieldDefinition *fd = getFieldDef(i);
      sql += ", ";
      sql += fieldDeclaration(fd);
    }
    sql += ")";
    // - create it
    sqlite3pp::command cmd(paramStore, sql.c_str());
    cmd.execute();
    // create index for parentID (first field, getKeyDef(0))
    sql = string_format("CREATE INDEX %s_parentIndex ON %s (%s)", tableName(), tableName(), getKeyDef(0)->fieldName);
    cmd.prepare(sql.c_str());
    cmd.execute();
  }
  else {
    // table exists, but maybe not all fields
    // - just try to add each of them. SQLite will not accept duplicates anyway
    for (size_t i=0; i<numFieldDefs(); ++i) {
      const FieldDefinition *fd = getFieldDef(i);
      sql = string_format("ALTER TABLE %s ADD ", tableName());
      sql += fieldDeclaration(fd);
      sqlite3pp::command cmd(paramStore);
      if (cmd.prepare(sql.c_str())==SQLITE_OK)
        cmd.execute();
    }
  }
}



size_t PersistentParams::appendfieldList(string &sql, bool keyFields, bool aAppendFields, bool aWithParamAssignment)
{
  size_t numfields = keyFields ? numKeyDefs() : numFieldDefs();
  for (int i=0; i<numfields; ++i) {
    const FieldDefinition *fd = keyFields ? getKeyDef(i) : getFieldDef(i);
    if (aAppendFields)
      sql += ", ";
    sql += fd->fieldName;
    if (aWithParamAssignment)
      sql += "=?";
    aAppendFields = true; // from second field on, always append
  }
  return numfields;
}


// helper for implementation of loadChildren()
sqlite3pp::query *PersistentParams::newLoadAllQuery(const char *aParentIdentifier)
{
  sqlite3pp::query * queryP = new sqlite3pp::query(paramStore);
  string sql = "SELECT ROWID";
  // key fields
  appendfieldList(sql, true , true, false);
  // other fields
  appendfieldList(sql, false, true, false);
  // limit to entries linked to parent
  string_format_append(sql, " FROM %s WHERE %s='%s'", tableName(), getKeyDef(0)->fieldName, aParentIdentifier);
  FOCUSLOG("newLoadAllQuery for parent='%s': %s\n", aParentIdentifier, sql.c_str());
  // now execute query
  if (queryP->prepare(sql.c_str())!=SQLITE_OK) {
    FOCUSLOG("- query not successful - assume wrong schema -> calling checkAndUpdateSchema()\n");
    // - error could mean schema is not up to date
    queryP->reset();
    checkAndUpdateSchema();
    FOCUSLOG("newLoadAllQuery: retrying newLoadAllQuery after schema update: %s\n", sql.c_str());
    if (queryP->prepare(sql.c_str())!=SQLITE_OK) {
      LOG(LOG_ERR, "newLoadAllQuery: %s - failed: %s\n", sql.c_str(), paramStore.error()->description().c_str());
      // error now means something is really wrong
      delete queryP;
      return NULL;
    }
  }
  return queryP;
}



/// load values from passed row
void PersistentParams::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  // - load ROWID which is always there
  rowid = aRow->get<long long>(aIndex++);
  FOCUSLOG("loadFromRow: fetching ROWID=%lld\n", rowid);
  // - skip the row that identifies the parent (because that's the fetch criteria)
  aIndex++;
}


ErrorPtr PersistentParams::loadFromStore(const char *aParentIdentifier)
{
  ErrorPtr err;
  rowid = 0; // loading means that we'll get the rowid from the DB, so forget any previous one
  sqlite3pp::query *queryP = newLoadAllQuery(aParentIdentifier);
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
      uint64_t flags; // storage to distribute flags over hierarchy
      loadFromRow(row, index, &flags); // might set dirty when assigning properties...
      dirty = false; // ...so: just loaded: make clean
    }
    delete queryP; queryP = NULL;
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
void PersistentParams::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  // the parent identifier is the first column to bind
  aStatement.bind(aIndex++, aParentIdentifier, false); // text not static
}


ErrorPtr PersistentParams::saveToStore(const char *aParentIdentifier, bool aMultipleChildrenAllowed)
{
  ErrorPtr err;
  if (dirty) {
    sqlite3pp::command cmd(paramStore);
    string sql;
    // cleanup: remove all previous records for that parent if not multiple children allowed
    if (!aMultipleChildrenAllowed) {
      sql = string_format("DELETE FROM %s WHERE %s='%s'", tableName(), getKeyDef(0)->fieldName, aParentIdentifier);
      if (rowid!=0) {
        string_format_append(sql, " AND ROWID!=%lld", rowid);
      }
      FOCUSLOG("- cleanup before save: %s\n", sql.c_str());
      if (paramStore.executef(sql.c_str()) != SQLITE_OK) {
        LOG(LOG_ERR, "- cleanup error (ignored): %s\n", sql.c_str(), paramStore.error()->description().c_str());
      }
    }
    // now save
    if (rowid!=0) {
      // already exists in the DB, just update
      sql = string_format("UPDATE %s SET ", tableName());
      // - update all fields, even key fields may change (as long as they don't collide with another entry)
      appendfieldList(sql, true, false, true);
      appendfieldList(sql, false, true, true);
      string_format_append(sql, " WHERE ROWID=%lld", rowid);
      // now execute command
      FOCUSLOG("saveToStore: update existing row for parent='%s': %s\n", aParentIdentifier, sql.c_str());
      if (cmd.prepare(sql.c_str())!=SQLITE_OK) {
        // error on update is always a real error - if we loaded the params from the DB, schema IS ok!
        err = paramStore.error();
      }
      if (Error::isOK(err)) {
        // bind the values
        int index = 1; // SQLite parameter indexes are 1-based!
        bindToStatement(cmd, index, aParentIdentifier, 0); // no flags yet, class hierarchy will collect them
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
      size_t numFields = appendfieldList(sql, true, false, false);
      numFields += appendfieldList(sql, false, true, false);
      sql += ") VALUES (";
      bool first = true;
      for (int i=0; i<numFields; i++) {
        if (!first) sql += ", ";
        sql += "?";
        first = false;
      }
      sql += ")";
      // prepare
      FOCUSLOG("saveToStore: insert new row for parent='%s': %s\n", aParentIdentifier, sql.c_str());
      if (cmd.prepare(sql.c_str())!=SQLITE_OK) {
        FOCUSLOG("- insert not successful - assume wrong schema -> calling checkAndUpdateSchema()\n");
        // - error on INSERT could mean schema is not up to date
        cmd.reset();
        checkAndUpdateSchema();
        FOCUSLOG("saveToStore: retrying insert after schema update: %s\n", sql.c_str());
        if (cmd.prepare(sql.c_str())!=SQLITE_OK) {
          // error now means something is really wrong
          err = paramStore.error();
        }
      }
      if (Error::isOK(err)) {
        // bind the values
        int index = 1; // SQLite parameter indexes are 1-based!
        bindToStatement(cmd, index, aParentIdentifier, 0); // no flags yet, class hierarchy will collect them
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
    if (!Error::isOK(err)) {
      LOG(LOG_ERR, "saveToStore: %s - failed: %s\n", sql.c_str(), err->description().c_str());
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
    FOCUSLOG("deleteFromStore: deleting row %lld in table %s\n", rowid, tableName());
    if (paramStore.executef("DELETE FROM %s WHERE ROWID=%lld", tableName(), rowid) != SQLITE_OK) {
      err = paramStore.error();
    }
    // deleted, forget
    rowid = 0;
  }
  // anyway, remove children
  if (Error::isOK(err)) {
    err = deleteChildren();
  }
  if (!Error::isOK(err)) {
    LOG(LOG_ERR, "deleteFromStore: table=%s, ROWID=%lld - failed: %s\n", tableName(), rowid, err->description().c_str());
  }
  return err;
}


