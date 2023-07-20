/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_DATABASELOADER_H
#define LNM_DATABASELOADER_H

#include "fs/navdatabaseflags.h"
#include "fs/fspaths.h"
#include "db/dbtypes.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QReadWriteLock>

class DatabaseProgressDialog;

namespace atools {
namespace sql {
class SqlDatabase;
}
namespace fs {
class NavDatabaseProgress;
class NavDatabaseErrors;
class NavDatabaseOptions;
class NavDatabase;
}
}

class QElapsedTimer;

/*
 * Loads scenery library databases in the background using a thread.
 * Also shows error messages and progress dialog.
 */
class DatabaseLoader :
  public QObject
{
  Q_OBJECT

public:
  explicit DatabaseLoader(QObject *parent);
  virtual ~DatabaseLoader() override;

  /* Start the scenery loading process in background and return immediately.
   * Also opens the progress dialog. */
  void loadScenery();

  /* Stops loading, waits for thread to be finished and closes progress dialog */
  void loadSceneryStop();

  /* true if loading is in process or confirmation dialog is shown */
  bool isLoadingProgress() const;

  /* Put progress window to foreground if open */
  void showProgressWindow() const;

  /* Return text to display loaded features */
  const QString& getDatabaseInfoText() const
  {
    return databaseInfoText;
  }

  /* Read inactive scenery elements */
  void setReadInactive(bool value)
  {
    readInactive = value;
  }

  /* Read P3D add-on.xml files */
  void setReadAddOnXml(bool value)
  {
    readAddOnXml = value;
  }

  /* List of simulator installations and databases */
  void setSimulators(const SimulatorTypeMap& value)
  {
    simulators = value;
  }

  /* Currently selected simulator in the load scenery database dialog */
  void setSelectedFsType(const atools::fs::FsPaths::SimulatorType& value)
  {
    selectedFsType = value;
  }

  /* Set name for temporary database */
  void setDatabaseFilename(const QString& value)
  {
    dbFilename = value;
  }

  const QString& getDatabaseFilename() const
  {
    return dbFilename;
  }

  /* Get compilation results. Do not access while thread is active. */
  const atools::fs::ResultFlags& getResultFlags() const
  {
    return resultFlagsShared;
  }

  /* Set a compilation result flag. Do not access while thread is active. */
  void setResultFlag(atools::fs::ResultFlag value, bool on = true)
  {
    resultFlagsShared.setFlag(value, on);
  }

  /* Clear all flags. Do not access while thread is active. */
  void clearResultFlags()
  {
    resultFlagsShared = atools::fs::COMPILE_NONE;
  }

signals:
  /* Sent after loading when thread terminated */
  void loadingFinished();

  /* Used internally to decouple thread progress from main thread.
   * Connected to progressCallback() using a queued connection. */
  void progressCallbackSignal();

private:
  /* Called from progressCallbackSignal() */
  void progressCallback();

  /* Show compilation errors if any */
  void showErrors();

  /* Called once thread is terminated and compilation is done */
  void compileDatabasePost();

  void deleteProgressDialog();

  /* Progress dialog is opened on demand */
  DatabaseProgressDialog *progressDialog = nullptr;

  /* Dialog display texts */
  QString databaseLoadingText, databaseTimeText, databaseInfoText;

  atools::fs::NavDatabaseOptions *navDatabaseOpts;
  atools::fs::NavDatabaseErrors *navDatabaseErrors;
  atools::fs::NavDatabaseProgress *navDatabaseProgressShared; // Shared between thread and main - locked by progressLock

  /* Database opened on demand in main and used in thread context */
  atools::sql::SqlDatabase *compileDb = nullptr;

  bool readInactive, readAddOnXml;
  SimulatorTypeMap simulators;
  atools::fs::FsPaths::SimulatorType selectedFsType;
  QString currentBglFilePath, dbFilename;
  atools::fs::ResultFlags resultFlagsShared = atools::fs::COMPILE_NONE; // Shared between thread and main - locked by resultFlagsLock
  QElapsedTimer timer; /* Timer used in main thread context */

  // Thread related members used and called in thread context ==========
  bool progressCallbackThread(const atools::fs::NavDatabaseProgress& progress);

  /* Used to fetch result from thread */
  QFuture<atools::fs::ResultFlags> future;

  /* Calles compileDatabasePost() once thread is finished */
  QFutureWatcher<atools::fs::ResultFlags> watcher;

  QReadWriteLock progressLock, resultFlagsLock;

  /* timer called in thread context to avoid too often updates */
  qint64 progressTimerElapsedThread = 0L;
  QElapsedTimer timerThread;
};

#endif // LNM_DATABASELOADER_H
