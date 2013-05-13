//
//  sqlite3persistence.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 13.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "sqlite3persistence.hpp"

#include "logger.hpp"

using namespace p44;

int SQLite3Persistence::connectAndInitialize(const char *aDatabaseFileName, int aNeededSchemaVersion)
{
  int err;
  int currentSchemaVersion = 0; // assume DB not yet existing

  err = connect(aDatabaseFileName);
  if (err!=SQLITE_OK) {
    LOG(LOG_ERR, "SQLite3Persistence: Cannot open %s : %s\n", aDatabaseFileName, error_msg());
    return err;
  }
  // query the DB version
  sqlite3pp::query qry(*this, "SELECT schemaVersion FROM globs");
  sqlite3pp::query::iterator row = qry.begin();
  if (row!=qry.end()) {
    // get current db version
    currentSchemaVersion = row->get<int>(0);
  }
  qry.finish();
  // migrate if needed
  while (currentSchemaVersion!=aNeededSchemaVersion) {
    // get SQL statements for migration
    int nextSchemaVersion = aNeededSchemaVersion;
    std::string migrationSQL = dbSchemaUpgradeSQL(currentSchemaVersion, nextSchemaVersion);
    if (migrationSQL.size()==0) {
      LOG(LOG_ERR, "SQLite3Persistence: Cannot update from version %d to %d\n", currentSchemaVersion, nextSchemaVersion);
      return -1; // error, but not SQL
    }
    // execute the migration SQL
    sqlite3pp::command migrationCmd(*this, migrationSQL.c_str());
    err = migrationCmd.execute_all();
    if (err!=SQLITE_OK) {
      LOG(LOG_ERR,
        "SQLite3Persistence: Error executing migration SQL from version %d to %d = %s : %s\n",
        currentSchemaVersion, nextSchemaVersion, migrationSQL.c_str(), error_msg()
      );
      return -1; // error, but not SQL
    }
    // successful, we have reached a new version
    currentSchemaVersion = nextSchemaVersion;
    // set it in globs
    err = executef("UPDATE globs SET schemaVersion = %d", currentSchemaVersion);
    if (err!=SQLITE_OK) {
      LOG(LOG_ERR, "SQLite3Persistence: Cannot set globs.schemaVersion = %d\n", currentSchemaVersion, error_msg());
      return -1; // error, but not SQL
    }
  }
  return SQLITE_OK;
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
      ");";
  }
  return s;
}
