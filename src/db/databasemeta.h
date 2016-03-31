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

#ifndef DATABASEMETA_H
#define DATABASEMETA_H

#include <QDateTime>

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class DatabaseMeta
{
public:
  DatabaseMeta(atools::sql::SqlDatabase *sqlDb);

  int getMajorVersion() const
  {
    return majorVersion;
  }

  int getMinorVersion() const
  {
    return minorVersion;
  }

  QDateTime getLastLoadTime() const
  {
    return lastLoadTime;
  }

  void update(int majorVer, int minorVer);

  bool isValid() const
  {
    return valid;
  }

private:
  atools::sql::SqlDatabase *db;
  int majorVersion = 0, minorVersion = 0;
  QDateTime lastLoadTime;
  bool valid = false;
};

#endif // DATABASEMETA_H
