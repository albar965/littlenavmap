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

#include "db/databaseloader.h"

#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "db/databaseprogressdialog.h"
#include "db/dbtools.h"
#include "fs/navdatabase.h"
#include "fs/navdatabaseerrors.h"
#include "fs/navdatabaseoptions.h"
#include "fs/navdatabaseprogress.h"
#include "gui/errorhandler.h"
#include "gui/textdialog.h"
#include "app/navapp.h"
#include "options/optionsdialog.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"

#include <QDir>
#include <QElapsedTimer>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

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

// Limit text length in dialog
const static int MAX_TEXT_LENGTH = 120;

// Limit number of messages in error dialog
const static int MAX_ERROR_BGL_MESSAGES = 400;
const static int MAX_ERROR_SCENERY_MESSAGES = 400;

// Update not more often than this rate
#ifdef DEBUG_INFORMATION
const static int UPDATE_RATE_MS = 100;
#else
const static int UPDATE_RATE_MS = 250;
#endif

DatabaseLoader::DatabaseLoader(QObject *parent)
  : QObject(parent)
{
  databaseInfoText = QObject::tr(
    "<table>"
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
    "<b>%1</b><br/>" // Scenery:
    "<br/><br/>" // File:
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

  // Initialize aggregated classes used in thread
  navDatabaseErrors = new atools::fs::NavDatabaseErrors;
  navDatabaseOpts = new atools::fs::NavDatabaseOptions;
  navDatabaseProgressShared = new atools::fs::NavDatabaseProgress;

  SqlDatabase::addDatabase(dbtools::DATABASE_TYPE, dbtools::DATABASE_NAME_LOADER);
  compileDb = new atools::sql::SqlDatabase(dbtools::DATABASE_NAME_LOADER);

  // Progress messages converting from thread to main thread context
  connect(this, &DatabaseLoader::progressCallbackSignal, this, &DatabaseLoader::progressCallback, Qt::QueuedConnection);
  connect(&watcher, &QFutureWatcher<atools::fs::ResultFlags>::finished, this, &DatabaseLoader::compileDatabasePost);
}

DatabaseLoader::~DatabaseLoader()
{
  // Send stop signal and wait for finish
  loadSceneryStop();

  delete compileDb;
  SqlDatabase::removeDatabase(dbtools::DATABASE_NAME_LOADER);
  deleteProgressDialog();

  delete navDatabaseOpts;
  delete navDatabaseErrors;
  delete navDatabaseProgressShared;
}

void DatabaseLoader::loadScenery()
{
  // Get configuration file path from resources or overloaded path
  QString config = Settings::getOverloadedPath(lnm::DATABASE_NAVDATAREADER_CONFIG);
  qInfo() << Q_FUNC_INFO << "Config file" << config << "Database" << compileDb->databaseName();

  dbtools::openDatabaseFile(compileDb, dbFilename, false /* readonly */, true /* createSchema */);

  // Clear errors ========================================
  *navDatabaseErrors = atools::fs::NavDatabaseErrors();

  // Prepare options ========================================
  *navDatabaseOpts = NavDatabaseOptions();
  QSettings settings(config, QSettings::IniFormat);
  navDatabaseOpts->loadFromSettings(settings);

  navDatabaseOpts->setReadInactive(readInactive);
  navDatabaseOpts->setReadAddOnXml(readAddOnXml);
  navDatabaseOpts->setLanguage(OptionsDialog::getLocale());

  // Add exclude paths from option dialog ===================
  const OptionData& optionData = OptionData::instance();

  // Add include directories ================================================
  for(const QString& path : optionData.getDatabaseInclude())
    navDatabaseOpts->addIncludeGui(path);

  // Add excludes for files and directories ================================================
  for(const QString& path : optionData.getDatabaseExclude())
    navDatabaseOpts->addExcludeGui(path);

  // Add add-on excludes for files and directories ================================================
  for(const QString& path : optionData.getDatabaseAddonExclude())
    navDatabaseOpts->addAddonExcludeGui(path);

  // Select simulator db to load
  navDatabaseOpts->setSimulatorType(selectedFsType);

  // New progress dialog
  deleteProgressDialog();
  // No parent to allow non-modal dialog
  progressDialog = new DatabaseProgressDialog(nullptr, atools::fs::FsPaths::typeToShortName(selectedFsType));

  // Add to dock handler to enable auto raise and closing on exit as well as applying stay-on-top status from main
  NavApp::addDialogToDockHandler(progressDialog);

  QString basePath = simulators.value(selectedFsType).basePath;
  navDatabaseOpts->setSceneryFile(simulators.value(selectedFsType).sceneryCfg);
  navDatabaseOpts->setBasepath(basePath);

  // Set MSFS pecularities
  if(selectedFsType == atools::fs::FsPaths::MSFS)
  {
    navDatabaseOpts->setMsfsCommunityPath(FsPaths::getMsfsCommunityPath(basePath));
    navDatabaseOpts->setMsfsOfficialPath(FsPaths::getMsfsOfficialPath(basePath));
  }
  else
  {
    navDatabaseOpts->setMsfsCommunityPath(QString());
    navDatabaseOpts->setMsfsOfficialPath(QString());
  }

  // Reset timers used in progress callback in thread context
  progressTimerElapsedThread = 0L;
  timerThread.restart();

  // Initial text
  progressDialog->setLabelText(databaseTimeText.arg(tr("Counting files ...")).
                               arg(QString()).arg(QString()).arg(QString()).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0).arg(0));

  // Dialog does not close when clicking cancel
  progressDialog->show();

  navDatabaseOpts->setProgressCallback(std::bind(&DatabaseLoader::progressCallbackThread, this, std::placeholders::_1));
  navDatabaseOpts->setCallDefaultCallback(true);

  // Print all options to log file =================================
  qInfo() << Q_FUNC_INFO << *navDatabaseOpts;

  // ==================================================================================
  // Compile navdata in background ==================================================================
  atools::fs::NavDatabase navDatabase(navDatabaseOpts, compileDb, navDatabaseErrors, GIT_REVISION_LITTLENAVMAP);

  // resultFlags = navDatabase.compileDatabase();
  future = QtConcurrent::run(navDatabase, &atools::fs::NavDatabase::compileDatabase);

  // Calls DatabaseLoader::compileDatabasePost()
  watcher.setFuture(future);
}

void DatabaseLoader::compileDatabasePost()
{
  qDebug() << Q_FUNC_INFO;

  // Exceptions are passed from thread and are thrown again when calling future.result()
  try
  {
    resultFlagsShared = future.result();
  }
  catch(std::exception& e)
  {
    // Show dialog if something went wrong but do not exit program
    NavApp::closeSplashScreen();
    ErrorHandler(progressDialog).handleException(e, currentBglFilePath.isEmpty() ?
                                                 QString() : tr("Processed files:\n%1\n").arg(currentBglFilePath));
    resultFlagsShared |= atools::fs::COMPILE_FAILED;
  }

  // Show errors that occured during loading, if any
  if(!resultFlagsShared.testFlag(atools::fs::COMPILE_CANCELED))
    showErrors();

  if(!resultFlagsShared.testFlag(atools::fs::COMPILE_CANCELED) && !resultFlagsShared.testFlag(atools::fs::COMPILE_FAILED))
  {
    // Show results and wait until user selects ok or cancel
    progressDialog->setFinishedState();
    NavApp::setStayOnTop(progressDialog);

    // Show dialog modal and wait for discard / use database answer
    int result = progressDialog->exec();

    if(result == QDialog::Rejected)
      // Discard clicked
      resultFlagsShared.setFlag(atools::fs::COMPILE_CANCELED);
    else if(result == QDialog::Accepted)
    {
      // Use database clicked - raise main window
      NavApp::getQMainWidget()->activateWindow();
      NavApp::getQMainWidget()->raise();
    }

  }
  // else reopen loading dialog

  deleteProgressDialog();

  dbtools::closeDatabaseFile(compileDb);

  emit loadingFinished();
}

void DatabaseLoader::progressCallback()
{
  if(progressDialog == nullptr)
    return;

  if(!progressDialog->wasCanceled())
  {
    QReadLocker locker(&progressLock);
    if(navDatabaseProgressShared->isFirstCall())
    {
      qDebug() << Q_FUNC_INFO << "first call";
      timer.start();
      progressDialog->setValue(navDatabaseProgressShared->getCurrent());
      progressDialog->setMinimum(0);
      progressDialog->setMaximum(navDatabaseProgressShared->getTotal());
    }

    progressDialog->setValue(navDatabaseProgressShared->getCurrent());

    if(navDatabaseProgressShared->isNewOther())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "running other";
#endif

      currentBglFilePath.clear();

      // Run script etc.
      progressDialog->setLabelText(
        databaseTimeText.arg(atools::elideTextShortMiddle(navDatabaseProgressShared->getOtherAction(), MAX_TEXT_LENGTH)).
        arg(formatter::formatElapsed(timer)).
        arg(QString()).
        arg(QString()).
        arg(navDatabaseProgressShared->getNumErrors()).
        arg(navDatabaseProgressShared->getNumFiles()).
        arg(navDatabaseProgressShared->getNumAirports()).
        arg(navDatabaseProgressShared->getNumVors()).
        arg(navDatabaseProgressShared->getNumIls()).
        arg(navDatabaseProgressShared->getNumNdbs()).
        arg(navDatabaseProgressShared->getNumMarker()).
        arg(navDatabaseProgressShared->getNumWaypoints()).
        arg(navDatabaseProgressShared->getNumBoundaries()));
    }
    else if(navDatabaseProgressShared->isNewSceneryArea() || navDatabaseProgressShared->isNewFile())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "running new file or scenery area";
#endif

      currentBglFilePath = navDatabaseProgressShared->getFilePath();

      QString path = atools::nativeCleanPath(navDatabaseProgressShared->getSceneryPath());
      QString file = navDatabaseProgressShared->getFileName();

      // Switched to a new scenery area
      progressDialog->setLabelText(
        databaseLoadingText.arg(atools::elideTextShortMiddle(navDatabaseProgressShared->getSceneryTitle(), MAX_TEXT_LENGTH)).
        arg(atools::elideTextShortMiddle(path, MAX_TEXT_LENGTH)).
        arg(atools::elideTextShortMiddle(file, MAX_TEXT_LENGTH)).
        arg(formatter::formatElapsed(timer)).
        arg(navDatabaseProgressShared->getNumErrors()).
        arg(navDatabaseProgressShared->getNumFiles()).
        arg(navDatabaseProgressShared->getNumAirports()).
        arg(navDatabaseProgressShared->getNumVors()).
        arg(navDatabaseProgressShared->getNumIls()).
        arg(navDatabaseProgressShared->getNumNdbs()).
        arg(navDatabaseProgressShared->getNumMarker()).
        arg(navDatabaseProgressShared->getNumWaypoints()).
        arg(navDatabaseProgressShared->getNumBoundaries()));
    }
    else if(navDatabaseProgressShared->isLastCall())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "last call";
#endif

      currentBglFilePath.clear();
      progressDialog->setValue(navDatabaseProgressShared->getTotal());

      // Last report
      progressDialog->setLabelText(
        databaseTimeText.arg(tr("<big>Done.</big>")).
        arg(formatter::formatElapsed(timer)).
        arg(QString()).
        arg(QString()).
        arg(navDatabaseProgressShared->getNumErrors()).
        arg(navDatabaseProgressShared->getNumFiles()).
        arg(navDatabaseProgressShared->getNumAirports()).
        arg(navDatabaseProgressShared->getNumVors()).
        arg(navDatabaseProgressShared->getNumIls()).
        arg(navDatabaseProgressShared->getNumNdbs()).
        arg(navDatabaseProgressShared->getNumMarker()).
        arg(navDatabaseProgressShared->getNumWaypoints()).
        arg(navDatabaseProgressShared->getNumBoundaries()));
    }
  } // if(!progressDialog->wasCanceled())
  else
  {
    qDebug() << Q_FUNC_INFO << "canceled";

    // Set flags to canceled to that progressCallbackThread() can return false to quit compilation
    QWriteLocker locker(&resultFlagsLock);
    resultFlagsShared.setFlag(atools::fs::COMPILE_CANCELED);
  }
}

bool DatabaseLoader::progressCallbackThread(const atools::fs::NavDatabaseProgress& progress)
{
  bool canceled = false;

  // Update only every UPDATE_RATE_MS or for first and last event
  if((timerThread.elapsed() - progressTimerElapsedThread) > UPDATE_RATE_MS || progress.isFirstCall() || progress.isLastCall())
  {
    progressTimerElapsedThread = timerThread.elapsed();

    // Copy progress to memeber
    {
      QWriteLocker locker(&progressLock);
      *navDatabaseProgressShared = progress;
    }

    // Check if status indicates that user clicked cancel in progress dialog which is indicated in flags
    {
      QReadLocker locker(&resultFlagsLock);
      canceled = resultFlagsShared.testFlag(atools::fs::COMPILE_CANCELED);
    }

    // Passed queued signal to main thread to allow dialog and window stuff in main event queue
    // Connected to progressCallback()
    emit progressCallbackSignal();
  }
  return canceled;
}

void DatabaseLoader::showErrors()
{
  qDebug() << Q_FUNC_INFO;

  if(navDatabaseErrors->getTotal() > 0)
  {
    int totalErrors = navDatabaseErrors->getTotalErrors();
    int totalNotes = navDatabaseErrors->getTotalWarnings();

    // Adjust text depending on warnings and errors =========================================
    QStringList headerText;

    if(totalErrors > 0)
      headerText.append(tr("%1 %2").arg(totalErrors).arg(totalErrors > 1 ? tr("errors") : tr("error")));

    if(totalNotes > 0)
      headerText.append(tr("%1 %2").arg(totalNotes).arg(totalNotes > 1 ?
                                                        tr("notes", "Database loading notes") :
                                                        tr("note", "Database loading notes")));

    QString texts;
    texts.append(tr("<h3>Found %1 in %2 scenery entries when loading the scenery database</h3>").
                 arg(atools::strJoin(headerText, tr(", "), tr(" and "))).arg(navDatabaseErrors->sceneryErrors.size()));

    if(totalErrors > 0)
    {
      // Show contact for real errors =========================================
      texts.append(tr("<b>If you wish to report these errors attach the log and configuration files "
                        "to your report, add all other available information and send it to "
                        "the contact address below.</b>"
                        "<hr/>%1"
                          "<hr/>%2").
                   arg(atools::gui::Application::getContactHtml()).
                   arg(atools::gui::Application::getReportPathHtml()));

      texts.append(tr("<hr/>Some files or scenery directories could not be read.<br/>"
                      "You should check if the airports of the affected sceneries display "
                      "correctly and show the correct information.<hr/>"));
    }
    else if(totalNotes > 0)
      // Show normal header for encryption warnings =========================================
      texts.append(tr("<hr/>Some files or scenery directories could not be read properly "
                        "due to encrypted airport data.<hr/>"));

    int numScenery = 0;
    for(const atools::fs::NavDatabaseErrors::SceneryErrors& scErr : qAsConst(navDatabaseErrors->sceneryErrors))
    {
      if(numScenery >= MAX_ERROR_SCENERY_MESSAGES)
      {
        texts.append(tr("<b>More scenery entries ...</b>"));
        break;
      }

      int numBgl = 0;
      texts.append(tr("<b>Scenery Title: %1</b><br/>").arg(scErr.scenery.getTitle()));

      for(const QString& err : scErr.sceneryErrorsMessages)
        texts.append(err + "<br/>");

      for(const atools::fs::NavDatabaseErrors::SceneryFileError& bglErr : scErr.fileErrors)
      {
        if(numBgl >= MAX_ERROR_BGL_MESSAGES)
        {
          texts.append(tr("<b>More files ...</b>"));
          break;
        }
        numBgl++;

        texts.append(tr("<b>File:</b> \"%1\"<br/><b>Message:</b> %2<br/>").
                     arg(bglErr.filepath).arg(bglErr.errorMessage));
      }
      texts.append("<br/>");
      numScenery++;
    }

    if(totalErrors > 0 || Settings::instance().valueBool(lnm::ACTIONS_SHOW_DATABASE_HINTS, true))
    {
      // Let the error dialog be a child of main to block main window
      TextDialog errorDialog(NavApp::getQMainWidget(), tr("%1 - Load Scenery Library Results").arg(QApplication::applicationName()),
                             "SCENERY.html#errors"); // anchor for future use

      // Show check box in case of notes
      errorDialog.setNotShowAgainCheckBoxVisible(totalNotes > 0);
      errorDialog.setNotShowAgainCheckBoxText(tr("&Do not show hints again and open this dialog only for errors."));

      errorDialog.setHtmlMessage(texts, true /* print to log */);

      NavApp::setStayOnTop(&errorDialog);
      errorDialog.exec();

      if(totalNotes > 0)
        // Save check box state in case of notes
        Settings::instance().setValue(lnm::ACTIONS_SHOW_DATABASE_HINTS, !errorDialog.isNotShowAgainChecked());
    }

    // Raise progress again
    progressDialog->activateWindow();
  }
}

void DatabaseLoader::loadSceneryStop()
{
  qDebug() << Q_FUNC_INFO << "stopping";
  if(future.isRunning())
  {
    {
      QWriteLocker locker(&resultFlagsLock);
      // Set cancel flag so progress handler in thread can return false to navdatabase compiler
      resultFlagsShared.setFlag(atools::fs::COMPILE_CANCELED);
    }

    try
    {
      qDebug() << Q_FUNC_INFO << "waiting for finish ...";
      future.waitForFinished();
      qDebug() << Q_FUNC_INFO << "done waiting";
    }
    catch(std::exception& e)
    {
      // Ignore exception on shutdown - only print to log
      qWarning() << Q_FUNC_INFO << e.what();
      resultFlagsShared |= atools::fs::COMPILE_FAILED;
    }

    deleteProgressDialog();
  }
}

void DatabaseLoader::deleteProgressDialog()
{
  NavApp::removeDialogFromDockHandler(progressDialog);
  delete progressDialog;
  progressDialog = nullptr;
}

bool DatabaseLoader::isLoadingProgress() const
{
  return progressDialog != nullptr && progressDialog->isVisible();
}

void DatabaseLoader::showProgressWindow() const
{
  if(progressDialog != nullptr && progressDialog->isVisible())
  {
    progressDialog->show();
    progressDialog->raise();
    progressDialog->activateWindow();
  }
}
