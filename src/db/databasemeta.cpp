/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "databasemeta.h"

#include "sql/sqlquery.h"
#include "sql/sqlutil.h"

using atools::sql::SqlUtil;
using atools::sql::SqlQuery;

DatabaseMeta::DatabaseMeta(atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb)
{
  if(SqlUtil(db).hasTable("metadata"))
  {
    SqlQuery query(db);

    query.exec("select db_version_major, db_version_minor, last_load_timestamp from metadata");

    if(query.next())
    {
      majorVersion = query.value("db_version_major").toInt();
      minorVersion = query.value("db_version_minor").toInt();
      lastLoadTime = query.value("last_load_timestamp").toDateTime();
      valid = true;
    }
  }
}

void DatabaseMeta::updateVersion(int majorVer, int minorVer)
{
  majorVersion = majorVer;
  minorVersion = minorVer;

  SqlQuery query(db);
  query.exec("delete from metadata");

  query.prepare("insert into metadata (db_version_major, db_version_minor) "
                "values(:major, :minor)");
  query.bindValue(":major", majorVersion);
  query.bindValue(":minor", minorVersion);
  query.exec();
  db->commit();
}

void DatabaseMeta::updateTimestamp()
{
  lastLoadTime = QDateTime::currentDateTime();

  SqlQuery query(db);
  query.prepare("update metadata set last_load_timestamp = :loadts");
  query.bindValue(":loadts", lastLoadTime);
  query.exec();
  db->commit();
}

bool DatabaseMeta::hasSchema()
{
  return SqlUtil(db).hasTable("airport");
}

bool DatabaseMeta::hasData()
{
  return hasSchema() && SqlUtil(db).rowCount("airport") > 0;
}

bool DatabaseMeta::isDatabaseCompatible(int version)
{
  if(isValid())
    return version == getMajorVersion();

  return false;
}
