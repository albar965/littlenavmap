/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "userdata/userdatacontroller.h"

#include "fs/userdata/userdatamanager.h"
#include "common/constants.h"
#include "sql/sqlrecord.h"
#include "navapp.h"
#include "common/formatter.h"
#include "route/routecontroller.h"
#include "atools.h"
#include "gui/dialog.h"
#include "fs/util/fsutil.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "userdata/userdatadialog.h"
#include "userdata/userdataicons.h"
#include "settings/settings.h"
#include "query/airportquery.h"
#include "options/optiondata.h"
#include "search/userdatasearch.h"
#include "common/maptypes.h"
#include "sql/sqltransaction.h"
#include "userdata/userdataexportdialog.h"
#include "gui/errorhandler.h"
#include "exception.h"
#include "common/unit.h"

#include <QDebug>
#include <QStandardPaths>

using atools::sql::SqlTransaction;
using atools::sql::SqlRecord;
using atools::geo::Pos;

UserdataController::UserdataController(atools::fs::userdata::UserdataManager *userdataManager, MainWindow *parent)
  : manager(userdataManager), mainWindow(parent)
{
  dialog = new atools::gui::Dialog(mainWindow);
  icons = new UserdataIcons(mainWindow);
  icons->loadIcons();
  lastAddedRecord = new SqlRecord();
}

UserdataController::~UserdataController()
{
  delete aircraftAtTakeoff;
  delete dialog;
  delete icons;
  delete lastAddedRecord;
}

void UserdataController::showSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  ui->tabWidgetSearch->setCurrentIndex(3);
}

QString UserdataController::getDefaultType(const QString& type)
{
  return icons->getDefaultType(type);
}

void UserdataController::addToolbarButton()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  QToolButton *button = new QToolButton(ui->toolbarMapOptions);

  // Create and add toolbar button =====================================
  button->setIcon(QIcon(":/littlenavmap/resources/icons/userpoint_POI.svg"));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolTip(tr("Select userpoints for display"));
  button->setStatusTip(button->toolTip());
  button->setCheckable(true);
  userdataToolButton = button;

  // Insert before show route
  ui->toolbarMapOptions->insertWidget(ui->actionMapShowRoute, button);
  ui->toolbarMapOptions->insertSeparator(ui->actionMapShowRoute);

  // Create and add select all action =====================================
  actionAll = new QAction(tr("All"), button);
  actionAll->setToolTip(tr("Enable all userpoints"));
  actionAll->setStatusTip(actionAll->toolTip());
  button->addAction(actionAll);
  connect(actionAll, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select none action =====================================
  actionNone = new QAction(tr("None"), button);
  actionNone->setToolTip(tr("Disable all userpoints"));
  actionNone->setStatusTip(actionAll->toolTip());
  button->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select unknown action =====================================
  actionUnknown = new QAction(tr("Unknown Types"), button);
  actionUnknown->setToolTip(tr("Enable or disable unknown userpoint types"));
  actionUnknown->setStatusTip(tr("Enable or disable unknown userpoint types"));
  actionUnknown->setCheckable(true);
  button->addAction(actionUnknown);
  ui->menuViewUserpoints->addAction(actionUnknown);
  ui->menuViewUserpoints->addSeparator();
  connect(actionUnknown, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);

  // Create and add select an action for each registered type =====================================
  for(const QString& type : icons->getAllTypes())
  {
    QIcon icon(icons->getIconPath(type));
    QAction *action = new QAction(icon, type, button);
    action->setData(QVariant(type));
    action->setCheckable(true);
    button->addAction(action);
    ui->menuViewUserpoints->addAction(action);
    connect(action, &QAction::triggered, this, &UserdataController::toolbarActionTriggered);
    actions.append(action);
  }
}

void UserdataController::toolbarActionTriggered()
{
  qDebug() << Q_FUNC_INFO;

  QAction *action = dynamic_cast<QAction *>(sender());
  if(action != nullptr)
  {
    if(action == actionAll)
    {
      // Select all buttons
      actionUnknown->setChecked(true);
      for(QAction *a : actions)
        if(a->data().type() == QVariant::String)
          a->setChecked(true);
    }
    else if(action == actionNone)
    {
      // Deselect all buttons
      actionUnknown->setChecked(false);
      for(QAction *a : actions)
        if(a->data().type() == QVariant::String)
          a->setChecked(false);
    }
    // Copy action state to class data
    actionsToTypes();
  }
  emit userdataChanged();
}

void UserdataController::actionsToTypes()
{
  // Copy state for known types
  selectedTypes.clear();
  for(QAction *action : actions)
  {
    if(action->isChecked())
      selectedTypes.append(action->data().toString());
  }

  selectedUnknownType = actionUnknown->isChecked();
  userdataToolButton->setChecked(!selectedTypes.isEmpty() || selectedUnknownType);
  qDebug() << Q_FUNC_INFO << selectedTypes;
}

void UserdataController::typesToActions()
{
  // Copy state for known types
  for(QAction *action : actions)
    action->setChecked(selectedTypes.contains(action->data().toString()));
  actionUnknown->setChecked(selectedUnknownType);
  userdataToolButton->setChecked(!selectedTypes.isEmpty() || selectedUnknownType);
}

void UserdataController::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA, selectedTypes);
  atools::settings::Settings::instance().setValue(lnm::MAP_USERDATA_UNKNOWN, selectedUnknownType);
}

void UserdataController::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    atools::settings::Settings& settings = atools::settings::Settings::instance();

    // Enable all as default
    QStringList list = settings.valueStrList(lnm::MAP_USERDATA, getAllTypes());
    selectedUnknownType = settings.valueBool(lnm::MAP_USERDATA_UNKNOWN, true);

    // Remove all types from the restored list which were not found in the list of registered types
    const QStringList& availableTypes = icons->getAllTypes();
    for(const QString& type : list)
    {
      if(availableTypes.contains(type))
        selectedTypes.append(type);
    }
  }
  else
    resetSettingsToDefault();
  typesToActions();
}

void UserdataController::resetSettingsToDefault()
{
  selectedTypes.append(icons->getAllTypes());
  selectedUnknownType = true;
  typesToActions();
}

QStringList UserdataController::getAllTypes() const
{
  return icons->getAllTypes();
}

void UserdataController::addUserpointFromMap(const map::MapSearchResult& result, atools::geo::Pos pos)
{
  qDebug() << Q_FUNC_INFO;
  if(result.isEmpty(map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT))
    // No prefill start empty dialog of with last added data
    addUserpoint(-1, pos);
  else
  {
    // Prepare the dialog prefill data
    SqlRecord prefill = manager->getEmptyRecord();
    if(result.hasAirports())
    {
      const map::MapAirport& ap = result.airports.first();

      prefill.appendFieldAndValue("ident", ap.ident)
      .appendFieldAndValue("name", ap.name)
      .appendFieldAndValue("type", "Airport")
      .appendFieldAndValue("region", ap.region);
      pos = ap.position;
    }
    else if(result.hasVor())
    {
      const map::MapVor& vor = result.vors.first();
      prefill.appendFieldAndValue("ident", vor.ident)
      .appendFieldAndValue("name", map::vorText(vor))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", vor.region);
      pos = vor.position;
    }
    else if(result.hasNdb())
    {
      const map::MapNdb& ndb = result.ndbs.first();
      prefill.appendFieldAndValue("ident", ndb.ident)
      .appendFieldAndValue("name", map::ndbText(ndb))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", ndb.region);
      pos = ndb.position;
    }
    else if(result.hasWaypoints())
    {
      const map::MapWaypoint& wp = result.waypoints.first();
      prefill.appendFieldAndValue("ident", wp.ident)
      .appendFieldAndValue("name", map::waypointText(wp))
      .appendFieldAndValue("type", "Waypoint")
      .appendFieldAndValue("region", wp.region);
      pos = wp.position;
    }
    else
      prefill.appendFieldAndValue("type", UserdataDialog::DEFAULT_TYPE);

    prefill.appendFieldAndValue("altitude", pos.getAltitude());

    addUserpointInternal(-1, pos, prefill);
  }
}

void UserdataController::deleteUserpointFromMap(int id)
{
  deleteUserpoints({id});
}

void UserdataController::moveUserpointFromMap(const map::MapUserpoint& userpoint)
{
  SqlRecord rec;
  rec.appendFieldAndValue("lonx", userpoint.position.getLonX());
  rec.appendFieldAndValue("laty", userpoint.position.getLatY());

  SqlTransaction transaction(manager->getDatabase());

  // Change coordinate columns for id
  manager->updateByRecord(rec, {userpoint.id});
  transaction.commit();

  // No need to update search
  emit userdataChanged();
  mainWindow->setStatusMessage(tr("Userpoint moved."));
}

void UserdataController::backup()
{
  manager->backup();
}

void UserdataController::clearTemporary()
{
  manager->clearTemporary();
}

void UserdataController::setMagDecReader(atools::fs::common::MagDecReader *magDecReader)
{
  manager->setMagDecReader(magDecReader);
}

void UserdataController::aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  createTakoffLanding(aircraft, true /*takeoff*/, 0.f, 0.f);
}

void UserdataController::aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm,
                                         float averageTasKts)
{
  createTakoffLanding(aircraft, false /*takeoff*/, flownDistanceNm, averageTasKts);
}

void UserdataController::createTakoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff,
                                             float flownDistanceNm, float averageTasKts)
{
  if(NavApp::getMainUi()->actionUserdataCreateLogbook->isChecked())
  {
    QVector<map::MapRunway> runways;

    // Use inflated rectangle for query
    atools::geo::Rect rect(aircraft.getPosition());
    rect.inflate(0.5f, 0.5f);

    // Get runways that more or less match the aircraft heading
    float track = aircraft.getTrackDegTrue();
    AirportQuery *airportQuery = NavApp::getAirportQuerySim();
    airportQuery->getRunways(runways, rect, aircraft.getPosition());

    // Get closes runway that matches heading
    map::MapRunwayEnd runwayEnd;
    map::MapAirport airport;
    airportQuery->getBestRunwayEndAndAirport(runwayEnd, airport, runways, track);

    if(!airport.isValid())
      qWarning() << Q_FUNC_INFO << "No runways or airports found for takeoff/landing"
                 << aircraft.getPosition() << track;

    qDebug() << Q_FUNC_INFO << runwayEnd.name << aircraft.getPosition() << track << airport.ident;

    QString departureArrivalText = takeoff ? tr("Departure") : tr("Arrival");
    QString runwayText = runwayEnd.isValid() ? tr(" runway %1").arg(runwayEnd.name) : QString();
    Pos position = airport.isValid() ? airport.position : aircraft.getPosition();
    QString airportText = airport.isValid() ? map::airportText(airport) : Unit::coords(position);

    // Build record for new userpoint
    SqlRecord record = manager->getEmptyRecord();
    record.setValue("last_edit_timestamp", QDateTime::currentDateTime());
    record.setValue("lonx", position.getLonX());
    record.setValue("laty", position.getLatY());
    record.setValue("ident", airport.ident);
    record.setValue("type", "Logbook");
    record.setValue("tags", departureArrivalText);
    record.setValue("name", airport.name);
    record.setValue("visible_from", 500);
    record.setValue("altitude", position.getAltitude());

    // Build description text =========================================================
    QStringList description;

    description << tr("%1 at %2%3").arg(departureArrivalText).arg(airportText).arg(runwayText);

    description << tr("Simulator Date and Time: %1 %2, %3 %4").
      arg(QLocale().toString(aircraft.getZuluTime(), QLocale::ShortFormat)).
      arg(aircraft.getZuluTime().timeZoneAbbreviation()).
      arg(QLocale().toString(aircraft.getLocalTime().time(), QLocale::ShortFormat)).
      arg(aircraft.getLocalTime().timeZoneAbbreviation());

    description << tr("Date and Time: %1").arg(QDateTime::currentDateTime().toString());

    // Add flight plan file name =========================================================
    QString file = NavApp::getRouteController()->getCurrentRouteFilename();
    if(!file.isEmpty())
      description << QString() << tr("Flight Plan:") << QFileInfo(file).fileName();
    const Route& route = NavApp::getRouteConst();

    // Current start and destination =========================================================
    if(!route.isEmpty())
    {
      QString from, to;
      if(route.first().getAirport().isValid())
        from = map::airportText(route.first().getAirport());
      else
        from = route.first().getIdent();

      if(route.last().getAirport().isValid())
        to = map::airportText(route.last().getAirport());
      else
        to = route.last().getIdent();

      description << tr("From: %2 to %3").arg(from).arg(to);
      description << tr("Cruising altitude: %1").arg(Unit::altFeet(route.getCruisingAltitudeFeet()));
    }

    // Add aircraft information =========================================================
    description << QString() << tr("Aircraft:");

    if(!aircraft.getAirplaneTitle().isEmpty())
      description << tr("Title: %1").arg(aircraft.getAirplaneTitle());

    if(!aircraft.getAirplaneAirline().isEmpty())
      description << tr("Airline: %1").arg(aircraft.getAirplaneAirline());

    if(!aircraft.getAirplaneFlightnumber().isEmpty())
      description << tr("Flight Number: %1").arg(aircraft.getAirplaneFlightnumber());

    if(!aircraft.getAirplaneModel().isEmpty())
      description << tr("Model: %1").arg(aircraft.getAirplaneModel());

    if(!aircraft.getAirplaneRegistration().isEmpty())
      description << tr("Registration: %1").arg(aircraft.getAirplaneRegistration());

    QString type;
    if(!aircraft.getAirplaneType().isEmpty())
      type = aircraft.getAirplaneType();
    else
      // Convert model ICAO code to a name
      type = atools::fs::util::aircraftTypeForCode(aircraft.getAirplaneModel());

    if(!type.isEmpty())
      description << tr("Type: %1").arg(type);

    if(!takeoff && aircraftAtTakeoff != nullptr && aircraft.isSameAircraft(*aircraftAtTakeoff))
    {
      // Landing and have same aircraft state saved

      // Add trip state =========================================================
      description << QString() << tr("Trip:");

      float travelTimeHours = aircraft.getTravelingTimeMinutes(*aircraftAtTakeoff) / 60.f;

      description << tr("Time: %1").
        arg(formatter::formatMinutesHoursLong(travelTimeHours));
      if(!route.isEmpty())
        description << tr("Flight Plan Distance: %1").arg(Unit::distNm(route.getTotalDistance()));

      if(flownDistanceNm > 1.f)
      {
        description << tr("Flown Distance: %1").arg(Unit::distNm(flownDistanceNm));
        if(travelTimeHours > 0.01f)
          description << tr("Average Groundspeed: %1").arg(Unit::speedKts(flownDistanceNm / travelTimeHours));
      }

      if(averageTasKts > 1.f)
        description << tr("Average True Airspeed: %1").arg(Unit::speedKts(averageTasKts));

      description << tr("Fuel consumed: %1").arg(
        Unit::weightLbs(aircraft.getConsumedFuelLbs(*aircraftAtTakeoff)) + tr(", ") +
        Unit::volGallon(aircraft.getConsumedFuelGallons(*aircraftAtTakeoff)));
      description << tr("Average fuel flow: %1").arg(
        Unit::ffLbs(aircraft.getAverageFuelFlowPPH(*aircraftAtTakeoff)) + tr(", ") +
        Unit::ffGallon(aircraft.getAverageFuelFlowGPH(*aircraftAtTakeoff)));
    }
    record.setValue("description", description.join("\n"));

    // Add to database
    SqlTransaction transaction(manager->getDatabase());
    manager->insertByRecord(record);
    transaction.commit();
    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("Logbook Entry for %1 at %2%3 added.").
                                 arg(departureArrivalText).
                                 arg(airport.ident).
                                 arg(runwayText));
  }

  delete aircraftAtTakeoff;
  aircraftAtTakeoff = nullptr;

  if(takeoff)
    aircraftAtTakeoff = new atools::fs::sc::SimConnectUserAircraft(aircraft);
}

void UserdataController::editUserpointFromMap(const map::MapSearchResult& result)
{
  qDebug() << Q_FUNC_INFO;
  editUserpoints({result.userpoints.first().id});
}

void UserdataController::addUserpoint(int id, const atools::geo::Pos& pos)
{
  addUserpointInternal(id, pos, SqlRecord());
}

void UserdataController::addUserpointInternal(int id, const atools::geo::Pos& pos,
                                              const SqlRecord& prefill)
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec;

  if(id != -1 /*&& lastAddedRecord->isEmpty()*/)
    // Get prefill from given database id
    rec = manager->getRecord(id);
  else
    // Use last added dataset
    rec = *lastAddedRecord;

  if(!prefill.isEmpty())
    // Use given record
    rec = prefill;

  if(rec.isEmpty())
    // Otherwise fill nothing
    rec = manager->getEmptyRecord();

  if(pos.isValid())
  {
    // Take coordinates for prefill if given
    rec.appendFieldAndValue("lonx", pos.getLonX()).appendFieldAndValue("laty", pos.getLatY());
    if(pos.getAltitude() < map::INVALID_ALTITUDE_VALUE)
      rec.appendFieldAndValue("altitude", pos.getAltitude());
  }

  qDebug() << Q_FUNC_INFO << rec;

  UserdataDialog dlg(mainWindow, ud::ADD, icons);
  dlg.restoreState();

  dlg.setRecord(rec);
  int retval = dlg.exec();
  if(retval == QDialog::Accepted)
  {
    *lastAddedRecord = dlg.getRecord();

    if(lastAddedRecord->contains("import_file_path"))
      lastAddedRecord->setNull("import_file_path");

    qDebug() << Q_FUNC_INFO << rec;

    // Add to database
    SqlTransaction transaction(manager->getDatabase());
    manager->insertByRecord(*lastAddedRecord);
    transaction.commit();
    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("Userpoint added."));
  }
  dlg.saveState();
}

void UserdataController::editUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  SqlRecord rec = manager->getRecord(ids.first());
  if(!rec.isEmpty())
  {
    UserdataDialog dlg(mainWindow, ids.size() > 1 ? ud::EDIT_MULTIPLE : ud::EDIT_ONE, icons);
    dlg.restoreState();

    dlg.setRecord(rec);
    int retval = dlg.exec();
    if(retval == QDialog::Accepted)
    {
      // Change modified columns for all given ids
      SqlTransaction transaction(manager->getDatabase());
      manager->updateByRecord(dlg.getRecord(), ids);
      transaction.commit();

      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
      emit userdataChanged();
      mainWindow->setStatusMessage(tr("%n userpoint(s) updated.", "", ids.size()));
    }
    dlg.saveState();
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record" << rec;
}

void UserdataController::deleteUserpoints(const QVector<int>& ids)
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Delete %n userpoint(s)?", "", ids.size()));

  if(retval == QMessageBox::Yes)
  {
    SqlTransaction transaction(manager->getDatabase());
    manager->removeRows(ids);
    transaction.commit();

    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
    emit userdataChanged();
    mainWindow->setStatusMessage(tr("%n userpoint(s) deleted.", "", ids.size()));
  }
}

void UserdataController::importCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {

    QStringList files = dialog->openFileDialogMulti(
      tr("Open Userpoint CSV File(s)"),
      tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV), "Userdata/Csv");

    int numImported = 0;
    for(const QString& file:files)
    {
      if(!file.isEmpty())
        numImported += manager->importCsv(file, atools::fs::userdata::NONE, ',', '"');
    }

    if(!files.isEmpty())
    {
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
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

void UserdataController::importXplaneUserFixDat()
{
  qDebug() << Q_FUNC_INFO;
  try
  {

    QString file = dialog->openFileDialog(
      tr("Open X-Plane user_fix.dat File"),
      tr("X-Plane User Fix Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_FIX_DAT), "Userdata/UserFixDat",
      xplaneUserWptDatPath());

    if(!file.isEmpty())
    {
      int numImported = manager->importXplane(file);
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
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

void UserdataController::importGarmin()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    QString file = dialog->openFileDialog(
      tr("Open Garmin User Waypoint File"),
      tr("Garmin User Waypoint Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_WPT), "Userdata/UserWptDat",
      garminGtnUserWptPath());

    if(!file.isEmpty())
    {
      int numImported = manager->importGarmin(file);
      mainWindow->setStatusMessage(tr("%n userpoint(s) imported.", "", numImported));
      emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
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

void UserdataController::exportCsv()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool exportSelected, append;
    if(exportSelectedQuestion(exportSelected, append, true /* append allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export Userpoint CSV File"),
        tr("CSV Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USERDATA_CSV),
        ".csv",
        "Userdata/Csv", QString(), QString(), append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(exportSelected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportCsv(file, ids, append ? atools::fs::userdata::APPEND : atools::fs::userdata::NONE);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
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

void UserdataController::exportXplaneUserFixDat()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool exportSelected, append;
    if(exportSelectedQuestion(exportSelected, append, true /* append allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export X-Plane user_fix.dat File"),
        tr("X-Plane User Fix Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_FIX_DAT),
        ".dat",
        "Userdata/UserFixDat",
        xplaneUserWptDatPath(), "user_fix.dat", append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(exportSelected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportXplane(file, ids, append ? atools::fs::userdata::APPEND : atools::fs::userdata::NONE);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
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

void UserdataController::exportGarmin()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool exportSelected, append;
    if(exportSelectedQuestion(exportSelected, append, true /* append allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export Garmin User Waypoint File"),
        tr("Garmin User Waypoint Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_USER_WPT),
        ".dat",
        "Userdata/UserWptDat",
        xplaneUserWptDatPath(), "user.wpt", append /* dont confirm overwrite */);

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(exportSelected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportGarmin(file, ids, append ? atools::fs::userdata::APPEND : atools::fs::userdata::NONE);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
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

void UserdataController::exportBglXml()
{
  qDebug() << Q_FUNC_INFO;
  try
  {
    bool exportSelected, append;
    if(exportSelectedQuestion(exportSelected, append, false /* append allowed */))
    {
      QString file = dialog->saveFileDialog(
        tr("Export XML File for FSX/P3D BGL Compiler"),
        tr("XML Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_BGL_XML),
        ".xml",
        "Userdata/BglXml");

      if(!file.isEmpty())
      {
        QVector<int> ids;
        if(exportSelected)
          ids = NavApp::getUserdataSearch()->getSelectedIds();
        int numExported =
          manager->exportBgl(file, ids);
        mainWindow->setStatusMessage(tr("%n userpoint(s) exported.", "", numExported));
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

void UserdataController::clearDatabase()
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton retval =
    QMessageBox::question(mainWindow, QApplication::applicationName(),
                          tr("Really delete all userpoints?\n\n"
                             "A backup will be created in\n"
                             "\"%1\"\n"
                             "before deleting.")
                          .arg(atools::settings::Settings::instance().getPath()));

  if(retval == QMessageBox::Yes)
  {
    manager->clearData();
    emit refreshUserdataSearch(false /* load all */, false /* keep selection */);
  }
}

QString UserdataController::xplaneUserWptDatPath()
{
  QString xpBasePath = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE11);
  if(xpBasePath.isEmpty())
    xpBasePath = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
  else
    xpBasePath = atools::buildPathNoCase({xpBasePath, "Custom Data"});
  return xpBasePath;
}

QString UserdataController::garminGtnUserWptPath()
{
  QString path;
#ifdef Q_OS_WIN32
  QString gtnPath(qgetenv("GTNSIMDATA"));
  path = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\GTN" : gtnPath;
#elif DEBUG_INFORMATION
  path = atools::buildPath({QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first(),
                            "Garmin", "Trainers", "GTN"});
#else
  path = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
#endif
  return path;
}

bool UserdataController::exportSelectedQuestion(bool& exportSelected, bool& append, bool appendAllowed)
{
  int numSelected = NavApp::getUserdataSearch()->getSelectedRowCount();

  if(numSelected == 0 && !appendAllowed)
    // nothing select and not append option - do not show dialog
    return true;

  UserdataExportDialog exportDialog(mainWindow,
                                    numSelected == 0 /* disable export selected */,
                                    false /* disable append */);
  exportDialog.restoreState();
  int retval = exportDialog.exec();

  if(retval == QDialog::Accepted)
  {
    exportSelected = exportDialog.isExportSelected() && numSelected > 0;
    append = exportDialog.isAppendToFile();
    exportDialog.saveState();
    return true;
  }
  else
    return false;
}
