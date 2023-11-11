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

#include "db/databasemanager.h"

#include "atools.h"
#include "common/constants.h"
#include "common/settingsmigrate.h"
#include "db/databasedialog.h"
#include "db/databaseloader.h"
#include "db/dbtools.h"
#include "fs/db/databasemeta.h"
#include "fs/navdatabase.h"
#include "fs/online/onlinedatamanager.h"
#include "fs/scenery/aircraftindex.h"
#include "fs/scenery/languagejson.h"
#include "fs/userdata/logdatamanager.h"
#include "fs/userdata/userdatamanager.h"
#include "fs/xp/scenerypacks.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "gui/mainwindow.h"
#include "io/fileroller.h"
#include "app/navapp.h"
#include "options/optiondata.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlexception.h"
#include "sql/sqltransaction.h"
#include "sql/sqlutil.h"
#include "track/trackmanager.h"
#include "ui_mainwindow.h"
#include "util/version.h"
#include "gui/signalblocker.h"

#include <QDir>

using atools::sql::SqlUtil;
using atools::fs::NavDatabase;
using atools::settings::Settings;
using atools::sql::SqlDatabase;
using atools::sql::SqlTransaction;
using atools::fs::db::DatabaseMeta;
using atools::fs::FsPaths;

// Maximum age of database before showing a warning dialog
const static int MAX_AGE_DAYS = 60;

DatabaseManager::DatabaseManager(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  databaseMetaText = tr("<p><big>Last Update: %1. Database Version: %2. Program Version: %3.%4</big></p>");
  databaseAiracCycleText = tr(" AIRAC Cycle %1.");

  dialog = new atools::gui::Dialog(mainWindow);

  // Keeps MSFS translations from table "translation" in memory
  languageIndex = new atools::fs::scenery::LanguageJson;

  // Aircraft config read from MSFS folders to get more user aircraft details
  aircraftIndex = new atools::fs::scenery::AircraftIndex;

  // Also loads list of simulators from settings ======================================
  restoreState();

  databaseDirectory = Settings::getPath() + QDir::separator() + lnm::DATABASE_DIR;
  if(!QDir().mkpath(databaseDirectory))
    qWarning() << "Cannot create db dir" << databaseDirectory;

  QString name = buildDatabaseFileName(FsPaths::NAVIGRAPH);
  if(name.isEmpty() && !QFile::exists(name))
    // Set to off if not database found
    navDatabaseStatus = navdb::OFF;

  // Find simulators by default registry entries and pre-fill the navDatabaseStatus if the list is empty (i.e. new)
  simulators.fillDefault(simulators.isEmpty() ? navDatabaseStatus : navdb::UNKNOWN);

  // Take navdatabase status from current sim selection
  navDatabaseStatus = simulators[currentFsType].navDatabaseStatus;

  // Find any stale databases that do not belong to a simulator and update installed and has database flags
  updateSimulatorFlags();

  qDebug() << Q_FUNC_INFO << "Detected simulators =====================================================";
  for(auto it = simulators.constBegin(); it != simulators.constEnd(); ++it)
    qDebug() << Q_FUNC_INFO << it.key() << it.value();

  // Correct if current simulator is invalid
  correctSimulatorType();

  qDebug() << Q_FUNC_INFO << "fs type" << currentFsType;

  if(mainWindow != nullptr)
  {
    databaseDialog = new DatabaseDialog(mainWindow, simulators);
    databaseDialog->setReadInactive(readInactive);
    databaseDialog->setReadAddOnXml(readAddOnXml);

    connect(databaseDialog, &DatabaseDialog::simulatorChanged, this, &DatabaseManager::simulatorChangedFromComboBox);
  }

  SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_SIM);
  SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_NAV);
  SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_DLG_INFO_TEMP);
  SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_TEMP);

  databaseSim = new SqlDatabase(dbtools::DATABASE_NAME_SIM);
  databaseNav = new SqlDatabase(dbtools::DATABASE_NAME_NAV);

  if(mainWindow != nullptr)
  {
    // Open only for instantiation in main window and not in main function
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_USER);
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_TRACK);
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_LOGBOOK);
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_ONLINE);

    // Airspace databases
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_USER_AIRSPACE);
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_SIM_AIRSPACE);
    SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_NAV_AIRSPACE);

    // Variable databases (user can edit or program downloads data)
    databaseUser = new SqlDatabase(dbtools::DATABASE_NAME_USER);
    databaseTrack = new SqlDatabase(dbtools::DATABASE_NAME_TRACK);
    databaseLogbook = new SqlDatabase(dbtools::DATABASE_NAME_LOGBOOK);
    databaseOnline = new SqlDatabase(dbtools::DATABASE_NAME_ONLINE);

    // Airspace databases
    databaseUserAirspace = new SqlDatabase(dbtools::DATABASE_NAME_USER_AIRSPACE);

    // ... as duplicate connections to sim and nav databases but independent of nav switch
    databaseSimAirspace = new SqlDatabase(dbtools::DATABASE_NAME_SIM_AIRSPACE);
    databaseNavAirspace = new SqlDatabase(dbtools::DATABASE_NAME_NAV_AIRSPACE);

    // Open user point database =================================
    openWriteableDatabase(databaseUser, "userdata", "user", true /* backup */);
    userdataManager = new atools::fs::userdata::UserdataManager(databaseUser);
    if(!userdataManager->hasSchema())
      userdataManager->createSchema(false /* verboseLogging */);
    else
      userdataManager->updateSchema();

    // Open logbook database =================================
    openWriteableDatabase(databaseLogbook, "logbook", "logbook", true /* backup */);
    logdataManager = new atools::fs::userdata::LogdataManager(databaseLogbook);
    if(!logdataManager->hasSchema())
      logdataManager->createSchema(false /* verboseLogging */);
    else
      logdataManager->updateSchema();

    // Open user airspace database =================================
    openWriteableDatabase(databaseUserAirspace, "userairspace", "userairspace", false /* backup */);
    if(!SqlUtil(databaseUserAirspace).hasTable("boundary"))
    {
      SqlTransaction transaction(databaseUserAirspace);
      // Create schema on demand
      dbtools::createEmptySchema(databaseUserAirspace, true /* boundary only */);
      transaction.commit();
    }

    // Open track database =================================
    openWriteableDatabase(databaseTrack, "track", "track", false /* backup */);
    trackManager = new TrackManager(databaseTrack, databaseNav);
    trackManager->createSchema(false /* verboseLogging */);
    // trackManager->initQueries();

    // Open online network database ==============================
    atools::settings::Settings& settings = atools::settings::Settings::instance();
    bool verbose = settings.getAndStoreValue(lnm::OPTIONS_WHAZZUP_PARSER_DEBUG, false).toBool();

    openWriteableDatabase(databaseOnline, "onlinedata", "online network", false /* backup */);
    onlinedataManager = new atools::fs::online::OnlinedataManager(databaseOnline, verbose);
    onlinedataManager->createSchema();
    onlinedataManager->initQueries();

    if(migrate::getOptionsVersion().isValid() && migrate::getOptionsVersion() <= atools::util::Version("2.8.1.beta"))
    {
      qDebug() << Q_FUNC_INFO << "Cleaning undo/redo in logbook and userdata";
      logdataManager->clearUndoRedoData();
      userdataManager->clearUndoRedoData();
    }
  }

  // Run if instantiated from the GUI
  if(mainWindow != nullptr)
  {
    databaseLoader = new DatabaseLoader(mainWindow);
    connect(databaseLoader, &DatabaseLoader::loadingFinished, this, &DatabaseManager::loadSceneryInternalPost);

    // Correct navdata selection automatically if enabled
    assignSceneryCorrection();
  }
}

DatabaseManager::~DatabaseManager()
{
  // Delete simulator switch actions
  freeActions();

  ATOOLS_DELETE_LOG(databaseLoader);
  ATOOLS_DELETE_LOG(databaseDialog);
  ATOOLS_DELETE_LOG(dialog);
  ATOOLS_DELETE_LOG(userdataManager);
  ATOOLS_DELETE_LOG(trackManager);
  ATOOLS_DELETE_LOG(logdataManager);
  ATOOLS_DELETE_LOG(onlinedataManager);

  closeAllDatabases();
  closeUserDatabase();
  closeTrackDatabase();
  closeLogDatabase();
  closeUserAirspaceDatabase();
  closeOnlineDatabase();

  ATOOLS_DELETE_LOG(databaseSim);
  ATOOLS_DELETE_LOG(databaseNav);
  ATOOLS_DELETE_LOG(databaseUser);
  ATOOLS_DELETE_LOG(databaseTrack);
  ATOOLS_DELETE_LOG(databaseLogbook);
  ATOOLS_DELETE_LOG(databaseOnline);
  ATOOLS_DELETE_LOG(databaseUserAirspace);
  ATOOLS_DELETE_LOG(databaseSimAirspace);
  ATOOLS_DELETE_LOG(databaseNavAirspace);
  ATOOLS_DELETE_LOG(languageIndex);
  ATOOLS_DELETE_LOG(aircraftIndex);

  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_SIM);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_NAV);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_USER);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_TRACK);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_LOGBOOK);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_DLG_INFO_TEMP);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_TEMP);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_USER_AIRSPACE);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_SIM_AIRSPACE);
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_NAV_AIRSPACE);
}

bool DatabaseManager::checkIncompatibleDatabases(bool *databasesErased)
{
  bool ok = true;

  if(databasesErased != nullptr)
    *databasesErased = false;

  // Need empty block to delete sqlDb before removing driver
  {
    // Create a temporary database
    SqlDatabase sqlDb(dbtools::DATABASE_NAME_TEMP);
    QStringList databaseNames, databaseFiles;

    // Collect all incompatible databases

    for(auto it = simulators.constBegin(); it != simulators.constEnd(); ++it)
    {
      QString dbName = buildDatabaseFileName(it.key());
      if(QFile::exists(dbName))
      {
        // Database file exists
        sqlDb.setDatabaseName(dbName);
        sqlDb.open();

        DatabaseMeta meta(&sqlDb);
        if(!meta.hasSchema())
          // No schema create an empty one anyway
          dbtools::createEmptySchema(&sqlDb);
        else if(!meta.isDatabaseCompatible())
        {
          // Not compatible add to list
          databaseNames.append("<i>" + FsPaths::typeToDisplayName(it.key()) + "</i>");
          databaseFiles.append(dbName);
          qWarning() << "Incompatible database" << dbName;
        }
        sqlDb.close();
      }
    }

    // Delete the dummy database without dialog if needed
    QString dummyName = buildDatabaseFileName(atools::fs::FsPaths::NONE);
    sqlDb.setDatabaseName(dummyName);
    sqlDb.open();
    DatabaseMeta meta(&sqlDb);
    if(!meta.hasSchema() || !meta.isDatabaseCompatible())
    {
      qDebug() << Q_FUNC_INFO << "Updating dummy database" << dummyName;
      dbtools::createEmptySchema(&sqlDb);
    }
    sqlDb.close();

    if(!databaseNames.isEmpty())
    {
      QString msg, trailingMsg;
      if(databaseNames.size() == 1)
      {
        msg = tr("The database for the simulator "
                 "below is not compatible with this program version or was incompletly loaded:<br/><br/>"
                 "%1<br/><br/>Erase it?<br/><br/>%2");
        trailingMsg = tr("You can reload the Scenery Library Database again after erasing.");
      }
      else
      {
        msg = tr("The databases for the simulators "
                 "below are not compatible with this program version or were incompletly loaded:<br/><br/>"
                 "%1<br/><br/>Erase them?<br/><br/>%2");
        trailingMsg = tr("You can reload these Scenery Library Databases again after erasing.");
      }

      // Avoid the splash screen hiding the dialog
      NavApp::closeSplashScreen();

      QMessageBox box(QMessageBox::Question, QApplication::applicationName(), msg.arg(databaseNames.join("<br/>")).arg(trailingMsg),
                      QMessageBox::No | QMessageBox::Yes, mainWindow);
      box.button(QMessageBox::No)->setText(tr("&No and Exit Application"));
      box.button(QMessageBox::Yes)->setText(tr("&Erase"));

      int result = box.exec();

      if(result == QMessageBox::No)
        // User does not want to erase incompatible databases - exit application
        ok = false;
      else if(result == QMessageBox::Yes)
      {
        NavApp::closeSplashScreen();
        QMessageBox *simpleProgressDialog = atools::gui::Dialog::showSimpleProgressDialog(mainWindow, tr("Deleting ..."));
        atools::gui::Application::processEventsExtended();

        int i = 0;
        for(const QString& dbfile : databaseFiles)
        {
          simpleProgressDialog->setText(tr("Erasing database for %1 ...").arg(databaseNames.at(i)));
          atools::gui::Application::processEventsExtended();
          simpleProgressDialog->repaint();
          atools::gui::Application::processEventsExtended();

          if(QFile::remove(dbfile))
          {
            qInfo() << "Removed" << dbfile;

            // Create new database
            sqlDb.setDatabaseName(dbfile);
            sqlDb.open();
            dbtools::createEmptySchema(&sqlDb);
            sqlDb.close();

            if(databasesErased != nullptr)
              *databasesErased = true;
          }
          else
          {
            qWarning() << "Removing database failed" << dbfile;
            atools::gui::Dialog::warning(mainWindow,
                                         tr("Deleting of database<br/><br/>\"%1\"<br/><br/>failed.<br/><br/>"
                                            "Remove the database file manually and restart the program.").arg(dbfile));
            ok = false;
          }
          i++;
        }
        atools::gui::Dialog::deleteSimpleProgressDialog(simpleProgressDialog);
      }
    }
  }
  return ok;
}

void DatabaseManager::checkCopyAndPrepareDatabases()
{
  QString appDb = buildDatabaseFileNameAppDir(FsPaths::NAVIGRAPH);
  QString settingsDb = buildDatabaseFileName(FsPaths::NAVIGRAPH);
  bool hasApp = false, hasSettings = false, settingsNeedsPreparation = false;

  QDateTime appLastLoad = QDateTime::fromMSecsSinceEpoch(0), settingsLastLoad = QDateTime::fromMSecsSinceEpoch(0);
  QString appCycle, settingsCycle;
  QString appSource, settingsSource;

  // Open databases and get loading timestamp from metadata
  if(QFile::exists(appDb))
  {
    // Database in application directory
    const DatabaseMeta appMeta = databaseMetadataFromFile(appDb);
    appLastLoad = appMeta.getLastLoadTime();
    appCycle = appMeta.getAiracCycle();
    appSource = appMeta.getDataSource();
    hasApp = true;
  }

  if(QFile::exists(settingsDb))
  {
    // Database in settings directory
    const DatabaseMeta settingsMeta = databaseMetadataFromFile(settingsDb);
    settingsLastLoad = settingsMeta.getLastLoadTime();
    settingsCycle = settingsMeta.getAiracCycle();
    settingsSource = settingsMeta.getDataSource();
    settingsNeedsPreparation = settingsMeta.hasScript();
    hasSettings = true;
  }
  int appCycleNum = appCycle.toInt();
  int settingsCycleNum = settingsCycle.toInt();

  qInfo() << Q_FUNC_INFO << "settings database" << settingsDb << settingsLastLoad << settingsCycle;
  qInfo() << Q_FUNC_INFO << "app database" << appDb << appLastLoad << appCycle;
  qInfo() << Q_FUNC_INFO << "hasApp" << hasApp << "hasSettings" << hasSettings << "settingsNeedsPreparation" << settingsNeedsPreparation;

  if(hasApp)
  {
    int result = QMessageBox::Yes;

    // Compare cycles first and then compilation time
    if(appCycleNum > settingsCycleNum || (appCycleNum == settingsCycleNum && appLastLoad > settingsLastLoad))
    {
      if(hasSettings)
      {
        NavApp::closeSplashScreen();
        result = dialog->showQuestionMsgBox(
          lnm::ACTIONS_SHOW_OVERWRITE_DATABASE,
          tr("Your current navdata is older than the navdata included in the Little Navmap download archive.<br/><br/>"
             "Overwrite the current navdata file with the new one?"
             "<hr/>Current file to overwrite:<br/><br/>"
             "<i>%1<br/><br/>"
             "%2, cycle %3, compiled on %4</i>"
             "<hr/>New file:<br/><br/>"
             "<i>%5<br/><br/>"
             "%6, cycle %7, compiled on %8</i><hr/><br/>"
             ).
          arg(settingsDb).
          arg(settingsSource).
          arg(settingsCycle).
          arg(QLocale().toString(settingsLastLoad, QLocale::ShortFormat)).
          arg(appDb).
          arg(appSource).
          arg(appCycle).
          arg(QLocale().toString(appLastLoad, QLocale::ShortFormat)),
          tr("Do not &show this dialog again and skip copying."),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::No);
      }

      if(result == QMessageBox::Yes)
      {
        // We have a database in the application folder and it is newer than the one in the settings folder
        QMessageBox *simpleProgressDialog =
          atools::gui::Dialog::showSimpleProgressDialog(mainWindow, tr("Preparing %1 Database ...").
                                                        arg(FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH)));
        atools::gui::Application::processEventsExtended();

        bool resultRemove = true, resultCopy = false;
        // Remove target
        if(hasSettings)
        {
          resultRemove = QFile(settingsDb).remove();
          qDebug() << Q_FUNC_INFO << "removed" << settingsDb << resultRemove;
        }

        // Copy to target
        if(resultRemove)
        {
          simpleProgressDialog->setText(tr("Preparing %1 Database: Copying file ...").arg(FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH)));
          atools::gui::Application::processEventsExtended();
          simpleProgressDialog->repaint();
          atools::gui::Application::processEventsExtended();
          resultCopy = QFile(appDb).copy(settingsDb);
          qDebug() << Q_FUNC_INFO << "copied" << appDb << "to" << settingsDb << resultCopy;
        }

        // Create indexes and delete script afterwards
        if(resultRemove && resultCopy)
        {
          SqlDatabase tempDb(dbtools::DATABASE_NAME_TEMP);
          dbtools::openDatabaseFile(&tempDb, settingsDb, false /* readonly */, true /* createSchema */);
          simpleProgressDialog->setText(tr("Preparing %1 Database: Creating indexes ...").
                                        arg(FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH)));
          atools::gui::Application::processEventsExtended();
          simpleProgressDialog->repaint();
          atools::gui::Application::processEventsExtended();
          NavDatabase::runPreparationScript(tempDb);

          simpleProgressDialog->setText(tr("Preparing %1 Database: Analyzing ...").arg(FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH)));
          atools::gui::Application::processEventsExtended();
          simpleProgressDialog->repaint();
          atools::gui::Application::processEventsExtended();
          tempDb.analyze();
          dbtools::closeDatabaseFile(&tempDb);
          settingsNeedsPreparation = false;
        }
        atools::gui::Dialog::deleteSimpleProgressDialog(simpleProgressDialog);

        if(!resultRemove)
          atools::gui::Dialog::warning(mainWindow,
                                       tr("Deleting of database<br/><br/>\"%1\"<br/><br/>failed.<br/><br/>"
                                          "Remove the database file manually and restart the program.").arg(settingsDb));

        if(!resultCopy)
          atools::gui::Dialog::warning(mainWindow,
                                       tr("Cannot copy database<br/><br/>\"%1\"<br/><br/>to<br/><br/>"
                                          "\"%2\"<br/><br/>.").arg(appDb).arg(settingsDb));
      }
    }
  }

  if(settingsNeedsPreparation && hasSettings)
  {
    NavApp::closeSplashScreen();
    QMessageBox *simpleProgressDialog = atools::gui::Dialog::showSimpleProgressDialog(mainWindow, tr("Preparing %1 Database ...").
                                                                                      arg(FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH)));
    atools::gui::Application::processEventsExtended();
    simpleProgressDialog->repaint();
    atools::gui::Application::processEventsExtended();

    SqlDatabase tempDb(dbtools::DATABASE_NAME_TEMP);
    dbtools::openDatabaseFile(&tempDb, settingsDb, false /* readonly */, true /* createSchema */);

    // Delete all tables that are not used in Little Navmap versions > 2.4.5
    if(atools::util::Version(QApplication::applicationVersion()) > atools::util::Version(2, 4, 5))
      NavDatabase::runPreparationPost245(tempDb);

    // Executes all statements like create index in the table script and deletes it afterwards
    NavDatabase::runPreparationScript(tempDb);

    tempDb.vacuum();
    tempDb.analyze();
    dbtools::closeDatabaseFile(&tempDb);

    atools::gui::Dialog::deleteSimpleProgressDialog(simpleProgressDialog);
  }
}

bool DatabaseManager::isAirportDatabaseXPlane(bool navdata) const
{
  if(navdata)
    // Fetch from navdatabase - X-Plane airport only if navdata is not used
    return atools::fs::FsPaths::isAnyXplane(currentFsType) && navDatabaseStatus == navdb::OFF;
  else
    // Fetch from sim database - X-Plane airport only if navdata is not used for all
    return atools::fs::FsPaths::isAnyXplane(currentFsType) && navDatabaseStatus != navdb::ALL;
}

QString DatabaseManager::getCurrentSimulatorBasePath() const
{
  return getSimulatorBasePath(currentFsType);
}

QString DatabaseManager::getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type) const
{
  return simulators.value(type).basePath;
}

QString DatabaseManager::getSimulatorFilesPathBest(const FsPaths::SimulatorTypeVector& types, const QString& defaultPath) const
{
  QString path;
  FsPaths::SimulatorType type = simulators.getBestInstalled(types);
  switch(type)
  {
    // All not depending on installation path which might be changed by the user
    case atools::fs::FsPaths::FSX:
    case atools::fs::FsPaths::FSX_SE:
    case atools::fs::FsPaths::P3D_V3:
    case atools::fs::FsPaths::P3D_V4:
    case atools::fs::FsPaths::P3D_V5:
    case atools::fs::FsPaths::P3D_V6:
    case atools::fs::FsPaths::MSFS:
      // Ignore user changes of path for now
      path = FsPaths::getFilesPath(type);
      break;

    case atools::fs::FsPaths::XPLANE_11:
    case atools::fs::FsPaths::XPLANE_12:
      {
        // Might change with base path by user
        QString base = getSimulatorBasePath(type);
        if(!base.isEmpty())
          path = atools::buildPathNoCase({base, "Output", "FMS plans"});
      }
      break;

    case atools::fs::FsPaths::DFD:
    case atools::fs::FsPaths::ALL_SIMULATORS:
    case atools::fs::FsPaths::NONE:
      break;
  }
  return path.isEmpty() ? defaultPath : path;
}

QString DatabaseManager::getSimulatorBasePathBest(const FsPaths::SimulatorTypeVector& types) const
{
  FsPaths::SimulatorType type = simulators.getBestInstalled(types);
  switch(type)
  {
    // All not depending on installation path which might be changed by the user
    case atools::fs::FsPaths::FSX:
    case atools::fs::FsPaths::FSX_SE:
    case atools::fs::FsPaths::P3D_V3:
    case atools::fs::FsPaths::P3D_V4:
    case atools::fs::FsPaths::P3D_V5:
    case atools::fs::FsPaths::P3D_V6:
    case atools::fs::FsPaths::XPLANE_11:
    case atools::fs::FsPaths::XPLANE_12:
    case atools::fs::FsPaths::MSFS:
      return FsPaths::getBasePath(type);

    case atools::fs::FsPaths::DFD:
    case atools::fs::FsPaths::ALL_SIMULATORS:
    case atools::fs::FsPaths::NONE:
      return QString();
  }
  return QString();
}

atools::sql::SqlDatabase *DatabaseManager::getDatabaseOnline() const
{
  return onlinedataManager->getDatabase();
}

void DatabaseManager::insertSimSwitchActions()
{
  qDebug() << Q_FUNC_INFO;
  Ui::MainWindow *ui = NavApp::getMainUi();

  freeActions();

  // Create group to get radio button like behavior
  simDbGroup = new QActionGroup(ui->menuDatabase);
  simDbGroup->setExclusive(true);

  // Sort keys to avoid random order
  QList<FsPaths::SimulatorType> keys = simulators.keys();
  QList<FsPaths::SimulatorType> sims;
  std::sort(keys.begin(), keys.end(), [](FsPaths::SimulatorType t1, FsPaths::SimulatorType t2) {
    return FsPaths::typeToShortName(t1) < FsPaths::typeToShortName(t2);
  });

  // Add real simulators first
  for(atools::fs::FsPaths::SimulatorType type : qAsConst(keys))
  {
    const FsPathType pathType = simulators.value(type);

    if(pathType.isInstalled || pathType.hasDatabase)
      // Create an action for each simulator installation or database found
      sims.append(type);
  }

  int index = 1;
  bool foundSim = false, foundDb = false;
  for(atools::fs::FsPaths::SimulatorType type : sims)
  {
    insertSimSwitchAction(type, ui->menuViewAirspaceSource->menuAction(), ui->menuDatabase, index++);
    foundSim |= simulators.value(type).isInstalled;
    foundDb |= simulators.value(type).hasDatabase;
  }

  // Insert disabled action if nothing was found at all ===============================
  if(!foundDb && !foundSim)
    insertSimSwitchAction(atools::fs::FsPaths::NONE, ui->menuViewAirspaceSource->menuAction(), ui->menuDatabase, index++);

  menuDbSeparator = ui->menuDatabase->insertSeparator(ui->menuViewAirspaceSource->menuAction());

  // Update Reload scenery item ===============================
  ui->actionReloadScenery->setEnabled(foundSim);
  if(foundSim)
    ui->actionReloadScenery->setText(tr("&Load Scenery Library ..."));
  else
    ui->actionReloadScenery->setText(tr("Load Scenery Library (no simulator)"));

  // Noting to select if there is only one option ========================
  if(simDbActions.size() == 1)
    simDbActions.constFirst()->setDisabled(true);

  // Insert Navigraph menu ==================================
  QString file = buildDatabaseFileName(FsPaths::NAVIGRAPH);

  if(!file.isEmpty())
  {
    const DatabaseMeta meta = databaseMetadataFromFile(file);
    QString cycle = meta.getAiracCycle();
    QString suffix;

    if(!cycle.isEmpty())
      suffix = tr(" - AIRAC Cycle %1").arg(cycle);
    else
      suffix = tr(" - No AIRAC Cycle");

    if(!meta.hasData())
      suffix += tr(" (database is empty)");

#ifdef DEBUG_INFORMATION
    suffix += " (" + meta.getLastLoadTime().toString() + " | " + meta.getDataSource() + ")";
#endif

    QString dbname = FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH);
    navDbSubMenu = new QMenu(tr("&%1%2").arg(dbname).arg(suffix));
    navDbSubMenu->setToolTipsVisible(NavApp::isMenuToolTipsVisible());
    navDbGroup = new QActionGroup(navDbSubMenu);

    navDbActionAuto = new QAction(tr("&Select Automatically"), navDbSubMenu);
    navDbActionAuto->setCheckable(true);
    navDbActionAuto->setChecked(navDatabaseAuto);
    navDbActionAuto->setStatusTip(tr("Select best navdata mode for simulator"));
    navDbSubMenu->addAction(navDbActionAuto);
    navDbSubMenu->addSeparator();

    navDbActionAll = new QAction(tr("Use %1 for &all Features").arg(dbname), navDbSubMenu);
    navDbActionAll->setCheckable(true);
    navDbActionAll->setChecked(navDatabaseStatus == navdb::ALL);
    navDbActionAll->setStatusTip(tr("Use all of %1 database features").arg(dbname));
    navDbActionAll->setActionGroup(navDbGroup);
    navDbSubMenu->addAction(navDbActionAll);

    navDbActionMixed = new QAction(tr("Use %1 for &Navaids and Procedures").arg(dbname), navDbSubMenu);
    navDbActionMixed->setCheckable(true);
    navDbActionMixed->setChecked(navDatabaseStatus == navdb::MIXED);
    navDbActionMixed->setStatusTip(tr("Use only navaids, airways, airspaces and procedures from %1 database").arg(dbname));
    navDbActionMixed->setActionGroup(navDbGroup);
    navDbSubMenu->addAction(navDbActionMixed);

    navDbActionOff = new QAction(tr("Do &not use %1 database").arg(dbname), navDbSubMenu);
    navDbActionOff->setCheckable(true);
    navDbActionOff->setChecked(navDatabaseStatus == navdb::OFF);
    navDbActionOff->setStatusTip(tr("Do not use %1 database").arg(dbname));
    navDbActionOff->setActionGroup(navDbGroup);
    navDbSubMenu->addAction(navDbActionOff);

    ui->menuDatabase->insertMenu(ui->menuViewAirspaceSource->menuAction(), navDbSubMenu);
    menuNavDbSeparator = ui->menuDatabase->insertSeparator(ui->menuViewAirspaceSource->menuAction());

    // triggered() does not send signal on programmatic change
    connect(navDbActionAuto, &QAction::triggered, this, &DatabaseManager::switchNavAutoFromMainMenu);
    connect(navDbActionAll, &QAction::triggered, this, &DatabaseManager::switchNavFromMainMenu);
    connect(navDbActionMixed, &QAction::triggered, this, &DatabaseManager::switchNavFromMainMenu);
    connect(navDbActionOff, &QAction::triggered, this, &DatabaseManager::switchNavFromMainMenu);

    // Enable nav selection menu items depending on automatic state
    updateNavMenuStatus();
  }
}

void DatabaseManager::insertSimSwitchAction(atools::fs::FsPaths::SimulatorType type, QAction *before, QMenu *menu, int index)
{
  if(type == atools::fs::FsPaths::NONE)
  {
    QAction *action = new QAction(tr("No Scenery Library and no Simulator found"), menu);
    action->setToolTip(tr("No scenery library database and no simulator found"));
    action->setStatusTip(action->toolTip());
    action->setData(QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));
    action->setActionGroup(simDbGroup);

    menu->insertAction(before, action);
    simDbActions.append(action);
  }
  else
  {
    QString suffix;
    QStringList atts;
    const DatabaseMeta meta = databaseMetadataFromFile(buildDatabaseFileName(type));
    if(atools::fs::FsPaths::isAnyXplane(type))
    {
      QString cycle = meta.getAiracCycle();
      if(!cycle.isEmpty())
        suffix = tr(" - AIRAC Cycle %1").arg(cycle);
    }

    // Built string for hint ===============
    if(!meta.hasData())
      atts.append(tr("empty"));
    else
    {
      if(meta.getDatabaseVersion() < DatabaseMeta::getApplicationVersion())
        atts.append(tr("prev. version - reload advised"));
      else if(meta.getLastLoadTime() < QDateTime::currentDateTime().addDays(-MAX_AGE_DAYS))
      {
        qint64 days = meta.getLastLoadTime().date().daysTo(QDate::currentDate());
        atts.append(tr("%1 days old - reload advised").arg(days));
      }
    }

    if(!simulators.value(type).isInstalled)
      atts.append(tr("no simulator"));

    if(!atts.isEmpty())
      suffix.append(tr(" (%1)").arg(atts.join(tr(", "))));

    QAction *action = new QAction(tr("&%1 %2%3").arg(index).arg(FsPaths::typeToDisplayName(type)).arg(suffix), menu);
    action->setToolTip(tr("Switch to %1 database").arg(FsPaths::typeToDisplayName(type)));
    action->setStatusTip(action->toolTip());
    action->setData(QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));
    action->setCheckable(true);
    action->setActionGroup(simDbGroup);

    if(type == currentFsType)
    {
      QSignalBlocker blocker(action);
      Q_UNUSED(blocker)
      action->setChecked(true);
    }

    menu->insertAction(before, action);

    connect(action, &QAction::triggered, this, &DatabaseManager::switchSimFromMainMenu);
    simDbActions.append(action);
  }
}

void DatabaseManager::updateNavMenuStatus()
{
  navDbActionAll->setDisabled(navDatabaseAuto);
  navDbActionMixed->setDisabled(navDatabaseAuto);
  navDbActionOff->setDisabled(navDatabaseAuto);
}

/* User changed nav auto main menu */
void DatabaseManager::switchNavAutoFromMainMenu()
{
  qDebug() << Q_FUNC_INFO;
  navDatabaseAuto = navDbActionAuto->isChecked();
  updateNavMenuStatus();
  switchSimInternal(currentFsType);
}

/* User changed navdatabase in main menu */
void DatabaseManager::switchNavFromMainMenu()
{
  qDebug() << Q_FUNC_INFO;

  if(navDatabaseAuto) // Should never appear here
    return;

  bool switchDatabase = true;

  if(navDbActionAll->isChecked())
  {
    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineNavdatabasesUrl, lnm::helpLanguageOnline());
    QString message =
      tr("<p style='white-space:pre'>The scenery mode \"%1\" ignores all airports of the simulator.<p/>"
         "<p>Airport information is limited in this mode.<br/>"
         "This means that aprons, taxiways, parking positions, runway surface information and other information is not available.<br/>"
         "Smaller airports might be missing and runway layout might not match the runway layout in the simulator.</p>"
         "<p><b>Normally you should not use this mode.</b></p>"
           "<p>Really switch to mode \"%1\" now?</p>"
             "<p><a href=\"%1\">Click here for more information in the Little Navmap online manual</a></p>").
      arg(navDbActionAll->text().remove("&")).arg(url.toString());

    int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_NAVDATA_WARNING, message,
                                            tr("Do not &show this dialog again and switch the mode."),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::Yes);

    if(result == QMessageBox::No)
    {
      // Revert to previous mode
      atools::gui::SignalBlocker blocker({navDbActionMixed, navDbActionOff, navDbActionAll});
      navDbActionAll->setChecked(false);
      navDbActionMixed->setChecked(navDatabaseStatus == navdb::MIXED);
      navDbActionOff->setChecked(navDatabaseStatus == navdb::OFF);
      switchDatabase = false;
    }
  }

  if(switchDatabase)
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // Disconnect all queries
    emit preDatabaseLoad();

    clearLanguageIndex();
    closeAllDatabases();

    QString text;
    if(navDbActionAll->isChecked())
    {
      navDatabaseStatus = navdb::ALL;
      text = tr("Enabled all features for %1.");
    }
    else if(navDbActionMixed->isChecked())
    {
      navDatabaseStatus = navdb::MIXED;
      text = tr("Enabled navaids, airways, airspaces and procedures for %1.");
    }
    else if(navDbActionOff->isChecked())
    {
      navDatabaseStatus = navdb::OFF;
      text = tr("Disabled %1.");
    }

    // Remember nav selection value for simulator
    simulators[currentFsType].navDatabaseStatus = navDatabaseStatus;

    qDebug() << Q_FUNC_INFO << "usingNavDatabase" << navDatabaseStatus;

    openAllDatabases();
    loadLanguageIndex();
    loadAircraftIndex();

    QGuiApplication::restoreOverrideCursor();

    emit postDatabaseLoad(currentFsType);

    mainWindow->setStatusMessage(text.arg(FsPaths::typeToDisplayName(FsPaths::NAVIGRAPH)));

    saveState();
  } // if(switchDatabase)
}

void DatabaseManager::switchSimFromMainMenu()
{
  QAction *action = dynamic_cast<QAction *>(sender());

  qDebug() << Q_FUNC_INFO << (action != nullptr ? action->text() : "null");

  if(action != nullptr)
  {
    atools::fs::FsPaths::SimulatorType type = action->data().value<atools::fs::FsPaths::SimulatorType>();
    if(currentFsType != type)
      // Switch only if changed
      switchSimInternal(type);
  }
}

void DatabaseManager::switchSimInternal(atools::fs::FsPaths::SimulatorType type)
{
  qDebug() << Q_FUNC_INFO;

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  // Disconnect all queries
  emit preDatabaseLoad();

  clearLanguageIndex();
  closeAllDatabases();

  // Set new simulator
  currentFsType = type;

  if(navDatabaseAuto)
    // Correct navdata selection automatically if enabled
    assignSceneryCorrection();
  else
    // Assign navdata status from remembered value from simulator
    navDatabaseStatus = simulators.value(currentFsType).navDatabaseStatus;

  openAllDatabases();
  loadLanguageIndex();
  loadAircraftIndex();

  QGuiApplication::restoreOverrideCursor();

  // Reopen all with new database
  emit postDatabaseLoad(currentFsType);
  mainWindow->setStatusMessage(tr("Switched to %1.").arg(FsPaths::typeToDisplayName(currentFsType)));

  saveState();
  checkDatabaseVersion();

  {
    // Check and uncheck manually since the QActionGroup is unreliable
    atools::gui::SignalBlocker blocker(simDbActions);
    for(QAction *act : qAsConst(simDbActions))
      act->setChecked(act->data().value<atools::fs::FsPaths::SimulatorType>() == currentFsType);
  }
}

void DatabaseManager::openWriteableDatabase(atools::sql::SqlDatabase *database, const QString& name,
                                            const QString& displayName, bool backup)
{
  QString databaseName = databaseDirectory + QDir::separator() + lnm::DATABASE_PREFIX + name + lnm::DATABASE_SUFFIX;

  QString databaseNameBackup = databaseDirectory + QDir::separator() + QFileInfo(databaseName).baseName() + "_backup" +
                               lnm::DATABASE_SUFFIX;

  try
  {
    if(backup)
    {
      // Roll copies
      // .../ABarthel/little_navmap_db/little_navmap_userdata_backup.sqlite
      // .../ABarthel/little_navmap_db/little_navmap_userdata_backup.sqlite.1
      atools::io::FileRoller roller(1);
      roller.rollFile(databaseNameBackup);

      // Copy database before opening
      bool result = QFile(databaseName).copy(databaseNameBackup);
      qInfo() << Q_FUNC_INFO << "Copied" << databaseName << "to" << databaseNameBackup << "result" << result;
    }

    dbtools::openDatabaseFileExt(database, databaseName, false /* readonly */, false /* createSchema */,
                                 false /* exclusive */, false /* auto transactions */);
  }
  catch(atools::sql::SqlException& e)
  {
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          tr("Cannot open %1 database. Reason:<br/><br/>"
                             "%2<br/><br/>"
                             "Is another instance of <i>%3</i> running?<br/><br/>"
                             "Exiting now.").
                          arg(displayName).
                          arg(e.getSqlError().databaseText()).
                          arg(QApplication::applicationName()));

    std::exit(1);
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

void DatabaseManager::closeUserDatabase()
{
  dbtools::closeDatabaseFile(databaseUser);
}

void DatabaseManager::closeTrackDatabase()
{
  dbtools::closeDatabaseFile(databaseTrack);
}

void DatabaseManager::closeUserAirspaceDatabase()
{
  dbtools::closeDatabaseFile(databaseUserAirspace);
}

void DatabaseManager::closeLogDatabase()
{
  dbtools::closeDatabaseFile(databaseLogbook);
}

void DatabaseManager::closeOnlineDatabase()
{
  dbtools::closeDatabaseFile(databaseOnline);
}

void DatabaseManager::clearLanguageIndex()
{
  languageIndex->clear();
}

void DatabaseManager::loadLanguageIndex()
{
  if(SqlUtil(databaseSim).hasTableAndRows("translation"))
    languageIndex->readFromDb(databaseSim, OptionData::instance().getLanguage());
}

void DatabaseManager::clearAircraftIndex()
{
  aircraftIndex->clear();
}

void DatabaseManager::loadAircraftIndex()
{
  if(currentFsType == FsPaths::MSFS && simulators.value(FsPaths::MSFS).isInstalled)
  {
    QString basePath = simulators.value(FsPaths::MSFS).basePath;
    if(atools::checkDir(Q_FUNC_INFO, basePath, true /* warn */))
      aircraftIndex->loadIndex({FsPaths::getMsfsCommunityPath(basePath), FsPaths::getMsfsOfficialPath(basePath)});
  }
}

void DatabaseManager::openAllDatabases()
{
  QString simDbFile = buildDatabaseFileName(currentFsType);
  QString navDbFile = buildDatabaseFileName(FsPaths::NAVIGRAPH);

  // Airspace databases are independent of switch
  QString simAirspaceDbFile = simDbFile;
  QString navAirspaceDbFile = navDbFile;

  if(navDatabaseStatus == navdb::ALL)
    simDbFile = navDbFile;
  else if(navDatabaseStatus == navdb::OFF)
    navDbFile = simDbFile;
  // else if(usingNavDatabase == MIXED)

  dbtools::openDatabaseFile(databaseSim, simDbFile, true /* readonly */, true /* createSchema */);
  dbtools::openDatabaseFile(databaseNav, navDbFile, true /* readonly */, true /* createSchema */);

  dbtools::openDatabaseFile(databaseSimAirspace, simAirspaceDbFile, true /* readonly */, true /* createSchema */);
  dbtools::openDatabaseFile(databaseNavAirspace, navAirspaceDbFile, true /* readonly */, true /* createSchema */);
}

void DatabaseManager::closeAllDatabases()
{
  dbtools::closeDatabaseFile(databaseSim);
  dbtools::closeDatabaseFile(databaseNav);
  dbtools::closeDatabaseFile(databaseSimAirspace);
  dbtools::closeDatabaseFile(databaseNavAirspace);
}

void DatabaseManager::checkForChangedNavAndSimDatabases()
{
  if(!showingDatabaseChangeWarning)
  {
    showingDatabaseChangeWarning = true;
    if(QGuiApplication::applicationState() & Qt::ApplicationActive)
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO;
#endif

      QStringList files;
      if(databaseSim != nullptr && databaseSim->isOpen() && databaseSim->isFileModified())
        files.append(QDir::toNativeSeparators(databaseSim->databaseName()));

      if(databaseNav != nullptr && databaseNav->isOpen() && databaseNav->isFileModified())
        files.append(QDir::toNativeSeparators(databaseNav->databaseName()));
      files.removeDuplicates();
      if(!files.isEmpty())
      {
        QMessageBox::warning(mainWindow, QApplication::applicationName(),
                             tr("<p style=\"white-space:pre\">"
                                  "Detected a modification of one or more database files:<br/><br/>"
                                  "&quot;%1&quot;"
                                  "<br/><br/>"
                                  "Always close <i>%2</i> before copying, overwriting or updating scenery library databases.</p>").
                             arg(files.join(tr("&quot;<br/>&quot;"))).
                             arg(QApplication::applicationName()));

        databaseNav->recordFileMetadata();
        databaseSim->recordFileMetadata();
      }
    }
    showingDatabaseChangeWarning = false;
  }
}

void DatabaseManager::loadScenery()
{
  qDebug() << Q_FUNC_INFO;

  Q_ASSERT(databaseLoader != nullptr);

  if(databaseLoader->isLoadingProgress())
    // Already loading or showing confirmation dialog - just raise progress window
    databaseLoader->showProgressWindow();
  else
  {
    if(simulators.value(currentFsType).isInstalled)
      // Use what is currently displayed on the map
      selectedFsType = currentFsType;

    databaseDialog->setCurrentFsType(selectedFsType);
    databaseDialog->setReadInactive(readInactive);
    databaseDialog->setReadAddOnXml(readAddOnXml);

    // Reload numbers in dialog
    updateDialogInfo(selectedFsType);

    loadSceneryInternal();
    // Signal DatabaseLoader::loadingFinished() calls method DatabaseManager::loadSceneryInternalPost()
    // Method loadSceneryInternalPost() calls method loadSceneryPost()
  }
}

void DatabaseManager::loadSceneryStop()
{
  databaseLoader->loadSceneryStop();
}

void DatabaseManager::showProgressWindow()
{
  databaseLoader->showProgressWindow();
}

bool DatabaseManager::isLoadingProgress()
{
  return databaseLoader->isLoadingProgress();
}

void DatabaseManager::loadSceneryPost()
{
  qDebug() << Q_FUNC_INFO;

  updateSimulatorFlags();
  insertSimSwitchActions();

  saveState();

#ifdef DEBUG_SCENERY_COMPILE
  checkSceneryOptions(false /* manualCheck */);
#else
  // Show only if compilation was ok
  if(!databaseLoader->getResultFlags().testFlag(atools::fs::COMPILE_CANCELED) &&
     !databaseLoader->getResultFlags().testFlag(atools::fs::COMPILE_FAILED))
    checkSceneryOptions(false /* manualCheck */);
#endif
}

const atools::fs::db::DatabaseMeta DatabaseManager::databaseMetadataFromType(atools::fs::FsPaths::SimulatorType type) const
{
  return databaseMetadataFromFile(buildDatabaseFileName(type));
}

const atools::fs::db::DatabaseMeta DatabaseManager::databaseMetadataFromFile(const QString& file) const
{
  // Get simulator database independent of settings in menu which might result in nav and sim using the same files =======
  // Open temporary database and read metadata
  SqlDatabase tempDb(dbtools::DATABASE_NAME_TEMP);
  dbtools::openDatabaseFile(&tempDb, file, true /* readonly */, false /* createSchema */);
  DatabaseMeta meta(tempDb);
  meta.deInit();
  dbtools::closeDatabaseFile(&tempDb);
  return meta;
}

void DatabaseManager::assignSceneryCorrection()
{
  if(navDatabaseAuto)
  {
    navdb::Correction correction = getSceneryCorrection(navDatabaseStatus, currentFsType);
    switch(correction)
    {
      case navdb::CORRECT_ALL:
      case navdb::CORRECT_MSFS_HAS_NAVIGRAPH:
      case navdb::CORRECT_XP_CYCLE_NAV_EQUAL:
      case navdb::CORRECT_FSX_P3D_UPDATED:
        // Assign value to simulator too to remember value
        simulators[currentFsType].navDatabaseStatus = navDatabaseStatus = navdb::MIXED;
        break;

      case navdb::CORRECT_MSFS_NO_NAVIGRAPH:
      case navdb::CORRECT_XP_CYCLE_NAV_SMALLER:
      case navdb::CORRECT_FSX_P3D_OUTDATED:
        // Assign value to simulator too to remember value
        simulators[currentFsType].navDatabaseStatus = navDatabaseStatus = navdb::OFF;
        break;

      case navdb::CORRECT_EMPTY:
        // Assign value to simulator too to remember value
        simulators[currentFsType].navDatabaseStatus = navDatabaseStatus = navdb::ALL;
        break;

      case navdb::CORRECT_NONE:
        // Nothing to correct
        break;
    }

    qDebug() << Q_FUNC_INFO << navDatabaseStatus;
  }
}

navdb::Correction DatabaseManager::getSceneryCorrection(const navdb::Status& navDbStatus, atools::fs::FsPaths::SimulatorType simType) const
{
  // Get simulator database independent of settings in menu which might result in nav and sim using the same files =======
  const DatabaseMeta metaSim = databaseMetadataFromType(simType);
  const DatabaseMeta metaNav = databaseMetadataFromType(FsPaths::NAVIGRAPH);
  navdb::Correction correction = navdb::CORRECT_NONE;

  if(metaSim.hasData())
  {
    if(simType == atools::fs::FsPaths::MSFS)
    {
      // ======================================================================================================
      // Correct scenery mode for MSFS ==============================================
      bool hasNavigraphUpdate = metaSim.hasProperty(atools::fs::db::PROPERTYNAME_MSFS_NAVIGRAPH_FOUND);

      if(hasNavigraphUpdate || databaseLoader->getResultFlags().testFlag(atools::fs::COMPILE_MSFS_NAVIGRAPH_FOUND))
      {
        if(navDbStatus != navdb::MIXED)
          correction = navdb::CORRECT_MSFS_HAS_NAVIGRAPH;
      }
      else if(navDbStatus != navdb::OFF)
        correction = navdb::CORRECT_MSFS_NO_NAVIGRAPH;
    }
    else if(FsPaths::isAnyXplane(simType))
    {
      // =========================================================================================================
      // Correct scenery mode for X-Plane ==============================================
      int cycleNav = metaNav.getAiracCycleInt();
      int cycleSim = metaSim.getAiracCycleInt();

      // Both airac cycles available
      if(cycleSim > 0 && cycleNav > 0)
      {
        // Recommend use mixed database if cycles are equal ====================================
        if(cycleNav == cycleSim && navDbStatus != navdb::MIXED)
          correction = navdb::CORRECT_XP_CYCLE_NAV_EQUAL;
        else if(cycleNav < cycleSim && navDbStatus != navdb::OFF)
          correction = navdb::CORRECT_XP_CYCLE_NAV_SMALLER;
      }
    }
    else
    {
      // ======================================================================================================
      // Correct scenery mode for FSX or P3D ==============================================
      if(!metaNav.isIncludedAiracCycle() && navDbStatus != navdb::MIXED)
        // Updated but ignoring new navdatabase or mode all
        correction = navdb::CORRECT_FSX_P3D_UPDATED;
      else if(metaNav.isIncludedAiracCycle() && navDbStatus != navdb::OFF)
        // Old database being used - suggest off
        correction = navdb::CORRECT_FSX_P3D_OUTDATED;
    }

    // Check for wrong mode navdata for all if it was not corrected before ===========================================
    if(navDbStatus == navdb::ALL && correction == navdb::CORRECT_NONE)
      correction = navdb::CORRECT_ALL;
  }
  else
    correction = navdb::CORRECT_EMPTY;

  return correction;
}

void DatabaseManager::correctSceneryOptions(navdb::Correction& correction)
{
  qDebug() << Q_FUNC_INFO << "correction" << correction;

  switch(correction)
  {
    case navdb::CORRECT_ALL:
    case navdb::CORRECT_FSX_P3D_UPDATED:
    case navdb::CORRECT_MSFS_HAS_NAVIGRAPH:
    case navdb::CORRECT_XP_CYCLE_NAV_EQUAL:
      navDbActionMixed->setChecked(true);
      switchNavFromMainMenu();
      break;

    case navdb::CORRECT_FSX_P3D_OUTDATED:
    case navdb::CORRECT_MSFS_NO_NAVIGRAPH:
    case navdb::CORRECT_XP_CYCLE_NAV_SMALLER:
      navDbActionOff->setChecked(true);
      switchNavFromMainMenu();
      break;

    case navdb::CORRECT_EMPTY:
      navDbActionAll->setChecked(true);
      switchNavFromMainMenu();
      break;

    case navdb::CORRECT_NONE:
      break;
  }

}

void DatabaseManager::checkSceneryOptions(bool manualCheck)
{
  navdb::Correction correction = getSceneryCorrection(navDatabaseStatus, currentFsType);
  const DatabaseMeta metaSim = databaseMetadataFromType(currentFsType);
  const DatabaseMeta metaNav = databaseMetadataFromType(FsPaths::NAVIGRAPH);

  qDebug() << Q_FUNC_INFO << "manualCheck" << manualCheck << "correction" << correction;

  switch(correction)
  {
    case navdb::CORRECT_NONE:
      if(manualCheck)
        QMessageBox::information(mainWindow, tr("%1 - Validate scenery library mode").arg(QApplication::applicationName()),
                                 navDbActionAuto->isChecked() ?
                                 tr("Scenery library mode is correct. Mode is set automatically.") :
                                 tr("No issues found. Scenery library mode is correct."));
      break;

    case navdb::CORRECT_EMPTY:
      if(manualCheck)
        QMessageBox::information(mainWindow, tr("%1 - Validate Scenery Library Settings").arg(QApplication::applicationName()),
                                 tr("Showing Navigraph airports since simulator database is empty.\n\n"
                                    "You can load the simulator scenery library database in the menu\n"
                                    "\"Scenery Library\" -> \"Load Scenery Library\"."));
      break;

    case navdb::CORRECT_MSFS_HAS_NAVIGRAPH:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_MSFS_HAS_NAVIGRAPH,
                                    tr("<p style='white-space:pre'>You are using MSFS with the Navigraph navdata update.</p>"
                                         "<p>You should update the Little Navmap navdata with the "
                                           "Navigraph FMS Data Manager as well and use the right scenery library mode "
                                           "\"Use Navigraph for Navaids and Procedures\" "
                                           "to avoid issues with airport information in Little Navmap.</p>"
                                           "<p style='white-space:pre'>You can change the mode manually in the menu<br/>"
                                           "\"Scenery Library\" -> \"Navigraph\" -> \"Use Navigraph for Navaids and Procedures\".</p>"
                                           "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items"),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;

    case navdb::CORRECT_MSFS_NO_NAVIGRAPH:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_MSFS_NO_NAVIGRAPH,
                                    tr("<p style='white-space:pre'>You are using MSFS without the Navigraph navdata update.</p>"
                                         "<p>You should use the scenery library mode \"Do not use Navigraph Database\" "
                                           "to avoid issues with airport information in Little Navmap.</p>"
                                           "<p style='white-space:pre'>You can change the mode manually in the menu<br/>"
                                           "\"Scenery Library\" -> \"Navigraph\" -> \"Do not use Navigraph Database\".</p>"
                                           "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items"),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;

    case navdb::CORRECT_XP_CYCLE_NAV_EQUAL:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_XP_CYCLE_NAV_EQUAL,
                                    tr("<p style='white-space:pre'>The AIRAC cycle %1 of your navigation data is "
                                         "equal to the simulator cycle.<p>"
                                         "<p>You should use the scenery library mode \"Use Navigraph for Navaids and Procedures\" "
                                           "to fetch airports from the simulator and navdata from the update.</p>"
                                           "<p style='white-space:pre'>You can change this manually in the menu<br/>"
                                           "\"Scenery Library\" -> \"Navigraph\" -> \"Use Navigraph for Navaids and Procedures\".</p>"
                                           "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items").
                                    arg(metaNav.getAiracCycle()),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;

    case navdb::CORRECT_XP_CYCLE_NAV_SMALLER:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_XP_CYCLE_NAV_SMALLER,
                                    tr("<p style='white-space:pre'>The AIRAC cycle %1 of your navigation data is "
                                         "older than the simulator cycle %2.</p>"
                                         "<p>This can result in warning messages when loading flight plans in X-Plane.</p>"
                                           "<p>Update the Little Navmap navdata to use the same cycle as the X-Plane "
                                             "navdata with the Navigraph FMS Data Manager to fix this.</p>"
                                             "<p>You can also use the scenery library mode \"Do not use Navigraph Database\" "
                                               "to fetch all data from the simulator.</p>"
                                               "<p style='white-space:pre'>You can change this manually in the menu<br/>"
                                               "\"Scenery Library\" -> \"Navigraph\" -> \"Do not use Navigraph Database\".</p>"
                                               "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items").
                                    arg(metaNav.getAiracCycle()).arg(metaSim.getAiracCycle()),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;

    case navdb::CORRECT_FSX_P3D_OUTDATED:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_FSX_P3D_OUTDATED,
                                    tr("<p style='white-space:pre'>Your navdata based on AIRAC cycle %1 is outdated.</p>"
                                         "<p>This can result in warning messages when loading flight plans.</p>"
                                           "<p>Update the Little Navmap navdata with the Navigraph FMS Data Manager to fix this.</p>"
                                             "<p>You can also use the scenery library mode \"Do not use Navigraph Database\" "
                                               "to fetch all data from the simulator.</p>"
                                               "<p style='white-space:pre'>You can change this manually in the menu<br/>"
                                               "\"Scenery Library\" -> \"Navigraph\" -> \"Do not use Navigraph Database\".</p>"
                                               "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items").
                                    arg(DatabaseMeta::getDbIncludedNavdataCycle()),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;

    case navdb::CORRECT_FSX_P3D_UPDATED:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_FSX_P3D_UPDATED,
                                    tr("<p style='white-space:pre'>"
                                         "<p>You are using an updated Navigraph database with a not optimal scenery library mode.</p>"
                                           "<p style='white-space:pre'>You can fix this in the menu<br/>"
                                           "\"Scenery Library\" -> \"Navigraph\" -> \"Use Navigraph for Navaids and Procedures\".</p>"
                                           "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items"),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;

    case navdb::CORRECT_ALL:
      if(dialog->showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_DATABASE_MSFS_NAVIGRAPH_ALL,
                                    tr("<p style='white-space:pre'>Your current scenery library mode is "
                                         "\"Use Navigraph for all Features\".</p>"
                                         "<p>All information from the simulator scenery library is ignored in this mode.</p>"
                                           "<p>Note that airport information is limited in this mode. "
                                             "This means that aprons, taxiways, parking positions, runway surfaces and more are not available, "
                                             "smaller airports will be missing and the runway layout might not match the one in the simulator.</p>"
                                             "<p style='white-space:pre'>You can change this manually in the menu<br/>"
                                             "\"Scenery Library\" -> \"Navigraph\" -> \"Use Navigraph for Navaids and Procedures\".</p>"
                                             "<p><b>Normally you should not use this mode.</b></p>"
                                               "<p>Correct the scenery library mode now?</p>", "Sync texts with menu items"),
                                    manualCheck ? QString() : tr("Do not &show this dialog again and always correct mode after loading."),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes) == QMessageBox::Yes)
        correctSceneryOptions(correction);
      break;
  }
}

bool DatabaseManager::checkValidBasePaths() const
{
  using atools::gui::Dialog;

  bool configValid = true;
  QStringList errors;
  if(!NavDatabase::isBasePathValid(databaseDialog->getBasePath(), errors, selectedFsType))
  {
    QString resetPath(tr("<p>Click \"Reset paths\" in the dialog \"Load Scenery Library\" for a possible fix.</p>"));
    if(selectedFsType == atools::fs::FsPaths::MSFS)
    {
      // Check if base path is valid - all simulators ========================================================
      Dialog::warning(databaseDialog,
                      tr("<p style='white-space:pre'>Cannot read base path \"%1\".<br/><br/>"
                         "Reason:<br/>"
                         "%2<br/><br/>"
                         "Either the \"OneStore\" or the \"Steam\" paths have to exist.<br/>"
                         "The path \"Community\" is always needed for add-ons.</p>%3").
                      arg(databaseDialog->getBasePath()).arg(errors.join("<br/>")).arg(resetPath));
    }
    else
    {
      Dialog::warning(databaseDialog,
                      tr("<p style='white-space:pre'>Cannot read base path \"%1\".<br/><br/>"
                         "Reason:<br/>"
                         "%2</p>%3").arg(databaseDialog->getBasePath()).arg(errors.join("<br/>")).arg(resetPath));
    }
    configValid = false;
  }

  // Do further checks if basepath is valid =================
  if(configValid)
  {
    if(atools::fs::FsPaths::isAnyXplane(selectedFsType))
    {
      // Check scenery_packs.ini for X-Plane ========================================================
      QString filepath;
      if(!readInactive && !atools::fs::xp::SceneryPacks::exists(databaseDialog->getBasePath(), errors, filepath))
      {
        Dialog::warning(databaseDialog,
                        tr("<p style='white-space:pre'>Cannot read scenery configuration \"%1\".<br/><br/>"
                           "Reason:<br/>"
                           "%2<br/><br/>"
                           "Enable the option \"Read inactive or disabled Scenery Entries\"<br/>"
                           "or start X-Plane once to create the file.</p>").arg(filepath).arg(errors.join("<br/>")));
        configValid = false;
      }
    }
    else if(selectedFsType != atools::fs::FsPaths::MSFS)
    {
      // Check scenery.cfg for FSX and P3D ========================================================
      QString sceneryCfgCodec = (selectedFsType == atools::fs::FsPaths::P3D_V4 || selectedFsType == atools::fs::FsPaths::P3D_V5 ||
                                 selectedFsType == atools::fs::FsPaths::P3D_V6) ? "UTF-8" : QString();

      if(!NavDatabase::isSceneryConfigValid(databaseDialog->getSceneryConfigFile(), sceneryCfgCodec, errors))
      {
        Dialog::warning(databaseDialog,
                        tr("<p style='white-space:pre'>Cannot read scenery configuration \"%1\".<br/><br/>"
                           "Reason:<br/>"
                           "%2</p>").arg(databaseDialog->getSceneryConfigFile()).arg(errors.join("<br/>")));
        configValid = false;
      }
    }
  } // if(configValid)
  return configValid;
}

/* Shows scenery database loading dialog.
 * @return true if execution was successfull. false if it was cancelled */
void DatabaseManager::loadSceneryInternal()
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    NavApp::setStayOnTop(databaseDialog);

    // Show loading dialog
    int retval = databaseDialog->exec();

    // Copy the changed path structures also if the dialog was closed only
    updateSimulatorPathsFromDialog();

    // Get the simulator database we'll update/load
    selectedFsType = databaseDialog->getCurrentFsType();

    readInactive = databaseDialog->isReadInactive();
    readAddOnXml = databaseDialog->isReadAddOnXml();

    if(retval == QDialog::Accepted)
    {
      bool configValid = checkValidBasePaths();

      // Start compilation if all is valid ====================================================
      if(configValid)
      {
        // Compile into a temporary database file
        QString tempFilename = buildCompilingDatabaseFileName();

        // Remove database and probable journal file =========
        if(QFile::remove(tempFilename))
          qInfo() << "Removed" << tempFilename;
        else
          qWarning() << "Removing" << tempFilename << "failed";

        QFile journal(tempFilename + "-journal");
        if(journal.exists() && journal.size() == 0)
        {
          if(journal.remove())
            qInfo() << "Removed" << journal.fileName();
          else
            qWarning() << "Removing" << journal.fileName() << "failed";
        }

        // Prepare loader for background loading ==========================================
        databaseLoader->setReadAddOnXml(readAddOnXml);
        databaseLoader->setReadInactive(readInactive);
        databaseLoader->setSelectedFsType(selectedFsType);
        databaseLoader->setSimulators(simulators);
        databaseLoader->setDatabaseFilename(tempFilename);
        databaseLoader->clearResultFlags();

        if(!backgroundHintShown)
        {
          dialog->showInfoMsgBox(lnm::ACTIONS_SHOW_DATABASE_BACKGROUND_HINT,
                                 tr("Note that you can put the scenery library loading window into the background and "
                                    "continue working with Little Navmap while it is loading."),
                                 tr("Do not &show this dialog again."));
          backgroundHintShown = true;
        }

        // Load ==========================================
        // Signal DatabaseLoader::loadingFinished() calls method DatabaseManager::loadSceneryInternalPost()
        // Method loadSceneryInternalPost() calls method loadSceneryPost()
        databaseLoader->loadScenery();
      } // if(configValid)
    } // if(retval == QDialog::Accepted)
    else
      // User hit close or cancel - set flag in case this was skipped
      databaseLoader->setResultFlag(atools::fs::COMPILE_CANCELED);
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

void DatabaseManager::loadSceneryInternalPost()
{
  qDebug() << Q_FUNC_INFO;

  if(NavApp::isCloseCalled())
    return;

  // Temporary compile database
  QString dbFilename = databaseLoader->getDatabaseFilename();

  // Target database to copy to on success
  QString selectedFilename = buildDatabaseFileName(selectedFsType);

  bool reopenDialog = false;
  atools::fs::ResultFlags resultFlags = databaseLoader->getResultFlags();

  if(!resultFlags.testFlag(atools::fs::COMPILE_CANCELED) && !resultFlags.testFlag(atools::fs::COMPILE_FAILED))
  {
    // Successfully loaded - replace old database with new one ============================================
    reopenDialog = false;

    clearLanguageIndex();

    // Notify all objects in program to disconnect queries
    emit preDatabaseLoad();
    closeAllDatabases();

    // Remove old database
    if(QFile::remove(selectedFilename))
      qInfo() << "Removed" << selectedFilename;
    else
      qWarning() << "Removing" << selectedFilename << "failed";

    // Rename temporary file to new database
    if(QFile::rename(dbFilename, selectedFilename))
      qInfo() << "Renamed" << dbFilename << "to" << selectedFilename;
    else
      qWarning() << "Renaming" << dbFilename << "to" << selectedFilename << "failed";

    // Syncronize display with loaded database
    currentFsType = selectedFsType;

    if(navDatabaseAuto)
      // Correct navdata selection automatically if enabled
      assignSceneryCorrection();
    else
      // Assign navdata status from remembered value from simulator
      navDatabaseStatus = simulators.value(currentFsType).navDatabaseStatus;

    openAllDatabases();

    clearLanguageIndex();
    loadLanguageIndex();

    clearAircraftIndex();
    loadAircraftIndex();

    // Notify all objects in program on change
    emit postDatabaseLoad(currentFsType);
  }
  else
  {
    // Failed or canceled - remove compiled database ============================================
    reopenDialog = true;

    if(QFile::remove(dbFilename))
      qInfo() << "Removed" << dbFilename;
    else
      qWarning() << "Removing" << dbFilename << "failed";

    QFile journal2(dbFilename + "-journal");
    if(journal2.exists() && journal2.size() == 0)
    {
      if(journal2.remove())
        qInfo() << "Removed" << journal2.fileName();
      else
        qWarning() << "Removing" << journal2.fileName() << "failed";
    }
  }

  // Check databases and simulator selection - show warning dialogs if something is odd
  loadSceneryPost();

  if(reopenDialog)
    // Failed or canceled - reopen dialog in event queue to avoid recursion
    QTimer::singleShot(0, this, &DatabaseManager::loadSceneryInternal);
}

/* Simulator was changed in scenery database loading dialog */
void DatabaseManager::simulatorChangedFromComboBox(FsPaths::SimulatorType value)
{
  selectedFsType = value;
  updateDialogInfo(selectedFsType);
}

bool DatabaseManager::hasDataInSimDatabase()
{
  return databaseMetadataFromType(currentFsType).hasData();
}

/* Checks if the current database contains data. Exits program if this fails */
bool DatabaseManager::hasData(atools::sql::SqlDatabase *db) const
{
  try
  {
    return DatabaseMeta(db).hasData();
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

/* Checks if the current database is compatible with this program. Exits program if this fails */
bool DatabaseManager::isDatabaseCompatible(atools::sql::SqlDatabase *db) const
{
  try
  {
    return DatabaseMeta(db).isDatabaseCompatible();
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

void DatabaseManager::saveState()
{
  Settings& settings = Settings::instance();
  settings.setValueVar(lnm::DATABASE_PATHS, QVariant::fromValue(simulators));
  settings.setValue(lnm::DATABASE_SIMULATOR, atools::fs::FsPaths::typeToShortName(currentFsType));
  settings.setValue(lnm::DATABASE_LOADINGSIMULATOR, atools::fs::FsPaths::typeToShortName(selectedFsType));
  settings.setValue(lnm::DATABASE_LOAD_INACTIVE, readInactive);
  settings.setValue(lnm::DATABASE_LOAD_ADDONXML, readAddOnXml);
  settings.setValue(lnm::DATABASE_NAVDB_STATUS, static_cast<int>(navDatabaseStatus));
  settings.setValue(lnm::DATABASE_NAVDB_AUTO, navDatabaseAuto);
}

void DatabaseManager::restoreState()
{
  const Settings& settings = Settings::instance();
  simulators = settings.valueVar(lnm::DATABASE_PATHS).value<SimulatorTypeMap>();
  currentFsType = atools::fs::FsPaths::stringToType(settings.valueStr(lnm::DATABASE_SIMULATOR, QString()));
  selectedFsType = atools::fs::FsPaths::stringToType(settings.valueStr(lnm::DATABASE_LOADINGSIMULATOR));
  readInactive = settings.valueBool(lnm::DATABASE_LOAD_INACTIVE, false);
  readAddOnXml = settings.valueBool(lnm::DATABASE_LOAD_ADDONXML, true);
  navDatabaseStatus = static_cast<navdb::Status>(settings.valueInt(lnm::DATABASE_NAVDB_STATUS, navdb::MIXED));
  navDatabaseAuto = settings.valueBool(lnm::DATABASE_NAVDB_AUTO, true);
}

/* Updates metadata, version and object counts in the scenery loading dialog */
void DatabaseManager::updateDialogInfo(atools::fs::FsPaths::SimulatorType value)
{
  Q_ASSERT(mainWindow != nullptr);

  QString metaText;

  QString databaseFile = buildDatabaseFileName(value);
  SqlDatabase tempDb(dbtools::DATABASE_NAME_DLG_INFO_TEMP);

  if(QFileInfo::exists(databaseFile))
  { // Open temp database to show statistics
    tempDb.setDatabaseName(databaseFile);
    tempDb.setReadonly();
    tempDb.open();
  }

  atools::util::Version applicationVersion = DatabaseMeta::getApplicationVersion();
  if(tempDb.isOpen())
  {
    DatabaseMeta dbmeta(tempDb);
    atools::util::Version databaseVersion = dbmeta.getDatabaseVersion();

    if(!dbmeta.isValid())
      metaText = databaseMetaText.arg(tr("None")).arg(tr("None")).arg(applicationVersion.getVersionString()).arg(QString());
    else
    {
      QString cycleText;
      if(!dbmeta.getAiracCycle().isEmpty())
        cycleText = databaseAiracCycleText.arg(dbmeta.getAiracCycle());

      metaText = databaseMetaText.
                 arg(dbmeta.getLastLoadTime().isValid() ? dbmeta.getLastLoadTime().toString() : tr("None")).
                 arg(databaseVersion.getVersionString()).arg(applicationVersion.getVersionString()).arg(cycleText);
    }
  }
  else
    metaText = databaseMetaText.arg(tr("None")).arg(tr("None")).arg(applicationVersion.getVersionString()).arg(QString());

  QString tableText, databaseInfoText = databaseLoader->getDatabaseInfoText();
  if(tempDb.isOpen() && dbtools::hasSchema(&tempDb))
  {
    atools::sql::SqlUtil util(tempDb);

    // Get row counts for the dialog
    tableText = databaseInfoText.arg(util.rowCount("bgl_file")).
                arg(util.rowCount("airport")).
                arg(util.rowCount("vor")).
                arg(util.rowCount("ils")).
                arg(util.rowCount("ndb")).
                arg(util.rowCount("marker")).
                arg(util.rowCount("waypoint")).
                arg(util.rowCount("boundary"));
  }
  else
    tableText = databaseInfoText.arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0);

  const OptionData& options = OptionData::instance();
  QStringList optionsHeader;
  if(!options.getDatabaseInclude().isEmpty())
    optionsHeader.append(tr("%1 %2 included for loading").
                         arg(options.getDatabaseInclude().size()).
                         arg(options.getDatabaseInclude().size() > 1 ? tr("extra directories are") : tr("extra directory is")));
  if(!options.getDatabaseExclude().isEmpty())
    optionsHeader.append(tr("%1 %2 excluded from loading").
                         arg(options.getDatabaseExclude().size()).
                         arg(options.getDatabaseExclude().size() > 1 ? tr("directories are") : tr("directory is")));
  if(!options.getDatabaseAddonExclude().isEmpty())
    optionsHeader.append(tr("%1 %2 excluded from add-on detection").
                         arg(options.getDatabaseAddonExclude().size()).
                         arg(options.getDatabaseAddonExclude().size() > 1 ? tr("directories are") : tr("directory is")));

  if(!optionsHeader.isEmpty())
  {
    optionsHeader = QStringList(atools::strJoin(tr("<b>Note:</b> "), optionsHeader, tr(", "), tr(" and "), tr(".")));
    optionsHeader.append(tr("Included and excluded directories can be changed in options on page \"Scenery Library Database\"."));
  }

  databaseDialog->setHeader(metaText +
                            atools::strJoin(tr("<p>"), optionsHeader, tr("<br/>"), tr("<br/>"), tr("</p>")) +
                            tr("<p><big>Currently Loaded:</big></p><p>%1</p>").arg(tableText));

  if(tempDb.isOpen())
    tempDb.close();
}

/* Create database name including simulator short name */
QString DatabaseManager::buildDatabaseFileName(atools::fs::FsPaths::SimulatorType type) const
{
  return databaseDirectory +
         QDir::separator() + lnm::DATABASE_PREFIX +
         atools::fs::FsPaths::typeToShortName(type).toLower() + lnm::DATABASE_SUFFIX;
}

/* Create database name including simulator short name in application directory */
QString DatabaseManager::buildDatabaseFileNameAppDir(atools::fs::FsPaths::SimulatorType type) const
{
  return QCoreApplication::applicationDirPath() +
         QDir::separator() + lnm::DATABASE_DIR +
         QDir::separator() + lnm::DATABASE_PREFIX +
         atools::fs::FsPaths::typeToShortName(type).toLower() + lnm::DATABASE_SUFFIX;
}

QString DatabaseManager::buildCompilingDatabaseFileName() const
{
  return databaseDirectory + QDir::separator() + lnm::DATABASE_PREFIX + "compiling" + lnm::DATABASE_SUFFIX;
}

void DatabaseManager::freeActions()
{
  if(menuDbSeparator != nullptr)
  {
    menuDbSeparator->deleteLater();
    menuDbSeparator = nullptr;
  }
  if(menuNavDbSeparator != nullptr)
  {
    menuNavDbSeparator->deleteLater();
    menuNavDbSeparator = nullptr;
  }
  if(simDbGroup != nullptr)
  {
    simDbGroup->deleteLater();
    simDbGroup = nullptr;
  }
  if(navDbActionAll != nullptr)
  {
    navDbActionAll->deleteLater();
    navDbActionAll = nullptr;
  }
  if(navDbActionMixed != nullptr)
  {
    navDbActionMixed->deleteLater();
    navDbActionMixed = nullptr;
  }
  if(navDbActionOff != nullptr)
  {
    navDbActionOff->deleteLater();
    navDbActionOff = nullptr;
  }
  if(navDbSubMenu != nullptr)
  {
    navDbSubMenu->deleteLater();
    navDbSubMenu = nullptr;
  }
  if(navDbGroup != nullptr)
  {
    navDbGroup->deleteLater();
    navDbGroup = nullptr;
  }
  for(QAction *action : qAsConst(simDbActions))
    action->deleteLater();
  simDbActions.clear();
}

/* Uses the simulator map copy from the dialog to update the changed paths */
void DatabaseManager::updateSimulatorPathsFromDialog()
{
  const SimulatorTypeMap& dlgPaths = databaseDialog->getPaths();

  for(auto it = dlgPaths.constBegin(); it != dlgPaths.constEnd(); ++it)
  {
    const FsPaths::SimulatorType type = it.key();
    if(simulators.contains(type))
    {
      simulators[type].basePath = dlgPaths.value(type).basePath;
      simulators[type].sceneryCfg = dlgPaths.value(type).sceneryCfg;
    }
  }
}

/* Updates the flags for installed simulators and removes all entries where neither database
 * not simulator installation was found */
void DatabaseManager::updateSimulatorFlags()
{
  for(atools::fs::FsPaths::SimulatorType type : FsPaths::getAllSimulatorTypes())
    // Already present or not - update database status since file exists
    simulators[type].hasDatabase = QFile::exists(buildDatabaseFileName(type));
}

void DatabaseManager::correctSimulatorType()
{
  if(currentFsType == atools::fs::FsPaths::NONE ||
     (!simulators.value(currentFsType).hasDatabase && !simulators.value(currentFsType).isInstalled))
  {
    currentFsType = simulators.getBest();
    navDatabaseStatus = simulators[currentFsType].navDatabaseStatus;
  }

  if(currentFsType == atools::fs::FsPaths::NONE)
  {
    currentFsType = simulators.getBestInstalled();
    navDatabaseStatus = simulators[currentFsType].navDatabaseStatus;
  }

  // Correct if loading simulator is invalid - get the best installed
  if(selectedFsType == atools::fs::FsPaths::NONE || !simulators.getAllInstalled().contains(selectedFsType))
  {
    selectedFsType = simulators.getBestInstalled();
    navDatabaseStatus = simulators[currentFsType].navDatabaseStatus;
  }
}

void DatabaseManager::checkDatabaseVersion()
{
  const DatabaseMeta *databaseMetaSim = NavApp::getDatabaseMetaSim();
  if(navDatabaseStatus != navdb::ALL && databaseMetaSim != nullptr && databaseMetaSim->hasData())
  {
    QStringList msg;
    if(databaseMetaSim->getDatabaseVersion() < DatabaseMeta::getApplicationVersion())
      msg.append(tr("The scenery library database was created using a previous version of Little Navmap."));

    if(databaseMetaSim->getLastLoadTime() < QDateTime::currentDateTime().addMonths(-MAX_AGE_DAYS))
    {
      qint64 days = databaseMetaSim->getLastLoadTime().date().daysTo(QDate::currentDate());
      msg.append(tr("Scenery library database was not reloaded for more than %1 days.").arg(days));
    }

    if(!msg.isEmpty())
    {
      qDebug() << Q_FUNC_INFO << msg;

      int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_DATABASE_OLD,
                                              tr("<p>%1</p>"
                                                   "<p>It is advised to reload the scenery library database after each Little Navmap update, "
                                                     "after installing new add-on scenery or after a flight simulator update to "
                                                     "enable new features or benefit from bug fixes.</p>"
                                                     "<p>You can do this in menu \"Scenery Library\" -> "
                                                       "\"Load Scenery Library\".</p>"
                                                       "<p>Open the \"Load Scenery Library\" dialog window now?</p>").
                                              arg(msg.join(tr("<br/>"))),
                                              tr("Do not &show this dialog again."), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes,
                                              QMessageBox::No);

      if(result == QMessageBox::Yes)
        loadScenery();
    }
  }
}
