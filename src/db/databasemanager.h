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

#ifndef LITTLENAVMAP_DATABASEMANAGER_H
#define LITTLENAVMAP_DATABASEMANAGER_H

#include "fs/fspaths.h"
#include "db/dbtypes.h"

#include <QAction>
#include <QObject>

namespace atools {
namespace gui {
class Dialog;
}
namespace fs {
namespace db {
class DatabaseMeta;
}
namespace scenery {
class AircraftIndex;
class LanguageJson;
}

}
namespace sql {
class SqlDatabase;
}
namespace fs {
namespace userdata {
class UserdataManager;
class LogdataManager;
}

namespace online {
class OnlinedataManager;
}

}
}

class DatabaseDialog;
class MainWindow;
class TrackManager;
class DatabaseLoader;

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
  explicit DatabaseManager(MainWindow *parent);

  /* Also closes database if not already done */
  virtual ~DatabaseManager() override;

  DatabaseManager(const DatabaseManager& other) = delete;
  DatabaseManager& operator=(const DatabaseManager& other) = delete;

  /* Opens the dialog that allows to (re)load a new scenery database in the background. */
  void loadScenery();

  /* Stop background loading and hide progress dialog */
  void loadSceneryStop();

  /* Raise and activate progress window */
  void showProgressWindow();

  /* true if loading is in process or confirmation dialog is shown */
  bool isLoadingProgress();

  /* Save and restore all paths and current simulator settings */
  void saveState();

  /* Returns true if there are any flight simulator installations found in the registry */
  bool hasInstalledSimulators() const
  {
    return !simulators.getAllInstalled().isEmpty();
  }

  /* Returns true if there is an installation for the given sim found */
  bool hasInstalledSimulator(atools::fs::FsPaths::SimulatorType type) const
  {
    return simulators.value(type).isInstalled;
  }

  /* Returns true if there are any flight simulator databases found (probably copied by the user) */
  bool hasSimulatorDatabases() const
  {
    return !simulators.getAllHavingDatabase().isEmpty();
  }

  /* Opens Sim, Nav and respective airspace Sqlite databases in readonly mode. If the database is new or does not contain a schema
   * an empty schema is created.
   * Will not return if an exception is caught during opening.
   * Only for scenery database */
  void openAllDatabases();

  /* Load MSFS translations for current language */
  void loadLanguageIndex();

  /* Load MSFS aircraft.cfg files from paths */
  void loadAircraftIndex();

  /* Open a writeable database for userpoints or online network data. Automatic transactions are off.  */
  void openWriteableDatabase(atools::sql::SqlDatabase *database, const QString& name, const QString& displayName, bool backup);
  void closeLogDatabase();
  void closeUserDatabase();
  void closeTrackDatabase();
  void closeUserAirspaceDatabase();
  void closeOnlineDatabase();

  /* Close all simulator databases - not the user database.
   * Will not return if an exception is caught during opening. */
  void closeAllDatabases();

  /* Get the simulator database. Will return null if not opened before. Connection inside is changed depending on settings. */
  atools::sql::SqlDatabase *getDatabaseSim()
  {
    return databaseSim;
  }

  /* Get navaid database or same as above if it does not exist.Connection inside is changed depending on settings. */
  atools::sql::SqlDatabase *getDatabaseNav()
  {
    return databaseNav;
  }

  /* Get the simulator database for airspaces which is independent of nav data mode. Will return null if not opened before. */
  atools::sql::SqlDatabase *getDatabaseSimAirspace()
  {
    return databaseSimAirspace;
  }

  /* Get the nav database for airspaces which is independent of nav data mode. Will return null if not opened before. */
  atools::sql::SqlDatabase *getDatabaseNavAirspace()
  {
    return databaseNavAirspace;
  }

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

  /* Only true if airports come from X-Plane database as default */
  bool isAirportDatabaseXPlane(bool navdata) const;

  /* Base paths which might also be changed by the user */
  QString getCurrentSimulatorBasePath() const;
  QString getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type) const;

  /* Get files path for installed simulators in order of the given list.
   * Also considers probably changed paths by user */
  QString getSimulatorFilesPathBest(const atools::fs::FsPaths::SimulatorTypeVector& types, const QString& defaultPath) const;

  /* Same as above but for simulator base path */
  QString getSimulatorBasePathBest(const atools::fs::FsPaths::SimulatorTypeVector& types) const;

  navdb::Status getNavDatabaseStatus() const
  {
    return navDatabaseStatus;
  }

  atools::fs::userdata::UserdataManager *getUserdataManager() const
  {
    return userdataManager;
  }

  TrackManager *getTrackManager() const
  {
    return trackManager;
  }

  atools::fs::userdata::LogdataManager *getLogdataManager() const
  {
    return logdataManager;
  }

  atools::fs::online::OnlinedataManager *getOnlinedataManager() const
  {
    return onlinedataManager;
  }

  atools::sql::SqlDatabase *getDatabaseUser() const
  {
    return databaseUser;
  }

  atools::sql::SqlDatabase *getDatabaseTrack() const
  {
    return databaseTrack;
  }

  atools::sql::SqlDatabase *getDatabaseLogbook() const
  {
    return databaseLogbook;
  }

  atools::sql::SqlDatabase *getDatabaseUserAirspace() const
  {
    return databaseUserAirspace;
  }

  atools::sql::SqlDatabase *getDatabaseOnline() const;

  /* MSFS translations from table "translation" */
  const atools::fs::scenery::LanguageJson& getLanguageIndex() const
  {
    return *languageIndex;
  }

  /* MSFS translations from table "translation" */
  atools::fs::scenery::AircraftIndex& getAircraftIndex() const
  {
    return *aircraftIndex;
  }

  /* Checks if size and last modification time have changed on the readonly nav and sim databases.
   * Shows an error dialog if this is the case */
  void checkForChangedNavAndSimDatabases();

  /* Checks if version of database is smaller than application database version and shows a warning dialog if it is */
  void checkDatabaseVersion();

  /* Validate scenery library mode and show warning dialogs which allow to set the recommended mode */
  void checkSceneryOptionsManual()
  {
    checkSceneryOptions(true /* manualCheck */);
  }

  /* Checks the simulator database independent of scenery library settings. Expensive since it opens the file */
  bool hasDataInSimDatabase();

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
  void restoreState();

  bool isDatabaseCompatible(atools::sql::SqlDatabase *db) const;
  bool hasData(atools::sql::SqlDatabase *db) const;

  /* Correct navdata selection according to simulator, AIRAC cycle and other. Updates simulators and navDatabaseStatus */
  void assignSceneryCorrection();

  void simulatorChangedFromComboBox(atools::fs::FsPaths::SimulatorType value);
  void loadSceneryInternal();

  /* Called by signal DatabaseLoader::loadingFinished() */
  void loadSceneryInternalPost();

  /* Called by loadSceneryInternalPost() */
  void loadSceneryPost();

  void updateDialogInfo(atools::fs::FsPaths::SimulatorType value);

  /* Database stored in settings directory */
  QString buildDatabaseFileName(atools::fs::FsPaths::SimulatorType currentFsType) const;

  /* Database stored in application directory */
  QString buildDatabaseFileNameAppDir(atools::fs::FsPaths::SimulatorType type) const;

  /* Temporary name stored in settings directory */
  QString buildCompilingDatabaseFileName() const;

  /* Simulator changed from main menu */
  void switchSimFromMainMenu();
  void switchSimInternal(atools::fs::FsPaths::SimulatorType type);

  /* Navdatabase mode change from main menu */
  void switchNavFromMainMenu();

  /* Navdatabase auto mode changed from main menu */
  void switchNavAutoFromMainMenu();

  /* Set menus according to correction */
  void correctSceneryOptions(navdb::Correction& correction);

  void freeActions();
  void insertSimSwitchAction(atools::fs::FsPaths::SimulatorType type, QAction *before, QMenu *menu, int index);
  void updateSimulatorFlags();
  void updateSimulatorPathsFromDialog();

  /* Checks for missing files and assigns a new simulator if needed */
  void correctSimulatorType();
  navdb::Correction getSceneryCorrection(const navdb::Status& navDbStatus, atools::fs::FsPaths::SimulatorType simType) const;

  /* Get cycle metadata from a database file */
  const atools::fs::db::DatabaseMeta databaseMetadataFromFile(const QString& file) const;

  void clearLanguageIndex();

  void clearAircraftIndex();

  bool checkValidBasePaths() const;

  /* Disable or enable nav menu items depending on auto status */
  void updateNavMenuStatus();

  /* Validate scenery library mode and show warning dialogs which allow to set the recommended mode.
   * manualCheck shows an "Ok" button if all is valid and ignores "do not show again" state. */
  void checkSceneryOptions(bool manualCheck);

  /* Get metadata independent of scenery library settings from the database files */
  const atools::fs::db::DatabaseMeta databaseMetadataFromType(atools::fs::FsPaths::SimulatorType type) const;

  atools::gui::Dialog *dialog;
  DatabaseDialog *databaseDialog = nullptr;
  QString databaseDirectory;

  // Need a pointer since it has to be deleted before the destructor is left
  atools::sql::SqlDatabase
  *databaseSim = nullptr /* Database for simulator content. Connection inside is exchanged depending on settings. */,
  *databaseNav = nullptr /* Database for third party navigation data. Connection inside is exchanged depending on settings. */,
  *databaseUser = nullptr /* Database for user data */,
  *databaseTrack = nullptr /* Database for tracks like NAT, PACOTS and AUSOTS */,
  *databaseLogbook = nullptr /* Database for logbook */,
  *databaseUserAirspace = nullptr /* Database for user airspaces */,
  *databaseSimAirspace = nullptr /* Airspace database from simulator independent from nav switch */,
  *databaseNavAirspace = nullptr /* Airspace database from navdata independent from nav switch */,
  *databaseOnline = nullptr /* Database for network online data */;

  bool showingDatabaseChangeWarning = false;

  MainWindow *mainWindow = nullptr;

  /* Switch simulator actions */
  QActionGroup *simDbGroup = nullptr, *navDbGroup = nullptr;
  QList<QAction *> simDbActions;

  QMenu *navDbSubMenu = nullptr;
  QAction *navDbActionOff = nullptr, *navDbActionMixed = nullptr, *navDbActionAll = nullptr, *navDbActionAuto = nullptr,
          *menuDbSeparator = nullptr, *menuNavDbSeparator = nullptr;

  atools::fs::FsPaths::SimulatorType
  /* Currently selected simulator which will be used in the map, search, etc. */
    currentFsType = atools::fs::FsPaths::NONE,
  /* Currently selected simulator in the load scenery database dialog */
    selectedFsType = atools::fs::FsPaths::NONE;

  /* Using Navigraph update or not */
  navdb::Status navDatabaseStatus = navdb::OFF;
  bool navDatabaseAuto = true;

  DatabaseLoader *databaseLoader = nullptr;

  /* List of simulator installations and databases */
  SimulatorTypeMap simulators;
  bool readInactive = false, readAddOnXml = true;

  QString databaseMetaText, databaseAiracCycleText;

  /* Also keep the database-close manager classes here */
  TrackManager *trackManager = nullptr;
  atools::fs::userdata::UserdataManager *userdataManager = nullptr;
  atools::fs::userdata::LogdataManager *logdataManager = nullptr;
  atools::fs::online::OnlinedataManager *onlinedataManager = nullptr;

  /* MSFS translations from table "translation" */
  atools::fs::scenery::LanguageJson *languageIndex = nullptr;
  atools::fs::scenery::AircraftIndex *aircraftIndex = nullptr;

  /* Show hint dialog only once per session */
  bool backgroundHintShown = false;
};

#endif // LITTLENAVMAP_DATABASEMANAGER_H
