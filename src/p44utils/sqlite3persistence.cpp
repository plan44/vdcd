//
//  sqlite3persistence.cpp
//
//  Created by Lukas Zeller on 13.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "sqlite3persistence.hpp"

#include "logger.hpp"

using namespace p44;


const char *SQLite3Error::domain()
{
  return "SQLite3";
}


const char *SQLite3Error::getErrorDomain() const
{
  return SQLite3Error::domain();
};


SQLite3Error::SQLite3Error(int aSQLiteError, const char *aSQLiteMessage, const char *aContextMessage) :
  Error(ErrorCode(aSQLiteError), string(nonNullCStr(aContextMessage)).append(nonNullCStr(aSQLiteMessage)))
{
}


ErrorPtr SQLite3Error::err(int aSQLiteError, const char *aSQLiteMessage, const char *aContextMessage)
{
  if (aSQLiteError==SQLITE_OK)
    return ErrorPtr();
  return ErrorPtr(new SQLite3Error(aSQLiteError, aSQLiteMessage, aContextMessage));
}


SQLite3Persistence::SQLite3Persistence() :
  initialized(false)
{
}


SQLite3Persistence::~SQLite3Persistence()
{
  finalizeAndDisconnect();
}


ErrorPtr SQLite3Persistence::error(const char *aContextMessage)
{
  return SQLite3Error::err(error_code(), error_msg(), aContextMessage);
}


bool SQLite3Persistence::isAvailable()
{
  return initialized;
}


ErrorPtr SQLite3Persistence::connectAndInitialize(const char *aDatabaseFileName, int aNeededSchemaVersion, bool aFactoryReset)
{
  int err;
  ErrorPtr errPtr;
  int currentSchemaVersion = 0; // assume DB not yet existing

  if (aFactoryReset) {
    // make sure we are disconnected
    if (initialized)
      finalizeAndDisconnect();
    // first delete the database entirely
    unlink(aDatabaseFileName);
  }

  if (!initialized) {
    err = connect(aDatabaseFileName);
    if (err!=SQLITE_OK) {
      LOG(LOG_ERR, "SQLite3Persistence: Cannot open %s : %s\n", aDatabaseFileName, error_msg());
      return error();
    }
    // query the DB version
    sqlite3pp::query qry(*this);
    if (qry.prepare("SELECT schemaVersion FROM globs")==SQLITE_OK) {
      sqlite3pp::query::iterator row = qry.begin();
      if (row!=qry.end()) {
        // get current db version
        currentSchemaVersion = row->get<int>(0);
      }
      qry.finish();
    }
    // migrate if needed
    if (currentSchemaVersion>aNeededSchemaVersion) {
      errPtr = SQLite3Error::err(SQLITE_PERSISTENCE_ERR_SCHEMATOONEW,"Database has too new schema version: cannot be used");
    }
    else {
      while (currentSchemaVersion<aNeededSchemaVersion) {
        // get SQL statements for migration
        int nextSchemaVersion = aNeededSchemaVersion;
        std::string migrationSQL = dbSchemaUpgradeSQL(currentSchemaVersion, nextSchemaVersion);
        if (migrationSQL.size()==0) {
          LOG(LOG_ERR, "SQLite3Persistence: Cannot update from version %d to %d\n", currentSchemaVersion, nextSchemaVersion);
          errPtr = SQLite3Error::err(SQLITE_PERSISTENCE_ERR_MIGRATION,"Database migration error: no update path available");
          break;
        }
        // execute the migration SQL
        sqlite3pp::command migrationCmd(*this);
        err = migrationCmd.prepare(migrationSQL.c_str());
        if (err==SQLITE_OK)
          err = migrationCmd.execute_all();
        if (err!=SQLITE_OK) {
          LOG(LOG_ERR,
            "SQLite3Persistence: Error executing migration SQL from version %d to %d = %s : %s\n",
            currentSchemaVersion, nextSchemaVersion, migrationSQL.c_str(), error_msg()
          );
          errPtr = error("Error executing migration SQL: ");
          break;
        }
        migrationCmd.finish();
        // successful, we have reached a new version
        currentSchemaVersion = nextSchemaVersion;
        // set it in globs
        err = executef("UPDATE globs SET schemaVersion = %d", currentSchemaVersion);
        if (err!=SQLITE_OK) {
          LOG(LOG_ERR, "SQLite3Persistence: Cannot set globs.schemaVersion = %d\n", currentSchemaVersion, error_msg());
          errPtr = error("Error setting schema version: ");
          break;
        }
      }
    }
    if (errPtr) {
      disconnect();
    }
    else {
      initialized = true;
    }
  }
  // return status
  return errPtr;
}


void SQLite3Persistence::finalizeAndDisconnect()
{
  if (initialized) {
    disconnect();
    initialized = false;
  }
}



string SQLite3Persistence::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string s;
  // default is creating the globs table when starting from scratch
  if (aFromVersion==0) {
    s =
      "CREATE TABLE globs ("
      " ROWID INTEGER PRIMARY KEY AUTOINCREMENT,"
      " schemaVersion INTEGER"
      ");"
      "INSERT INTO globs (schemaVersion) VALUES (0);";
  }
  return s;
}
