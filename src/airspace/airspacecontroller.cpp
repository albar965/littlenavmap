/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "airspace/airspacetoolbarhandler.h"
#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "db/airspacedialog.h"
#include "db/dbtools.h"
#include "exception.h"
#include "fs/common/metadatawriter.h"
#include "fs/userdata/airspacereaderivao.h"
#include "fs/userdata/airspacereaderopenair.h"
#include "fs/userdata/airspacereadervatsim.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "gui/textdialog.h"
#include "gui/widgetstate.h"
#include "query/airportquery.h"
#include "query/airspacequeries.h"
#include "query/airspacequery.h"
#include "query/querymanager.h"
#include "sql/sqltransaction.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"

#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QProgressDialog>

AirspaceController::AirspaceController(MainWindow *mainWindowParam)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{
  // Button and action handler =================================
  qDebug() << Q_FUNC_INFO;

  airspaceHandler = new AirspaceToolBarHandler(NavApp::getMainWindow());
  airspaceHandler->createToolButtons();

  connect(airspaceHandler, &AirspaceToolBarHandler::updateAirspaceTypes, this, &AirspaceController::updateAirspaceTypes);

  // Connect source selection actions ========================
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionViewAirspaceSrcSimulator, &QAction::toggled, this, &AirspaceController::sourceToggled);
  connect(ui->actionViewAirspaceSrcNavigraph, &QAction::toggled, this, &AirspaceController::sourceToggled);
  connect(ui->actionViewAirspaceSrcUser, &QAction::toggled, this, &AirspaceController::sourceToggled);
  connect(ui->actionViewAirspaceSrcOnline, &QAction::toggled, this, &AirspaceController::sourceToggled);
}

AirspaceController::~AirspaceController()
{
  ATOOLS_DELETE_LOG(airspaceHandler);
}

void AirspaceController::sourceToggled()
{
  AirspaceQueries *airspaceQueries = QueryManager::instance()->getQueriesGui()->getAirspaceQueries();
  Ui::MainWindow *ui = NavApp::getMainUi();

  airspaceQueries->setSourceFlag(map::AIRSPACE_SRC_SIM, ui->actionViewAirspaceSrcSimulator->isChecked());
  airspaceQueries->setSourceFlag(map::AIRSPACE_SRC_NAV, ui->actionViewAirspaceSrcNavigraph->isChecked());
  airspaceQueries->setSourceFlag(map::AIRSPACE_SRC_USER, ui->actionViewAirspaceSrcUser->isChecked());
  airspaceQueries->setSourceFlag(map::AIRSPACE_SRC_ONLINE, ui->actionViewAirspaceSrcOnline->isChecked());

  qDebug() << Q_FUNC_INFO << airspaceQueries->getSources();

  {
    // Need to synchronize web queries since threads are calling it
    QueryLocker locker(QueryManager::instance()->getQueriesWeb());
    locker.getLockable()->getAirspaceQueries()->setSources(airspaceQueries->getSources());
  }

  emit updateAirspaceSources(airspaceQueries->getSources());

  updateButtonsAndActions();
}

void AirspaceController::sourceToActions()
{
  AirspaceQueries *airspaceQueries = QueryManager::instance()->getQueriesGui()->getAirspaceQueries();
  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->actionViewAirspaceSrcSimulator->setChecked(airspaceQueries->hasSourceFlag(map::AIRSPACE_SRC_SIM));
  ui->actionViewAirspaceSrcNavigraph->setChecked(airspaceQueries->hasSourceFlag(map::AIRSPACE_SRC_NAV));
  ui->actionViewAirspaceSrcUser->setChecked(airspaceQueries->hasSourceFlag(map::AIRSPACE_SRC_USER));
  ui->actionViewAirspaceSrcOnline->setChecked(airspaceQueries->hasSourceFlag(map::AIRSPACE_SRC_ONLINE));
}

void AirspaceController::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::AIRSPACE_CONTROLLER_WIDGETS, false /* visibility */, true /* block signals */);
  state.restore({ui->actionViewAirspaceSrcSimulator, ui->actionViewAirspaceSrcNavigraph, ui->actionViewAirspaceSrcUser,
                 ui->actionViewAirspaceSrcOnline});

  sourceToggled();
}

void AirspaceController::saveState() const
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::AIRSPACE_CONTROLLER_WIDGETS).save(QList<const QObject *>({ui->actionViewAirspaceSrcSimulator,
                                                                                          ui->actionViewAirspaceSrcNavigraph,
                                                                                          ui->actionViewAirspaceSrcUser,
                                                                                          ui->actionViewAirspaceSrcOnline}));

}

void AirspaceController::optionsChanged()
{
  AirspaceQueries *airspaceQueries = QueryManager::instance()->getQueriesGui()->getAirspaceQueries();
  if(!airspaceQueries->isLoadingUserAirspaces())
  {
    airspaceQueries->initQueries();

    {
      // Need to synchronize web queries since threads are calling it
      QueryLocker locker(QueryManager::instance()->getQueriesWeb());
      locker.getLockable()->getAirspaceQueries()->initQueries();
    }
  }
}

void AirspaceController::preDatabaseLoad()
{
  AirspaceQueries *airspaceQueries = QueryManager::instance()->getQueriesGui()->getAirspaceQueries();
  // Avoid recursion from signal which is reflected by the database manager from
  // preDatabaseLoadAirspaces and postDatabaseLoadAirspaces
  if(!airspaceQueries->isLoadingUserAirspaces())
  {
    airspaceQueries->deInitQueries();

    {
      // Need to synchronize web queries since threads are calling it
      QueryLocker locker(QueryManager::instance()->getQueriesWeb());
      locker.getLockable()->getAirspaceQueries()->deInitQueries();
    }
  }
}

void AirspaceController::postDatabaseLoad()
{
  AirspaceQueries *airspaceQueries = QueryManager::instance()->getQueriesGui()->getAirspaceQueries();
  // Avoid recursion from signal which is reflected by the database manager from
  // preDatabaseLoadAirspaces and postDatabaseLoadAirspaces
  if(!airspaceQueries->isLoadingUserAirspaces())
  {
    airspaceQueries->initQueries();

    {
      // Need to synchronize web queries since threads are calling it
      QueryLocker locker(QueryManager::instance()->getQueriesWeb());
      locker.getLockable()->getAirspaceQueries()->initQueries();
    }
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

void AirspaceController::onlineClientAndAtcUpdated()
{
  AirspaceQueries *airspaceQueries = QueryManager::instance()->getQueriesGui()->getAirspaceQueries();
  if(!airspaceQueries->isLoadingUserAirspaces())
  {
    airspaceQueries->onlineClientAndAtcUpdated();

    {
      // Need to synchronize web queries since threads are calling it
      QueryLocker locker(QueryManager::instance()->getQueriesWeb());
      locker.getLockable()->getAirspaceQueries()->onlineClientAndAtcUpdated();
    }
  }
}

void AirspaceController::updateButtonsAndActions()
{
  airspaceHandler->updateButtonsAndActions();
}

void AirspaceController::loadAirspaces()
{
  qDebug() << Q_FUNC_INFO;

  using atools::fs::userdata::AirspaceReaderBase;

  AirspaceDialog dialog(mainWindow);
  int result = dialog.exec();

  if(result == QDialog::Accepted)
  {
    QString basePath = dialog.getAirspacePath();
    qDebug() << Q_FUNC_INFO << basePath;

    // Disable queries to avoid locked database
    emit preDatabaseLoadAirspaces();

    bool success = false;
    int sceneryId = 1, fileId = 1, numReadTotal = 0, numFiles = 0;
    QStringList errors;

    try
    {
      // Get database and drop and create schema =======================================
      atools::sql::SqlDatabase *dbUserAirspace = NavApp::getDatabaseUserAirspace();

      // Use a manual transaction
      atools::sql::SqlTransaction transaction(dbUserAirspace);

      dbtools::createEmptySchema(dbUserAirspace, true /* boundary */);

      // Write scenery area for display in information window =====================
      atools::fs::common::MetadataWriter metadataWriter(*dbUserAirspace);
      metadataWriter.writeSceneryArea(basePath, "User Airspaces", sceneryId);

      // Prepare filters and flags for folder search ========================
      QStringList filter = dialog.getAirspaceFilePatterns().simplified().split(" ");
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
      atools::fs::userdata::AirspaceReaderOpenAir openAirReader(dbUserAirspace);
      atools::fs::userdata::AirspaceReaderIvao ivaoReader(dbUserAirspace);
      atools::fs::userdata::AirspaceReaderVatsim vatsimReader(dbUserAirspace);

      int fileProgress = 0;
      bool canceled = false;
      QDirIterator dirIter(basePath, filter, filterFlags, iterFlags);
      int nextAirspaceId = 0;
      while(dirIter.hasNext())
      {
        int numReadFile = 0;
        QString file = dirIter.next();

        // Write file metadata for display in information window
        metadataWriter.writeFile(file, QString(), sceneryId, fileId);

        // Get first lines at beginning of file and remove empty lines
        AirspaceReaderBase::Format format = AirspaceReaderBase::detectFileFormat(file);

        // Check if file is IVAO JSON which starts with an array at top level
        if(format == AirspaceReaderBase::IVAO_JSON)
        {
          // Read IVAO JSON file =============================================================
          loadAirspace(ivaoReader, file, fileId, nextAirspaceId, numReadFile);
          collectErrors(errors, ivaoReader, basePath);
        }

        // Check if file is a GeoJSON which starts with an object at top level
        if(numReadFile == 0 && format == AirspaceReaderBase::VATSIM_GEO_JSON)
        {
          loadAirspace(vatsimReader, file, fileId, nextAirspaceId, numReadFile);
          collectErrors(errors, vatsimReader, basePath);
        }

        // OpenAir starts with a comment "*" or an upper case letter
        if(numReadFile == 0 && format == AirspaceReaderBase::OPEN_AIR)
        {
          // Read OpenAir file =============================================================
          loadAirspace(openAirReader, file, fileId, nextAirspaceId, numReadFile);
          collectErrors(errors, openAirReader, basePath);
        }

        progress.setValue(fileProgress++);

        if(progress.wasCanceled())
        {
          canceled = true;
          break;
        }

        fileId++;
        numReadTotal += numReadFile;
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
                           "\"%3\".").arg(numReadTotal).arg(numFiles).arg(basePath);

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
        for(const QString& err :  std::as_const(errors))
          html.li(err);
        html.olEnd();

        TextDialog error(mainWindow, QCoreApplication::applicationName() + tr(" - Errors"));
        error.setHtmlMessage(html.getHtml(), true /* print to log */);
        error.exec();
      }
      else
        // No errors ======================
        atools::gui::Dialog::information(mainWindow, message);
    }

    // Re-initialize queries again
    emit postDatabaseLoadAirspaces(NavApp::getCurrentSimulatorDb());

    // Let online controller update airspace shapes
    emit userAirspacesUpdated();
  }
}

void AirspaceController::loadAirspace(atools::fs::userdata::AirspaceReaderBase& reader, const QString& file, int fileId,
                                      int& nextAirspaceId, int& numReadFile)
{
  // Read OpenAir file =============================================================
  qDebug() << Q_FUNC_INFO << "Reading" << file << "as OpenAIR";
  reader.setFetchAirportCoords(std::bind(&AirspaceController::fetchAirportCoordinates, this, std::placeholders::_1));

  reader.setFileId(fileId);
  reader.setAirspaceId(nextAirspaceId);
  reader.readFile(file);
  numReadFile += reader.getNumAirspacesRead();
  nextAirspaceId = reader.getNextAirspaceId();
}

atools::geo::Pos AirspaceController::fetchAirportCoordinates(const QString& airportIdent)
{
  if(!NavApp::isLoadingDatabase())
  {
    const Queries *queries = QueryManager::instance()->getQueriesGui();
    if(atools::fs::FsPaths::isAnyXplane(NavApp::getCurrentSimulatorDb()))
      return queries->getAirportQuerySim()->getAirportPosByIdentOrIcao(airportIdent);
    else
      return queries->getAirportQuerySim()->getAirportPosByIdent(airportIdent);
  }
  else
    return atools::geo::EMPTY_POS;
}

void AirspaceController::collectErrors(QStringList& errors, const atools::fs::userdata::AirspaceReaderBase& reader, const QString& basePath)
{
  for(const atools::fs::userdata::AirspaceReaderOpenAir::AirspaceErr& err : reader.getErrors())
    errors.append(tr("File \"%1\" line %2: %3").arg(QDir(basePath).relativeFilePath(err.file)).arg(err.line).arg(err.message));
}
