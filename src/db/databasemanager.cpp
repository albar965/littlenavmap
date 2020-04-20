/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "gui/textdialog.h"
#include "gui/application.h"
#include "options/optiondata.h"
#include "common/constants.h"
#include "fs/db/databasemeta.h"
#include "db/databasedialog.h"
#include "settings/settings.h"
#include "fs/navdatabaseoptions.h"
#include "fs/navdatabaseprogress.h"
#include "common/formatter.h"
#include "fs/fspaths.h"
#include "fs/xp/scenerypacks.h"
#include "gui/helphandler.h"
#include "fs/navdatabase.h"
#include "sql/sqlutil.h"
#include "sql/sqltransaction.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "navapp.h"
#include "gui/dialog.h"
#include "fs/userdata/userdatamanager.h"
#include "fs/userdata/logdatamanager.h"
#include "fs/online/onlinedatamanager.h"
#include "io/fileroller.h"
#include "atools.h"
#include "sql/sqlexception.h"
#include "track/trackmanager.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressDialog>
#include <QApplication>
#include <QFileInfo>
#include <QList>
#include <QMenu>
#include <QDir>
#include <QMessageBox>
#include <QAbstractButton>
#include <QSettings>
#include <QSplashScreen>

using atools::gui::ErrorHandler;
using atools::sql::SqlUtil;
using atools::fs::FsPaths;
using atools::fs::NavDatabaseOptions;
using atools::fs::NavDatabase;
using atools::settings::Settings;
using atools::sql::SqlDatabase;
using atools::sql::SqlTransaction;
using atools::sql::SqlQuery;
using atools::fs::db::DatabaseMeta;

const int MAX_ERROR_BGL_MESSAGES = 400;
const int MAX_ERROR_SCENERY_MESSAGES = 400;
const int MAX_TEXT_LENGTH = 120;

DatabaseManager::DatabaseManager(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  databaseMetaText = QObject::tr(
    "<p><big>Last Update: %1. Database Version: %2.%3. Program Version: %4.%5.%6</big></p>");

  databaseAiracCycleText = QObject::tr(" AIRAC Cycle %1.");

  databaseInfoText = QObject::tr("<table>"
                                   "<tbody>"
                                     "<tr> "
                                       "<td width=\"60\"><b>Files:</b>"
                                       "</td>    "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L6"
                                       "</td> "
                                       "<td width=\"60\"><b>VOR:</b>"
                                       "</td> "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L8"
                                       "</td> "
                                       "<td width=\"60\"><b>Markers:</b>"
                                       "</td>     "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L11"
                                       "</td>"
                                     "</tr>"
                                     "<tr> "
                                       "<td width=\"60\"><b>Airports:</b>"
                                       "</td> "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L7"
                                       "</td> "
                                       "<td width=\"60\"><b>ILS:</b>"
                                       "</td> "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L9"
                                       "</td> "
                                       "<td width=\"60\"><b>Waypoints:</b>"
                                       "</td>  "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L12"
                                       "</td>"
                                     "</tr>"
                                     "<tr> "
                                       "<td width=\"60\">"
                                       "</td>"
                                       "<td width=\"60\">"
                                       "</td>"
                                       "<td width=\"60\"><b>NDB:</b>"
                                       "</td> "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L10"
                                       "</td> "
                                       "<td width=\"60\"><b>Airspaces:</b>"
                                       "</td>  "
                                       "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%L13"
                                       "</td>"
                                     "</tr>"
                                   "</tbody>"
                                 "</table>"
                                 );

  databaseTimeText = QObject::tr(
    "<b>%1</b><br/><br/><br/>"
    "<b>Time:</b> %2<br/>%3%4"
      "<b>Errors:</b> %5<br/><br/>"
      "<big>Found:</big></br>"
    ) + databaseInfoText;

  databaseLoadingText = QObject::tr(
    "<b>Scenery:</b> %1 (%2)<br/>"
    "<b>File:</b> %3<br/><br/>"
    "<b>Time:</b> %4<br/>"
    "<b>Errors:</b> %5<br/><br/>"
    "<big>Found:</big></br>"
    ) + databaseInfoText;

  // Also loads list of simulators
  restoreState();

  databaseDirectory = Settings::getPath() + QDir::separator() + lnm::DATABASE_DIR;
  if(!QDir().mkpath(databaseDirectory))
    qWarning() << "Cannot create db dir" << databaseDirectory;

  QString name = buildDatabaseFileName(FsPaths::NAVIGRAPH);
  if(name.isEmpty() && !QFile::exists(name))
    // Set to off if not database found
    navDatabaseStatus = dm::NAVDATABASE_OFF;

  // Find simulators by default registry entries
  simulators.fillDefault();

  // Find any stale databases that do not belong to a simulator and update installed and has database flags
  updateSimulatorFlags();

  for(atools::fs::FsPaths::SimulatorType t : simulators.keys())
    qDebug() << t << simulators.value(t);

  // Correct if current simulator is invalid
  correctSimulatorType();

  qDebug() << "fs type" << currentFsType;

  if(mainWindow != nullptr)
  {
    databaseDialog = new DatabaseDialog(mainWindow, simulators);
    databaseDialog->setReadInactive(readInactive);
    databaseDialog->setReadAddOnXml(readAddOnXml);

    connect(databaseDialog, &DatabaseDialog::simulatorChanged, this, &DatabaseManager::simulatorChangedFromComboBox);
  }

  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_SIM);
  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_NAV);
  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_MORA);
  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_DLG_INFO_TEMP);
  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_TEMP);

  databaseSim = new SqlDatabase(DATABASE_NAME_SIM);
  databaseNav = new SqlDatabase(DATABASE_NAME_NAV);
  databaseMora = new SqlDatabase(DATABASE_NAME_MORA);

  if(mainWindow != nullptr)
  {
    // Open only for instantiation in main window and not in main function
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_USER);
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_TRACK);
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_LOGBOOK);
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_ONLINE);

    // Airspace databases
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_USER_AIRSPACE);
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_SIM_AIRSPACE);
    SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_NAV_AIRSPACE);

    // Variable databases (user can edit or program downloads data)
    databaseUser = new SqlDatabase(DATABASE_NAME_USER);
    databaseTrack = new SqlDatabase(DATABASE_NAME_TRACK);
    databaseLogbook = new SqlDatabase(DATABASE_NAME_LOGBOOK);
    databaseOnline = new SqlDatabase(DATABASE_NAME_ONLINE);

    // Airspace databases
    databaseUserAirspace = new SqlDatabase(DATABASE_NAME_USER_AIRSPACE);

    // ... as duplicate connections to sim and nav databases but independent of nav switch
    databaseSimAirspace = new SqlDatabase(DATABASE_NAME_SIM_AIRSPACE);
    databaseNavAirspace = new SqlDatabase(DATABASE_NAME_NAV_AIRSPACE);

    // Open user point database =================================
    openWriteableDatabase(databaseUser, "userdata", "user", true /* backup */);
    userdataManager = new atools::fs::userdata::UserdataManager(databaseUser);
    if(!userdataManager->hasSchema())
      userdataManager->createSchema();
    else
      userdataManager->updateSchema();

    // Open logbook database =================================
    openWriteableDatabase(databaseLogbook, "logbook", "logbook", true /* backup */);
    logdataManager = new atools::fs::userdata::LogdataManager(databaseLogbook);
    if(!logdataManager->hasSchema())
      logdataManager->createSchema();
    else
      logdataManager->updateSchema();

    // Open user airspace database =================================
    openWriteableDatabase(databaseUserAirspace, "userairspace", "userairspace", false /* backup */);
    if(!SqlUtil(databaseUserAirspace).hasTable("boundary"))
    {
      SqlTransaction transaction(databaseUserAirspace);
      // Create schema on demand
      createEmptySchema(databaseUserAirspace, true /* boundary only */);
      transaction.commit();
    }

    // Open track database =================================
    openWriteableDatabase(databaseTrack, "track", "track", false /* backup */);
    trackManager = new TrackManager(databaseTrack, databaseNav);
    if(!trackManager->hasSchema())
      trackManager->createSchema();
    else
      trackManager->updateSchema();

    // Open online network database ==============================
    atools::settings::Settings& settings = atools::settings::Settings::instance();
    bool verbose = settings.getAndStoreValue(lnm::OPTIONS_WHAZZUP_PARSER_DEBUG, false).toBool();

    openWriteableDatabase(databaseOnline, "onlinedata", "online network", false /* backup */);
    onlinedataManager = new atools::fs::online::OnlinedataManager(databaseOnline, verbose);
    onlinedataManager->createSchema();
    onlinedataManager->initQueries();
  }
}

DatabaseManager::~DatabaseManager()
{
  // Delete simulator switch actions
  freeActions();

  delete databaseDialog;
  delete progressDialog;
  delete userdataManager;
  delete trackManager;
  delete logdataManager;
  delete onlinedataManager;

  closeAllDatabases();
  closeUserDatabase();
  closeTrackDatabase();
  closeLogDatabase();
  closeUserAirspaceDatabase();
  closeOnlineDatabase();

  delete databaseSim;
  delete databaseNav;
  delete databaseMora;
  delete databaseUser;
  delete databaseTrack;
  delete databaseLogbook;
  delete databaseOnline;
  delete databaseUserAirspace;
  delete databaseSimAirspace;
  delete databaseNavAirspace;

  SqlDatabase::removeDatabase(DATABASE_NAME_SIM);
  SqlDatabase::removeDatabase(DATABASE_NAME_NAV);
  SqlDatabase::removeDatabase(DATABASE_NAME_MORA);
  SqlDatabase::removeDatabase(DATABASE_NAME_USER);
  SqlDatabase::removeDatabase(DATABASE_NAME_TRACK);
  SqlDatabase::removeDatabase(DATABASE_NAME_LOGBOOK);
  SqlDatabase::removeDatabase(DATABASE_NAME_DLG_INFO_TEMP);
  SqlDatabase::removeDatabase(DATABASE_NAME_TEMP);
  SqlDatabase::removeDatabase(DATABASE_NAME_USER_AIRSPACE);
  SqlDatabase::removeDatabase(DATABASE_NAME_SIM_AIRSPACE);
  SqlDatabase::removeDatabase(DATABASE_NAME_NAV_AIRSPACE);
}

bool DatabaseManager::checkIncompatibleDatabases(bool *databasesErased)
{
  bool ok = true;

  if(databasesErased != nullptr)
    *databasesErased = false;

  // Need empty block to delete sqlDb before removing driver
  {
    // Create a temporary database
    SqlDatabase sqlDb(DATABASE_NAME_TEMP);
    QStringList databaseNames, databaseFiles;

    // Collect all incompatible databases
    for(atools::fs::FsPaths::SimulatorType type : simulators.keys())
    {
      QString dbName = buildDatabaseFileName(type);
      if(QFile::exists(dbName))
      {
        // Database file exists
        sqlDb.setDatabaseName(dbName);
        sqlDb.open();

        DatabaseMeta meta(&sqlDb);
        if(!meta.hasSchema())
          // No schema create an empty one anyway
          createEmptySchema(&sqlDb);
        else if(!meta.isDatabaseCompatible())
        {
          // Not compatible add to list
          databaseNames.append("<i>" + FsPaths::typeToName(type) + "</i>");
          databaseFiles.append(dbName);
          qWarning() << "Incompatible database" << dbName;
        }
        sqlDb.close();
      }
    }

    // Delete the dummy database without dialog if needed
    QString dummyName = buildDatabaseFileName(atools::fs::FsPaths::UNKNOWN);
    sqlDb.setDatabaseName(dummyName);
    sqlDb.open();
    DatabaseMeta meta(&sqlDb);
    if(!meta.hasSchema() || !meta.isDatabaseCompatible())
    {
      qDebug() << "Updating dummy database" << dummyName;
      createEmptySchema(&sqlDb);
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
      NavApp::deleteSplashScreen();

      QMessageBox box(QMessageBox::Question, QApplication::applicationName(),
                      msg.arg(databaseNames.join("<br/>")).arg(trailingMsg),
                      QMessageBox::No | QMessageBox::Yes,
                      mainWindow);
      box.button(QMessageBox::No)->setText(tr("&No and Exit Application"));
      box.button(QMessageBox::Yes)->setText(tr("&Erase"));

      int result = box.exec();

      if(result == QMessageBox::No)
        // User does not want to erase incompatible databases - exit application
        ok = false;
      else if(result == QMessageBox::Yes)
      {
        QMessageBox *dialog = showSimpleProgressDialog(tr("Deleting ..."));

        int i = 0;
        for(const QString& dbfile : databaseFiles)
        {
          dialog->setText(tr("Erasing database for %1 ...").arg(databaseNames.at(i)));
          atools::gui::Application::processEventsExtended();

          if(QFile::remove(dbfile))
          {
            qInfo() << "Removed" << dbfile;

            // Create new database
            sqlDb.setDatabaseName(dbfile);
            sqlDb.open();
            createEmptySchema(&sqlDb);
            sqlDb.close();

            if(databasesErased != nullptr)
              *databasesErased = true;
          }
          else
          {
            qWarning() << "Removing database failed" << dbfile;
            atools::gui::Dialog::warning(nullptr,
                                         tr("Deleting of database<br/><br/>\"%1\"<br/><br/>failed.<br/><br/>"
                                            "Remove the database file manually and restart the program.").arg(dbfile));
            ok = false;
          }
          i++;
        }
        QGuiApplication::restoreOverrideCursor();

        deleteSimpleProgressDialog(dialog);
      }
    }
  }
  return ok;
}

void DatabaseManager::deleteSimpleProgressDialog(QMessageBox *messageBox)
{
  messageBox->close();
  messageBox->deleteLater();

  QGuiApplication::restoreOverrideCursor();
}

QMessageBox *DatabaseManager::showSimpleProgressDialog(const QString& message)
{
  // Avoid the splash screen hiding the dialog
  NavApp::deleteSplashScreen();

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  QMessageBox *progressBox = new QMessageBox(QMessageBox::NoIcon, QApplication::applicationName(), message);
  progressBox->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
  progressBox->setStandardButtons(QMessageBox::NoButton);
  progressBox->show();
  atools::gui::Application::processEventsExtended();
  return progressBox;
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
    metaFromFile(&appCycle, &appLastLoad, nullptr, &appSource, appDb);
    hasApp = true;
  }

  if(QFile::exists(settingsDb))
  {
    // Database in settings directory
    metaFromFile(&settingsCycle, &settingsLastLoad, &settingsNeedsPreparation, &settingsSource, settingsDb);
    hasSettings = true;
  }
  int appCycleNum = appCycle.toInt();
  int settingsCycleNum = settingsCycle.toInt();

  qInfo() << Q_FUNC_INFO << "settings database" << settingsDb << settingsLastLoad << settingsCycle;
  qInfo() << Q_FUNC_INFO << "app database" << appDb << appLastLoad << appCycle;
  qInfo() << Q_FUNC_INFO << "hasApp" << hasApp
          << "hasSettings" << hasSettings
          << "settingsNeedsPreparation" << settingsNeedsPreparation;

  if(hasApp)
  {
    int result = QMessageBox::Yes;

    // Compare cycles first and then compilation time
    if(appCycleNum > settingsCycleNum || (appCycleNum == settingsCycleNum && appLastLoad > settingsLastLoad))
    {
      if(hasSettings)
      {
        NavApp::deleteSplashScreen();
        result = atools::gui::Dialog(nullptr).showQuestionMsgBox(
          lnm::ACTIONS_SHOW_OVERWRITE_DATABASE,
          tr("Your current navdata is older than the navdata included in the Little Navmap download archive.<br/><br/>"
             "Overwrite your current navdata file with the new one?"
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
          tr("Do not &show this dialog again and skip copying in the future."),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No, QMessageBox::No);
      }

      if(result == QMessageBox::Yes)
      {
        // We have a database in the application folder and it is newer than the one in the settings folder
        QMessageBox *dialog =
          showSimpleProgressDialog(tr("Preparing %1 Database ...").arg(FsPaths::typeToName(FsPaths::NAVIGRAPH)));

        bool resultRemove = true, resultCopy = false;
        // Remove target
        if(hasSettings)
        {
          resultRemove = QFile(settingsDb).remove();
          qDebug() << "removed" << settingsDb << resultRemove;
        }

        // Copy to target
        if(resultRemove)
        {
          dialog->setText(tr("Preparing %1 Database: Copying file ...").arg(FsPaths::typeToName(FsPaths::NAVIGRAPH)));
          atools::gui::Application::processEventsExtended();
          resultCopy = QFile(appDb).copy(settingsDb);
          qDebug() << "copied" << appDb << "to" << settingsDb << resultCopy;
        }

        // Create indexes and delete script afterwards
        if(resultRemove && resultCopy)
        {
          SqlDatabase tempDb(DATABASE_NAME_TEMP);
          openDatabaseFile(&tempDb, settingsDb, false /* readonly */, true /* createSchema */);
          dialog->setText(tr("Preparing %1 Database: Creating indexes ...").arg(FsPaths::typeToName(FsPaths::NAVIGRAPH)));
          atools::gui::Application::processEventsExtended();
          NavDatabase::runPreparationScript(tempDb);

          dialog->setText(tr("Preparing %1 Database: Analyzing ...").arg(FsPaths::typeToName(FsPaths::NAVIGRAPH)));
          atools::gui::Application::processEventsExtended();
          tempDb.analyze();
          closeDatabaseFile(&tempDb);
          settingsNeedsPreparation = false;
        }
        deleteSimpleProgressDialog(dialog);

        if(!resultRemove)
          atools::gui::Dialog::warning(nullptr,
                                       tr("Deleting of database<br/><br/>\"%1\"<br/><br/>failed.<br/><br/>"
                                          "Remove the database file manually and restart the program.").arg(settingsDb));

        if(!resultCopy)
          atools::gui::Dialog::warning(nullptr,
                                       tr("Cannot copy database<br/><br/>\"%1\"<br/><br/>to<br/><br/>"
                                          "\"%2\"<br/><br/>.").arg(appDb).arg(settingsDb));
      }
    }
  }

  if(settingsNeedsPreparation && hasSettings)
  {
    QMessageBox *dialog =
      showSimpleProgressDialog(tr("Preparing %1 Database ...").arg(FsPaths::typeToName(FsPaths::NAVIGRAPH)));

    SqlDatabase tempDb(DATABASE_NAME_TEMP);
    openDatabaseFile(&tempDb, settingsDb, false /* readonly */, true /* createSchema */);
    NavDatabase::runPreparationScript(tempDb);
    tempDb.analyze();
    closeDatabaseFile(&tempDb);

    deleteSimpleProgressDialog(dialog);
  }
}

QString DatabaseManager::getCurrentSimulatorBasePath() const
{
  return getSimulatorBasePath(currentFsType);
}

QString DatabaseManager::getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type) const
{
  return simulators.value(type).basePath;
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
  for(atools::fs::FsPaths::SimulatorType type : keys)
  {
    const FsPathType& pathType = simulators.value(type);

    if(type == FsPaths::XPLANE11)
    {
      if(!pathType.basePath.isEmpty() || pathType.hasDatabase)
        sims.append(type);
    }
    else if(pathType.isInstalled || pathType.hasDatabase)
      // Create an action for each simulator installation or database found
      sims.append(type);
  }

  int index = 1;
  for(atools::fs::FsPaths::SimulatorType type : sims)
    insertSimSwitchAction(type, ui->actionDatabaseFiles, ui->menuDatabase, index++);

  if(!sims.isEmpty())
    menuDbSeparator = ui->menuDatabase->insertSeparator(ui->actionDatabaseFiles);

  if(actions.size() == 1)
    // Noting to select if there is only one option
    actions.first()->setDisabled(true);

  QString file = buildDatabaseFileName(FsPaths::NAVIGRAPH);

  if(!file.isEmpty())
  {
    QString cycle, source;
    QDateTime date;
    metaFromFile(&cycle, &date, nullptr, &source, file);

    if(!cycle.isEmpty())
      cycle = tr(" - AIRAC Cycle %1").arg(cycle);

#ifdef DEBUG_INFORMATION
    cycle += " (" + date.toString() + " | " + source + ")";
#endif

    QString dbname = FsPaths::typeToName(FsPaths::NAVIGRAPH);
    navDbSubMenu = new QMenu("&" + QString::number(index) + " " + dbname + cycle);
    navDbGroup = new QActionGroup(navDbSubMenu);

    navDbActionAll = new QAction(tr("Use %1 for &all Features").arg(dbname), navDbSubMenu);
    navDbActionAll->setCheckable(true);
    navDbActionAll->setChecked(navDatabaseStatus == dm::NAVDATABASE_ALL);
    navDbActionAll->setStatusTip(tr("Use all of %1 database features").arg(dbname));
    navDbActionAll->setActionGroup(navDbGroup);
    navDbSubMenu->addAction(navDbActionAll);

    navDbActionBlend = new QAction(tr("Use %1 for &Navaids and Procedures").arg(dbname), navDbSubMenu);
    navDbActionBlend->setCheckable(true);
    navDbActionBlend->setChecked(navDatabaseStatus == dm::NAVDATABASE_MIXED);
    navDbActionBlend->setStatusTip(tr("Use only navaids, airways, airspaces and procedures from %1 database").arg(
                                     dbname));
    navDbActionBlend->setActionGroup(navDbGroup);
    navDbSubMenu->addAction(navDbActionBlend);

    navDbActionOff = new QAction(tr("Do &not use %1 database").arg(dbname), navDbSubMenu);
    navDbActionOff->setCheckable(true);
    navDbActionOff->setChecked(navDatabaseStatus == dm::NAVDATABASE_OFF);
    navDbActionOff->setStatusTip(tr("Do not use %1 database").arg(dbname));
    navDbActionOff->setActionGroup(navDbGroup);
    navDbSubMenu->addAction(navDbActionOff);

    ui->menuDatabase->insertMenu(ui->actionDatabaseFiles, navDbSubMenu);
    menuNavDbSeparator = ui->menuDatabase->insertSeparator(ui->actionDatabaseFiles);

    connect(navDbActionAll, &QAction::triggered, this, &DatabaseManager::switchNavFromMainMenu);
    connect(navDbActionBlend, &QAction::triggered, this, &DatabaseManager::switchNavFromMainMenu);
    connect(navDbActionOff, &QAction::triggered, this, &DatabaseManager::switchNavFromMainMenu);
  }
}

void DatabaseManager::insertSimSwitchAction(atools::fs::FsPaths::SimulatorType type, QAction *before, QMenu *menu,
                                            int index)
{
  QString cycle;
  if(type == FsPaths::XPLANE11)
  {
    metaFromFile(&cycle, nullptr, nullptr, nullptr, buildDatabaseFileName(type));
    if(!cycle.isEmpty())
      cycle = tr(" - AIRAC Cycle %1").arg(cycle);
  }

  QAction *action = new QAction("&" + QString::number(index) + " " + FsPaths::typeToName(type) + cycle, menu);
  action->setStatusTip(tr("Switch to %1 database").arg(FsPaths::typeToName(type)));
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

  actions.append(action);
}

/* User changed simulator in main menu */
void DatabaseManager::switchNavFromMainMenu()
{
  qDebug() << Q_FUNC_INFO;

  if(navDbActionAll->isChecked())
  {
    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineNavdatabasesUrl, lnm::helpLanguageOnline());
    QString message = tr(
      "<p>Note that airport information is limited in this mode.<br/>"
      "This means that aprons, taxiways, parking positions, runway surface information and other information is not available.</p>"
      "<p>Additionally, smaller airports might be missing.</p>"
        "<p>Runway layout might not match the runway layout in the simulator if you use stock or older airport scenery.</p>"
          "<p><b>Click the link below for more information:<br/><br/>"
          "<a href=\"%1\">Online Manual - Navigation Databases</a></b><br/></p>").arg(url.toString());

    atools::gui::Dialog(nullptr).showInfoMsgBox(lnm::ACTIONS_SHOW_NAVDATA_WARNING, message,
                                                QObject::tr("Do not &show this dialog again."));
  }

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  // Disconnect all queries
  emit preDatabaseLoad();

  closeAllDatabases();

  QString text;
  if(navDbActionAll->isChecked())
  {
    navDatabaseStatus = dm::NAVDATABASE_ALL;
    text = tr("Enabled all features for %1.");
  }
  else if(navDbActionBlend->isChecked())
  {
    navDatabaseStatus = dm::NAVDATABASE_MIXED;
    text = tr("Enabled navaids, airways, airspaces and procedures for %1.");
  }
  else if(navDbActionOff->isChecked())
  {
    navDatabaseStatus = dm::NAVDATABASE_OFF;
    text = tr("Disabled %1.");
  }
  qDebug() << Q_FUNC_INFO << "usingNavDatabase" << navDatabaseStatus;

  openAllDatabases();

  QGuiApplication::restoreOverrideCursor();

  emit postDatabaseLoad(currentFsType);

  mainWindow->setStatusMessage(text.arg(FsPaths::typeToName(FsPaths::NAVIGRAPH)));

  saveState();

}

void DatabaseManager::switchSimFromMainMenu()
{
  QAction *action = dynamic_cast<QAction *>(sender());

  qDebug() << Q_FUNC_INFO << (action != nullptr ? action->text() : "null");

  if(action != nullptr && currentFsType != action->data().value<atools::fs::FsPaths::SimulatorType>())
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // Disconnect all queries
    emit preDatabaseLoad();

    closeAllDatabases();

    // Set new simulator
    currentFsType = action->data().value<atools::fs::FsPaths::SimulatorType>();
    openAllDatabases();

    QGuiApplication::restoreOverrideCursor();

    // Reopen all with new database
    emit postDatabaseLoad(currentFsType);
    mainWindow->setStatusMessage(tr("Switched to %1.").arg(FsPaths::typeToName(currentFsType)));

    saveState();

  }

  // Check and uncheck manually since the QActionGroup is unreliable
  for(QAction *act : actions)
  {
    QSignalBlocker blocker(act);
    Q_UNUSED(blocker)
    act->setChecked(act->data().value<atools::fs::FsPaths::SimulatorType>() == currentFsType);
  }
}

void DatabaseManager::openWriteableDatabase(atools::sql::SqlDatabase *database, const QString& name,
                                            const QString& displayName, bool backup)
{
  QString databaseName = databaseDirectory +
                         QDir::separator() +
                         lnm::DATABASE_PREFIX +
                         name +
                         lnm::DATABASE_SUFFIX;

  QString databaseNameBackup = databaseDirectory +
                               QDir::separator() +
                               QFileInfo(databaseName).baseName() +
                               "_backup" +
                               lnm::DATABASE_SUFFIX;

  try
  {
    if(backup)
    {
      // Roll copies
      // .../ABarthel/little_navmap_db/little_navmap_userdata_backup.sqlite
      // .../ABarthel/little_navmap_db/little_navmap_userdata_backup.sqlite.1
      // .../ABarthel/little_navmap_db/little_navmap_userdata_backup.sqlite.2
      // .../ABarthel/little_navmap_db/little_navmap_userdata_backup.sqlite.3
      atools::io::FileRoller roller(3);
      roller.rollFile(databaseNameBackup);

      // Copy database before opening
      bool result = QFile(databaseName).copy(databaseNameBackup);
      qInfo() << Q_FUNC_INFO << "Copied" << databaseName << "to" << databaseNameBackup << "result" << result;
    }

    openDatabaseFileInternal(database, databaseName, false /* readonly */, false /* createSchema */,
                             false /* exclusive */, false /* auto transactions */);
  }
  catch(atools::sql::SqlException& e)
  {
    QMessageBox::critical(mainWindow, QApplication::applicationName(),
                          tr("Cannot open %1 database. Reason:\n\n"
                             "%2\n\n"
                             "Is another %3 running?\n\n"
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
  closeDatabaseFile(databaseUser);
}

void DatabaseManager::closeTrackDatabase()
{
  closeDatabaseFile(databaseTrack);
}

void DatabaseManager::closeUserAirspaceDatabase()
{
  closeDatabaseFile(databaseUserAirspace);
}

void DatabaseManager::closeLogDatabase()
{
  closeDatabaseFile(databaseLogbook);
}

void DatabaseManager::closeOnlineDatabase()
{
  closeDatabaseFile(databaseOnline);
}

void DatabaseManager::openAllDatabases()
{
  QString simDbFile = buildDatabaseFileName(currentFsType);
  QString navDbFile = buildDatabaseFileName(FsPaths::NAVIGRAPH);
  QString moraDbFile = buildDatabaseFileName(FsPaths::NAVIGRAPH);

  // Airspace databases are independent of switch
  QString simAirspaceDbFile = simDbFile;
  QString navAirspaceDbFile = navDbFile;

  if(navDatabaseStatus == dm::NAVDATABASE_ALL)
    simDbFile = navDbFile;
  else if(navDatabaseStatus == dm::NAVDATABASE_OFF)
    navDbFile = simDbFile;
  // else if(usingNavDatabase == MIXED)

  openDatabaseFile(databaseSim, simDbFile, true /* readonly */, true /* createSchema */);
  openDatabaseFile(databaseNav, navDbFile, true /* readonly */, true /* createSchema */);
  openDatabaseFile(databaseMora, moraDbFile, true /* readonly */, true /* createSchema */);

  openDatabaseFile(databaseSimAirspace, simAirspaceDbFile, true /* readonly */, true /* createSchema */);
  openDatabaseFile(databaseNavAirspace, navAirspaceDbFile, true /* readonly */, true /* createSchema */);
}

void DatabaseManager::openDatabaseFile(atools::sql::SqlDatabase *db, const QString& file, bool readonly,
                                       bool createSchema)
{
  try
  {
    openDatabaseFileInternal(db, file, readonly, createSchema, true /* exclusive */, true /* auto transactions */);
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

void DatabaseManager::openDatabaseFileInternal(atools::sql::SqlDatabase *db, const QString& file, bool readonly,
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

  qDebug() << "Opening database" << file;
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

  DatabaseMeta dbmeta(db);
  qInfo().nospace() << "Database version "
                    << dbmeta.getMajorVersion() << "." << dbmeta.getMinorVersion();

  qInfo().nospace() << "Application database version "
                    << DatabaseMeta::DB_VERSION_MAJOR << "." << DatabaseMeta::DB_VERSION_MINOR;
}

void DatabaseManager::closeAllDatabases()
{
  closeDatabaseFile(databaseSim);
  closeDatabaseFile(databaseNav);
  closeDatabaseFile(databaseMora);
  closeDatabaseFile(databaseSimAirspace);
  closeDatabaseFile(databaseNavAirspace);
}

void DatabaseManager::closeDatabaseFile(atools::sql::SqlDatabase *db)
{
  try
  {
    if(db != nullptr && db->isOpen())
    {
      qDebug() << "Closing database" << db->databaseName();
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

atools::sql::SqlDatabase *DatabaseManager::getDatabaseSim()
{
  return databaseSim;
}

atools::sql::SqlDatabase *DatabaseManager::getDatabaseNav()
{
  return databaseNav;
}

atools::sql::SqlDatabase *DatabaseManager::getDatabaseMora()
{
  return databaseMora;
}

atools::sql::SqlDatabase *DatabaseManager::getDatabaseSimAirspace()
{
  return databaseSimAirspace;
}

atools::sql::SqlDatabase *DatabaseManager::getDatabaseNavAirspace()
{
  return databaseNavAirspace;
}

void DatabaseManager::run()
{
  qDebug() << Q_FUNC_INFO;

  if(simulators.value(currentFsType).isInstalled)
    // Use what is currently displayed on the map
    selectedFsType = currentFsType;

  databaseDialog->setCurrentFsType(selectedFsType);
  databaseDialog->setReadInactive(readInactive);
  databaseDialog->setReadAddOnXml(readAddOnXml);

  updateDialogInfo(selectedFsType);

  // try until user hits cancel or the database was loaded successfully
  while(runInternal())
    ;

  updateSimulatorFlags();
  insertSimSwitchActions();

  saveState();
}

/* Shows scenery database loading dialog.
 * @return true if execution was successfull. false if it was cancelled */
bool DatabaseManager::runInternal()
{
  qDebug() << Q_FUNC_INFO;

  bool reopenDialog = true;
  try
  {
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
      bool configValid = true;
      QString err;
      if(!atools::fs::NavDatabase::isBasePathValid(databaseDialog->getBasePath(), err, selectedFsType))
      {
        atools::gui::Dialog::warning(databaseDialog, tr("Cannot read base path \"%1\". Reason: %2.").
                                     arg(databaseDialog->getBasePath()).arg(err));
        configValid = false;
      }

      if(selectedFsType == atools::fs::FsPaths::XPLANE11)
      {
        QString filepath;
        if(!readInactive && !atools::fs::xp::SceneryPacks::exists(databaseDialog->getBasePath(), err, filepath))
        {
          atools::gui::Dialog::warning(databaseDialog, tr("Cannot read scenery configuration \"%1\". Reason: %2.\n\n"
                                                          "Enable the option \"Read inactive or disabled Scenery Entries\" "
                                                          "or start X-Plane once to create the file.").
                                       arg(filepath).arg(err));
          configValid = false;
        }
      }
      else
      {
        QString sceneryCfgCodec = (selectedFsType == atools::fs::FsPaths::P3D_V4 ||
                                   selectedFsType == atools::fs::FsPaths::P3D_V5) ? "UTF-8" : QString();
        if(!atools::fs::NavDatabase::isSceneryConfigValid(databaseDialog->getSceneryConfigFile(), sceneryCfgCodec, err))
        {
          atools::gui::Dialog::warning(databaseDialog, tr("Cannot read scenery configuration \"%1\". Reason: %2.").
                                       arg(databaseDialog->getSceneryConfigFile()).arg(err));
          configValid = false;
        }
      }

      if(configValid)
      {
        // Compile into a temporary database file
        QString selectedFilename = buildDatabaseFileName(selectedFsType);
        QString tempFilename = buildCompilingDatabaseFileName();

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

        SqlDatabase tempDb(DATABASE_NAME_TEMP);
        openDatabaseFile(&tempDb, tempFilename, false /* readonly */, true /* createSchema */);

        if(loadScenery(&tempDb))
        {
          // Successfully loaded
          reopenDialog = false;

          closeDatabaseFile(&tempDb);

          emit preDatabaseLoad();
          closeAllDatabases();

          // Remove old database
          if(QFile::remove(selectedFilename))
            qInfo() << "Removed" << selectedFilename;
          else
            qWarning() << "Removing" << selectedFilename << "failed";

          // Rename temporary file to new database
          if(QFile::rename(tempFilename, selectedFilename))
            qInfo() << "Renamed" << tempFilename << "to" << selectedFilename;
          else
            qWarning() << "Renaming" << tempFilename << "to" << selectedFilename << "failed";

          // Syncronize display with loaded database
          currentFsType = selectedFsType;

          openAllDatabases();
          emit postDatabaseLoad(currentFsType);
        }
        else
        {
          closeDatabaseFile(&tempDb);
          if(QFile::remove(tempFilename))
            qInfo() << "Removed" << tempFilename;
          else
            qWarning() << "Removing" << tempFilename << "failed";

          QFile journal2(tempFilename + "-journal");
          if(journal2.exists() && journal2.size() == 0)
          {
            if(journal2.remove())
              qInfo() << "Removed" << journal2.fileName();
            else
              qWarning() << "Removing" << journal2.fileName() << "failed";
          }
        }
      }
    }
    else
      // User hit close
      reopenDialog = false;
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
  }
  return reopenDialog;
}

/* Opens progress dialog and loads scenery
 * @return true if loading was successfull. false if cancelled or an error occured */
bool DatabaseManager::loadScenery(atools::sql::SqlDatabase *db)
{
  using atools::fs::NavDatabaseOptions;

  bool success = true;
  // Get configuration file path from resources or overloaded path
  QString config = Settings::getOverloadedPath(lnm::DATABASE_NAVDATAREADER_CONFIG);
  qInfo() << "loadScenery: Config file" << config;

  QSettings settings(config, QSettings::IniFormat);

  NavDatabaseOptions navDatabaseOpts;
  navDatabaseOpts.loadFromSettings(settings);

  navDatabaseOpts.setReadInactive(readInactive);
  navDatabaseOpts.setReadAddOnXml(readAddOnXml);

  // Add exclude paths from option dialog
  const OptionData& optionData = OptionData::instance();
  navDatabaseOpts.addToAddonDirectoryExcludes(optionData.getDatabaseAddonExclude());

  for(const QString& fileOrPath : optionData.getDatabaseExclude())
  {
    QFileInfo fileInfo(fileOrPath);

    if(fileInfo.exists())
    {
      if(QFileInfo(fileOrPath).isDir())
      {
        qInfo() << Q_FUNC_INFO << "Directory exclusion" << fileOrPath;
        navDatabaseOpts.addToDirectoryExcludes({fileOrPath});
      }
      else
      {
        qInfo() << Q_FUNC_INFO << "File exclusion" << fileOrPath;
        navDatabaseOpts.addToFilePathExcludes({fileOrPath});
      }
    }
    else
      qWarning() << Q_FUNC_INFO << "Exclusion does not exist" << fileOrPath;
  }

  navDatabaseOpts.setSimulatorType(selectedFsType);

  delete progressDialog;
  progressDialog = new QProgressDialog(mainWindow);
  progressDialog->setWindowFlags(progressDialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  progressDialog->setWindowModality(Qt::ApplicationModal);

  progressDialog->setWindowTitle(tr("%1 - Loading %2").
                                 arg(QApplication::applicationName()).
                                 arg(atools::fs::FsPaths::typeToShortName(selectedFsType)));

  // Label will be owned by progress
  QLabel *label = new QLabel(progressDialog);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setIndent(10);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setMinimumWidth(800);

  progressDialog->setLabel(label);
  progressDialog->setAutoClose(false);
  progressDialog->setAutoReset(false);
  progressDialog->setMinimumDuration(0);

  navDatabaseOpts.setSceneryFile(simulators.value(selectedFsType).sceneryCfg);
  navDatabaseOpts.setBasepath(simulators.value(selectedFsType).basePath);

  QElapsedTimer timer;
  progressTimerElapsed = 0L;

  progressDialog->setLabelText(
    databaseTimeText.arg(QString()).
    arg(QString()).
    arg(QString()).arg(QString()).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0));

  progressDialog->show();

  navDatabaseOpts.setProgressCallback(std::bind(&DatabaseManager::progressCallback, this,
                                                std::placeholders::_1, timer));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  atools::fs::NavDatabaseErrors errors;

  qInfo() << Q_FUNC_INFO << navDatabaseOpts;

  try
  {
    atools::fs::NavDatabase nd(&navDatabaseOpts, db, &errors, GIT_REVISION);
    QString sceneryCfgCodec = (selectedFsType == atools::fs::FsPaths::P3D_V4 ||
                               selectedFsType == atools::fs::FsPaths::P3D_V5) ? "UTF-8" : QString();
    nd.create(sceneryCfgCodec);
  }
  catch(atools::Exception& e)
  {
    // Show dialog if something went wrong but do not exit
    ErrorHandler(progressDialog).handleException(
      e, currentBglFilePath.isEmpty() ? QString() : tr("Processed files:\n%1\n").arg(currentBglFilePath));
    success = false;
  }
  catch(...)
  {
    // Show dialog if something went wrong but do not exit
    ErrorHandler(progressDialog).handleUnknownException(
      currentBglFilePath.isEmpty() ? QString() : tr("Processed files:\n%1\n").arg(currentBglFilePath));
    success = false;
  }

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // Show errors that occured during loading, if any
  if(errors.getTotalErrors() > 0)
  {
    QString errorTexts;
    errorTexts.append(tr("<h3>Found %1 errors in %2 scenery entries when loading the scenery database</h3>").
                      arg(errors.getTotalErrors()).arg(errors.sceneryErrors.size()));

    errorTexts.append(tr("<b>If you wish to report this error attach the log and configuration files "
                           "to your report, add all other available information and send it to one "
                           "of the contact addresses below.</b>"
                           "<hr/>%1"
                             "<hr/>%2").
                      arg(atools::gui::Application::getEmailHtml()).
                      arg(atools::gui::Application::getReportPathHtml()));

    errorTexts.append(tr("<hr/>Some files or scenery directories could not be read.<br/>"
                         "You should check if the airports of the affected sceneries display "
                         "correctly and show the correct information.<hr/>"));

    int numScenery = 0;
    for(const atools::fs::NavDatabaseErrors::SceneryErrors& scErr : errors.sceneryErrors)
    {
      if(numScenery >= MAX_ERROR_SCENERY_MESSAGES)
      {
        errorTexts.append(tr("<b>More scenery entries ...</b>"));
        break;
      }

      int numBgl = 0;
      errorTexts.append(tr("<b>Scenery Title: %1</b><br/>").arg(scErr.scenery.getTitle()));

      for(const QString& err : scErr.sceneryErrorsMessages)
        errorTexts.append(err + "<br/>");

      for(const atools::fs::NavDatabaseErrors::SceneryFileError& bglErr : scErr.fileErrors)
      {
        if(numBgl >= MAX_ERROR_BGL_MESSAGES)
        {
          errorTexts.append(tr("<b>More files ...</b>"));
          break;
        }
        numBgl++;

        errorTexts.append(tr("<b>File:</b> \"%1\"<br/><b>Error:</b> %2<br/>").
                          arg(bglErr.filepath).arg(bglErr.errorMessage));
      }
      errorTexts.append("<br/>");
      numScenery++;
    }

    TextDialog errorDialog(progressDialog,
                           QApplication::applicationName() + tr(" - Load Scenery Library Errors"),
                           "SCENERY.html#errors"); // anchor for future use
    errorDialog.setHtmlMessage(errorTexts, true /* print to log */);
    errorDialog.exec();
  }

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  if(!progressDialog->wasCanceled() && success)
  {
    // Show results and wait until user selects ok
    progressDialog->setCancelButtonText(tr("&OK"));
    progressDialog->exec();
  }
  else
    // Loading was cancelled
    success = false;

  delete progressDialog;
  progressDialog = nullptr;

  return success;
}

/* Simulator was changed in scenery database loading dialog */
void DatabaseManager::simulatorChangedFromComboBox(FsPaths::SimulatorType value)
{
  selectedFsType = value;
  updateDialogInfo(selectedFsType);
}

/* Called by atools::fs::NavDatabase. Updates progress bar and statistics */
bool DatabaseManager::progressCallback(const atools::fs::NavDatabaseProgress& progress, QElapsedTimer& timer)
{
  if(progressDialog->wasCanceled())
    return true;

  if(progress.isFirstCall())
  {
    timer.start();
    progressDialog->setMinimum(0);
    progressDialog->setMaximum(progress.getTotal());
  }

  // Update only four times a second
  if((timer.elapsed() - progressTimerElapsed) > 250 || progress.isLastCall())
  {
    progressDialog->setValue(progress.getCurrent());

    if(progress.isNewOther())
    {
      currentBglFilePath.clear();

      // Run script etc.
      progressDialog->setLabelText(
        databaseTimeText.arg(atools::elideTextShortMiddle(progress.getOtherAction(), MAX_TEXT_LENGTH)).
        arg(formatter::formatElapsed(timer)).
        arg(QString()).
        arg(QString()).
        arg(progress.getNumErrors()).
        arg(progress.getNumFiles()).
        arg(progress.getNumAirports()).
        arg(progress.getNumVors()).
        arg(progress.getNumIls()).
        arg(progress.getNumNdbs()).
        arg(progress.getNumMarker()).
        arg(progress.getNumWaypoints()).
        arg(progress.getNumBoundaries()));
    }
    else if(progress.isNewSceneryArea() || progress.isNewFile())
    {
      currentBglFilePath = progress.getBglFilePath();

      // Switched to a new scenery area
      progressDialog->setLabelText(
        databaseLoadingText.arg(atools::elideTextShortMiddle(progress.getSceneryTitle(), MAX_TEXT_LENGTH)).
        arg(atools::elideTextShortMiddle(progress.getSceneryPath(), MAX_TEXT_LENGTH)).
        arg(atools::elideTextShortMiddle(progress.getBglFileName(), MAX_TEXT_LENGTH)).
        arg(formatter::formatElapsed(timer)).
        arg(progress.getNumErrors()).
        arg(progress.getNumFiles()).
        arg(progress.getNumAirports()).
        arg(progress.getNumVors()).
        arg(progress.getNumIls()).
        arg(progress.getNumNdbs()).
        arg(progress.getNumMarker()).
        arg(progress.getNumWaypoints()).
        arg(progress.getNumBoundaries()));
    }
    else if(progress.isLastCall())
    {
      currentBglFilePath.clear();
      progressDialog->setValue(progress.getTotal());

      // Last report
      progressDialog->setLabelText(
        databaseTimeText.arg(tr("<big>Done.</big>")).
        arg(formatter::formatElapsed(timer)).
        arg(QString()).
        arg(QString()).
        arg(progress.getNumErrors()).
        arg(progress.getNumFiles()).
        arg(progress.getNumAirports()).
        arg(progress.getNumVors()).
        arg(progress.getNumIls()).
        arg(progress.getNumNdbs()).
        arg(progress.getNumMarker()).
        arg(progress.getNumWaypoints()).
        arg(progress.getNumBoundaries()));
    }

    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    progressTimerElapsed = timer.elapsed();
  }

  return progressDialog->wasCanceled();
}

/* Checks if the current database has a schema. Exits program if this fails */
bool DatabaseManager::hasSchema(atools::sql::SqlDatabase *db)
{
  try
  {
    return DatabaseMeta(db).hasSchema();
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

/* Checks if the current database contains data. Exits program if this fails */
bool DatabaseManager::hasData(atools::sql::SqlDatabase *db)
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
bool DatabaseManager::isDatabaseCompatible(atools::sql::SqlDatabase *db)
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

void DatabaseManager::createEmptySchema(atools::sql::SqlDatabase *db, bool boundary)
{
  try
  {
    NavDatabaseOptions opts;
    if(boundary)
      // Does not use a transaction
      NavDatabase(&opts, db, nullptr, GIT_REVISION).createAirspaceSchema();
    else
    {
      NavDatabase(&opts, db, nullptr, GIT_REVISION).createSchema();
      DatabaseMeta(db).updateVersion();
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

bool DatabaseManager::hasInstalledSimulators() const
{
  return !simulators.getAllInstalled().isEmpty();
}

bool DatabaseManager::hasSimulatorDatabases() const
{
  return !simulators.getAllHavingDatabase().isEmpty();
}

void DatabaseManager::saveState()
{
  Settings& s = Settings::instance();
  s.setValueVar(lnm::DATABASE_PATHS, QVariant::fromValue(simulators));
  s.setValue(lnm::DATABASE_SIMULATOR, atools::fs::FsPaths::typeToShortName(currentFsType));
  s.setValue(lnm::DATABASE_LOADINGSIMULATOR, atools::fs::FsPaths::typeToShortName(selectedFsType));
  s.setValue(lnm::DATABASE_LOAD_INACTIVE, readInactive);
  s.setValue(lnm::DATABASE_LOAD_ADDONXML, readAddOnXml);
  s.setValue(lnm::DATABASE_USE_NAV, static_cast<int>(navDatabaseStatus));
}

void DatabaseManager::restoreState()
{
  Settings& s = Settings::instance();
  simulators = s.valueVar(lnm::DATABASE_PATHS).value<SimulatorTypeMap>();
  currentFsType = atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_SIMULATOR, QString()));
  selectedFsType = atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_LOADINGSIMULATOR));
  readInactive = s.valueBool(lnm::DATABASE_LOAD_INACTIVE, false);
  readAddOnXml = s.valueBool(lnm::DATABASE_LOAD_ADDONXML, true);
  navDatabaseStatus = static_cast<dm::NavdatabaseStatus>(s.valueInt(lnm::DATABASE_USE_NAV, dm::NAVDATABASE_MIXED));
}

/* Updates metadata, version and object counts in the scenery loading dialog */
void DatabaseManager::updateDialogInfo(atools::fs::FsPaths::SimulatorType value)
{
  QString metaText;

  QString databaseFile = buildDatabaseFileName(value);
  SqlDatabase tempDb(DATABASE_NAME_DLG_INFO_TEMP);

  if(QFileInfo::exists(databaseFile))
  { // Open temp database to show statistics
    tempDb.setDatabaseName(databaseFile);
    tempDb.setReadonly();
    tempDb.open();
  }

  if(tempDb.isOpen())
  {
    DatabaseMeta dbmeta(tempDb);
    if(!dbmeta.isValid())
      metaText = databaseMetaText.arg(tr("None")).arg(tr("None")).arg(tr("None")).
                 arg(DatabaseMeta::DB_VERSION_MAJOR).arg(DatabaseMeta::DB_VERSION_MINOR).arg(QString());
    else
    {
      QString cycleText;
      if(!dbmeta.getAiracCycle().isEmpty())
        cycleText = databaseAiracCycleText.arg(dbmeta.getAiracCycle());

      metaText = databaseMetaText.
                 arg(dbmeta.getLastLoadTime().isValid() ? dbmeta.getLastLoadTime().toString() : tr("None")).
                 arg(dbmeta.getMajorVersion()).
                 arg(dbmeta.getMinorVersion()).
                 arg(DatabaseMeta::DB_VERSION_MAJOR).
                 arg(DatabaseMeta::DB_VERSION_MINOR).
                 arg(cycleText);
    }
  }
  else
    metaText = databaseMetaText.arg(tr("None")).arg(tr("None")).arg(tr("None")).
               arg(DatabaseMeta::DB_VERSION_MAJOR).arg(DatabaseMeta::DB_VERSION_MINOR).arg(QString());

  QString tableText;
  if(tempDb.isOpen() && hasSchema(&tempDb))
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

  databaseDialog->setHeader(metaText + tr("<p><big>Currently Loaded:</big></p><p>%1</p>").arg(tableText));

  if(tempDb.isOpen())
    tempDb.close();
}

/* Create database name including simulator short name */
QString DatabaseManager::buildDatabaseFileName(atools::fs::FsPaths::SimulatorType type)
{
  return databaseDirectory +
         QDir::separator() + lnm::DATABASE_PREFIX +
         atools::fs::FsPaths::typeToShortName(type).toLower() + lnm::DATABASE_SUFFIX;
}

/* Create database name including simulator short name in application directory */
QString DatabaseManager::buildDatabaseFileNameAppDir(atools::fs::FsPaths::SimulatorType type)
{
  return QCoreApplication::applicationDirPath() +
         QDir::separator() + lnm::DATABASE_DIR +
         QDir::separator() + lnm::DATABASE_PREFIX +
         atools::fs::FsPaths::typeToShortName(type).toLower() + lnm::DATABASE_SUFFIX;
}

QString DatabaseManager::buildCompilingDatabaseFileName()
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
  if(navDbActionBlend != nullptr)
  {
    navDbActionBlend->deleteLater();
    navDbActionBlend = nullptr;
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
  for(QAction *action : actions)
    action->deleteLater();
  actions.clear();
}

/* Uses the simulator map copy from the dialog to update the changed paths */
void DatabaseManager::updateSimulatorPathsFromDialog()
{
  const SimulatorTypeMap& dlgPaths = databaseDialog->getPaths();

  for(FsPaths::SimulatorType type : dlgPaths.keys())
  {
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
  if(currentFsType == atools::fs::FsPaths::UNKNOWN ||
     (!simulators.value(currentFsType).hasDatabase && !simulators.value(currentFsType).isInstalled))
    currentFsType = simulators.getBest();

  if(currentFsType == atools::fs::FsPaths::UNKNOWN)
    currentFsType = simulators.getBestInstalled();

  // Correct if loading simulator is invalid - get the best installed
  if(selectedFsType == atools::fs::FsPaths::UNKNOWN || !simulators.getAllInstalled().contains(selectedFsType))
    selectedFsType = simulators.getBestInstalled();
}

void DatabaseManager::metaFromFile(QString *cycle, QDateTime *compilationTime, bool *settingsNeedsPreparation,
                                   QString *source, const QString& file)
{
  SqlDatabase tempDb(DATABASE_NAME_TEMP);
  tempDb.setDatabaseName(file);
  tempDb.setReadonly();
  tempDb.open();
  {
    DatabaseMeta meta(tempDb);

    if(cycle != nullptr)
      *cycle = meta.getAiracCycle();

    if(source != nullptr)
      *source = meta.getDataSource();

    if(compilationTime != nullptr)
      *compilationTime = meta.getLastLoadTime();

    if(settingsNeedsPreparation != nullptr)
      *settingsNeedsPreparation = SqlUtil(tempDb).hasTableAndRows("script");
  }

  closeDatabaseFile(&tempDb);
}
