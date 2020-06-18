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

#include "logbook/logdatacontroller.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "exception.h"
#include "fs/userdata/logdatamanager.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/helphandler.h"
#include "gui/mainwindow.h"
#include "gui/textdialog.h"
#include "perf/aircraftperfcontroller.h"
#include "search/searchcontroller.h"
#include "logbook/logdataconverter.h"
#include "common/aircrafttrack.h"
#include "logbook/logdatadialog.h"
#include "logbook/logstatisticsdialog.h"
#include "zip/gzip.h"
#include "navapp.h"
#include "query/airportquery.h"
#include "route/route.h"
#include "route/routealtitude.h"
#include "search/logdatasearch.h"
#include "settings/settings.h"
#include "sql/sqlrecord.h"
#include "sql/sqltransaction.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"
#include "options/optiondata.h"
#include "fs/pln/flightplanio.h"
#include "fs/perf/aircraftperf.h"
#include "gui/choicedialog.h"
#include "gui/errorhandler.h"

#include <QDebug>
#include <QStandardPaths>

using atools::sql::SqlTransaction;
using atools::sql::SqlRecord;
using atools::geo::Pos;
using atools::fs::pln::FlightplanIO;

LogdataController::LogdataController(atools::fs::userdata::LogdataManager *logdataManager, MainWindow *parent)
  : manager(logdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
  statsDialog = new LogStatisticsDialog(mainWindow, this);

  connect(this, &LogdataController::logDataChanged, statsDialog, &LogStatisticsDialog::logDataChanged);
}

LogdataController::~LogdataController()
{
  delete statsDialog;
  delete aircraftAtTakeoff;
  delete dialog;
}

void LogdataController::showSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_LOG);
}

void LogdataController::saveState()
{
}

void LogdataController::restoreState()
{
}

void LogdataController::optionsChanged()
{
  statsDialog->optionsChanged();
}

void LogdataController::deleteLogEntryFromMap(int id)
{
  deleteLogEntries({id});
}

map::MapLogbookEntry LogdataController::getLogEntryById(int id)
{
  map::MapLogbookEntry obj;
  MapTypesFactory().fillLogbookEntry(manager->getRecord(id), obj);
  return obj;
}

atools::sql::SqlRecord LogdataController::getLogEntryRecordById(int id)
{
  return manager->getRecord(id);
}

void LogdataController::getFlightStatsTime(QDateTime& earliest, QDateTime& latest, QDateTime& earliestSim,
                                           QDateTime& latestSim)
{
  manager->getFlightStatsTime(earliest, latest, earliestSim, latestSim);
}

void LogdataController::getFlightStatsDistance(float& distTotal, float& distMax, float& distAverage)
{
  manager->getFlightStatsDistance(distTotal, distMax, distAverage);
}

void LogdataController::getFlightStatsAirports(int& numDepartAirports, int& numDestAirports)
{
  manager->getFlightStatsAirports(numDepartAirports, numDestAirports);
}

void LogdataController::getFlightStatsTripTime(float& timeMaximum, float& timeAverage, float& timeMaximumSim,
                                               float& timeAverageSim)
{
  manager->getFlightStatsTripTime(timeMaximum, timeAverage, timeMaximumSim, timeAverageSim);
}

void LogdataController::getFlightStatsAircraft(int& numTypes, int& numRegistrations, int& numNames, int& numSimulators)
{
  manager->getFlightStatsAircraft(numTypes, numRegistrations, numNames, numSimulators);
}

void LogdataController::getFlightStatsSimulator(QVector<std::pair<int, QString> >& numSimulators)
{
  manager->getFlightStatsSimulator(numSimulators);
}

void LogdataController::showStatistics()
{
  statsDialog->show();
}

atools::sql::SqlDatabase *LogdataController::getDatabase() const
{
  return manager->getDatabase();
}

void LogdataController::aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  createTakeoffLanding(aircraft, true /*takeoff*/, 0.f, 0.f);
}

void LogdataController::aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm,
                                        float averageTasKts)
{
  createTakeoffLanding(aircraft, false /*takeoff*/, flownDistanceNm, averageTasKts);
}

void LogdataController::createTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff,
                                             float flownDistanceNm, float averageTasKts)
{
  Q_UNUSED(averageTasKts)

  if(NavApp::getMainUi()->actionLogdataCreateLogbook->isChecked())
  {
    // Get nearest airport on takeoff/landing and runway
    map::MapRunwayEnd runwayEnd;
    map::MapAirport airport;
    if(!NavApp::getAirportQuerySim()->getBestRunwayEndForPosAndCourse(runwayEnd, airport,
                                                                      aircraft.getPosition(),
                                                                      aircraft.getTrackDegTrue()))
    {
      // Not even an airport was found
      qWarning() << Q_FUNC_INFO << "No runway found for aircraft"
                 << aircraft.getPosition() << aircraft.getTrackDegTrue();
      return;
    }

    QString departureArrivalText = takeoff ? tr("Departure") : tr("Arrival");
    QString runwayText = runwayEnd.isValid() ? tr(" runway %1").arg(runwayEnd.name) : QString();

    if(takeoff)
    {
      // Build record for new logbook entry =======================================
      SqlRecord record = manager->getEmptyRecord();
      record.setValue("aircraft_name", aircraft.getAirplaneType()); // varchar(250),
      record.setValue("aircraft_type", aircraft.getAirplaneModel()); // varchar(250),
      record.setValue("aircraft_registration", aircraft.getAirplaneRegistration()); // varchar(50),
      record.setValue("flightplan_number", aircraft.getAirplaneFlightnumber()); // varchar(100),
      record.setValue("flightplan_cruise_altitude", NavApp::getRouteCruiseAltFt()); // integer,
      record.setValue("flightplan_file", NavApp::getCurrentRouteFilepath()); // varchar(1024),
      record.setValue("performance_file", NavApp::getCurrentAircraftPerfFilepath()); // varchar(1024),
      record.setValue("block_fuel", NavApp::getAltitudeLegs().getBlockFuel(NavApp::getAircraftPerformance())); // integer,
      record.setValue("trip_fuel", NavApp::getAltitudeLegs().getTripFuel()); // integer,
      record.setValue("grossweight", aircraft.getAirplaneTotalWeightLbs()); // integer,
      record.setValue("departure_ident", airport.ident); // varchar(10),
      record.setValue("departure_name", airport.name); // varchar(200),
      record.setValue("departure_runway", runwayEnd.name); // varchar(200),
      record.setValue("departure_lonx", airport.position.getLonX()); // integer,
      record.setValue("departure_laty", airport.position.getLatY()); // integer,
      record.setValue("departure_alt", airport.position.getAltitude()); // integer,
      record.setValue("departure_time", QDateTime::currentDateTime()); // varchar(100),
      record.setValue("departure_time_sim", aircraft.getZuluTime()); // varchar(100),
      record.setValue("simulator", NavApp::getCurrentSimulatorName()); // varchar(50),
      record.setValue("route_string", NavApp::getRouteString()); // varchar(1024),

      // Record flight plan and aircraft performance =========================
      recordFlightplanAndPerf(record);

      // Determine fuel type =========================
      float weightVolRatio = 0.f;
      bool jetfuel = aircraft.isJetfuel(weightVolRatio);
      if(weightVolRatio > 0.f)
        record.setValue("is_jetfuel", jetfuel); // integer,

      // Add to database and remember created id
      SqlTransaction transaction(manager->getDatabase());
      manager->insertByRecord(record, &logEntryId);
      transaction.commit();

      logChanged(false /* load all */, true /* keep selection */);

      mainWindow->setStatusMessage(tr("Logbook Entry for %1 at %2%3 added.").
                                   arg(departureArrivalText).
                                   arg(airport.ident).
                                   arg(runwayText), true /* addToLog */);
    }
    else if(logEntryId >= 0)
    {
      // Update takeoff record with landing data ===========================================
      atools::sql::SqlRecord record = manager->getRecord(logEntryId);
      record.setValue("distance", NavApp::getRoute().getTotalDistance()); // integer,
      record.setValue("distance_flown", flownDistanceNm); // integer,
      if(aircraftAtTakeoff != nullptr)
        record.setValue("used_fuel", aircraftAtTakeoff->getFuelTotalWeightLbs() - aircraft.getFuelTotalWeightLbs()); // integer,
      record.setValue("destination_ident", airport.ident); // varchar(10),
      record.setValue("destination_name", airport.name); // varchar(200),
      record.setValue("destination_runway", runwayEnd.name); // varchar(200),
      record.setValue("destination_lonx", airport.position.getLonX()); // integer,
      record.setValue("destination_laty", airport.position.getLatY()); // integer,
      record.setValue("destination_alt", airport.position.getAltitude()); // integer,
      record.setValue("destination_time", QDateTime::currentDateTime()); // varchar(100),
      record.setValue("destination_time_sim", aircraft.getZuluTime()); // varchar(100),

      // Update flight plan and aircraft performance =========================
      recordFlightplanAndPerf(record);

      // Save GPX with simplified flight plan and trail =========================
      atools::geo::LineString track;
      QVector<quint32> timestamps;
      NavApp::getAircraftTrack().convert(&track, &timestamps);
      record.setValue("aircraft_trail",
                      FlightplanIO().saveGpxGz(NavApp::getRoute().adjustedToOptions(rf::DEFAULT_OPTS_GPX).getFlightplan(),
                                               track, timestamps,
                                               static_cast<int>(NavApp::getRouteConst().getCruisingAltitudeFeet()))); // blob

      // Determine fuel type again =========================
      float weightVolRatio = 0.f;
      bool jetfuel = aircraft.isJetfuel(weightVolRatio);
      if(weightVolRatio > 0.f)
        record.setValue("is_jetfuel", jetfuel); // integer,

      SqlTransaction transaction(manager->getDatabase());
      manager->updateByRecord(record, {logEntryId});
      transaction.commit();

      logChanged(false /* load all */, false /* keep selection */);

      mainWindow->setStatusMessage(tr("Logbook Entry for %1 at %2%3 updated.").
                                   arg(departureArrivalText).
                                   arg(airport.ident).
                                   arg(runwayText), true /* addToLog */);

      logEntryId = -1;
    }
    else
      qWarning() << Q_FUNC_INFO << "no previous takeoff";
  }

  resetTakeoffLandingDetection();

  if(takeoff)
    aircraftAtTakeoff = new atools::fs::sc::SimConnectUserAircraft(aircraft);
}

void LogdataController::logChanged(bool loadAll, bool keepSelection)
{
  // Clear cache and update map screen index
  manager->clearGeometryCache();
  emit logDataChanged();

  // Reload search
  emit refreshLogSearch(loadAll, keepSelection);
}

void LogdataController::recordFlightplanAndPerf(atools::sql::SqlRecord& record)
{
  atools::fs::pln::Flightplan fp = NavApp::getRoute().adjustedToOptions(rf::DEFAULT_OPTS_LNMPLN).getFlightplan();
  record.setValue("flightplan", FlightplanIO().saveLnmGz(fp)); // blob
  record.setValue("aircraft_perf", NavApp::getAircraftPerformance().saveXmlGz()); // blob
}

void LogdataController::resetTakeoffLandingDetection()
{
  delete aircraftAtTakeoff;
  aircraftAtTakeoff = nullptr;
}

bool LogdataController::isDirectPreviewShown()
{
  return NavApp::getMainUi()->actionSearchLogdataShowDirect->isChecked();
}

bool LogdataController::isRoutePreviewShown()
{
  return NavApp::getMainUi()->actionSearchLogdataShowRoute->isChecked();
}

bool LogdataController::isTrackPreviewShown()
{
  return NavApp::getMainUi()->actionSearchLogdataShowTrack->isChecked();
}

bool LogdataController::hasRouteAttached(int id)
{
  return manager->hasRouteAttached(id);
}

bool LogdataController::hasPerfAttached(int id)
{
  return manager->hasPerfAttached(id);
}

bool LogdataController::hasTrackAttached(int id)
{
  return manager->hasTrackAttached(id);
}

void LogdataController::preDatabaseLoad()
{
  // no-op
}

void LogdataController::postDatabaseLoad()
{
  manager->clearGeometryCache();
}

void LogdataController::displayOptionsChanged()
{
  manager->clearGeometryCache();
}

const atools::geo::LineString *LogdataController::getRouteGeometry(int id)
{
  const atools::fs::userdata::LogEntryGeometry *entry = manager->getGeometry(id);
  return entry != nullptr ? &entry->route : nullptr;
}

const atools::geo::LineString *LogdataController::getTrackGeometry(int id)
{
  const atools::fs::userdata::LogEntryGeometry *entry = manager->getGeometry(id);
  return entry != nullptr ? &entry->track : nullptr;
}

void LogdataController::editLogEntryFromMap(int id)
{
  qDebug() << Q_FUNC_INFO;
  editLogEntries({id});
}

void LogdataController::connectDialogSignals(LogdataDialog *dialog)
{
  connect(dialog, &LogdataDialog::planOpen, this, &LogdataController::planOpen);
  connect(dialog, &LogdataDialog::planAdd, this, &LogdataController::planAdd);
  connect(dialog, &LogdataDialog::planSaveAs, this, &LogdataController::planSaveAs);
  connect(dialog, &LogdataDialog::gpxAdd, this, &LogdataController::gpxAdd);
  connect(dialog, &LogdataDialog::gpxSaveAs, this, &LogdataController::gpxSaveAs);
  connect(dialog, &LogdataDialog::perfOpen, this, &LogdataController::perfOpen);
  connect(dialog, &LogdataDialog::perfAdd, this, &LogdataController::perfAdd);
  connect(dialog, &LogdataDialog::perfSaveAs, this, &LogdataController::perfSaveAs);
}

void LogdataController::editLogEntries(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO << ids;

  SqlRecord rec = manager->getRecord(ids.first());
  if(!rec.isEmpty())
  {
    LogdataDialog dlg(mainWindow, ids.size() == 1 ? ld::EDIT_ONE : ld::EDIT_MULTIPLE);
    connectDialogSignals(&dlg);
    dlg.restoreState();

    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      SqlTransaction transaction(manager->getDatabase());
      manager->updateByRecord(dlg.getRecord(), ids);
      transaction.commit();

      logChanged(false /* load all */, true /* keep selection */);

      mainWindow->setStatusMessage(tr("%1 logbook %2 updated.").
                                   arg(ids.size()).arg(ids.size() == 1 ? tr("entry") : tr("entries")));
    }
    dlg.saveState();
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record" << rec;
}

void LogdataController::addLogEntry()
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec = manager->getEmptyRecord();

  qDebug() << Q_FUNC_INFO << rec;

  LogdataDialog dlg(mainWindow, ld::ADD);
  connectDialogSignals(&dlg);
  dlg.restoreState();

  dlg.setRecord(rec);
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
    qDebug() << Q_FUNC_INFO << rec;

    // Add to database
    SqlTransaction transaction(manager->getDatabase());
    manager->insertByRecord(dlg.getRecord());
    transaction.commit();

    logChanged(false /* load all */, false /* keep selection */);

    mainWindow->setStatusMessage(tr("Logbook entry added."));
  }
  dlg.saveState();
}

void LogdataController::deleteLogEntries(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  QString txt = ids.size() == 1 ? tr("entry") : tr("entries");
  int retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Delete %1 logbook %2?").arg(ids.size()).arg(txt) +
                          tr("\n\nThis cannot be undone."), QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

  if(retval == QMessageBox::Yes)
  {
    SqlTransaction transaction(manager->getDatabase());
    manager->removeRows(ids);
    transaction.commit();

    logChanged(false /* load all */, false /* keep selection */);

    mainWindow->setStatusMessage(tr("%1 logbook %2 deleted.").arg(ids.size()).arg(txt));
  }
}

void LogdataController::importXplane()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE11);
    if(xpBasePath.isEmpty())
      xpBasePath = atools::documentsDir();
    else
      xpBasePath = atools::buildPathNoCase({xpBasePath, "Output", "logbooks"});

    QString file = dialog->openFileDialog(
      tr("Open X-Plane Logbook File"),
      tr("X-Plane Logbook Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_XPLANE_LOGBOOK), "Logdata/XPlane",
      xpBasePath);

    int numImported = 0;
    if(!file.isEmpty())
    {
      numImported += manager->importXplane(file, fetchAirportCoordinates);
      mainWindow->setStatusMessage(tr("Imported %1 %2 X-Plane logbook.").arg(numImported).
                                   arg(numImported == 1 ? tr("entry") : tr("entries")));

      logChanged(false /* load all */, false /* keep selection */);

      /*: The text "Imported from X-Plane logbook" has to match the one in atools::fs::userdata::LogdataManager::importXplane */
      emit showInSearch(map::LOGBOOK,
                        atools::sql::SqlRecord().appendFieldAndValue("description",
                                                                     tr("*Imported from X-Plane logbook*")),
                        false /* select */);
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
}

void LogdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString file = dialog->openFileDialog(
      tr("Open Logbook CSV File"),
      tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), "Logdata/Csv");

    int numImported = 0;
    if(!file.isEmpty())
    {
      numImported += manager->importCsv(file);
      mainWindow->setStatusMessage(tr("Imported %1 %2 from CSV file.").arg(numImported).
                                   arg(numImported == 1 ? tr("entry") : tr("entries")));
      mainWindow->showLogbookSearch();
      logChanged(false /* load all */, false /* keep selection */);
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
}

void LogdataController::exportCsv()
{

  qDebug() << Q_FUNC_INFO;
  try
  {
    // Checkbox ids
    enum
    {
      SELECTED, APPEND, HEADER, EXPORTPLAN, EXPORTPERF, EXPORTGPX
    };

    // Build a choice dialog with several checkboxes =========================
    ChoiceDialog choiceDialog(mainWindow, QApplication::applicationName() + tr(" - Logbook Export"),
                              QString(),
                              tr("Select export options for logbook"),
                              lnm::LOGDATA_EXPORT_CSV, "LOGBOOK.html#import-and-export");

    QString attachmentToolTip = tr("Content of attached file will be added to the exported CSV if selected.\n"
                                   "Note that not all programs will be able to read this.\n"
                                   "Columns will be empty on export if disabled.");

    int numSelected = NavApp::getLogdataSearch()->getSelectedRowCount();
    choiceDialog.addCheckBox(APPEND, tr("&Append to an already present file"),
                             tr("File header will be ignore if this is enabled."), false);
    choiceDialog.addCheckBox(SELECTED, tr("Export &selected entries only"), QString(), true,
                             numSelected == 0 /* disabled */);
    choiceDialog.addCheckBox(HEADER, tr("Add a &header to the first line"), QString(), false);
    choiceDialog.addLine();
    choiceDialog.addCheckBox(EXPORTPLAN, tr("&Flight plan in LNMPLN format"), attachmentToolTip, false);
    choiceDialog.addCheckBox(EXPORTPERF, tr("&Aircraft performance"), attachmentToolTip, false);
    choiceDialog.addCheckBox(EXPORTGPX, tr(
                               "&GPX file containing flight plan points and trail"), attachmentToolTip, false);
    choiceDialog.restoreState();

    // Disable/enable header depending on append option
    choiceDialog.getCheckBox(HEADER)->setDisabled(choiceDialog.isChecked(APPEND));
    ChoiceDialog *dlgPtr = &choiceDialog;
    connect(&choiceDialog, &ChoiceDialog::checkBoxToggled, [dlgPtr](int id, bool checked) {
      if(id == APPEND)
        dlgPtr->getCheckBox(HEADER)->setDisabled(checked);
    });

    if(choiceDialog.exec() == QDialog::Accepted)
    {
      QString file = dialog->saveFileDialog(
        tr("Export Logbook Entry CSV File"),
        tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), ".csv", "Logdata/Csv",
        QString(), QString(), choiceDialog.isChecked(APPEND),
        OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(choiceDialog.isChecked(SELECTED))
          ids = NavApp::getLogdataSearch()->getSelectedIds();

        int numExported = manager->exportCsv(file, ids,
                                             choiceDialog.isChecked(EXPORTPLAN),
                                             choiceDialog.isChecked(EXPORTPERF),
                                             choiceDialog.isChecked(EXPORTGPX),
                                             choiceDialog.isChecked(HEADER) && !choiceDialog.isChecked(APPEND),
                                             choiceDialog.isChecked(APPEND));
        mainWindow->setStatusMessage(tr("%1 logbook %2 exported.").
                                     arg(numExported).arg(numExported == 1 ? tr("entry") : tr("entries")));
      }
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
}

void LogdataController::convertUserdata()
{
  qDebug() << Q_FUNC_INFO;

  int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_LOGBOOK_CONVERSION,
                                          tr("This will convert all userpoints of type "
                                             "<code>Logbook</code> to logbook entries.<br/><br/>"
                                             "This works best if you did not modify the field "
                                             "<code>Description</code> in the userpoints and if "
                                               "you did not insert entries manually.<br/><br/>"
                                               "Note that not all fields can be converted automatically.<br/><br/>"
                                               "The created log entries can be found by searching"
                                               "for<br/><code>*Converted from userdata*</code><br/>"
                                               "in the field &quot;Remarks&quot;.<br/><br/>"
                                               "Continue?"),
                                          tr("Do not &show this dialog again and run the conversion in the future."),
                                          QMessageBox::Yes | QMessageBox::No | QMessageBox::Help,
                                          QMessageBox::No, QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    LogdataConverter converter(NavApp::getDatabaseUser(), manager, NavApp::getAirportQuerySim());

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // Do the conversion ===================================
    int numCreated = converter.convertFromUserdata();

    QString resultText = tr("Created %1 log entries.").arg(numCreated);

    if(!converter.getErrors().isEmpty())
    {
      // Show errors and warnings ======================
      atools::util::HtmlBuilder html(true);

      html.p(tr("Logbook Conversion"), atools::util::html::BOLD | atools::util::html::BIG);

      html.p(resultText);

      html.p(tr("Conversion Errors/Warnings"), atools::util::html::BOLD | atools::util::html::BIG);
      html.p(tr("Some warnings might appear because of terminated flights, "
                "repeated langings and/or takeoffs. These can be ignored."));
      html.ol();
      for(const QString& err : converter.getErrors())
        html.li(err, atools::util::html::NO_ENTITIES);
      html.olEnd();

      TextDialog error(mainWindow, QApplication::applicationName() + tr(" - Conversion Errors"),
                       "LOGBOOK.html#convert-errors"); // anchor for future use
      error.setHtmlMessage(html.getHtml(), true /* print to log */);
      QGuiApplication::restoreOverrideCursor();
      error.exec();
    }
    else
    {
      // No errors ======================
      QGuiApplication::restoreOverrideCursor();
      QMessageBox::information(mainWindow, QApplication::applicationName(), resultText);
    }

    mainWindow->showLogbookSearch();

    logChanged(false /* load all */, false /* keep selection */);

    /*: The text "Converted from userdata" has to match the one in LogdataConverter::convertFromUserdata */
    emit showInSearch(map::LOGBOOK,
                      atools::sql::SqlRecord().appendFieldAndValue("description", tr("*Converted from userdata*")),
                      false /* select */);
  }
  else if(result == QMessageBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "LOGBOOK.html#convert",
                                             lnm::helpLanguageOnline());
}

void LogdataController::fetchAirportCoordinates(atools::geo::Pos& pos, QString& name, const QString& airportIdent)
{
  map::MapAirport airport = NavApp::getAirportQuerySim()->getAirportByIdent(airportIdent);

  if(airport.isValid())
  {
    pos = airport.position;
    name = airport.name;
  }
}

void LogdataController::planOpen(atools::sql::SqlRecord *record, QWidget *parent)
{
  try
  {
    qDebug() << Q_FUNC_INFO;
    // Open flight plan in table replacing the current plan - attachement is always LNMPLN
    mainWindow->routeOpenFileLnmStr(
      QString(atools::zip::gzipDecompress(record->value("flightplan").toByteArray())));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::planAdd(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    // Store a new flight plan as logbook entry attachement - loaded plan can be any supported format
    // Plan is always attached in LNMPLN format
    QString filename = mainWindow->routeOpenFileDialog();
    if(!filename.isEmpty())
    {
      // Load flight plan in any supported format
      atools::fs::pln::Flightplan flightplan;
      atools::fs::pln::FlightplanIO().load(flightplan, filename);

      // Store in LNMPLN format
      SqlTransaction transaction(manager->getDatabase());
      record->setValue("flightplan", atools::fs::pln::FlightplanIO().saveLnmGz(flightplan));
      transaction.commit();
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::planSaveAs(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    // Load LNMPLN into flight plan
    atools::fs::pln::Flightplan flightplan;
    atools::fs::pln::FlightplanIO().loadLnmGz(flightplan, record->value("flightplan").toByteArray());

    // Build filename
    QString defFilename = flightplan.getFilenamePattern(OptionData::instance().getFlightplanPattern(), ".lnmpln");
    QString filename = mainWindow->routeSaveFileDialogLnm(defFilename);
    if(!filename.isEmpty())
      atools::fs::pln::FlightplanIO().saveLnm(flightplan, filename);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::gpxAdd(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString filename = dialog->openFileDialog(
      tr("Open GPX"),
      tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
      "Route/Gpx", atools::documentsDir());

    if(!filename.isEmpty())
    {
      QFile file(filename);
      if(file.open(QIODevice::ReadOnly | QIODevice::Text))
      {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");

        // Decompress GPX and save as is into the database
        SqlTransaction transaction(manager->getDatabase());
        record->setValue("aircraft_trail", atools::zip::gzipCompress(stream.readAll().toUtf8()));
        transaction.commit();

        file.close();
      }
      else
        atools::gui::ErrorHandler(parent).handleIOError(file, tr("Cannot load file."));
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::gpxSaveAs(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    // Get flight plan for file name building =========
    QString plan = QString(atools::zip::gzipDecompress(record->value("flightplan").toByteArray()));
    atools::fs::pln::Flightplan flightplan;
    atools::fs::pln::FlightplanIO().loadLnmStr(flightplan, plan);

    QString defFilename = flightplan.getFilenamePattern(OptionData::instance().getFlightplanPattern(), ".gpx");

    QString filename = dialog->saveFileDialog(
      tr("Save GPX"),
      tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
      "lnmpln", "Route/Gpx", atools::documentsDir(),
      defFilename,
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!filename.isEmpty())
    {
      QFile file(filename);
      if(file.open(QIODevice::WriteOnly | QIODevice::Text))
      {
        // Decompress and save track as is ==============
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << QString(atools::zip::gzipDecompress(record->value("aircraft_trail").toByteArray())).toUtf8();
        file.close();
      }
      else
        atools::gui::ErrorHandler(parent).handleIOError(file, tr("Cannot save file."));
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::perfAdd(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString filename = NavApp::getAircraftPerfController()->openFileDialog();

    if(!filename.isEmpty())
    {
      // Load aircraft performance in any format (INI or XML) ===================
      atools::fs::perf::AircraftPerf perf;
      perf.load(filename);

      // Save in new XML format ============
      SqlTransaction transaction(manager->getDatabase());
      record->setValue("aircraft_perf", perf.saveXmlGz());
      transaction.commit();
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parent).handleUnknownException();
  }
}

void LogdataController::perfOpen(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    NavApp::getAircraftPerfController()->loadStr(
      QString(atools::zip::gzipDecompress(record->value("aircraft_perf").toByteArray())));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}

void LogdataController::perfSaveAs(atools::sql::SqlRecord *record, QWidget *parent)
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    NavApp::getAircraftPerfController()->saveAsStr(
      QString(atools::zip::gzipDecompress(record->value("aircraft_perf").toByteArray())));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parent).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
  }
}
