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

#include "databasemanager.h"

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

#include <QDebug>
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressDialog>
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QList>
#include <QMenu>
#include <QDir>
#include <QMessageBox>
#include <QAbstractButton>
#include <QSettings>

using atools::gui::ErrorHandler;
using atools::sql::SqlUtil;
using atools::fs::FsPaths;
using atools::fs::NavDatabaseOptions;
using atools::fs::NavDatabase;
using atools::settings::Settings;
using atools::sql::SqlDatabase;
using atools::fs::db::DatabaseMeta;

const QString DATABASE_META_TEXT(
  QObject::tr("<p><b>Last Update: %1. Database Version: %2.%3. Program Version: %4.%5.</b></p>"));

const QString DATABASE_INFO_TEXT(QObject::tr("<table>"
                                               "<tbody>"
                                                 "<tr> "
                                                   "<td width=\"60\"><b>Files:</b>"
                                                   "</td>    "
                                                   "<td width=\"60\">%L5"
                                                   "</td> "
                                                   "<td width=\"60\"><b>VOR:</b>"
                                                   "</td> "
                                                   "<td width=\"60\">%L7"
                                                   "</td> "
                                                   "<td width=\"60\"><b>Marker:</b>"
                                                   "</td>     "
                                                   "<td width=\"60\">%L10"
                                                   "</td>"
                                                 "</tr>"
                                                 "<tr> "
                                                   "<td width=\"60\"><b>Airports:</b>"
                                                   "</td> "
                                                   "<td width=\"60\">%L6"
                                                   "</td> "
                                                   "<td width=\"60\"><b>ILS:</b>"
                                                   "</td> "
                                                   "<td width=\"60\">%L8"
                                                   "</td> "
                                                   "<td width=\"60\"><b>Waypoints:</b>"
                                                   "</td>  "
                                                   "<td width=\"60\">%L11"
                                                   "</td>"
                                                 "</tr>"
                                                 "<tr> "
                                                   "<td width=\"60\">"
                                                   "</td>"
                                                   "<td width=\"60\">"
                                                   "</td>"
                                                   "<td width=\"60\"><b>NDB:</b>"
                                                   "</td> "
                                                   "<td width=\"60\">%L9"
                                                   "</td> "
                                                 "</tr>"
                                               "</tbody>"
                                             "</table>"
                                             ));

const QString DATABASE_TIME_TEXT(QObject::tr(
                                   "<b>%1</b><br/><br/><br/>"
                                   "<b>Time:</b> %2<br/>%3%4"
                                   ) + DATABASE_INFO_TEXT);

const QString DATABASE_FILE_TEXT(QObject::tr(
                                   "<b>Scenery:</b> %1 (%2)<br/>"
                                   "<b>File:</b> %3<br/><br/>"
                                   "<b>Time:</b> %4<br/>"
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

  // Find any stale databases that do not belong to a simulator
  updateSimulatorFlags();

  for(atools::fs::FsPaths::SimulatorType t : simulators.keys())
    qDebug() << t << simulators.value(t);

  // Correct if current simulator is invalid
  if(currentFsType == atools::fs::FsPaths::UNKNOWN || !simulators.contains(currentFsType))
    currentFsType = simulators.getBest();

  // Correct if loading simulator is invalid - get the best installed
  if(loadingFsType == atools::fs::FsPaths::UNKNOWN || !simulators.getAllInstalled().contains(loadingFsType))
    loadingFsType = simulators.getBestInstalled();

  databaseFile = buildDatabaseFileName(currentFsType);

  qDebug() << "fs type" << currentFsType << "db file" << databaseFile;

  if(mainWindow != nullptr)
  {
    databaseDialog = new DatabaseDialog(mainWindow, simulators);

    connect(databaseDialog, &DatabaseDialog::simulatorChanged, this,
            &DatabaseManager::simulatorChangedFromComboBox);
  }

  if(!SqlDatabase::contains(DATABASE_NAME))
    // Add database driver if not already done
    db = new SqlDatabase(SqlDatabase::addDatabase(DATABASE_TYPE, DATABASE_NAME));
}

DatabaseManager::~DatabaseManager()
{
  // Delete simulator switch actions
  freeActions();

  delete databaseDialog;
  delete progressDialog;

  closeDatabase();
  delete db;
  SqlDatabase::removeDatabase(DATABASE_NAME);
}

bool DatabaseManager::checkIncompatibleDatabases()
{
  bool ok = true;

  // Need empty block to delete sqlDb before removing driver
  {
    // Create a temporary database
    SqlDatabase sqlDb = SqlDatabase::addDatabase(DATABASE_TYPE, "tempdb");
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
          databaseNames.append(FsPaths::typeToName(type));
          databaseFiles.append(dbName);
          qWarning() << "Incompatible database" << dbName;
        }
        sqlDb.close();
      }
    }

    if(!databaseNames.isEmpty())
    {
      QString msg;
      if(databaseNames.size() == 1)
        msg = tr("The database for the simulator "
                 "below is not compatible with this program version or was incompletly loaded:<br/><br/>"
                 "%1<br/><br/>Erase it?");
      else
        msg = tr("The databases for the simulators "
                 "below are not compatible with this program version or were incompletly loaded:<br/><br/>"
                 "%1<br/><br/>Erase them?");

      QMessageBox box(QMessageBox::Question, QApplication::applicationName(),
                      msg.arg(databaseNames.join("<br/>")),
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
        for(const QString& dbfile : databaseFiles)
        {
          sqlDb.setDatabaseName(dbfile);
          sqlDb.open();
          createEmptySchema(&sqlDb);
          sqlDb.close();
        }
      }
    }
  }
  SqlDatabase::removeDatabase("tempdb");
  return ok;
}

void DatabaseManager::insertSimSwitchActions(QAction *before, QMenu *menu)
{
  freeActions();

  if(simulators.size() > 1)
  {
    // Create group to get radio button like behavior
    group = new QActionGroup(menu);
    int index = 1;

    // Sort keys to avoid random order
    QList<FsPaths::SimulatorType> keys = simulators.keys();
    std::sort(keys.begin(), keys.end());

    for(atools::fs::FsPaths::SimulatorType type : keys)
    {
      // Create an action for each simulator installation or database found
      QAction *action = new QAction("&" + QString::number(index++) + " " + FsPaths::typeToName(type), menu);
      action->setData(QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));
      action->setCheckable(true);
      action->setActionGroup(group);

      if(type == currentFsType)
        action->setChecked(true);

      menu->insertAction(before, action);

      connect(action, &QAction::triggered, this, &DatabaseManager::switchSimFromMainMenu);

      actions.append(action);
    }
    menu->insertSeparator(before);
  }
}

/* Update simuator select actions in main menu */
void DatabaseManager::updateSimSwitchActions()
{
  for(QAction *action : actions)
  {
    atools::fs::FsPaths::SimulatorType type = action->data().value<atools::fs::FsPaths::SimulatorType>();
    action->setChecked(type == currentFsType);
  }
}

/* User changed simulator in main menu */
void DatabaseManager::switchSimFromMainMenu()
{
  qDebug() << "switchSim";

  QAction *action = dynamic_cast<QAction *>(sender());

  if(action != nullptr)
  {
    // Disconnect all queries
    emit preDatabaseLoad();

    closeDatabase();

    // Set new simulator
    currentFsType = action->data().value<atools::fs::FsPaths::SimulatorType>();
    databaseFile = buildDatabaseFileName(currentFsType);
    openDatabase();

    // Reopen all with new database
    emit postDatabaseLoad(currentFsType);
    mainWindow->setStatusMessage(tr("Switched to %1.").arg(FsPaths::typeToName(currentFsType)));
  }
}

void DatabaseManager::openDatabase()
{
  // size * 1024 bytes if value is negative
  static const QString cacheSizePragma("PRAGMA cache_size = -20000");

  try
  {
    qDebug() << "Opening database" << databaseFile;
    db->setDatabaseName(databaseFile);

    // Set foreign keys only on demand because they can decrease loading performance
    if(Settings::instance().getAndStoreValue(lnm::OPTIONS_FOREIGNKEYS, false).toBool())
      db->open({"PRAGMA foreign_keys = ON", cacheSizePragma});
    else
      db->open({"PRAGMA foreign_keys = OFF", cacheSizePragma});

    atools::sql::SqlQuery query(db);
    query.exec("PRAGMA foreign_keys");
    if(query.next())
      qDebug() << "Foreign keys are" << query.value(0).toBool();

    query.exec("PRAGMA cache_size");
    if(query.next())
      qDebug() << "Cache size is" << query.value(0).toInt();

    if(!hasSchema())
      createEmptySchema(db);
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
  try
  {
    qDebug() << "Closing database" << databaseFile;
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

QString DatabaseManager::getSimulatorShortName() const
{
  return atools::fs::FsPaths::typeToShortName(currentFsType);
}

atools::sql::SqlDatabase *DatabaseManager::getDatabase()
{
  return db;
}

void DatabaseManager::run()
{
  // Disconnect all queries
  emit preDatabaseLoad();

  closeDatabase();
  databaseFile = buildDatabaseFileName(loadingFsType);
  openDatabase();

  databaseDialog->setCurrentFsType(loadingFsType);

  updateDialogInfo();

  // try until user hits cancel or the database was loaded successfully
  while(runInternal())
    ;

  closeDatabase();
  databaseFile = buildDatabaseFileName(currentFsType);
  openDatabase();

  // Reconnect all queries
  emit postDatabaseLoad(currentFsType);
}

/* Shows scenery database loading dialog.
 * @return true if execution was successfull. false if it was cancelled */
bool DatabaseManager::runInternal()
{
  bool reopenDialog = true;
  try
  {
    // Show loading dialog
    int retval = databaseDialog->exec();

    // Copy the changed path structures also if the dialog was closed only
    updateSimulatorPathsFromDialog();

    // Get the simulator database we'll update/load
    loadingFsType = databaseDialog->getCurrentFsType();

    if(retval == QDialog::Accepted)
    {
      QString err;
      if(atools::fs::NavDatabase::isBasePathValid(databaseDialog->getBasePath(), err))
      {
        if(atools::fs::NavDatabase::isSceneryConfigValid(databaseDialog->getSceneryConfigFile(), err))
        {
          if(loadScenery())
          {
            // Successfully loaded
            DatabaseMeta dbmeta(db);
            dbmeta.updateAll();
            reopenDialog = false;
          }
        }
        else
          QMessageBox::warning(databaseDialog, QApplication::applicationName(),
                               tr("Cannot read \"%1\". Reason: %2.").arg(databaseDialog->getSceneryConfigFile(
                                                                           )).arg(err));
      }
      else
        QMessageBox::warning(databaseDialog, QApplication::applicationName(),
                             tr("Cannot read \"%1\". Reason: %2.").arg(databaseDialog->getBasePath()).arg(err));
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
bool DatabaseManager::loadScenery()
{
  using atools::fs::NavDatabaseOptions;

  bool success = true;
  // Get configuration file path from resources or overloaded path
  QString config = Settings::getOverloadedPath(lnm::DATABASE_NAVDATAREADER_CONFIG);
  qInfo() << "loadScenery: Config file" << config;

  QSettings settings(config, QSettings::IniFormat);

  NavDatabaseOptions bglReaderOpts;
  bglReaderOpts.loadFromSettings(settings);

  // Add exclude paths from option dialog
  const OptionData& optionData = OptionData::instance();
  bglReaderOpts.addToAddonDirectoryExcludes(optionData.getDatabaseAddonExclude());
  bglReaderOpts.addToDirectoryExcludes(optionData.getDatabaseExclude());

  delete progressDialog;
  progressDialog = new QProgressDialog(databaseDialog);

  // Label will be owned by progress
  QLabel *label = new QLabel(progressDialog);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setIndent(10);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setMinimumWidth(800);

  progressDialog->setWindowModality(Qt::WindowModal);
  progressDialog->setLabel(label);
  progressDialog->setAutoClose(false);
  progressDialog->setAutoReset(false);
  progressDialog->setMinimumDuration(0);
  progressDialog->show();

  bglReaderOpts.setSceneryFile(simulators.value(loadingFsType).sceneryCfg);
  bglReaderOpts.setBasepath(simulators.value(loadingFsType).basePath);

  QElapsedTimer timer;
  bglReaderOpts.setProgressCallback(std::bind(&DatabaseManager::progressCallback, this,
                                              std::placeholders::_1, timer));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  try
  {
    // Create a backup of the database and delete the original file
    backupDatabaseFile();

    atools::fs::NavDatabase nd(&bglReaderOpts, db);
    nd.create();
  }
  catch(atools::Exception& e)
  {
    // Show dialog if something went wrong
    ErrorHandler(databaseDialog).handleException(e);
    success = false;
  }
  catch(...)
  {
    // Show dialog if something went wrong
    ErrorHandler(databaseDialog).handleUnknownException();
    success = false;
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

  if(!success)
    // Something went wrong of loading was cancelled - restore backup
    restoreDatabaseFileBackup();

  // Delete backup file anyway after success of restore
  removeDatabaseFileBackup();

  delete progressDialog;
  progressDialog = nullptr;

  return success;
}

/* Simulator was changed in scenery database loading dialog */
void DatabaseManager::simulatorChangedFromComboBox(FsPaths::SimulatorType value)
{
  // Close and reopen database to show statistics - but do not let the rest of the program know about it (no emit)
  closeDatabase();
  loadingFsType = value;
  databaseFile = buildDatabaseFileName(loadingFsType);
  openDatabase();
  updateDialogInfo();
}

/* Closes database, creates a database backup and reopens the database.
 * Result is a fresh database without schema. */
void DatabaseManager::backupDatabaseFile()
{
  qDebug() << "Creating database backup";
  db->close();

  QString backupName(db->databaseName() + lnm::DATABASE_BACKUP_SUFFIX);
  QFile backupFile(backupName);
  bool removed = backupFile.remove();
  qDebug() << "removed database backup" << backupFile.fileName() << removed;

  QFile dbFile(db->databaseName());
  bool renamed = dbFile.rename(backupName);
  qDebug() << "renamed database from" << db->databaseName() << "to" << backupName << renamed;

  db->open();
}

/* Closes database, restores the database backup and reopens the database. */
void DatabaseManager::restoreDatabaseFileBackup()
{
  qDebug() << "Restoring database backup";
  db->close();

  QFile dbFile(db->databaseName());
  bool removed = dbFile.remove();
  qDebug() << "removed database" << dbFile.fileName() << removed;

  QString backupName(db->databaseName() + lnm::DATABASE_BACKUP_SUFFIX);
  QFile backupFile(backupName);
  bool copied = backupFile.copy(db->databaseName());
  qDebug() << "copied database from" << backupName << "to" << db->databaseName() << copied;

  db->open();
}

/* Removes the database backup */
void DatabaseManager::removeDatabaseFileBackup()
{
  qDebug() << "Removing database backup";
  QString backupName(db->databaseName() + lnm::DATABASE_BACKUP_SUFFIX);
  QFile backupFile(backupName);
  bool removed = backupFile.remove();
  qDebug() << "removed database" << backupFile.fileName() << removed;
}

/* Called by atools::fs::NavDatabase. Updates progress bar and statistics */
bool DatabaseManager::progressCallback(const atools::fs::NavDatabaseProgress& progress,
                                       QElapsedTimer& timer)
{
  if(progress.isFirstCall())
  {
    timer.start();
    progressDialog->setMinimum(0);
    progressDialog->setMaximum(progress.getTotal());
  }
  progressDialog->setValue(progress.getCurrent());

  if(progress.isNewOther())
    // Run script etc.
    progressDialog->setLabelText(
      DATABASE_TIME_TEXT.arg(progress.getOtherAction()).
      arg(formatter::formatElapsed(timer)).
      arg(QString()).
      arg(QString()).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumWaypoints()));
  else if(progress.isNewSceneryArea() || progress.isNewFile())
    // Switched to a new scenery area
    progressDialog->setLabelText(
      DATABASE_FILE_TEXT.arg(progress.getSceneryTitle()).
      arg(progress.getSceneryPath()).
      arg(progress.getBglFilename()).
      arg(formatter::formatElapsed(timer)).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumWaypoints()));
  else if(progress.isLastCall())
    // Last report
    progressDialog->setLabelText(
      DATABASE_TIME_TEXT.arg(tr("Done")).
      arg(formatter::formatElapsed(timer)).
      arg(QString()).
      arg(QString()).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumWaypoints()));

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  return progressDialog->wasCanceled();
}

/* Checks if the current database has a schema. Exits program if this fails */
bool DatabaseManager::hasSchema()
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
bool DatabaseManager::hasData()
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
bool DatabaseManager::isDatabaseCompatible()
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
void DatabaseManager::createEmptySchema(atools::sql::SqlDatabase *sqlDatabase)
{
  try
  {
    NavDatabaseOptions opts;
    NavDatabase(&opts, sqlDatabase).createSchema();
    DatabaseMeta(sqlDatabase).updateVersion();
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
  s.setValue(lnm::DATABASE_LOADINGSIMULATOR, atools::fs::FsPaths::typeToShortName(loadingFsType));
}

void DatabaseManager::restoreState()
{
  Settings& s = Settings::instance();
  simulators = s.valueVar(lnm::DATABASE_PATHS).value<SimulatorTypeMap>();
  currentFsType =
    atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_SIMULATOR, QString()));
  loadingFsType = atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_LOADINGSIMULATOR));
}

/* Updates metadata, version and object counts in the scenery loading dialog */
void DatabaseManager::updateDialogInfo()
{
  QString metaText;
  DatabaseMeta dbmeta(db);
  if(!dbmeta.isValid())
    metaText = DATABASE_META_TEXT.arg(tr("None")).
               arg(tr("None")).
               arg(tr("None")).
               arg(DatabaseMeta::DB_VERSION_MAJOR).
               arg(DatabaseMeta::DB_VERSION_MINOR);
  else
    metaText = DATABASE_META_TEXT.
               arg(dbmeta.getLastLoadTime().isValid() ? dbmeta.getLastLoadTime().toString() : tr("None")).
               arg(dbmeta.getMajorVersion()).
               arg(dbmeta.getMinorVersion()).
               arg(DatabaseMeta::DB_VERSION_MAJOR).
               arg(DatabaseMeta::DB_VERSION_MINOR);

  QString tableText;
  if(hasSchema())
  {
    atools::sql::SqlUtil util(db);

    // Get row counts for the dialog
    tableText = DATABASE_INFO_TEXT.arg(util.rowCount("bgl_file")).
                arg(util.rowCount("airport")).
                arg(util.rowCount("vor")).
                arg(util.rowCount("ils")).
                arg(util.rowCount("ndb")).
                arg(util.rowCount("marker")).
                arg(util.rowCount("waypoint"));
  }
  else
    tableText = DATABASE_INFO_TEXT.arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0);

  databaseDialog->setHeader(metaText + tr("<p><b>Currently Loaded:</b></p><p>%1</p>").arg(tableText));
}

/* Create database name including simulator short name */
QString DatabaseManager::buildDatabaseFileName(atools::fs::FsPaths::SimulatorType type)
{
  return databaseDirectory + QDir::separator() + lnm::DATABASE_PREFIX +
         atools::fs::FsPaths::typeToShortName(type).toLower() + lnm::DATABASE_SUFFIX;
}

void DatabaseManager::freeActions()
{
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
  {
    if(QFile::exists(buildDatabaseFileName(type)))
      // Already present or not - update database status since file exists
      simulators[type].hasDatabase = true;
    else
    {
      if(simulators.contains(type))
      {
        // No database found and no registry entry - remove
        const FsPathType& path = simulators.value(type);
        if(!path.hasRegistry)
          simulators.remove(type);
      }
      // else no database but simulator install - leave alone
    }
  }
}
