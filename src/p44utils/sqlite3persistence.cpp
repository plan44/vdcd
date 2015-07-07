//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


ErrorPtr SQLite3Persistence::connectAndInitialize(const char *aDatabaseFileName, int aNeededSchemaVersion, int aLowestValidSchemaVersion, bool aFactoryReset)
{
  int err;
  ErrorPtr errPtr;
  int currentSchemaVersion;

  while (true) {
    // assume DB not yet existing
    currentSchemaVersion = 0;
    if (aFactoryReset) {
      // make sure we are disconnected
      if (initialized)
        finalizeAndDisconnect();
      // first delete the database entirely
      unlink(aDatabaseFileName);
    }
    // now initialize the DB
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
      // check for obsolete (ancient, not-to-be-migrated DB versions)
      if (currentSchemaVersion>0 && aLowestValidSchemaVersion!=0 && currentSchemaVersion<aLowestValidSchemaVersion) {
        // there is a DB, but it is obsolete and should be deleted
        aFactoryReset = true; // force a factory reset
        disconnect();
        continue; // try again
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
      break;
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
