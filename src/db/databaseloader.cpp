/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "app/navapp.h"
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
#include "options/optiondata.h"
#include "options/optionsdialog.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "ui_mainwindow.h"
#include "win/activationcontext.h"

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
  // General contents table also shown in the loading dialog ====================
  /* *INDENT-OFF* */
 databaseInfoText = QStringLiteral("<table><tbody>"
  "<tr>"
      "<td width=\"60\"><b>%2</b></td>" // Files
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%3</td>"
      "<td width=\"60\"><b>%4</b></td>" // VOR
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%5</td>"
      "<td width=\"60\"><b>%6</b></td>" // Markers
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%7</td>"
    "</tr>"
    "<tr>"
      "<td width=\"60\"><b>%8</b></td>"// Airports
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%9</td>"
      "<td width=\"60\"><b>%10</b></td>" // ILS
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%11</td>"
      "<td width=\"60\"><b>%12</b></td>" //Waypoints
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%13</td>"
    "</tr>"
    "<tr>"
      "<td width=\"60\"></td>"
      "<td width=\"60\"></td>"
      "<td width=\"60\"><b>%14</b></td>" // NDB
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%15</td>"
      "<td width=\"60\"><b>%16</b></td>"//Airspaces
      "<td width=\"60\">&nbsp;&nbsp;&nbsp;&nbsp;%17</td>"
    "</tr>"
  "</tbody></table>");
  /* *INDENT-ON* */

  // Header prefixed by errors, scenery files and other ================
  databaseTextLoading = QObject::tr("%1<br/><br/><b>Found:</b>") % databaseInfoText;

  // Initialize aggregated classes used in thread
  navDatabaseErrors = new atools::fs::NavDatabaseErrors;
  navDatabaseOpts = new atools::fs::NavDatabaseOptions;
  progressShared = new atools::fs::NavDatabaseProgress;

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
  delete progressShared;
}

void DatabaseLoader::loadScenery()
{
  if(selectedFsType == atools::fs::FsPaths::MSFS_2024)
  {
    // Disable loading and pause SimConnect to avoid conflicts
    // Keep menu items disabled to not let user select a MSFS connection
    NavApp::getMainUi()->actionConnectSimulator->setDisabled(true);
    NavApp::getMainUi()->actionConnectSimulatorToggle->setDisabled(true);
    NavApp::pauseSimConnect();
  }

  // Get configuration file path from resources or overloaded path
  QString config = Settings::getOverloadedPath(lnm::NAVDATAREADER_CONFIG);
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
  navDatabaseOpts->setLanguage(OptionData::getLanguageFromConfigFile());

  // Add exclude paths from option dialog ===================
  const OptionData& optionData = OptionData::instance();

  // Add include directories ================================================
  for(const QString& path : optionData.getDatabaseInclude())
    navDatabaseOpts->addIncludeGui(QFileInfo(path));

  // Add excludes for files and directories ================================================
  for(const QString& path : optionData.getDatabaseExclude())
    navDatabaseOpts->addExcludeGui(QFileInfo(path));

  // Add add-on excludes for files and directories ================================================
  for(const QString& path : optionData.getDatabaseAddonExclude())
    navDatabaseOpts->addAddonExcludeGui(QFileInfo(path));

  // Select simulator db to load
  navDatabaseOpts->setSimulatorType(selectedFsType);

  // New progress dialog
  deleteProgressDialog();
  // No parent to allow non-modal dialog
  progressDialog = new DatabaseProgressDialog(nullptr, atools::fs::FsPaths::typeToShortDisplayName(selectedFsType));

  QString basePath = simulators.value(selectedFsType).basePath;
  navDatabaseOpts->setSceneryFile(simulators.value(selectedFsType).sceneryCfg);
  navDatabaseOpts->setBasepath(basePath);

  // Clear defaults
  navDatabaseOpts->setMsfsCommunityPath(QStringLiteral());
  navDatabaseOpts->setMsfsOfficialPath(QStringLiteral());

  // Set MSFS pecularities
  if(selectedFsType == atools::fs::FsPaths::MSFS)
  {
    navDatabaseOpts->setMsfsCommunityPath(FsPaths::getMsfsCommunityPath(basePath));
    navDatabaseOpts->setMsfsOfficialPath(FsPaths::getMsfsOfficialPath(basePath));
  }

  // Reset timers used in progress callback in thread context
  progressTimerElapsedThread = 0L;
  timerThread.restart();

  // Initial text
  progressDialog->setLabelText(labelText(databaseTextLoading, {tr("<b>Counting files ...</b>")}));

  // Dialog does not close when clicking cancel
  progressDialog->show();

  navDatabaseOpts->setProgressCallback(std::bind(&DatabaseLoader::progressCallbackThread, this, std::placeholders::_1));
  navDatabaseOpts->setCallDefaultCallback(true);
  navDatabaseOpts->setTimeZoneDatabase(lnm::TIMEZONE_DATABASE);

  // ==================================================================================
  // Compile navdata in background ==================================================================
  atools::fs::NavDatabase navDatabase(*navDatabaseOpts, *compileDb, navDatabaseErrors, GIT_REVISION_LITTLENAVMAP);

  // Load MSFS 2024 SimConnect DLL since only this can be used to fetch facilities
  // The library will be freed after loading in compileDatabasePost()
  atools::win::ActivationContext *activationContext = NavApp::getActivationContext();
  activationContext->loadLibrary(lnm::SIMCONNECT_LOADER_DLL_NAME);
  navDatabase.setActivationContext(activationContext, lnm::SIMCONNECT_LOADER_DLL_NAME);

  // resultFlags = navDatabase.compileDatabase();
  future = QtConcurrent::run(&atools::fs::NavDatabase::compileDatabase, navDatabase);

  // Calls DatabaseLoader::compileDatabasePost()
  watcher.setFuture(future);
}

void DatabaseLoader::compileDatabasePost()
{
  qDebug() << Q_FUNC_INFO;

  if(selectedFsType == atools::fs::FsPaths::MSFS_2024)
  {
    // Start MSFS 2020 SimConnect again and enable menu items
    NavApp::resumeSimConnect();
    NavApp::getMainUi()->actionConnectSimulator->setEnabled(true);
    NavApp::getMainUi()->actionConnectSimulatorToggle->setEnabled(true);
  }

  // Exceptions are passed from thread and are thrown again when calling future.result()
  try
  {
    resultFlagsShared = future.result();
  }
  catch(std::exception& e)
  {
    // Show dialog if something went wrong but do not exit program
    ErrorHandler(progressDialog).handleException(e, currentBglFilePath.isEmpty() ?
                                                 QStringLiteral() : tr("Processed files:\n%1\n").arg(currentBglFilePath));
    resultFlagsShared |= atools::fs::COMPILE_FAILED;
  }

  // Unload MSFS 2024 DLL again
  NavApp::getActivationContext()->freeLibrary(lnm::SIMCONNECT_LOADER_DLL_NAME);

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
      QWidget *mainwindow = NavApp::getQMainWidget();
      // Use database clicked - raise main window
      mainwindow->setWindowState((mainwindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
      mainwindow->activateWindow();
      mainwindow->raise();
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
    QReadLocker locker(&progressSharedLock);
    if(progressShared->isFirstCall())
    {
      qDebug() << Q_FUNC_INFO << "first call";
      timer.start();
      progressDialog->setValue(progressShared->getCurrent());
      progressDialog->setMinimum(0);
      progressDialog->setMaximum(progressShared->getTotal());
    }

    progressDialog->setValue(progressShared->getCurrent());

    if(progressShared->isNewOther())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "running other";
#endif

      currentBglFilePath.clear();

      // Run script etc.
      progressDialog->setLabelText(labelText(databaseTextLoading, {
        QStringLiteral("<b>%1</b>").arg(atools::elideTextShortMiddle(progressShared->getOtherAction(), MAX_TEXT_LENGTH)),
        progressShared->isNavigraphDetected() ? tr("<b>Navigraph navdata update found in simulator.</b>") : QStringLiteral(),
        tr("<b>Time:</b> %1").arg(formatter::formatElapsed(timer)),
        tr("<b>Errors:</b> %1").arg(QLocale().toString(progressShared->getNumErrors()))
      }));
    }
    else if(progressShared->isNewSceneryArea() || progressShared->isNewFile())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "running new file or scenery area";
#endif

      currentBglFilePath = progressShared->getFilePath();

      QString path = atools::nativeCleanPath(progressShared->getSceneryPath());
      QString file = progressShared->getFileName();

      // Switched to a new scenery area
      progressDialog->setLabelText(labelText(databaseTextLoading, {
        tr("<b>Scenery:</b> %1 (%2)").
        arg(atools::elideTextShortMiddle(progressShared->getSceneryTitle(), MAX_TEXT_LENGTH),
            atools::elideTextShortMiddle(path, MAX_TEXT_LENGTH)),
        file.isEmpty() ? QStringLiteral() : tr("<b>File:</b> %1").arg(atools::elideTextShortMiddle(file, MAX_TEXT_LENGTH)),
        progressShared->isNavigraphDetected() ? tr("<b>Navigraph navdata update found in simulator.</b>") : QStringLiteral(),
        tr("<b>Time:</b> %1").arg(formatter::formatElapsed(timer)),
        tr("<b>Errors:</b> %1").arg(QLocale().toString(progressShared->getNumErrors()))
      }));
    }
    else if(progressShared->isLastCall())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "last call";
#endif

      currentBglFilePath.clear();
      progressDialog->setValue(progressShared->getTotal());

      // Last report
      progressDialog->setLabelText(labelText(databaseTextLoading, {
        tr("<b>Done.</b>"),
        progressShared->isNavigraphDetected() ? tr("<b>Navigraph navdata update found in simulator.</b>") : QStringLiteral(),
        tr("<b>Time:</b> %1").arg(formatter::formatElapsed(timer)),
        tr("<b>Errors:</b> %1").arg(QLocale().toString(progressShared->getNumErrors()))
      }));
    }
  } // if(!progressDialog->wasCanceled())
  else
  {
    qDebug() << Q_FUNC_INFO << "canceled";

    // Set flags to canceled to that progressCallbackThread() can return false to quit compilation
    QWriteLocker locker(&resultFlagsSharedLock);
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
      QWriteLocker locker(&progressSharedLock);
      *progressShared = progress;
    }

    // Check if status indicates that user clicked cancel in progress dialog which is indicated in flags
    {
      QReadLocker locker(&resultFlagsSharedLock);
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
                 arg(atools::strJoin(headerText, tr(", "), tr(" and "))).arg(navDatabaseErrors->getSceneryErrors().size()));

    if(totalErrors > 0)
    {
      // Show contact for real errors =========================================
      texts.append(tr("<b>If you wish to report these errors attach the log and configuration files "
                        "to your report, add all other available information and send it to "
                        "the contact address below.</b>"
                        "<hr/>%1"
                          "<hr/>%2").
                   arg(atools::gui::Application::getContactHtml(), atools::gui::Application::getReportPathHtml()));

      texts.append(tr("<hr/>Some files or scenery directories could not be read.<br/>"
                      "You should check if the airports of the affected sceneries display "
                      "correctly and show the correct information.<hr/>"));
    }
    else if(totalNotes > 0)
      // Show normal header for encryption warnings =========================================
      texts.append(tr("<hr/>Some files or scenery directories could not be read properly "
                        "due to encrypted airport data.<hr/>"));

    int numScenery = 0;
    for(const atools::fs::SceneryErrors& sceneryError : navDatabaseErrors->getSceneryErrors())
    {
      if(numScenery >= MAX_ERROR_SCENERY_MESSAGES)
      {
        texts.append(tr("<b>More scenery entries ...</b>"));
        break;
      }

      int numBgl = 0;
      texts.append(tr("<b>Scenery Title: %1</b><br/>").arg(sceneryError.getScenery().getTitle()));

      for(const QString& err : sceneryError.getSceneryErrorsMessages())
        texts.append(err + "<br/>");

      for(const atools::fs::SceneryFileError& fileErr : sceneryError.getFileErrors())
      {
        if(numBgl >= MAX_ERROR_BGL_MESSAGES)
        {
          texts.append(tr("<b>More files ...</b>"));
          break;
        }
        numBgl++;

        texts.append(tr("<b>File:</b> \"%1\"<br/><b>Message:</b> %2<br/>").
                     arg(fileErr.getFilepath(), fileErr.getErrorMessage()));
      }
      texts.append("<br/>");
      numScenery++;
    }

    if(totalErrors > 0 || Settings::instance().valueBool(lnm::ACTIONS_SHOW_DATABASE_HINTS, true))
    {
      // Let the error dialog be a child of the progress dialog to avoid issues when other modal dialog windows are open
      TextDialog errorDialog(progressDialog, tr("%1 - Load Scenery Library Results").arg(QCoreApplication::applicationName()),
                             QStringLiteral("SCENERY.html#errors")); // anchor for future use

      // Block only the parent progress dialog
      errorDialog.setWindowModality(Qt::WindowModal);

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
      QWriteLocker locker(&resultFlagsSharedLock);
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

QString DatabaseLoader::getDatabaseInfoText(atools::fs::FsPaths::SimulatorType sim, bool navdataUdpdateDetected, int numFiles,
                                            int numAirport, int numVor, int numIls, int numNdb, int numMarker, int numWaypoint,
                                            int numBoundary) const
{
  QLocale locale;
  if(sim != atools::fs::FsPaths::NONE)
  {
    QString files, filesNum;
    if(sim != atools::fs::FsPaths::MSFS_2024)
    {
      files = tr("Files:");
      filesNum = locale.toString(numFiles);
    }
    // else no files in MSFS 2024

    // Count only airports in MSFS 2024 with navdata update
    if(navdataUdpdateDetected && sim == atools::fs::FsPaths::MSFS_2024)
      return databaseInfoText.arg(tr("Airports:"), locale.toString(numAirport), // Files
                                  QStringLiteral(), QStringLiteral(), // VOR
                                  QStringLiteral(), QStringLiteral(), // Marker
                                  QStringLiteral(), QStringLiteral(), // Airports
                                  QStringLiteral(), QStringLiteral(), // ILS
                                  QStringLiteral(), QStringLiteral(), // Waypoints
                                  QStringLiteral(), QStringLiteral(), // NDB
                                  QStringLiteral(), QStringLiteral()); // Airspaces
    else
      return databaseInfoText.arg(files, filesNum,
                                  tr("VOR:"), locale.toString(numVor),
                                  tr("Marker:"), locale.toString(numMarker),
                                  tr("Airports:"), locale.toString(numAirport),
                                  tr("ILS:"), locale.toString(numIls),
                                  tr("Waypoints:"), locale.toString(numWaypoint),
                                  tr("NDB:"), locale.toString(numNdb),
                                  tr("Airspaces:"), locale.toString(numBoundary));
  }
  else
    // Database is empty
    return databaseInfoText.arg(QStringLiteral(), QStringLiteral(), // Files
                                QStringLiteral(), QStringLiteral(), // VOR
                                QStringLiteral(), QStringLiteral(), // Marker
                                QStringLiteral(), QStringLiteral(), // Airports
                                QStringLiteral(), QStringLiteral(), // ILS
                                QStringLiteral(), QStringLiteral(), // Waypoints
                                QStringLiteral(), QStringLiteral(), // NDB
                                QStringLiteral(), QStringLiteral()); // Airspaces
}

QString DatabaseLoader::labelText(const QString& textTemplate, QStringList header) const
{
  QLocale locale;
  header.removeAll(QStringLiteral());

  QString files, filesNum;
  if(selectedFsType != atools::fs::FsPaths::MSFS_2024)
  {
    files = tr("Files:");
    filesNum = locale.toString(progressShared->getNumFiles());
  }

  return textTemplate.
         arg(atools::strJoin(header, QStringLiteral("<br/>"))).
         arg(files).arg(filesNum).
         arg(tr("Airports:")).arg(locale.toString(progressShared->getNumAirports())).
         arg(tr("VOR:")).arg(locale.toString(progressShared->getNumVors())).
         arg(tr("ILS:")).arg(locale.toString(progressShared->getNumIls())).
         arg(tr("NDB:")).arg(locale.toString(progressShared->getNumNdbs())).
         arg(tr("Markers:")).arg(locale.toString(progressShared->getNumMarker())).
         arg(tr("Waypoints:")).arg(locale.toString(progressShared->getNumWaypoints())).
         arg(tr("Airspaces:")).arg(locale.toString(progressShared->getNumBoundaries()));
}
