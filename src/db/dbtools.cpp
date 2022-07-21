/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "db/dbtools.h"

#include "sql/sqldatabase.h"
#include "exception.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "gui/application.h"
#include "fs/db/databasemeta.h"
#include "fs/navdatabaseoptions.h"
#include "fs/navdatabase.h"

#include <QDebug>

namespace dbtools {

void openDatabaseFileExt(atools::sql::SqlDatabase *db, const QString& file, bool readonly,
                         bool createSchema, bool exclusive, bool autoTransactions)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  int databaseCacheKb = settings.getAndStoreValue(lnm::SETTINGS_DATABASE + "CacheKb", 50000).toInt();
  bool foreignKeys = settings.getAndStoreValue(lnm::SETTINGS_DATABASE + "ForeignKeys", false).toBool();

  // cache_size * 1024 bytes if value is negative
  QStringList databasePragmas({QString("PRAGMA cache_size=-%1").arg(databaseCacheKb), "PRAGMA page_size=8196"});

  if(exclusive)
  {
    // Best settings for loading databases accessed write only - unsafe
    databasePragmas.append("PRAGMA locking_mode=EXCLUSIVE");
    databasePragmas.append("PRAGMA journal_mode=TRUNCATE");
    databasePragmas.append("PRAGMA synchronous=OFF");
  }
  else
  {
    // Best settings for online and user databases which are updated often - read/write
    databasePragmas.append("PRAGMA locking_mode=NORMAL");
    databasePragmas.append("PRAGMA journal_mode=DELETE");
    databasePragmas.append("PRAGMA synchronous=NORMAL");
  }

  if(!readonly)
    databasePragmas.append("PRAGMA busy_timeout=2000");

  qDebug() << Q_FUNC_INFO << "Opening database" << file;
  db->setDatabaseName(file);

  // Set foreign keys only on demand because they can decrease loading performance
  if(foreignKeys)
    databasePragmas.append("PRAGMA foreign_keys = ON");
  else
    databasePragmas.append("PRAGMA foreign_keys = OFF");

  bool autocommit = db->isAutocommit();
  db->setAutocommit(false);
  db->setAutomaticTransactions(autoTransactions);
  db->open(databasePragmas);

  db->setAutocommit(autocommit);

  if(createSchema)
  {
    if(!hasSchema(db))
    {
      if(db->isReadonly())
      {
        // Reopen database read/write
        db->close();
        db->setReadonly(false);
        db->open(databasePragmas);
      }

      createEmptySchema(db);
    }
  }

  if(readonly && !db->isReadonly())
  {
    // Readonly requested - reopen database
    db->close();
    db->setReadonly();
    db->open(databasePragmas);
  }

  atools::fs::db::DatabaseMeta(db).logInfo();
}

void openDatabaseFile(atools::sql::SqlDatabase *db, const QString& file, bool readonly, bool createSchema)
{
  try
  {
    openDatabaseFileExt(db, file, readonly, createSchema, true /* exclusive */, true /* auto transactions */);
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
}

void closeDatabaseFile(atools::sql::SqlDatabase *db)
{
  try
  {
    if(db != nullptr && db->isOpen())
    {
      qDebug() << Q_FUNC_INFO << "Closing database" << db->databaseName();
      db->close();
    }
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
}

bool hasSchema(atools::sql::SqlDatabase *db)
{
  try
  {
    return atools::fs::db::DatabaseMeta(db).hasSchema();
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
}

void createEmptySchema(atools::sql::SqlDatabase *db, bool boundary)
{
  try
  {
    atools::fs::NavDatabaseOptions opts;
    if(boundary)
      // Does not use a transaction
      atools::fs::NavDatabase(&opts, db, nullptr, GIT_REVISION_LITTLENAVMAP).createAirspaceSchema();
    else
    {
      atools::fs::NavDatabase(&opts, db, nullptr, GIT_REVISION_LITTLENAVMAP).createSchema();
      atools::fs::db::DatabaseMeta(db).updateVersion();
    }
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
}

} // namespace db
