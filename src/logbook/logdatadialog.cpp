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

#include "logbook/logdatadialog.h"
#include "ui_logdatadialog.h"

#include "common/maptypes.h"
#include "common/constants.h"
#include "common/dialogrecordhelper.h"
#include "geo/calculations.h"
#include "gui/helphandler.h"
#include "common/unitstringtool.h"
#include "sql/sqlrecord.h"
#include "query/airportquery.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "util/htmlbuilder.h"
#include "fs/pln/flightplanio.h"
#include "gui/dialog.h"
#include "common/unit.h"
#include "gui/signalblocker.h"
#include "gui/errorhandler.h"
#include "exception.h"
#include "perf/aircraftperfcontroller.h"

#include <QFileInfo>

LogdataDialog::LogdataDialog(QWidget *parent, ld::LogdataDialogMode mode)
  : QDialog(parent), ui(new Ui::LogdataDialog), editMode(mode)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  record = new atools::sql::SqlRecord();

  bool showCheckbox = true;
  if(mode == ld::EDIT_ONE || mode == ld::ADD)
  {
    showCheckbox = false;

    if(mode == ld::EDIT_ONE)
      setWindowTitle(QApplication::applicationName() + tr(" - Edit Logbook Entry"));
    else if(mode == ld::ADD)
      setWindowTitle(QApplication::applicationName() + tr(" - Add Logbook Entry"));
  }
  else if(mode == ld::EDIT_MULTIPLE)
  {
    setWindowTitle(QApplication::applicationName() + tr(" - Edit Logbook Entries"));
    showCheckbox = true;

    // Hide all widgets that are not applicable to multi edit
    ui->lineEditDepartureRunway->setVisible(false);
    ui->lineEditDestinationRunway->setVisible(false);
    ui->labelDepartureRunway->setVisible(false);
    ui->labelDestinationRunway->setVisible(false);

    ui->labelFlightDistance->setVisible(false);
    ui->labelFlightDistanceFlown->setVisible(false);

    ui->spinBoxFlightDistance->setVisible(false);
    ui->spinBoxFlightDistanceFlown->setVisible(false);

    ui->labelDepartureDateTimeSim->setVisible(false);
    ui->labelDepartureDateTimeReal->setVisible(false);
    ui->dateTimeEditDepartureDateTimeSim->setVisible(false);
    ui->dateTimeEditDepartureDateTimeReal->setVisible(false);

    ui->labelDestinationDateTimeSim->setVisible(false);
    ui->labelDestinationDateTimeReal->setVisible(false);
    ui->dateTimeEditDestinationDateTimeSim->setVisible(false);
    ui->dateTimeEditDestinationDateTimeReal->setVisible(false);

    ui->spinBoxFuelBlock->setVisible(false);
    ui->spinBoxFuelTrip->setVisible(false);
    ui->spinBoxFuelUsed->setVisible(false);
    ui->spinBoxFuelGrossweight->setVisible(false);
    ui->comboBoxFuelUnits->setVisible(false);

    ui->labelFuelBlock->setVisible(false);
    ui->labelFuelTrip->setVisible(false);
    ui->labelFuelUsed->setVisible(false);
    ui->labelFuelUnits->setVisible(false);
    ui->labelFuelGrossweight->setVisible(false);
    ui->lineFuelWeight->setVisible(false);

    ui->labelAttachedPlan->setVisible(false);
    ui->pushButtonAttachedPlanOpen->setVisible(false);
    ui->pushButtonAttachedPlanAdd->setVisible(false);
    ui->pushButtonAttachedPlanSaveAs->setVisible(false);
    ui->pushButtonAttachedPlanClear->setVisible(false);

    ui->labelAttachedGpx->setVisible(false);
    ui->pushButtonAttachedGpxAdd->setVisible(false);
    ui->pushButtonAttachedGpxSaveAs->setVisible(false);
    ui->pushButtonAttachedGpxClear->setVisible(false);

    ui->labelAttachedPerf->setVisible(false);
    ui->pushButtonAttachedPerfOpen->setVisible(false);
    ui->pushButtonAttachedPerfAdd->setVisible(false);
    ui->pushButtonAttachedPerfSaveAs->setVisible(false);
    ui->pushButtonAttachedPerfClear->setVisible(false);
  }

  // Update units in widgets
  units = new UnitStringTool();
  units->init({
    ui->comboBoxFuelUnits,
    ui->spinBoxFlightCruiseAltitude,
    ui->spinBoxFlightDistance,
    ui->spinBoxFlightDistanceFlown,
    ui->spinBoxFuelTrip,
    ui->spinBoxFuelUsed,
    ui->spinBoxFuelBlock,
    ui->spinBoxFuelGrossweight
  });

  editCheckBoxList.append({
    ui->checkBoxDeparture,
    ui->checkBoxAircraftName,
    ui->checkBoxAircraftRegistration,
    ui->checkBoxAircraftType,
    ui->checkBoxDestination,
    ui->checkBoxFlightCruiseAltitude,
    ui->checkBoxFlightNumber,
    ui->checkBoxFlightPerfFile,
    ui->checkBoxFlightPlanFile,
    ui->checkBoxFlightSimulator,
    ui->checkBoxRouteString,
    ui->checkBoxAircraftDescription,
    ui->checkBoxFuelType
  });

  // Show checkboxes when editing more than one entry
  for(QCheckBox *checkBox : qAsConst(editCheckBoxList))
  {
    checkBox->setVisible(showCheckbox);
    connect(checkBox, &QCheckBox::toggled, this, &LogdataDialog::updateWidgets);
  }

  // Connect button box
  connect(ui->buttonBoxLogdata, &QDialogButtonBox::accepted, this, &LogdataDialog::acceptClicked);
  connect(ui->buttonBoxLogdata->button(QDialogButtonBox::Help), &QPushButton::clicked,
          this, &LogdataDialog::helpClicked);
  connect(ui->buttonBoxLogdata->button(QDialogButtonBox::Reset), &QPushButton::clicked,
          this, &LogdataDialog::resetClicked);
  connect(ui->buttonBoxLogdata, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // Airport ident line edits
  connect(ui->lineEditDeparture, &QLineEdit::textEdited, this, &LogdataDialog::departureAirportUpdated);
  connect(ui->lineEditDestination, &QLineEdit::textEdited, this, &LogdataDialog::destAirportUpdated);

  // Filepath line edits
  connect(ui->lineEditFlightPlanFile, &QLineEdit::textEdited, this, &LogdataDialog::flightplanFileUpdated);
  connect(ui->lineEditFlightPerfFile, &QLineEdit::textEdited, this, &LogdataDialog::perfFileUpdated);

  connect(ui->pushButtonFlightPerfFileSelect, &QPushButton::clicked, this, &LogdataDialog::perfFileClicked);
  connect(ui->pushButtonFlightPlanFileSelect, &QPushButton::clicked, this, &LogdataDialog::flightplanFileClicked);

  connect(ui->comboBoxFuelUnits, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &LogdataDialog::fuelUnitsChanged);

  connect(ui->pushButtonAttachedPlanOpen, &QPushButton::clicked, this, &LogdataDialog::planOpenClicked);
  connect(ui->pushButtonAttachedPlanAdd, &QPushButton::clicked, this, &LogdataDialog::planAddClicked);
  connect(ui->pushButtonAttachedPlanSaveAs, &QPushButton::clicked, this, &LogdataDialog::planSaveAsClicked);
  connect(ui->pushButtonAttachedPlanClear, &QPushButton::clicked, this, &LogdataDialog::planClearClicked);

  connect(ui->pushButtonAttachedGpxAdd, &QPushButton::clicked, this, &LogdataDialog::gpxAddClicked);
  connect(ui->pushButtonAttachedGpxSaveAs, &QPushButton::clicked, this, &LogdataDialog::gpxSaveAsClicked);
  connect(ui->pushButtonAttachedGpxClear, &QPushButton::clicked, this, &LogdataDialog::gpxClearClicked);

  connect(ui->pushButtonAttachedPerfOpen, &QPushButton::clicked, this, &LogdataDialog::perfOpenClicked);
  connect(ui->pushButtonAttachedPerfAdd, &QPushButton::clicked, this, &LogdataDialog::perfAddClicked);
  connect(ui->pushButtonAttachedPerfSaveAs, &QPushButton::clicked, this, &LogdataDialog::perfSaveAsClicked);
  connect(ui->pushButtonAttachedPerfClear, &QPushButton::clicked, this, &LogdataDialog::perfClearClicked);
}

LogdataDialog::~LogdataDialog()
{
  atools::gui::WidgetState(lnm::LOGDATA_EDIT_ADD_DIALOG).save(this);

  delete units;
  delete record;
  delete ui;
}

void LogdataDialog::setRecord(const atools::sql::SqlRecord& sqlRecord)
{
  *record = sqlRecord;
  recordToDialog();
  updateWidgets();
  fuelUnitsChanged();
  perfFileUpdated();
  flightplanFileUpdated();
}

void LogdataDialog::acceptClicked()
{
  // Copy widget data to record
  dialogToRecord();

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << record;
#endif

  QDialog::accept();
}

void LogdataDialog::helpClicked()
{
  if(editMode == ld::ADD)
    atools::gui::HelpHandler::openHelpUrlWeb(
      this, lnm::helpOnlineUrl + "LOGBOOK.html#logbook-dialog-add", lnm::helpLanguageOnline());
  else
    atools::gui::HelpHandler::openHelpUrlWeb(
      this, lnm::helpOnlineUrl + "LOGBOOK.html#logbook-dialog-edit", lnm::helpLanguageOnline());
}

void LogdataDialog::resetClicked()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(editMode == ld::EDIT_MULTIPLE)
  {
    // Reset checkboxes to unchecked
    for(QCheckBox *checkBox : qAsConst(editCheckBoxList))
      checkBox->setChecked(false);
  }

  if(editMode == ld::ADD)
  {
    // Clear all
    clearWidgets();
    clearAttached();
  }
  else if(editMode == ld::EDIT_MULTIPLE || editMode == ld::EDIT_ONE)
    recordToDialog();

  updateWidgets();
  fuelUnitsChanged();
  perfFileUpdated();
  flightplanFileUpdated();
}

void LogdataDialog::fuelUnitsChanged()
{
  bool volume = ui->comboBoxFuelUnits->currentIndex();

  if(volumeCurrent != volume)
  {
    bool jetfuel = ui->comboBoxFuelType->currentIndex();

    volumeCurrent = volume;

    switch(OptionData::instance().getUnitFuelAndWeight())
    {
      case opts::FUEL_WEIGHT_GAL_LBS:
        if(volume)
        {
          ui->spinBoxFuelTrip->setValue(atools::geo::fromLbsToGal(jetfuel, ui->spinBoxFuelTrip->value()));
          ui->spinBoxFuelUsed->setValue(atools::geo::fromLbsToGal(jetfuel, ui->spinBoxFuelUsed->value()));
          ui->spinBoxFuelBlock->setValue(atools::geo::fromLbsToGal(jetfuel, ui->spinBoxFuelBlock->value()));
        }
        else
        {
          ui->spinBoxFuelTrip->setValue(atools::geo::fromGalToLbs(jetfuel, ui->spinBoxFuelTrip->value()));
          ui->spinBoxFuelUsed->setValue(atools::geo::fromGalToLbs(jetfuel, ui->spinBoxFuelUsed->value()));
          ui->spinBoxFuelBlock->setValue(atools::geo::fromGalToLbs(jetfuel, ui->spinBoxFuelBlock->value()));
        }
        break;

      case opts::FUEL_WEIGHT_LITER_KG:
        if(volume)
        {
          ui->spinBoxFuelTrip->setValue(atools::geo::fromKgToLiter(jetfuel, ui->spinBoxFuelTrip->value()));
          ui->spinBoxFuelUsed->setValue(atools::geo::fromKgToLiter(jetfuel, ui->spinBoxFuelUsed->value()));
          ui->spinBoxFuelBlock->setValue(atools::geo::fromKgToLiter(jetfuel, ui->spinBoxFuelBlock->value()));
        }
        else
        {
          ui->spinBoxFuelTrip->setValue(atools::geo::fromLiterToKg(jetfuel, ui->spinBoxFuelTrip->value()));
          ui->spinBoxFuelUsed->setValue(atools::geo::fromLiterToKg(jetfuel, ui->spinBoxFuelUsed->value()));
          ui->spinBoxFuelBlock->setValue(atools::geo::fromLiterToKg(jetfuel, ui->spinBoxFuelBlock->value()));
        }
        break;
    }

    // Update units in widgets
    units->update(volume);
  }
}

void LogdataDialog::departureAirportUpdated()
{
  airportUpdated(ui->lineEditDeparture, ui->labelDepartureStatus);
}

void LogdataDialog::destAirportUpdated()
{
  airportUpdated(ui->lineEditDestination, ui->labelDestinationStatus);
}

void LogdataDialog::airportUpdated(QLineEdit *lineEdit, QLabel *label)
{
  QString ident = lineEdit->text();

  if(ident.isEmpty())
    label->setText(atools::util::HtmlBuilder::warningMessage(tr("No airport selected.")));
  else
  {
    // Try to get airport for ident
    QList<map::MapAirport> airports = NavApp::getAirportQuerySim()->getAirportsByOfficialIdent(ident.toUpper());

    if(!airports.isEmpty())
      label->setText(tr("%1,   elevation %2%3").
                     arg(airports.constFirst().name).
                     arg(Unit::altFeet(airports.constFirst().position.getAltitude())).
                     arg(airports.size() > 1 ? tr(" (more found)") : QString()));
    else
      label->setText(atools::util::HtmlBuilder::errorMessage(tr("No airport found.").arg(ident)));
  }
}

void LogdataDialog::flightplanFileUpdated()
{
  fileUpdated(ui->lineEditFlightPlanFile, ui->labelFlightPlanFileStatus, false);
}

void LogdataDialog::perfFileUpdated()
{
  fileUpdated(ui->lineEditFlightPerfFile, ui->labelFlightPerfFileStatus, true);
}

void LogdataDialog::flightplanFileClicked()
{
  qDebug() << Q_FUNC_INFO;
  QString filepath = atools::gui::Dialog(this).openFileDialog(
    tr("Open Flight Plan"),
    tr("Flight Plan Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLIGHTPLAN_LOAD),
    "Route/" + NavApp::getCurrentSimulatorShortName(),
    NavApp::getCurrentSimulatorFilesPath());

  if(!filepath.isEmpty())
  {
    ui->lineEditFlightPlanFile->setText(filepath);
    flightplanFileUpdated();
  }
}

void LogdataDialog::perfFileClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString perfFile = atools::gui::Dialog(this).openFileDialog(
    tr("Open Aircraft Performance File"),
    tr("Aircraft Performance Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AIRCRAFT_PERF),
    "AircraftPerformance/");

  if(!perfFile.isEmpty())
  {
    ui->lineEditFlightPerfFile->setText(perfFile);
    perfFileUpdated();
  }
}

void LogdataDialog::fileUpdated(QLineEdit *lineEdit, QLabel *label, bool perf)
{
  try
  {
    QString filepath = lineEdit->text();

    if(filepath.isEmpty())
      label->setText(tr("No file selected."));
    else
    {
      QFileInfo fi(filepath);
      if(fi.exists())
      {
        if(fi.isDir())
          label->setText(atools::util::HtmlBuilder::errorMessage(tr("File or directory.")));
        else
        {
          if(perf)
          {
            if(AircraftPerfController::isPerformanceFile(filepath))
              label->setText(tr("Valid aircraft performance file."));
            else
              label->setText(tr("File is not an aircraft performance file."));
          }
          else
          {
            if(atools::fs::pln::FlightplanIO::isFlightplanFile(filepath))
              label->setText(tr("Valid flight plan file."));
            else
              label->setText(tr("File is not a supported flight plan."));
          }
        }
      }
      else
        label->setText(atools::util::HtmlBuilder::errorMessage(tr("File not found.")));
    }
  }
  catch(atools::Exception& e)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(this).handleException(e);
  }
  catch(...)
  {
    NavApp::closeSplashScreen();
    atools::gui::ErrorHandler(this).handleUnknownException();
  }
}

void LogdataDialog::recordToDialog()
{
  using atools::roundToInt;
  // Avoid recursions on emitted signals
  atools::gui::SignalBlocker blocker({ui->lineEditDeparture, ui->lineEditDestination, ui->lineEditFlightPlanFile,
                                      ui->lineEditFlightPerfFile, ui->pushButtonFlightPerfFileSelect,
                                      ui->pushButtonFlightPlanFileSelect, ui->comboBoxFuelUnits,
                                      ui->comboBoxFuelType});

  // Aircraft =====================================
  ui->lineEditAircraftName->setText(record->valueStr("aircraft_name")); // varchar(250) collate nocase
  ui->lineEditAircraftType->setText(record->valueStr("aircraft_type")); // varchar(250) collate nocase
  ui->lineEditAircraftRegistration->setText(record->valueStr("aircraft_registration")); // varchar(50) collate nocase
  ui->lineEditFlightNumber->setText(record->valueStr("flightplan_number")); // varchar(100) collate nocase

  // Files =====================================
  ui->lineEditFlightPlanFile->setText(atools::nativeCleanPath(record->valueStr("flightplan_file"))); // varchar(1024) collate nocase
  ui->lineEditFlightPerfFile->setText(atools::nativeCleanPath(record->valueStr("performance_file"))); // varchar(1024) collate nocase

  ui->spinBoxFlightCruiseAltitude->setValue(roundToInt(Unit::altFeetF(record->valueFloat("flightplan_cruise_altitude")))); // double

  // Fuel =====================================
  ui->spinBoxFuelBlock->setValue(roundToInt(Unit::weightLbsF(record->valueFloat("block_fuel")))); // double
  ui->spinBoxFuelTrip->setValue(roundToInt(Unit::weightLbsF(record->valueFloat("trip_fuel")))); // double
  ui->spinBoxFuelUsed->setValue(roundToInt(Unit::weightLbsF(record->valueFloat("used_fuel")))); // double

  ui->spinBoxFuelGrossweight->setValue(roundToInt(Unit::weightLbsF(record->valueFloat("grossweight")))); // double
  ui->comboBoxFuelType->setCurrentIndex(record->valueInt("is_jetfuel")); // int

  // Distance =====================================
  ui->spinBoxFlightDistance->setValue(roundToInt(Unit::distNmF(record->valueFloat("distance")))); // double
  ui->spinBoxFlightDistanceFlown->setValue(roundToInt(Unit::distNmF(record->valueFloat("distance_flown")))); // double

  // Departurte and destination =====================================
  ui->lineEditDeparture->setText(record->valueStr("departure_ident")); // varchar(10) collate nocase
  ui->labelDepartureStatus->setText(record->valueStr("departure_name")); // varchar(200) collate nocase
  ui->lineEditDepartureRunway->setText(record->valueStr("departure_runway")); // varchar(10) collate nocase
  ui->lineEditDestination->setText(record->valueStr("destination_ident")); // varchar(10) collate nocase
  ui->labelDestinationStatus->setText(record->valueStr("destination_name")); // varchar(200) collate nocase
  ui->lineEditDestinationRunway->setText(record->valueStr("destination_runway")); // varchar(10) collate nocase

  ui->plainTextEditDescription->setPlainText(record->valueStr("description")); // varchar(2048) collate nocase
  ui->lineEditFlightSimulator->setText(record->valueStr("simulator")); // varchar(2048) collate nocase
  ui->lineEditRouteString->setText(record->valueStr("route_string")); // varchar(2048) collate nocase

  // Date and time ========================================================
  valueDateTime(ui->dateTimeEditDepartureDateTimeReal, "departure_time");
  valueDateTime(ui->dateTimeEditDepartureDateTimeSim, "departure_time_sim");
  valueDateTime(ui->dateTimeEditDestinationDateTimeReal, "destination_time");
  valueDateTime(ui->dateTimeEditDestinationDateTimeSim, "destination_time_sim");
}

void LogdataDialog::valueDateTime(QDateTimeEdit *edit, const QString& name) const
{
  QDateTime dateTime = record->valueDateTime(name);
  edit->setDateTime(dateTime.isValid() ? dateTime : QDateTime::currentDateTime());
}

void LogdataDialog::dialogToRecord()
{
  DialogRecordHelper helper(record, editMode == ld::EDIT_MULTIPLE);

  // Aircraft ========================================================
  helper.dialogToRecordStr(ui->lineEditAircraftName, "aircraft_name", ui->checkBoxAircraftName);
  helper.dialogToRecordStr(ui->lineEditAircraftRegistration, "aircraft_registration", ui->checkBoxAircraftRegistration);
  helper.dialogToRecordStr(ui->lineEditAircraftType, "aircraft_type", ui->checkBoxAircraftType);

  helper.dialogToRecordStr(ui->plainTextEditDescription, "description", ui->checkBoxAircraftDescription);
  helper.dialogToRecordStr(ui->lineEditFlightSimulator, "simulator", ui->checkBoxFlightSimulator);
  helper.dialogToRecordStr(ui->lineEditRouteString, "route_string", ui->checkBoxRouteString);

  helper.dialogToRecordInt(ui->spinBoxFlightCruiseAltitude, "flightplan_cruise_altitude", ui->checkBoxFlightCruiseAltitude, Unit::altFeetF);

  helper.dialogToRecordStr(ui->lineEditFlightNumber, "flightplan_number", ui->checkBoxFlightNumber);

  // Files ========================================================
  helper.dialogToRecordPath(ui->lineEditFlightPerfFile, "performance_file", ui->checkBoxFlightPerfFile);
  helper.dialogToRecordPath(ui->lineEditFlightPlanFile, "flightplan_file", ui->checkBoxFlightPlanFile);

  // Fuel and weight ========================================================
  int fuelAsVolume = ui->comboBoxFuelUnits->currentIndex();
  helper.dialogToRecordFuel(ui->spinBoxFuelBlock, "block_fuel", nullptr, Unit::fuelLbsGallonF, fuelAsVolume); // double
  helper.dialogToRecordFuel(ui->spinBoxFuelTrip, "trip_fuel", nullptr, Unit::fuelLbsGallonF, fuelAsVolume); // double
  helper.dialogToRecordFuel(ui->spinBoxFuelUsed, "used_fuel", nullptr, Unit::fuelLbsGallonF, fuelAsVolume); // double
  helper.dialogToRecordInt(ui->spinBoxFuelGrossweight, "grossweight", nullptr, Unit::weightLbsF); // double

  helper.dialogToRecordInt(ui->comboBoxFuelType, "is_jetfuel", ui->checkBoxFuelType); // int

  // Distance =====================================
  helper.dialogToRecordInt(ui->spinBoxFlightDistance, "distance", nullptr, Unit::distNmF); // double
  helper.dialogToRecordInt(ui->spinBoxFlightDistanceFlown, "distance_flown", nullptr, Unit::distNmF); // double

  // Date and time ========================================================
  helper.dialogToRecordDateTime(ui->dateTimeEditDepartureDateTimeReal, "departure_time", nullptr, true /* local */); // varchar(100)
  helper.dialogToRecordDateTime(ui->dateTimeEditDepartureDateTimeSim, "departure_time_sim", nullptr, false /* local */); // varchar(100)
  helper.dialogToRecordDateTime(ui->dateTimeEditDestinationDateTimeReal, "destination_time", nullptr, true /* local */); // varchar(100)
  helper.dialogToRecordDateTime(ui->dateTimeEditDestinationDateTimeSim, "destination_time_sim", nullptr, false /* local */); // varchar(100)

  // Runways ============================================================
  helper.dialogToRecordStr(ui->lineEditDepartureRunway, "departure_runway");
  helper.dialogToRecordStr(ui->lineEditDestinationRunway, "destination_runway");

  if(editMode != ld::EDIT_MULTIPLE)
  {
    // Check if departure airport has changed
    if(ui->lineEditDeparture->text().toUpper() != record->valueStr("departure_ident").toUpper() ||
       record->isNull("departure_lonx") || record->isNull("departure_laty"))
      setAirport(ui->lineEditDeparture->text(), "departure", true);

    // Check if destination airport has changed
    if(ui->lineEditDestination->text().toUpper() != record->valueStr("destination_ident").toUpper() ||
       record->isNull("destination_lonx") || record->isNull("destination_laty"))
      setAirport(ui->lineEditDestination->text(), "destination", true);
  }
  else
  {
    // Set or remove all departure airport fields if checked
    if(ui->checkBoxDeparture->isChecked())
      setAirport(ui->lineEditDeparture->text(), "departure", true);
    else
      removeAirport("departure");

    // Set or remove all destination airport fields if checked
    if(ui->checkBoxDestination->isChecked())
      setAirport(ui->lineEditDestination->text(), "destination", true);
    else
      removeAirport("destination");

    // Not multi-edited: all attached files, departure_runway, destination_runway, departure_time, destination_time,
    // departure_time_sim, destination_time_sim, distance, distance_flown, block_fuel, trip_fuel, used_fuel, grossweight
    // Erase attachement columns to avoid data loss
    record->remove({"flightplan", "aircraft_perf", "aircraft_trail"});
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << *record;
#endif
}

void LogdataDialog::removeAirport(const QString& prefix)
{
  record->remove(prefix + "_ident");
  record->remove(prefix + "_lonx");
  record->remove(prefix + "_laty");
  record->remove(prefix + "_alt");
  record->remove(prefix + "_name");
}

void LogdataDialog::setAirport(const QString& ident, const QString& prefix, bool includeIdent)
{
  if(includeIdent)
  {
    if(ident.isEmpty())
      record->setNull(prefix + "_ident");
    else
      record->setValue(prefix + "_ident", ident.toUpper());
  }

  QList<map::MapAirport> airports = NavApp::getAirportQuerySim()->getAirportsByOfficialIdent(ident.toUpper());

  if(!airports.isEmpty())
  {
    const map::MapAirport& ap = airports.constFirst();
    record->setValue(prefix + "_lonx", ap.position.getLonX());
    record->setValue(prefix + "_laty", ap.position.getLatY());
    record->setValue(prefix + "_alt", ap.position.getAltitude());
    record->setValue(prefix + "_name", ap.name);
  }
  else
  {
    record->setNull(prefix + "_lonx");
    record->setNull(prefix + "_laty");
    record->setNull(prefix + "_alt");
    record->setNull(prefix + "_name");
  }
}

void LogdataDialog::clearAttached()
{
  record->setNull("flightplan");
  record->setNull("aircraft_perf");
  record->setNull("aircraft_trail");
}

void LogdataDialog::clearWidgets()
{
  ui->dateTimeEditDepartureDateTimeReal->setDateTime(QDateTime::currentDateTime());
  ui->dateTimeEditDepartureDateTimeSim->setDateTime(QDateTime::currentDateTime());
  ui->dateTimeEditDestinationDateTimeReal->setDateTime(QDateTime::currentDateTime());
  ui->dateTimeEditDestinationDateTimeSim->setDateTime(QDateTime::currentDateTime());
  ui->lineEditAircraftName->clear();
  ui->lineEditAircraftRegistration->clear();
  ui->lineEditAircraftType->clear();
  ui->plainTextEditDescription->clear();
  ui->lineEditFlightSimulator->clear();
  ui->lineEditRouteString->clear();
  ui->lineEditDeparture->clear();
  ui->lineEditDepartureRunway->clear();
  ui->lineEditDestination->clear();
  ui->lineEditDestinationRunway->clear();
  ui->lineEditFlightNumber->clear();
  ui->lineEditFlightPerfFile->clear();
  ui->lineEditFlightPlanFile->clear();
  ui->spinBoxFlightCruiseAltitude->setValue(0);
  ui->spinBoxFlightDistance->setValue(0);
  ui->spinBoxFlightDistanceFlown->setValue(0);
  ui->spinBoxFuelBlock->setValue(0);
  ui->spinBoxFuelGrossweight->setValue(0);
  ui->spinBoxFuelTrip->setValue(0);
  ui->spinBoxFuelUsed->setValue(0);

  ui->comboBoxFuelType->blockSignals(true);
  ui->comboBoxFuelType->setCurrentIndex(0);
  ui->comboBoxFuelType->blockSignals(false);
}

void LogdataDialog::updateWidgets()
{
  if(editMode == ld::EDIT_MULTIPLE)
  {
    // Enable or disable edit widgets depending in check box status
    ui->lineEditDeparture->setEnabled(ui->checkBoxDeparture->isChecked());
    ui->lineEditAircraftName->setEnabled(ui->checkBoxAircraftName->isChecked());
    ui->lineEditAircraftRegistration->setEnabled(ui->checkBoxAircraftRegistration->isChecked());
    ui->lineEditAircraftType->setEnabled(ui->checkBoxAircraftType->isChecked());
    ui->plainTextEditDescription->setEnabled(ui->checkBoxAircraftDescription->isChecked());
    ui->lineEditFlightSimulator->setEnabled(ui->checkBoxFlightSimulator->isChecked());
    ui->lineEditRouteString->setEnabled(ui->checkBoxRouteString->isChecked());
    ui->lineEditDestination->setEnabled(ui->checkBoxDestination->isChecked());
    ui->spinBoxFlightCruiseAltitude->setEnabled(ui->checkBoxFlightCruiseAltitude->isChecked());
    ui->lineEditFlightNumber->setEnabled(ui->checkBoxFlightNumber->isChecked());
    ui->lineEditFlightPerfFile->setEnabled(ui->checkBoxFlightPerfFile->isChecked());
    ui->pushButtonFlightPerfFileSelect->setEnabled(ui->checkBoxFlightPerfFile->isChecked());
    ui->lineEditFlightPlanFile->setEnabled(ui->checkBoxFlightPlanFile->isChecked());
    ui->pushButtonFlightPlanFileSelect->setEnabled(ui->checkBoxFlightPlanFile->isChecked());
    ui->comboBoxFuelType->setEnabled(ui->checkBoxFuelType->isChecked());

    bool enable = false;
    for(const QCheckBox *checkBox : qAsConst(editCheckBoxList))
      enable |= checkBox->isChecked();

    // Disable dialog OK button if nothing is checked
    ui->buttonBoxLogdata->button(QDialogButtonBox::Ok)->setEnabled(enable);
  }

  updateAttachementWidgets();
  departureAirportUpdated();
  destAirportUpdated();
  flightplanFileUpdated();
  perfFileUpdated();
}

void LogdataDialog::updateAttachementWidgets()
{
  if(editMode != ld::EDIT_MULTIPLE)
  {
    bool hasPln = !record->isNull("flightplan") && !record->valueBytes("flightplan").isEmpty();
    ui->pushButtonAttachedPlanOpen->setEnabled(hasPln);
    ui->pushButtonAttachedPlanSaveAs->setEnabled(hasPln);
    ui->pushButtonAttachedPlanClear->setEnabled(hasPln);

    bool hasTrail = !record->isNull("aircraft_trail") && !record->valueBytes("aircraft_trail").isEmpty();
    ui->pushButtonAttachedGpxSaveAs->setEnabled(hasTrail);
    ui->pushButtonAttachedGpxClear->setEnabled(hasTrail);

    bool hasPerf = !record->isNull("aircraft_perf") && !record->valueBytes("aircraft_perf").isEmpty();
    ui->pushButtonAttachedPerfOpen->setEnabled(hasPerf);
    ui->pushButtonAttachedPerfSaveAs->setEnabled(hasPerf);
    ui->pushButtonAttachedPerfClear->setEnabled(hasPerf);
  }
}

void LogdataDialog::saveState()
{
  atools::gui::WidgetState(lnm::LOGDATA_EDIT_ADD_DIALOG).save({this, ui->tabWidgetLogbook});
}

void LogdataDialog::restoreState()
{
  atools::gui::WidgetState(lnm::LOGDATA_EDIT_ADD_DIALOG).restore({this, ui->tabWidgetLogbook});
}

void LogdataDialog::planOpenClicked()
{
  emit planOpen(record, this);
}

void LogdataDialog::planAddClicked()
{
  emit planAdd(record, this);
  updateAttachementWidgets();
}

void LogdataDialog::planSaveAsClicked()
{
  emit planSaveAs(record, this);
}

void LogdataDialog::gpxAddClicked()
{
  emit gpxAdd(record, this);
  updateAttachementWidgets();
}

void LogdataDialog::gpxSaveAsClicked()
{
  emit gpxSaveAs(record, this);
}

void LogdataDialog::perfOpenClicked()
{
  emit perfOpen(record, this);
}

void LogdataDialog::perfAddClicked()
{
  emit perfAdd(record, this);
  updateAttachementWidgets();
}

void LogdataDialog::perfSaveAsClicked()
{
  emit perfSaveAs(record, this);
}

void LogdataDialog::planClearClicked()
{
  record->setNull("flightplan");
  updateWidgets();
}

void LogdataDialog::perfClearClicked()
{
  record->setNull("aircraft_perf");
  updateWidgets();
}

void LogdataDialog::gpxClearClicked()
{
  record->setNull("aircraft_trail");
  updateWidgets();
}
