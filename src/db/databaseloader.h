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

#ifndef DATABASELOADER_H
#define DATABASELOADER_H

#include <QObject>

namespace atools {
namespace fs {
class BglReaderProgressInfo;
}
namespace sql {
class SqlDatabase;
}
}

class QProgressDialog;
class QElapsedTimer;
class DatabaseDialog;

class DatabaseLoader :
  public QObject
{
  Q_OBJECT

public:
  DatabaseLoader(QWidget *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~DatabaseLoader();

  void run();

  void saveState();
  void restoreState();

  bool progressCallback(const atools::fs::BglReaderProgressInfo& progress, QElapsedTimer& timer);

  const int DB_VERSION_MAJOR = 0;
  const int DB_VERSION_MINOR = 7;

signals:
  void preDatabaseLoad();
  void postDatabaseLoad();

private:
  atools::sql::SqlDatabase *db;
  QWidget *parentWidget;
  QProgressDialog *progressDialog = nullptr;
  bool loadScenery(QWidget *parent);

  QString basePath, sceneryCfg;
  void runInternal(DatabaseDialog& dlg);

};

#endif // DATABASELOADER_H
