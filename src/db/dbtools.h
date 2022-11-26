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

#ifndef LNM_DBTOOLS_H
#define LNM_DBTOOLS_H

#include <QString>

namespace atools {
namespace sql {
class SqlDatabase;
}
}

namespace dbtools {

/* Database name for all loaded from simulators */
const QString DATABASE_NAME_SIM = "LNMDBSIM";

/* Navaid database e.g. from Navigraph */
const QString DATABASE_NAME_NAV = "LNMDBNAV";

/* Userpoint database */
const QString DATABASE_NAME_USER = "LNMDBUSER";

/* NAT, PACOTS, AUSOTS */
const QString DATABASE_NAME_TRACK = "LNMDBTRACK";

/* Logbook database */
const QString DATABASE_NAME_LOGBOOK = "LNMDBLOG";

/* User, sim and navdata airspace database */
const QString DATABASE_NAME_USER_AIRSPACE = "LNMDBUSERAS";
const QString DATABASE_NAME_SIM_AIRSPACE = "LNMDBSIMAS";
const QString DATABASE_NAME_NAV_AIRSPACE = "LNMDBNAVAS";

/* Network online player data */
const QString DATABASE_NAME_ONLINE = "LNMDBONLINE";

/* Temporary database used for database checking, copying and preparation */
const QString DATABASE_NAME_TEMP = "LNMTEMPDB";

/* Temporary database used for compiling the scenery library */
const QString DATABASE_NAME_LOADER = "LNMLOADER";

/* Used to temporary load metadata */
const QString DATABASE_NAME_DLG_INFO_TEMP = "LNMTEMPDB2";

/* Common type for all databases */
const QString DATABASE_TYPE = "QSQLITE";

/* Catches exceptions and terminates program if any */
void openDatabaseFile(atools::sql::SqlDatabase *db, const QString& file, bool readonly, bool createSchema);

/* Ignores exceptions */
void openDatabaseFileExt(atools::sql::SqlDatabase *db, const QString& file, bool readonly, bool createSchema, bool exclusive,
                         bool autoTransactions);

/* Catches exceptions and terminates program if any */
void closeDatabaseFile(atools::sql::SqlDatabase *db);

/* Checks if the current database has a schema. Exits program if this fails */
bool hasSchema(atools::sql::SqlDatabase *db);

/* Create an empty database schema. Boundary option does not use transaction. */
void createEmptySchema(atools::sql::SqlDatabase *db, bool boundary = false);

} // namespace db

#endif // LNM_DBTOOLS_H
