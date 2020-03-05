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

#include "airspace/airspacecontroller.h"

#include "sql/sqlrecord.h"
#include "query/airspacequery.h"
#include "geo/linestring.h"
#include "common/constants.h"
#include "db/databasemanager.h"
#include "airspace/airspacetoolbarhandler.h"
#include "navapp.h"
#include "ui_mainwindow.h"
#include "gui/widgetstate.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "options/optionsdialog.h"
#include "fs/userdata/airspacereaderopenair.h"
#include "sql/sqltransaction.h"
#include "gui/textdialog.h"
#include "util/htmlbuilder.h"
#include "fs/common/metadatawriter.h"
#include "exception.h"
#include "gui/errorhandler.h"

#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QProgressDialog>

AirspaceController::AirspaceController(MainWindow *mainWindowParam,
                                       atools::sql::SqlDatabase *dbSim, atools::sql::SqlDatabase *dbNav,
                                       atools::sql::SqlDatabase *dbUser, atools::sql::SqlDatabase *dbOnline)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{
  // Create all query objects =================================
  if(dbSim != nullptr)
    queries.insert(map::AIRSPACE_SRC_SIM, new AirspaceQuery(dbSim, map::AIRSPACE_SRC_SIM));
  if(dbNav != nullptr)
    queries.insert(map::AIRSPACE_SRC_NAV, new AirspaceQuery(dbNav, map::AIRSPACE_SRC_NAV));
  if(dbUser != nullptr)
    queries.insert(map::AIRSPACE_SRC_USER, new AirspaceQuery(dbUser, map::AIRSPACE_SRC_USER));
  if(dbOnline != nullptr)
    queries.insert(map::AIRSPACE_SRC_ONLINE, new AirspaceQuery(dbOnline, map::AIRSPACE_SRC_ONLINE));

  for(AirspaceQuery *q:queries.values())
    q->initQueries();

  // Button and action handler =================================
  qDebug() << Q_FUNC_INFO << "Creating InfoController";
  airspaceHandler = new AirspaceToolBarHandler(NavApp::getMainWindow());
  airspaceHandler->createToolButtons();

  connect(airspaceHandler, &AirspaceToolBarHandler::updateAirspaceTypes,
          this, &AirspaceController::updateAirspaceTypes);

  // Connect source selection actions ========================
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionViewAirspaceSrcSimulator, &QAction::toggled, this, &AirspaceController::sourceToggled);
  connect(ui->actionViewAirspaceSrcNavigraph, &QAction::toggled, this, &AirspaceController::sourceToggled);
  connect(ui->actionViewAirspaceSrcUser, &QAction::toggled, this, &AirspaceController::sourceToggled);
  connect(ui->actionViewAirspaceSrcOnline, &QAction::toggled, this, &AirspaceController::sourceToggled);
}

AirspaceController::~AirspaceController()
{
  qDebug() << Q_FUNC_INFO << "delete airspaceHandler";
  delete airspaceHandler;

  qDeleteAll(queries);
  queries.clear();
}

void AirspaceController::sourceToggled()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  sources.setFlag(map::AIRSPACE_SRC_SIM, ui->actionViewAirspaceSrcSimulator->isChecked());
  sources.setFlag(map::AIRSPACE_SRC_NAV, ui->actionViewAirspaceSrcNavigraph->isChecked());
  sources.setFlag(map::AIRSPACE_SRC_USER, ui->actionViewAirspaceSrcUser->isChecked());
  sources.setFlag(map::AIRSPACE_SRC_ONLINE, ui->actionViewAirspaceSrcOnline->isChecked());

  emit updateAirspaceSources(sources);
}

void AirspaceController::sourceToActions()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionViewAirspaceSrcSimulator->setChecked(sources & map::AIRSPACE_SRC_SIM);
  ui->actionViewAirspaceSrcNavigraph->setChecked(sources & map::AIRSPACE_SRC_NAV);
  ui->actionViewAirspaceSrcUser->setChecked(sources & map::AIRSPACE_SRC_USER);
  ui->actionViewAirspaceSrcOnline->setChecked(sources & map::AIRSPACE_SRC_ONLINE);
}

void AirspaceController::getAirspaceById(map::MapAirspace& airspace, map::MapAirspaceId id)
{
  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return;

  AirspaceQuery *query = queries.value(id.src);
  if(query != nullptr)
    query->getAirspaceById(airspace, id.id);
}

map::MapAirspace AirspaceController::getAirspaceById(map::MapAirspaceId id)
{
  map::MapAirspace airspace;
  getAirspaceById(airspace, id);
  return airspace;
}

bool AirspaceController::hasAirspaceById(map::MapAirspaceId id)
{
  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return false;

  AirspaceQuery *query = queries.value(id.src);
  if(query != nullptr)
    return query->hasAirspaceById(id.id);

  return false;
}

atools::sql::SqlRecord AirspaceController::getOnlineAirspaceRecordById(int airspaceId)
{
  // Only for online centers
  AirspaceQuery *query = queries.value(map::AIRSPACE_SRC_ONLINE);
  if(query != nullptr)
    return query->getOnlineAirspaceRecordById(airspaceId);
  else
    return atools::sql::SqlRecord();
}

atools::sql::SqlRecord AirspaceController::getAirspaceInfoRecordById(map::MapAirspaceId id)
{
  atools::sql::SqlRecord rec;

  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return rec;

  if(!(id.src & map::AIRSPACE_SRC_ONLINE))
  {
    AirspaceQuery *query = queries.value(id.src);
    if(query != nullptr)
      return query->getAirspaceInfoRecordById(id.id);
  }

  return rec;
}

void AirspaceController::getAirspacesInternal(AirspaceVector& airspaceVector, const Marble::GeoDataLatLonBox& rect,
                                              const MapLayer *mapLayer,
                                              map::MapAirspaceFilter filter,
                                              float flightPlanAltitude, bool lazy,
                                              map::MapAirspaceSources src)
{
  if((src& map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return;

  // Check if requested source and enabled sources overlap
  if(src & sources)
  {
    AirspaceQuery *query = queries.value(src);
    if(query != nullptr)
    {
      // Get airspaces from cache
      const QList<map::MapAirspace> *airspaces = query->getAirspaces(rect, mapLayer, filter, flightPlanAltitude, lazy);

      if(airspaces != nullptr)
      {
        // Append pointers
        for(const map::MapAirspace& airspace : *airspaces)
          airspaceVector.append(&airspace);
      }
    }
  }
}

void AirspaceController::getAirspaces(AirspaceVector& airspaces, const Marble::GeoDataLatLonBox& rect,
                                      const MapLayer *mapLayer, map::MapAirspaceFilter filter,
                                      float flightPlanAltitude, bool lazy,
                                      map::MapAirspaceSources src)
{
  // Merge airspace pointers from all sources/caches into one list
  for(map::MapAirspaceSources s : map::MAP_AIRSPACE_SRC_VALUES)
  {
    if(src & s)
      getAirspacesInternal(airspaces, rect, mapLayer, filter, flightPlanAltitude, lazy, s);
  }
}

const atools::geo::LineString *AirspaceController::getAirspaceGeometry(map::MapAirspaceId id)
{
  if((id.src & map::AIRSPACE_SRC_USER) && loadingUserAirspaces)
    // Avoid deadlock while loading user airspaces
    return nullptr;

  AirspaceQuery *query = queries.value(id.src);
  if(query != nullptr)
    return query->getAirspaceGeometryByName(id.id);

  return nullptr;
}

void AirspaceController::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::AIRSPACE_CONTROLLER_WIDGETS, false /* visibility */, true /* block signals */);
  state.restore({ui->actionViewAirspaceSrcSimulator, ui->actionViewAirspaceSrcNavigraph, ui->actionViewAirspaceSrcUser,
                 ui->actionViewAirspaceSrcOnline});

  sourceToggled();
}

void AirspaceController::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::AIRSPACE_CONTROLLER_WIDGETS).save({ui->actionViewAirspaceSrcSimulator,
                                                                   ui->actionViewAirspaceSrcNavigraph,
                                                                   ui->actionViewAirspaceSrcUser,
                                                                   ui->actionViewAirspaceSrcOnline});

}

void AirspaceController::optionsChanged()
{
  if(!loadingUserAirspaces)
  {
    for(AirspaceQuery *q:queries.values())
      // Also calls deinit before and clears caches
      q->initQueries();
  }
}

void AirspaceController::preDatabaseLoad()
{
  // Avoid recursion from signal which is reflected by the database manager from
  // preDatabaseLoadAirspaces and postDatabaseLoadAirspaces
  if(!loadingUserAirspaces)
  {
    for(AirspaceQuery *q:queries.values())
      q->deInitQueries();
  }
}

void AirspaceController::postDatabaseLoad()
{
  // Avoid recursion from signal which is reflected by the database manager from
  // preDatabaseLoadAirspaces and postDatabaseLoadAirspaces
  if(!loadingUserAirspaces)
  {
    for(AirspaceQuery *q:queries.values())
      q->initQueries();
  }
}

void AirspaceController::onlineClientAndAtcUpdated()
{
  if(queries.contains(map::AIRSPACE_SRC_ONLINE))
    queries.value(map::AIRSPACE_SRC_ONLINE)->clearCache();
}

void AirspaceController::resetAirspaceOnlineScreenGeometry()
{
  if(queries.contains(map::AIRSPACE_SRC_ONLINE))
  {
    queries.value(map::AIRSPACE_SRC_ONLINE)->deInitQueries();
    queries.value(map::AIRSPACE_SRC_ONLINE)->initQueries();
  }
}

void AirspaceController::resetSettingsToDefault()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionViewAirspaceSrcSimulator->setChecked(true);
  ui->actionViewAirspaceSrcNavigraph->setChecked(true);
  ui->actionViewAirspaceSrcUser->setChecked(true);
  ui->actionViewAirspaceSrcOnline->setChecked(true);
  updateButtonsAndActions();
}

void AirspaceController::updateButtonsAndActions()
{
  airspaceHandler->updateButtonsAndActions();
}

bool AirspaceController::hasAnyAirspaces() const
{
  for(map::MapAirspaceSources src : map::MAP_AIRSPACE_SRC_VALUES)
  {
    if((sources & src) && queries.contains(src) && queries.value(src)->hasAirspacesDatabase())
      return true;
  }
  return false;
}

void AirspaceController::loadAirspaces()
{
  qDebug() << Q_FUNC_INFO;

  // Get base path from options dialog ===============================
  QString basePath = OptionData::instance().getCacheUserAirspacePath();

  if(basePath.isEmpty() || !QFileInfo(basePath).exists() || !QFileInfo(basePath).isDir())
  {
    // Path is either empty or not set - allow user to select a (new) folder ==========================
    basePath = NavApp::getOptionsDialog()->selectCacheUserAirspace();

    /*: Make sure that dialog and tab name match translations in the options dialog */
    atools::gui::Dialog(mainWindow).showWarnMsgBox(lnm::ACTIONS_SHOW_USER_AIRSPACE_NOTE,
                                                   tr("You can change this path in the dialog <code>Options</code> "
                                                      "on the tab <code>Cache and Files</code>."),
                                                   tr("Do not &show this dialog again."));
  }

  if(!basePath.isEmpty())
  {
    qDebug() << Q_FUNC_INFO << basePath;

    // Disable queries to avoid locked database
    preLoadAirpaces();

    bool success = false;
    int sceneryId = 1, fileId = 1, numRead = 0, numFiles = 0;
    QStringList errors;

    try
    {
      // Get database and drop and create schema =======================================
      atools::sql::SqlDatabase *dbUserAirspace = NavApp::getDatabaseUserAirspace();

      // Use a manual transaction
      atools::sql::SqlTransaction transaction(dbUserAirspace);

      NavApp::getDatabaseManager()->createEmptySchema(dbUserAirspace, true /* boundary */);

      // Write scenery area for display in information window =====================
      atools::fs::common::MetadataWriter metadataWriter(*dbUserAirspace);
      metadataWriter.writeSceneryArea(basePath, "User Airspaces", sceneryId);

      // Prepare filters and flags for folder search ========================
      QStringList filter = OptionData::instance().getCacheUserAirspaceExtensions().simplified().split(" ");
      QDir::Filters filterFlags = QDir::Files | QDir::Hidden | QDir::System;
      QDirIterator::IteratorFlags iterFlags = QDirIterator::Subdirectories | QDirIterator::FollowSymlinks;

      // Count files for progress =================================================
      QDirIterator countDirIter(basePath, filter, filterFlags, iterFlags);
      while(countDirIter.hasNext())
      {
        countDirIter.next();
        numFiles++;
      }

      // Set up progress dialog ==================================================
      QProgressDialog progress(tr("Reading airspaces ..."), tr("&Cancel"), 0, numFiles, mainWindow);
      progress.setWindowModality(Qt::WindowModal);
      progress.setMinimumDuration(0);
      progress.show();

      // Read files ==================================================
      atools::fs::userdata::AirspaceReaderOpenAir writer(dbUserAirspace);
      int fileProgress = 0;
      bool canceled = false;
      QDirIterator dirIter(basePath, filter, filterFlags, iterFlags);
      while(dirIter.hasNext())
      {
        QString file = dirIter.next();
        qDebug() << Q_FUNC_INFO << file;

        // Write file metadata for display in information window
        metadataWriter.writeFile(file, QString(), sceneryId, fileId);

        // Read OpenAir file =============================================================
        writer.readFile(fileId, file);
        numRead += writer.getNumAirspacesRead();

        progress.setValue(fileProgress++);

        if(progress.wasCanceled())
        {
          canceled = true;
          break;
        }

        for(const atools::fs::userdata::AirspaceReaderOpenAir::AirspaceErr& err : writer.getErrors())
          errors.append(tr("File \"%1\" line %2: %3").
                        arg(QDir(basePath).relativeFilePath(err.file)).arg(err.line).arg(err.message));

        fileId++;
      }
      progress.setValue(numFiles);

      if(canceled)
        // User bailed out - restore previous state
        transaction.rollback();
      else
      {
        transaction.commit();
        success = true;
      }
    }
    catch(atools::Exception& e)
    {
      atools::gui::ErrorHandler(mainWindow).handleException(e);
    }
    catch(...)
    {
      atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    }

    // Show messages only if no exception and not canceled by user
    if(success)
    {
      QString message = tr("Loaded %1 airspaces from %2 files from base path\n"
                           "\"%3\".").arg(numRead).arg(numFiles).arg(basePath);

      if(!errors.isEmpty())
      {
        // Show error dialog =================================================
        if(errors.size() > 1000)
        {
          errors = errors.mid(0, 1000);
          errors.append(tr("More ..."));
        }

        atools::util::HtmlBuilder html(true);
        html.p(tr("User Airspaces"), atools::util::html::BOLD | atools::util::html::BIG);
        html.p(message);

        html.p(tr("Conversion Errors/Warnings"), atools::util::html::BOLD | atools::util::html::BIG);
        html.ol();
        for(const QString& err :  errors)
          html.li(err);
        html.olEnd();

        TextDialog error(mainWindow, QApplication::applicationName() + tr(" - Errors"));
        error.setHtmlMessage(html.getHtml(), true /* print to log */);
        error.exec();
      }
      else
        // No errors ======================
        QMessageBox::information(mainWindow, QApplication::applicationName(), message);

      // Let online controller update airspace shapes
      emit userAirspacesUpdated();
    }

    // Re-initialize queries again
    postLoadAirpaces();
  }
}

atools::geo::LineString *AirspaceController::getOnlineAirspaceGeoByFile(const QString& callsign)
{
  // Avoid deadlock while loading user airspaces
  if(!loadingUserAirspaces)
  {
    AirspaceQuery *query = queries.value(map::AIRSPACE_SRC_USER);
    if(query != nullptr)
      return query->getAirspaceGeometryByFile(callsign);
  }
  return nullptr;
}

atools::geo::LineString *AirspaceController::getOnlineAirspaceGeoByName(const QString& callsign,
                                                                        const QString& facilityType)
{
  if(!loadingUserAirspaces)
  {
    AirspaceQuery *query = queries.value(map::AIRSPACE_SRC_USER);
    if(query != nullptr)
      return query->getAirspaceGeometryByName(callsign, facilityType);
  }
  return nullptr;
}

void AirspaceController::preLoadAirpaces()
{
  loadingUserAirspaces = true;
  if(queries.contains(map::AIRSPACE_SRC_USER))
    queries.value(map::AIRSPACE_SRC_USER)->deInitQueries();

  emit preDatabaseLoadAirspaces();
}

void AirspaceController::postLoadAirpaces()
{
  if(queries.contains(map::AIRSPACE_SRC_USER))
    queries.value(map::AIRSPACE_SRC_USER)->initQueries();
  loadingUserAirspaces = false;

  emit postDatabaseLoadAirspaces(NavApp::getCurrentSimulatorDb());
}

QStringList AirspaceController::getAirspaceSourcesStr() const
{
  QStringList retval;
  for(map::MapAirspaceSources src : map::MAP_AIRSPACE_SRC_VALUES)
  {
    if(sources & src)
      retval.append(map::airspaceSourceText(src));
  }

  return retval;
}
