/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "db/databaseerrordialog.h"
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
#include "fs/navdatabase.h"
#include "sql/sqlutil.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "navapp.h"

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
using atools::sql::SqlQuery;
using atools::fs::db::DatabaseMeta;

const int MAX_ERROR_BGL_MESSAGES = 400;
const int MAX_ERROR_SCENERY_MESSAGES = 400;

const QString DATABASE_META_TEXT(
  QObject::tr("<p><big>Last Update: %1. Database Version: %2.%3. Program Version: %4.%5.%6</big></p>"));

const QString DATABASE_AIRAC_CYCLE_TEXT(QObject::tr(" AIRAC Cycle %1."));

const QString DATABASE_INFO_TEXT(QObject::tr("<table>"
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
                                             ));

const QString DATABASE_TIME_TEXT(QObject::tr(
                                   "<b>%1</b><br/><br/><br/>"
                                   "<b>Time:</b> %2<br/>%3%4"
                                     "<b>Errors:</b> %5<br/><br/>"
                                     "<big>Found:</big></br>"
                                   ) + DATABASE_INFO_TEXT);

const QString DATABASE_LOADING_TEXT(QObject::tr(
                                      "<b>Scenery:</b> %1 (%2)<br/>"
                                      "<b>File:</b> %3<br/><br/>"
                                      "<b>Time:</b> %4<br/>"
                                      "<b>Errors:</b> %5<br/><br/>"
                                      "<big>Found:</big></br>"
                                      ) + DATABASE_INFO_TEXT);

DatabaseManager::DatabaseManager(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  // Also loads list of simulators
  restoreState();

  databaseDirectory = Settings::getPath() + QDir::separator() + lnm::DATABASE_DIR;
  if(!QDir().mkpath(databaseDirectory))
    qWarning() << "Cannot create db dir" << databaseDirectory;

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

  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME);
  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_DLG_INFO_TEMP);
  SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME_TEMP);

  database = new SqlDatabase(DATABASE_NAME);
}

DatabaseManager::~DatabaseManager()
{
  // Delete simulator switch actions
  freeActions();

  delete databaseDialog;
  delete progressDialog;

  closeDatabase();
  delete database;
  SqlDatabase::removeDatabase(DATABASE_NAME);
  SqlDatabase::removeDatabase(DATABASE_NAME_DLG_INFO_TEMP);
  SqlDatabase::removeDatabase(DATABASE_NAME_TEMP);
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
        // Create an empty schema for all incompatible databases
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);

        QMessageBox progressBox(QMessageBox::NoIcon, QApplication::applicationName(), tr("Deleting ..."));
        progressBox.setWindowFlags(progressBox.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        progressBox.setStandardButtons(QMessageBox::NoButton);
        progressBox.setWindowModality(Qt::ApplicationModal);
        progressBox.show();
        atools::gui::Application::processEventsExtended();

        int i = 0;
        for(const QString& dbfile : databaseFiles)
        {
          progressBox.setText(tr("Erasing database for %1 ...").arg(databaseNames.at(i)));
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
            QMessageBox::warning(nullptr, QApplication::applicationName(),
                                 tr("Deleting of database<br/><br/><i>%1</i><br/><br/>failed.<br/><br/>"
                                    "Remove the database file manually and restart the program.").arg(dbfile));
            ok = false;
          }
          i++;
        }
        QGuiApplication::restoreOverrideCursor();

        progressBox.close();
      }
    }
  }
  return ok;
}

QString DatabaseManager::getCurrentSimulatorBasePath() const
{
  return getSimulatorBasePath(currentFsType);
}

QString DatabaseManager::getSimulatorBasePath(atools::fs::FsPaths::SimulatorType type) const
{
  return simulators.value(type).basePath;
}

void DatabaseManager::insertSimSwitchActions()
{
  qDebug() << Q_FUNC_INFO;
  Ui::MainWindow *ui = NavApp::getMainUi();

  freeActions();

  // Create group to get radio button like behavior
  group = new QActionGroup(ui->menuDatabase);

  // Sort keys to avoid random order
  QList<FsPaths::SimulatorType> keys = simulators.keys();
  QList<FsPaths::SimulatorType> sims, external;
  std::sort(keys.begin(), keys.end());

  // Add real simulators first
  for(atools::fs::FsPaths::SimulatorType type : keys)
  {
    if(type == atools::fs::FsPaths::EXTERNAL || type == atools::fs::FsPaths::EXTERNAL2)
      continue;

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

  // Add external databases next
  menuDbSeparator = ui->menuDatabase->insertSeparator(ui->actionDatabaseFiles);
  for(atools::fs::FsPaths::SimulatorType type : keys)
  {
    if(type != atools::fs::FsPaths::EXTERNAL && type != atools::fs::FsPaths::EXTERNAL2)
      continue;

    // Create an action for each simulator installation or database found
    if(simulators.value(type).hasDatabase)
      external.append(type);
  }

  if(sims.size() + external.size() > 1)
  {
    int index = 1;
    for(atools::fs::FsPaths::SimulatorType type : sims)
      insertSimSwitchAction(type, ui->actionDatabaseFiles, ui->menuDatabase, index++);

    for(atools::fs::FsPaths::SimulatorType type : external)
      insertSimSwitchAction(type, ui->actionDatabaseFiles, ui->menuDatabase, index++);

    if(!external.isEmpty())
      menuExternDbSeparator = ui->menuDatabase->insertSeparator(ui->actionDatabaseFiles);
  }
}

void DatabaseManager::insertSimSwitchAction(atools::fs::FsPaths::SimulatorType type, QAction *before, QMenu *menu,
                                            int index)
{
  QAction *action = new QAction("&" + QString::number(index) + " " + FsPaths::typeToName(type), menu);
  action->setData(QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));
  action->setCheckable(true);
  action->setActionGroup(group);

  if(type == currentFsType)
  {
    QSignalBlocker blocker(action);
    Q_UNUSED(blocker);
    action->setChecked(true);
  }

  menu->insertAction(before, action);

  connect(action, &QAction::triggered, this, &DatabaseManager::switchSimFromMainMenu);

  actions.append(action);
}

/* User changed simulator in main menu */
void DatabaseManager::switchSimFromMainMenu()
{
  qDebug() << Q_FUNC_INFO;

  QAction *action = dynamic_cast<QAction *>(sender());

  if(action != nullptr && currentFsType != action->data().value<atools::fs::FsPaths::SimulatorType>())
  {
    // Disconnect all queries
    emit preDatabaseLoad();

    closeDatabase();

    // Set new simulator
    currentFsType = action->data().value<atools::fs::FsPaths::SimulatorType>();
    openDatabase();

    // Reopen all with new database
    emit postDatabaseLoad(currentFsType);
    mainWindow->setStatusMessage(tr("Switched to %1.").arg(FsPaths::typeToName(currentFsType)));

    saveState();
  }
}

void DatabaseManager::openDatabase()
{
  openDatabaseFile(database, buildDatabaseFileName(currentFsType), true /* readonly */);
}

void DatabaseManager::openDatabaseFile(atools::sql::SqlDatabase *db, const QString& file, bool readonly)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  int databaseCacheKb = settings.getAndStoreValue(lnm::SETTINGS_DATABASE + "CacheKb", 50000).toInt();
  bool foreignKeys = settings.getAndStoreValue(lnm::SETTINGS_DATABASE + "ForeignKeys", false).toBool();

  // cache_size * 1024 bytes if value is negative
  QStringList DATABASE_PRAGMAS({QString("PRAGMA cache_size=-%1").arg(databaseCacheKb),
                                "PRAGMA synchronous=OFF",
                                "PRAGMA journal_mode=TRUNCATE",
                                "PRAGMA page_size=8196",
                                "PRAGMA locking_mode=EXCLUSIVE"});

  QStringList DATABASE_PRAGMA_QUERIES({"PRAGMA foreign_keys", "PRAGMA cache_size", "PRAGMA synchronous",
                                       "PRAGMA journal_mode", "PRAGMA page_size", "PRAGMA locking_mode"});

  try
  {
    qDebug() << "Opening database" << file;
    db->setDatabaseName(file);

    // Set foreign keys only on demand because they can decrease loading performance
    if(foreignKeys)
      DATABASE_PRAGMAS.append("PRAGMA foreign_keys = ON");
    else
      DATABASE_PRAGMAS.append("PRAGMA foreign_keys = OFF");

    bool autocommit = db->isAutocommit();
    db->setAutocommit(false);
    db->open(DATABASE_PRAGMAS);

    atools::sql::SqlQuery query(db);
    for(const QString& pragmaQuery : DATABASE_PRAGMA_QUERIES)
    {
      query.exec(pragmaQuery);
      if(query.next())
        qDebug() << pragmaQuery << "value is now: " << query.value(0).toString();
      query.finish();
    }

    db->setAutocommit(autocommit);

    if(!hasSchema(db))
    {
      if(db->isReadonly())
      {
        // Reopen database read/write
        db->close();
        db->setReadonly(false);
        db->open(DATABASE_PRAGMAS);
      }

      createEmptySchema(db);
      db->commit();
    }

    if(readonly && !db->isReadonly())
    {
      // Readonly requested - reopen database
      db->close();
      db->setReadonly();
      db->open(DATABASE_PRAGMAS);
    }

    DatabaseMeta dbmeta(db);
    qInfo().nospace() << "Database version "
                      << dbmeta.getMajorVersion() << "." << dbmeta.getMinorVersion();

    qInfo().nospace() << "Application database version "
                      << DatabaseMeta::DB_VERSION_MAJOR << "." << DatabaseMeta::DB_VERSION_MINOR;
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

void DatabaseManager::closeDatabase()
{
  closeDatabaseFile(database);
}

void DatabaseManager::closeDatabaseFile(atools::sql::SqlDatabase *db)
{
  try
  {
    qDebug() << "Closing database" << db->databaseName();
    if(db->isOpen())
      db->close();
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

atools::sql::SqlDatabase *DatabaseManager::getDatabase()
{
  return database;
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

void DatabaseManager::copyAirspaces()
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    // The current database is read only so we cannot use the attach command
    SqlUtil fromUtil(database);
    if(fromUtil.hasTable("boundary"))
    {
      // We have a boundary table
      QString targetFile = buildDatabaseFileName(atools::fs::FsPaths::XPLANE11);

      if(QFile::exists(targetFile))
      {
        // X-Plane database file exists
        SqlDatabase xpDb(DATABASE_NAME_TEMP);
        xpDb.setDatabaseName(targetFile);
        xpDb.open();

        SqlUtil xpUtil(xpDb);

        if(xpUtil.hasTable("boundary"))
        {
          // X-Plane database file has a boundary table
          QGuiApplication::setOverrideCursor(Qt::WaitCursor);

          // Delete
          xpDb.exec("delete from boundary");
          xpDb.commit();

          // Build statements
          SqlQuery fromQuery(fromUtil.buildSelectStatement("boundary"), database);

          // Use named bindings to overcome different column order
          // Let SQLite generate the ID automatically
          SqlQuery xpQuery(xpDb);
          xpQuery.prepare(xpUtil.buildInsertStatement("boundary", QString(), {"boundary_id"},
                                                      true /* named bindings */));

          // Copy from one database to another
          std::function<bool(SqlQuery&, SqlQuery&)> func =
            [](SqlQuery&, SqlQuery& to) -> bool
            {
              // use an invalid value for file_id to avoid display in information window
              to.bindValue(":file_id", 0);
              return true;
            };
          int copied = SqlUtil::copyResultValues(fromQuery, xpQuery, func);
          xpDb.commit();

          QGuiApplication::restoreOverrideCursor();
          QMessageBox::information(mainWindow, QApplication::applicationName(),
                                   tr("Copied %1 airspaces to the X-Plane scenery database.").
                                   arg(copied));
        }
        else
          QMessageBox::warning(mainWindow, QApplication::applicationName(),
                               tr("X-Plane database has no airspace boundary table.").arg(targetFile));
        xpDb.close();
      }
      else
        QMessageBox::warning(mainWindow, QApplication::applicationName(),
                             tr("X-Plane database \"%1\" does not exist.").arg(targetFile));
    }
    else
      QMessageBox::warning(mainWindow, QApplication::applicationName(),
                           tr("Airspace boundary table not found in currently selected database"));
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
      QString err;
      if(atools::fs::NavDatabase::isBasePathValid(databaseDialog->getBasePath(), err, selectedFsType))
      {
        QString sceneryCfgCodec = selectedFsType == atools::fs::FsPaths::P3D_V4 ? "UTF-8" : QString();

        if(selectedFsType == atools::fs::FsPaths::XPLANE11 ||
           atools::fs::NavDatabase::isSceneryConfigValid(databaseDialog->getSceneryConfigFile(), sceneryCfgCodec, err))
        {
          // Compile into a temporary database file
          QString selectedFilename = buildDatabaseFileName(selectedFsType);
          QString tempFilename = buildCompilingDatabaseFileName();

          if(QFile::remove(tempFilename))
            qInfo() << "Removed" << tempFilename;

          if(QFile::remove(tempFilename + "-journal"))
            qInfo() << "Removed" << (tempFilename + "-journal");

          SqlDatabase tempDb(DATABASE_NAME_TEMP);
          openDatabaseFile(&tempDb, tempFilename, false /* readonly */);

          if(loadScenery(&tempDb))
          {
            // Successfully loaded
            reopenDialog = false;

            closeDatabaseFile(&tempDb);

            emit preDatabaseLoad();
            closeDatabase();

            // Remove old database
            if(!QFile::remove(selectedFilename))
              qWarning() << "Removing" << selectedFilename << "failed";

            // Rename temporary file to new database
            if(!QFile::rename(tempFilename, selectedFilename))
              qWarning() << "Renaming" << tempFilename << "to" << selectedFilename << "failed";

            // Syncronize display with loaded database
            currentFsType = selectedFsType;

            openDatabase();
            emit postDatabaseLoad(currentFsType);
          }
          else
          {
            closeDatabaseFile(&tempDb);
            if(QFile::remove(tempFilename))
              qInfo() << "Removed" << tempFilename;

            if(QFile::remove(tempFilename + "-journal"))
              qInfo() << "Removed" << (tempFilename + "-journal");
          }
        }
        else
          QMessageBox::warning(databaseDialog, QApplication::applicationName(), tr("Cannot read \"%1\". Reason: %2.").
                               arg(databaseDialog->getSceneryConfigFile()).arg(err));
      }
      else
        QMessageBox::warning(databaseDialog, QApplication::applicationName(), tr("Cannot read \"%1\". Reason: %2.").
                             arg(databaseDialog->getBasePath()).arg(err));
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
  navDatabaseOpts.addToDirectoryExcludes(optionData.getDatabaseExclude());
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
    DATABASE_TIME_TEXT.arg(QString()).
    arg(QString()).
    arg(QString()).arg(QString()).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0));

  progressDialog->show();

  navDatabaseOpts.setProgressCallback(std::bind(&DatabaseManager::progressCallback, this,
                                                std::placeholders::_1, timer));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  atools::fs::NavDatabaseErrors errors;

  try
  {
    atools::fs::NavDatabase nd(&navDatabaseOpts, db, &errors);
    QString sceneryCfgCodec = selectedFsType == atools::fs::FsPaths::P3D_V4 ? "UTF-8" : QString();
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

        errorTexts.append(tr("<b>File:</b> <i>%1</i><br/><b>Error:</b> <i>%2</i><br/>").
                          arg(bglErr.filepath).arg(bglErr.errorMessage));
      }
      errorTexts.append("<br/>");
      numScenery++;
    }

    DatabaseErrorDialog errorDialog(progressDialog);
    errorDialog.setErrorMessages(errorTexts);
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
        DATABASE_TIME_TEXT.arg(progress.getOtherAction()).
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
        DATABASE_LOADING_TEXT.arg(progress.getSceneryTitle()).
        arg(progress.getSceneryPath()).
        arg(progress.getBglFileName()).
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
        DATABASE_TIME_TEXT.arg(tr("<big>Done.</big>")).
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

/* Create an empty database schema. */
void DatabaseManager::createEmptySchema(atools::sql::SqlDatabase *db)
{
  try
  {
    NavDatabaseOptions opts;
    NavDatabase(&opts, db, nullptr).createSchema();
    DatabaseMeta(db).updateVersion();
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
}

void DatabaseManager::restoreState()
{
  Settings& s = Settings::instance();
  simulators = s.valueVar(lnm::DATABASE_PATHS).value<SimulatorTypeMap>();
  currentFsType = atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_SIMULATOR, QString()));
  selectedFsType = atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_LOADINGSIMULATOR));
  readInactive = s.valueBool(lnm::DATABASE_LOAD_INACTIVE, false);
  readAddOnXml = s.valueBool(lnm::DATABASE_LOAD_ADDONXML, true);
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
      metaText = DATABASE_META_TEXT.arg(tr("None")).arg(tr("None")).arg(tr("None")).
                 arg(DatabaseMeta::DB_VERSION_MAJOR).arg(DatabaseMeta::DB_VERSION_MINOR).arg(QString());
    else
    {
      QString cycleText;
      if(!dbmeta.getAiracCycle().isEmpty())
        cycleText = DATABASE_AIRAC_CYCLE_TEXT.arg(dbmeta.getAiracCycle());

      metaText = DATABASE_META_TEXT.
                 arg(dbmeta.getLastLoadTime().isValid() ? dbmeta.getLastLoadTime().toString() : tr("None")).
                 arg(dbmeta.getMajorVersion()).
                 arg(dbmeta.getMinorVersion()).
                 arg(DatabaseMeta::DB_VERSION_MAJOR).
                 arg(DatabaseMeta::DB_VERSION_MINOR).
                 arg(cycleText);
    }
  }
  else
    metaText = DATABASE_META_TEXT.arg(tr("None")).arg(tr("None")).arg(tr("None")).
               arg(DatabaseMeta::DB_VERSION_MAJOR).arg(DatabaseMeta::DB_VERSION_MINOR).arg(QString());

  QString tableText;
  if(tempDb.isOpen() && hasSchema(&tempDb))
  {
    atools::sql::SqlUtil util(tempDb);

    // Get row counts for the dialog
    tableText = DATABASE_INFO_TEXT.arg(util.rowCount("bgl_file")).
                arg(util.rowCount("airport")).
                arg(util.rowCount("vor")).
                arg(util.rowCount("ils")).
                arg(util.rowCount("ndb")).
                arg(util.rowCount("marker")).
                arg(util.rowCount("waypoint")).
                arg(util.rowCount("boundary"));
  }
  else
    tableText = DATABASE_INFO_TEXT.arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0);

  databaseDialog->setHeader(metaText + tr("<p><big>Currently Loaded:</big></p><p>%1</p>").arg(tableText));

  if(tempDb.isOpen())
    tempDb.close();
}

/* Create database name including simulator short name */
QString DatabaseManager::buildDatabaseFileName(atools::fs::FsPaths::SimulatorType type)
{
  return databaseDirectory + QDir::separator() + lnm::DATABASE_PREFIX +
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

  if(menuExternDbSeparator != nullptr)
  {
    menuExternDbSeparator->deleteLater();
    menuExternDbSeparator = nullptr;
  }

  if(group != nullptr)
  {
    group->deleteLater();
    group = nullptr;
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
  for(atools::fs::FsPaths::SimulatorType type : FsPaths::ALL_SIMULATOR_TYPES)
    // Already present or not - update database status since file exists
    simulators[type].hasDatabase = QFile::exists(buildDatabaseFileName(type));
}

void DatabaseManager::correctSimulatorType()
{
  if(currentFsType == atools::fs::FsPaths::UNKNOWN ||
     (!simulators.value(currentFsType).hasDatabase && !simulators.value(currentFsType).isInstalled))
    currentFsType = simulators.getBest();

  // Correct if loading simulator is invalid - get the best installed
  if(selectedFsType == atools::fs::FsPaths::UNKNOWN || !simulators.getAllInstalled().contains(selectedFsType))
    selectedFsType = simulators.getBestInstalled();
}
