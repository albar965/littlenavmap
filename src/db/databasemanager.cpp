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
#include "databasemeta.h"
#include "db/databasedialog.h"
#include "logging/loggingdefs.h"
#include "settings/settings.h"
#include <fs/bglreaderoptions.h>
#include <fs/bglreaderprogressinfo.h>
#include "common/formatter.h"
#include "fs/fspaths.h"
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressDialog>
#include <QSettings>
#include <QApplication>

#include <fs/navdatabase.h>

#include <sql/sqlutil.h>

#include <gui/errorhandler.h>
#include <QFileInfo>
#include <QMessageBox>
#include <QList>
#include <QMenu>
#include <QDir>

using atools::gui::ErrorHandler;
using atools::sql::SqlUtil;
using atools::fs::FsPaths;
using atools::fs::BglReaderOptions;
using atools::fs::Navdatabase;
using atools::settings::Settings;
using atools::sql::SqlDatabase;

const QString meta("<p><b>Last Update: %1. Database Version: %2.%3. Program Version: %4.%5.</b></p>");

const QString table(QObject::tr("<table>"
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
                                      "<td width=\"60\"><b>Boundaries:</b>"
                                      "</td> <td width=\"60\">%L11"
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
                                    "<td width=\"60\"><b>Waypoints:"
                                    "</b>"
                                  "</td>  "
                                  "<td width=\"60\">%L12"
                                  "</td>"
                                "</tr>"
                              "</tbody>"
                            "</table>"
                                ));

const QString text(QObject::tr(
                     "<b>%1</b><br/><br/><br/>"
                     "<b>Time:</b> %2<br/>%3%4"
                     ) + table);

const QString textWithFile(QObject::tr(
                             "<b>Scenery:</b> %1 (%2)<br/>"
                             "<b>File:</b> %3<br/><br/>"
                             "<b>Time:</b> %4<br/>"
                             ) + table);

DatabaseManager::DatabaseManager(QWidget *parent)
  : QObject(parent), parentWidget(parent)
{
  restoreState();

  databaseDirectory = Settings::getPath() + QDir::separator() + "little_navmap_db";
  QDir().mkpath(databaseDirectory);

  paths.fillDefault();

  if(currentFsType == atools::fs::FsPaths::UNKNOWN)
    currentFsType = paths.getLatestSimulator();
  origFsType = currentFsType;

  updateDatabaseFileName();

  dlg = new DatabaseDialog(parentWidget, paths);
  dlg->setCurrentFsType(currentFsType);
  connect(dlg, &DatabaseDialog::simulatorChanged, this, &DatabaseManager::simulatorChanged);

  db = SqlDatabase::addDatabase("QSQLITE");
}

DatabaseManager::~DatabaseManager()
{
  freeActions();

  delete dlg;
  delete progressDialog;
  closeDatabase();
}

void DatabaseManager::updateDatabaseFileName()
{
  databaseFile = databaseDirectory + QDir::separator() + "little_navmap_" +
                 atools::fs::FsPaths::typeToString(currentFsType).toLower() + ".sqlite";
}

void DatabaseManager::freeActions()
{
  if(group != nullptr)
  {
    delete group;
    group = nullptr;
  }
  qDeleteAll(actions);
  actions.clear();
}

void DatabaseManager::insertSimSwitchActions(QAction *before, QMenu *menu)
{
  freeActions();

  group = new QActionGroup(menu);

  for(atools::fs::FsPaths::SimulatorType type : paths.keys())
  {
    QAction *action = new QAction(FsPaths::typeToName(type), menu);
    action->setData(QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));
    action->setCheckable(true);
    action->setActionGroup(group);

    if(type == currentFsType)
      action->setChecked(true);

    menu->insertAction(before, action);

    connect(action, &QAction::triggered, this, &DatabaseManager::switchSimFromMenu);

    actions.append(action);
  }
  menu->insertSeparator(before);
}

void DatabaseManager::updateSimSwitchActions()
{
  for(QAction *action : actions)
  {
    atools::fs::FsPaths::SimulatorType type = action->data().value<atools::fs::FsPaths::SimulatorType>();
    action->setChecked(type == currentFsType);
  }
}

void DatabaseManager::switchSimFromMenu()
{
  qDebug() << "switchSim";

  QAction *action = dynamic_cast<QAction *>(sender());

  if(action != nullptr)
  {
    emit preDatabaseLoad();

    closeDatabase();
    currentFsType = action->data().value<atools::fs::FsPaths::SimulatorType>();
    updateDatabaseFileName();
    openDatabase();

    emit postDatabaseLoad();
  }
}

void DatabaseManager::openDatabase()
{
  try
  {
    qDebug() << "Opening database" << databaseFile;
    db.setDatabaseName(databaseFile);

    if(Settings::instance().getAndStoreValue("Options/ForeignKeys", false).toBool())
      db.open({"PRAGMA foreign_keys = ON"});
    else
      db.open({"PRAGMA foreign_keys = OFF"});

    atools::sql::SqlQuery query(db);
    query.exec("PRAGMA foreign_keys");
    if(query.next())
      qDebug() << "Foreign keys are" << query.value(0).toBool();

    if(!hasSchema())
      createEmptySchema();
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(parentWidget).handleException(e, "While opening database");
  }
  catch(...)
  {
    ErrorHandler(parentWidget).handleUnknownException("While opening database");
  }
}

void DatabaseManager::closeDatabase()
{
  try
  {
    qDebug() << "Closing database" << databaseFile;
    db.close();
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(parentWidget).handleException(e, "While closing database");
  }
  catch(...)
  {
    ErrorHandler(parentWidget).handleUnknownException("While closing database");
  }
}

atools::sql::SqlDatabase *DatabaseManager::getDatabase()
{
  return &db;
}

void DatabaseManager::run()
{
  emit preDatabaseLoad();
  dlg->setCurrentFsType(currentFsType);
  updateDialogInfo();

  bool loaded = false;
  // try until user hits cancel or the database was loaded successfully
  while(runInternal(loaded))
    ;

  if(!loaded && currentFsType != origFsType)
    simulatorChanged(origFsType);

  emit postDatabaseLoad();
}

bool DatabaseManager::runInternal(bool& loaded)
{
  bool reopenDialog = true;
  loaded = false;
  try
  {
    int retval = dlg->exec();

    if(retval == QDialog::Accepted)
    {
      QString err;
      if(atools::fs::Navdatabase::isBasePathValid(dlg->getBasePath(), err))
      {
        if(atools::fs::Navdatabase::isSceneryConfigValid(dlg->getSceneryConfigFile(), err))
        {
          // Copy the changed path structures
          paths = dlg->getPaths();
          if(loadScenery())
          {
            // Successfully loaded
            DatabaseMeta dbmeta(&db);
            dbmeta.updateVersion(DB_VERSION_MAJOR, DB_VERSION_MINOR);
            dbmeta.updateTimestamp();
            reopenDialog = false;
            loaded = true;
          }
        }
        else
          QMessageBox::warning(dlg, QApplication::applicationName(),
                               tr("Cannot read \"%1\". Reason: %2.").arg(dlg->getSceneryConfigFile()).arg(err));
      }
      else
        QMessageBox::warning(dlg, QApplication::applicationName(),
                             tr("Cannot read \"%1\". Reason: %2.").arg(dlg->getBasePath()).arg(err));
    }
    else
      reopenDialog = false;
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(parentWidget).handleException(e);
  }
  catch(...)
  {
    ErrorHandler(parentWidget).handleUnknownException();
  }
  return reopenDialog;
}

bool DatabaseManager::loadScenery()
{
  using atools::fs::BglReaderOptions;

  bool success = true;
  QString config = Settings::getOverloadedPath(":/littlenavmap/resources/config/navdatareader.cfg");
  qInfo() << "loadScenery: Config file" << config;

  QSettings settings(config, QSettings::IniFormat);

  BglReaderOptions opts;
  opts.loadFromSettings(settings);

  delete progressDialog;
  progressDialog = new QProgressDialog(dlg);

  QLabel *label = new QLabel(progressDialog);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setIndent(10);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setMinimumWidth(600);

  progressDialog->setWindowModality(Qt::WindowModal);
  progressDialog->setLabel(label);
  progressDialog->setAutoClose(false);
  progressDialog->setAutoReset(false);
  progressDialog->setMinimumDuration(0);
  progressDialog->show();

  opts.setSceneryFile(paths.value(currentFsType).sceneryCfg);
  opts.setBasepath(paths.value(currentFsType).basePath);

  QElapsedTimer timer;
  opts.setProgressCallback(std::bind(&DatabaseManager::progressCallback, this, std::placeholders::_1, timer));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  try
  {
    // Create a backup and delete the original file
    backupDatabaseFile();
    atools::fs::Navdatabase nd(&opts, &db);
    nd.create();
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(dlg).handleException(e);
    success = false;
  }
  catch(...)
  {
    ErrorHandler(dlg).handleUnknownException();
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

  delete progressDialog;
  progressDialog = nullptr;

  return success;
}

void DatabaseManager::simulatorChanged(FsPaths::SimulatorType value)
{
  closeDatabase();
  currentFsType = value;
  updateDatabaseFileName();
  openDatabase();
  updateDialogInfo();
  updateSimSwitchActions();
}

void DatabaseManager::backupDatabaseFile()
{
  qDebug() << "Creating database backup";
  db.close();

  QString backupName(db.databaseName() + "-backup");
  QFile backupFile(backupName);
  bool removed = backupFile.remove();
  qDebug() << "removed database backup" << backupFile.fileName() << removed;

  QFile dbFile(db.databaseName());
  bool renamed = dbFile.rename(backupName);
  qDebug() << "renamed database from" << db.databaseName() << "to" << backupName << renamed;

  db.open();
}

void DatabaseManager::restoreDatabaseFileBackup()
{
  qDebug() << "Restoring database backup";
  db.close();

  QFile dbFile(db.databaseName());
  bool removed = dbFile.remove();
  qDebug() << "removed database" << dbFile.fileName() << removed;

  QString backupName(db.databaseName() + "-backup");
  QFile backupFile(backupName);
  bool copied = backupFile.copy(db.databaseName());
  qDebug() << "copied database from" << backupName << "to" << db.databaseName() << copied;

  db.open();
}

bool DatabaseManager::progressCallback(const atools::fs::BglReaderProgressInfo& progress,
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
      text.arg(progress.getOtherAction()).
      arg(formatter::formatElapsed(timer)).
      arg(QString()).
      arg(QString()).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumBoundaries()).
      arg(progress.getNumWaypoints()));
  else if(progress.isNewSceneryArea() || progress.isNewFile())
    progressDialog->setLabelText(
      textWithFile.arg(progress.getSceneryTitle()).
      arg(progress.getSceneryPath()).
      arg(progress.getBglFilename()).
      arg(formatter::formatElapsed(timer)).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumBoundaries()).
      arg(progress.getNumWaypoints()));
  else if(progress.isLastCall())
    progressDialog->setLabelText(
      text.arg(tr("Done")).
      arg(formatter::formatElapsed(timer)).
      arg(QString()).
      arg(QString()).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumBoundaries()).
      arg(progress.getNumWaypoints()));

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  return progressDialog->wasCanceled();
}

bool DatabaseManager::hasSchema()
{
  try
  {
    return SqlUtil(&db).hasTable("airport");
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(parentWidget).handleException(e);
  }
  catch(...)
  {
    ErrorHandler(parentWidget).handleUnknownException();
  }
  return false;
}

bool DatabaseManager::hasData()
{
  try
  {
    return hasSchema() && SqlUtil(&db).rowCount("airport") > 0;
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(parentWidget).handleException(e);
  }
  catch(...)
  {
    ErrorHandler(parentWidget).handleUnknownException();
  }
  return false;
}

bool DatabaseManager::isDatabaseCompatible()
{
  DatabaseMeta dbmeta(&db);
  if(dbmeta.isValid())
    return DB_VERSION_MAJOR == dbmeta.getMajorVersion();

  return false;
}

void DatabaseManager::createEmptySchema()
{
  try
  {
    BglReaderOptions opts;
    Navdatabase(&opts, &db).createSchema();
    DatabaseMeta(&db).updateVersion(DB_VERSION_MAJOR, DB_VERSION_MINOR);
  }
  catch(atools::Exception& e)
  {
    ErrorHandler(parentWidget).handleException(e);
  }
  catch(...)
  {
    ErrorHandler(parentWidget).handleUnknownException();
  }
}

void DatabaseManager::saveState()
{
  Settings& s = Settings::instance();
  s->setValue("Database/Paths", QVariant::fromValue(paths));
  s->setValue("Database/Simulator", atools::fs::FsPaths::typeToString(currentFsType));
}

void DatabaseManager::restoreState()
{
  Settings& s = Settings::instance();
  paths = s->value("Database/Paths").value<FsPathMapList>();
  currentFsType = atools::fs::FsPaths::stringToType(s->value("Database/Simulator",
                                                             atools::fs::FsPaths::UNKNOWN).toString());
}

void DatabaseManager::updateDialogInfo()
{
  QString metaText;
  DatabaseMeta dbmeta(&db);
  if(!dbmeta.isValid())
    metaText = meta.arg(tr("None")).
               arg(tr("None")).
               arg(tr("None")).
               arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR);
  else
    metaText = meta.
               arg(dbmeta.getLastLoadTime().isValid() ? dbmeta.getLastLoadTime().toString() : "None").
               arg(dbmeta.getMajorVersion()).
               arg(dbmeta.getMinorVersion()).
               arg(DB_VERSION_MAJOR).
               arg(DB_VERSION_MINOR);

  QString tableText;
  if(hasSchema())
  {
    atools::sql::SqlUtil util(&db);

    // Get row counts for the dialog
    tableText = table.arg(util.rowCount("bgl_file")).
                arg(util.rowCount("airport")).
                arg(util.rowCount("vor")).
                arg(util.rowCount("ils")).
                arg(util.rowCount("ndb")).
                arg(util.rowCount("marker")).
                arg(util.rowCount("boundary")).
                arg(util.rowCount("waypoint"));
  }
  else
    tableText = table.arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0);

  dlg->setHeader(metaText + "<p><b>Currently Loaded:</b></p><p>" + tableText + "</p>");
}
