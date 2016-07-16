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
  paths.fillDefault();

  // Find any stale databases that do not belong to a simulator
  fillPathsFromDatabases();

  for(atools::fs::FsPaths::SimulatorType t : paths.keys())
    qDebug() << t << paths.value(t);

  if(currentFsType == atools::fs::FsPaths::UNKNOWN || !paths.contains(currentFsType))
    currentFsType = paths.getBestSimulator();

  if(loadingFsType == atools::fs::FsPaths::UNKNOWN || !paths.getAllRegistryPaths().contains(loadingFsType))
    loadingFsType = paths.getBestLoadingSimulator();

  databaseFile = buildDatabaseFileName(currentFsType);

  qDebug() << "fs type" << currentFsType << "db file" << databaseFile;

  if(mainWindow != nullptr)
  {
    databaseDialog = new DatabaseDialog(mainWindow, paths);

    connect(databaseDialog, &DatabaseDialog::simulatorChanged, this,
            &DatabaseManager::simulatorChangedFromCombo);
  }

  if(!SqlDatabase::contains(DB_NAME))
    db = new SqlDatabase(SqlDatabase::addDatabase(DB_TYPE, DB_NAME));
}

DatabaseManager::~DatabaseManager()
{
  freeActions();

  delete databaseDialog;
  delete progressDialog;

  closeDatabase();
  delete db;
  SqlDatabase::removeDatabase(DB_NAME);
}

bool DatabaseManager::checkIncompatibleDatabases()
{
  bool ok = true;

  // Need empty block to delete sqlDb before removing driver
  {
    SqlDatabase sqlDb = SqlDatabase::addDatabase(DB_TYPE, "tempdb");
    QStringList databaseNames, databaseFiles;

    // Collect all incompatible databases
    for(atools::fs::FsPaths::SimulatorType type : paths.keys())
    {
      QString dbName = buildDatabaseFileName(type);
      if(QFile::exists(dbName))
      {
        sqlDb.setDatabaseName(dbName);
        sqlDb.open();

        DatabaseMeta meta(&sqlDb);
        if(!meta.hasSchema())
          createEmptySchema(&sqlDb);
        else if(!meta.isDatabaseCompatible())
        {
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

  if(paths.size() > 1)
  {
    group = new QActionGroup(menu);
    int index = 1;
    for(atools::fs::FsPaths::SimulatorType type : paths.keys())
    {
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

void DatabaseManager::updateSimSwitchActions()
{
  for(QAction *action : actions)
  {
    atools::fs::FsPaths::SimulatorType type = action->data().value<atools::fs::FsPaths::SimulatorType>();
    action->setChecked(type == currentFsType);
  }
}

void DatabaseManager::switchSimFromMainMenu()
{
  qDebug() << "switchSim";

  QAction *action = dynamic_cast<QAction *>(sender());

  if(action != nullptr)
  {
    emit preDatabaseLoad();

    closeDatabase();

    currentFsType = action->data().value<atools::fs::FsPaths::SimulatorType>();
    databaseFile = buildDatabaseFileName(currentFsType);
    openDatabase();

    emit postDatabaseLoad(currentFsType);
    mainWindow->setStatusMessage(tr("Switched to %1.").arg(FsPaths::typeToName(currentFsType)));
  }
}

void DatabaseManager::fillPathsFromDatabases()
{
  try
  {
    for(atools::fs::FsPaths::SimulatorType type : FsPaths::ALL_SIMULATOR_TYPES)
    {
      QString dbFile = buildDatabaseFileName(type);
      if(QFile::exists(dbFile))
      {
        // Already present or not - update database status
        FsPathType& path = paths[type];
        path.hasDatabase = true;
      }
      else
      {
        if(paths.contains(type))
        {
          // No database found and no registry entry - remove
          const FsPathType& path = paths.value(type);
          if(!path.hasRegistry)
            paths.remove(type);
        }
      }
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

void DatabaseManager::openDatabase()
{
  try
  {
    qDebug() << "Opening database" << databaseFile;
    db->setDatabaseName(databaseFile);

    if(Settings::instance().getAndStoreValue(lnm::OPTIONS_FOREIGNKEYS, false).toBool())
      db->open({"PRAGMA foreign_keys = ON"});
    else
      db->open({"PRAGMA foreign_keys = OFF"});

    atools::sql::SqlQuery query(db);
    query.exec("PRAGMA foreign_keys");
    if(query.next())
      qDebug() << "Foreign keys are" << query.value(0).toBool();

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

QString DatabaseManager::getSimShortName() const
{
  return atools::fs::FsPaths::typeToShortName(currentFsType);
}

atools::sql::SqlDatabase *DatabaseManager::getDatabase()
{
  return db;
}

void DatabaseManager::run()
{
  emit preDatabaseLoad();
  runNoMessages();
  emit postDatabaseLoad(currentFsType);
}

void DatabaseManager::runNoMessages()
{
  closeDatabase();
  databaseFile = buildDatabaseFileName(loadingFsType);
  openDatabase();

  databaseDialog->setCurrentFsType(loadingFsType);

  updateDialogInfo();

  bool loaded = false;
  // try until user hits cancel or the database was loaded successfully
  while(runInternal(loaded))
    ;

  closeDatabase();
  databaseFile = buildDatabaseFileName(currentFsType);
  openDatabase();
}

bool DatabaseManager::runInternal(bool& loaded)
{
  bool reopenDialog = true;
  loaded = false;
  try
  {
    int retval = databaseDialog->exec();
    // Copy the changed path structures
    updatePathsFromDialog();
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
            loaded = true;
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

bool DatabaseManager::loadScenery()
{
  using atools::fs::NavDatabaseOptions;

  bool success = true;
  QString config = Settings::getOverloadedPath(lnm::DATABASE_NAVDATAREADER_CONFIG);
  qInfo() << "loadScenery: Config file" << config;

  QSettings settings(config, QSettings::IniFormat);

  NavDatabaseOptions bglReaderOpts;
  bglReaderOpts.loadFromSettings(settings);

  const OptionData& optionData = OptionData::instance();
  bglReaderOpts.addToAddonDirectoryExcludes(optionData.getDatabaseAddonExclude());
  bglReaderOpts.addToDirectoryExcludes(optionData.getDatabaseExclude());

  delete progressDialog;
  progressDialog = new QProgressDialog(databaseDialog);

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

  bglReaderOpts.setSceneryFile(paths.value(loadingFsType).sceneryCfg);
  bglReaderOpts.setBasepath(paths.value(loadingFsType).basePath);

  QElapsedTimer timer;
  bglReaderOpts.setProgressCallback(std::bind(&DatabaseManager::progressCallback, this,
                                              std::placeholders::_1, timer));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  try
  {
    // Create a backup and delete the original file
    backupDatabaseFile();
    atools::fs::NavDatabase nd(&bglReaderOpts, db);
    nd.create();
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(databaseDialog).handleException(e);
    success = false;
  }
  catch(...)
  {
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
    restoreDatabaseFileBackup();

  removeDatabaseFileBackup();

  delete progressDialog;
  progressDialog = nullptr;

  return success;
}

void DatabaseManager::simulatorChangedFromCombo(FsPaths::SimulatorType value)
{
  closeDatabase();
  loadingFsType = value;
  databaseFile = buildDatabaseFileName(loadingFsType);
  openDatabase();
  updateDialogInfo();
}

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

void DatabaseManager::removeDatabaseFileBackup()
{
  qDebug() << "Removing database backup";
  QString backupName(db->databaseName() + lnm::DATABASE_BACKUP_SUFFIX);
  QFile backupFile(backupName);
  bool removed = backupFile.remove();
  qDebug() << "removed database" << backupFile.fileName() << removed;
}

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

bool DatabaseManager::hasRegistrySims() const
{
  return !paths.getAllRegistryPaths().isEmpty();
}

void DatabaseManager::saveState()
{
  Settings& s = Settings::instance();
  s.setValueVar(lnm::DATABASE_PATHS, QVariant::fromValue(paths));
  s.setValue(lnm::DATABASE_SIMULATOR, atools::fs::FsPaths::typeToShortName(currentFsType));
  s.setValue(lnm::DATABASE_LOADINGSIMULATOR, atools::fs::FsPaths::typeToShortName(loadingFsType));
}

void DatabaseManager::restoreState()
{
  Settings& s = Settings::instance();
  paths = s.valueVar(lnm::DATABASE_PATHS).value<FsPathTypeMap>();
  currentFsType =
    atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_SIMULATOR, QString()));
  loadingFsType = atools::fs::FsPaths::stringToType(s.valueStr(lnm::DATABASE_LOADINGSIMULATOR));
}

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

void DatabaseManager::updatePathsFromDialog()
{
  const FsPathTypeMap& dlgPaths = databaseDialog->getPaths();

  for(FsPaths::SimulatorType type : dlgPaths.keys())
  {
    if(paths.contains(type))
    {
      paths[type].basePath = dlgPaths.value(type).basePath;
      paths[type].sceneryCfg = dlgPaths.value(type).sceneryCfg;
    }
  }
}
