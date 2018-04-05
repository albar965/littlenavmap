/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_DATABASEMANAGER_H
#define LITTLENAVMAP_DATABASEMANAGER_H

#include "sql/sqldatabase.h"
#include "fs/fspaths.h"
#include "db/dbtypes.h"

#include <QAction>
#include <QObject>

namespace atools {
namespace fs {
class NavDatabaseProgress;

namespace userdata {
class UserdataManager;
}

namespace online {
class OnlinedataManager;
}

}
}

class QProgressDialog;
class QElapsedTimer;
class DatabaseDialog;
class MainWindow;
class QSplashScreen;
class QMessageBox;

namespace dm {
enum NavdatabaseStatus
{
  NAVDATABASE_ALL, /* Only third party nav database */
  NAVDATABASE_MIXED, /* Airports from simulator rest from nav database */
  NAVDATABASE_OFF /* Only simulator database */
};

}

/*
 * Takes care of all scenery database management. Switching between flight simulators, loading of scenery
 * databases, validation of databases and comparing versions.
 */
class DatabaseManager :
  public QObject
{
  Q_OBJECT

public:
  /*
   * @param parent can be null if only checkIncompatibleDatabases is to be called
   */
  DatabaseManager(MainWindow *parent);

  /* Also closes database if not already done */
  virtual ~DatabaseManager();

  /* Opens the dialog that allows to (re)load a new scenery database. */
  void run();

  /* Copy the boundary table from the currently selected FSX/P3D database to the X-Plane database */
  void copyAirspaces();

  /* Save and restore all paths and current simulator settings */
  void saveState();

  /* Returns true if there are any flight simulator installations found in the registry */
  bool hasInstalledSimulators() const;

  /* Returns true if there are any flight simulator databases found (probably copied by the user) */
  bool hasSimulatorDatabases() const;

  /* Opens Sim and Nav Sqlite databases in readonly mode. If the database is new or does not contain a schema
   * an empty schema is created.
   * Will not return if an exception is caught during opening.
   * Only for scenery database */
  void openAllDatabases();

  /* Open a writeable database for userpoints or online network data. Automatic transactions are off.  */
  void openWriteableDatabase(atools::sql::SqlDatabase *database, const QString& name, const QString& displayName, bool backup);
  void closeUserDatabase();
  void closeOnlineDatabase();

  /* Close all simulator databases - not the user database.
   * Will not return if an exception is caught during opening. */
  void closeDatabases();

  /* Get the simulator database. Will return null if not opened before. */
  atools::sql::SqlDatabase *getDatabaseSim();

  /* Get navaid database or same as above if it does not exist */
  atools::sql::SqlDatabase *getDatabaseNav();

  /*
   * Insert actions for switching between installed flight simulators.
   * Actions have to be freed by the caller and are connected to switchSim
   * @param before Actions will be added to menu before this one
   * @param menu Add actions to this menu
   */
  void insertSimSwitchActions();

  /* if false quit application */
  bool checkIncompatibleDatabases(bool *databasesErased);

  /* Copy from app dir to settings directory if newer and create indexes if missing */
  void checkCopyAndPrepareDatabases();

  /* Get the settings directory where the database is stored */
  const QString& getDatabaseDirectory() const
  {
    return databaseDirectory;
  }

  /* Get currently selected simulator type (using insertSimSwitchActions). */
  atools::fs::FsPaths::SimulatorType getCurrentSimulator() const
  {
    return currentFsType;
  }

  QString getCurrentSimulatorBasePath() const;
  QString getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type) const;

  dm::NavdatabaseStatus getNavDatabaseStatus() const
  {
    return navDatabaseStatus;
  }

  atools::fs::userdata::UserdataManager *getUserdataManager() const
  {
    return userdataManager;
  }

  atools::fs::online::OnlinedataManager *getOnlinedataManager() const
  {
    return onlinedataManager;
  }

  atools::sql::SqlDatabase *getDatabaseUser() const
  {
    return databaseUser;
  }

  atools::sql::SqlDatabase *getDatabaseOnline() const;

signals:
  /* Emitted before opening the scenery database dialog, loading a database or switching to a new simulator database.
   * Recipients have to close all database connections and clear all caches. The database instance itself is not changed
   * just the connection behind it.*/
  void preDatabaseLoad();

  /* Emitted when a database was loaded, the loading database dialog was closed or a new simulator was selected
   * @param type new simulator
   */
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

private:
  /* Catches exceptions and terminates program if any */
  void openDatabaseFile(atools::sql::SqlDatabase *db, const QString& file, bool readonly, bool createSchema);

  /* Does not catch exceptions */
  void openDatabaseFileInternal(atools::sql::SqlDatabase *db, const QString& file, bool readonly, bool createSchema,
                                bool exclusive, bool autoTransactions);

  void closeDatabaseFile(atools::sql::SqlDatabase *db);

  void restoreState();

  void createEmptySchema(atools::sql::SqlDatabase *db);
  bool isDatabaseCompatible(atools::sql::SqlDatabase *db);
  bool hasSchema(atools::sql::SqlDatabase *db);
  bool hasData(atools::sql::SqlDatabase *db);

  bool progressCallback(const atools::fs::NavDatabaseProgress& progress, QElapsedTimer& timer);

  void simulatorChangedFromComboBox(atools::fs::FsPaths::SimulatorType value);
  bool runInternal();
  void updateDialogInfo(atools::fs::FsPaths::SimulatorType value);

  /* Database stored in settings directory */
  QString buildDatabaseFileName(atools::fs::FsPaths::SimulatorType currentFsType);

  /* Database stored in application directory or settings directory */
  QString buildDatabaseFileNameAppDirOrSettings(atools::fs::FsPaths::SimulatorType type);

  /* Database stored in application directory */
  QString buildDatabaseFileNameAppDir(atools::fs::FsPaths::SimulatorType type);

  /* Temporary name stored in settings directory */
  QString buildCompilingDatabaseFileName();

  void switchSimFromMainMenu();
  void switchNavFromMainMenu();

  void freeActions();
  void insertSimSwitchAction(atools::fs::FsPaths::SimulatorType type, QAction *before, QMenu *menu, int index);
  void updateSimulatorFlags();
  void updateSimulatorPathsFromDialog();
  bool loadScenery(atools::sql::SqlDatabase *db);
  void correctSimulatorType();
  QMessageBox *showSimpleProgressDialog(const QString& message);
  void deleteSimpleProgressDialog(QMessageBox *messageBox);

  /* Get cycle metadata from a database file */
  void metaFromFile(QString *cycle, QDateTime *compilationTime, bool *settingsNeedsPreparation, QString *source,
                    const QString& file);

  /* Database name for all loaded from simulators */
  const QString DATABASE_NAME = "LNMDB";

  /* Navaid database e.g. from Navigraph */
  const QString DATABASE_NAME_NAV = "LNMDBNAV";

  /* Userpoint database */
  const QString DATABASE_NAME_USER = "LNMDBUSER";

  /* Network online player data */
  const QString DATABASE_NAME_ONLINE = "LNMDBONLINE";

  const QString DATABASE_NAME_TEMP = "LNMTEMPDB";
  const QString DATABASE_NAME_DLG_INFO_TEMP = "LNMTEMPDB2";
  const QString DATABASE_TYPE = "QSQLITE";

  DatabaseDialog *databaseDialog = nullptr;
  QString databaseDirectory;
  qint64 progressTimerElapsed = 0L;

  // Need a pointer since it has to be deleted before the destructor is left
  atools::sql::SqlDatabase *databaseSim = nullptr /* Database for simulator content */,
                           *databaseNav = nullptr /* Database for third party navigation data */,
                           *databaseUser = nullptr /* Database for user data */,
                           *databaseOnline = nullptr /* Database for network online data */;

  MainWindow *mainWindow = nullptr;
  QProgressDialog *progressDialog = nullptr;

  /* Switch simulator actions */
  QActionGroup *simDbGroup = nullptr, *navDbGroup = nullptr;
  QList<QAction *> actions;
  QAction *navDbActionOff = nullptr, *navDbActionBlend = nullptr, *navDbActionAll = nullptr,
          *menuDbSeparator = nullptr, *menuNavDbSeparator = nullptr;
  QMenu *navDbSubMenu = nullptr;

  atools::fs::FsPaths::SimulatorType
  /* Currently selected simulator which will be used in the map, search, etc. */
    currentFsType = atools::fs::FsPaths::UNKNOWN,
  /* Currently selected simulator in the load scenery database dialog */
    selectedFsType = atools::fs::FsPaths::UNKNOWN;

  /* Using Navigraph update or not */
  dm::NavdatabaseStatus navDatabaseStatus = dm::NAVDATABASE_OFF;

  /* List of simulator installations and databases */
  SimulatorTypeMap simulators;
  bool readInactive = false, readAddOnXml = true;

  QString currentBglFilePath;

  QString databaseMetaText, databaseAiracCycleText, databaseInfoText, databaseLoadingText,
          databaseTimeText;

  /* Also keep the database-close manager classes here */
  atools::fs::userdata::UserdataManager *userdataManager = nullptr;
  atools::fs::online::OnlinedataManager *onlinedataManager = nullptr;

};

#endif // LITTLENAVMAP_DATABASEMANAGER_H
