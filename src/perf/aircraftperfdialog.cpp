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

#include "perf/aircraftperfdialog.h"
#include "ui_aircraftperfdialog.h"

#include "common/unit.h"
#include "atools.h"
#include "common/constants.h"
#include "geo/calculations.h"
#include "gui/helphandler.h"
#include "fs/perf/aircraftperf.h"
#include "gui/widgetstate.h"
#include "common/unitstringtool.h"
#include "settings/settings.h"
#include "common/formatter.h"
#include "util/htmlbuilder.h"

#include <QPushButton>

using atools::fs::perf::AircraftPerf;
using atools::roundToInt;

AircraftPerfDialog::AircraftPerfDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& aircraftPerformance,
                                       const QString& modeText)
  : QDialog(parent), ui(new Ui::AircraftPerfDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  setWindowTitle(windowTitle().arg(modeText));

  // Copy performance object
  perf = new AircraftPerf;
  *perf = aircraftPerformance;

  // All widgets that need their unit placeholders replaced on unit changes
  unitWidgets.append({
    ui->comboBoxFuelUnit,
    ui->comboBoxFuelType,
    ui->spinBoxClimbFuelFlow,
    ui->spinBoxCruiseSpeed,
    ui->spinBoxDescentFuelFlow,
    ui->spinBoxCruiseFuelFlow,
    ui->spinBoxContingencyFuel,
    ui->spinBoxClimbSpeed,
    ui->spinBoxExtraFuel,
    ui->spinBoxTaxiFuel,
    ui->spinBoxReserveFuel,
    ui->doubleSpinBoxClimbVertSpeed,
    ui->spinBoxDescentSpeed,
    ui->doubleSpinBoxDescentVertSpeed,
    ui->spinBoxAlternateSpeed,
    ui->spinBoxAlternateFuelFlow,
    ui->spinBoxRunwayLength,
    ui->spinBoxUsableFuel
  });

  restoreState();

  // Adjust vertical speed spin boxes for units
  if(Unit::getUnitVertSpeed() == opts::VERT_SPEED_MS)
  {
    ui->doubleSpinBoxClimbVertSpeed->setDecimals(2);
    ui->doubleSpinBoxClimbVertSpeed->setMinimum(1.);
    ui->doubleSpinBoxClimbVertSpeed->setMaximum(100.);
    ui->doubleSpinBoxClimbVertSpeed->setSingleStep(1.);

    ui->doubleSpinBoxDescentVertSpeed->setDecimals(2);
    ui->doubleSpinBoxDescentVertSpeed->setMinimum(-100.);
    ui->doubleSpinBoxDescentVertSpeed->setMaximum(-1.);
    ui->doubleSpinBoxDescentVertSpeed->setSingleStep(1.);
  }
  else if(Unit::getUnitVertSpeed() == opts::VERT_SPEED_FPM)
  {
    ui->doubleSpinBoxClimbVertSpeed->setDecimals(0);
    ui->doubleSpinBoxClimbVertSpeed->setMinimum(100.);
    ui->doubleSpinBoxClimbVertSpeed->setMaximum(10000.);
    ui->doubleSpinBoxClimbVertSpeed->setSingleStep(100.);

    ui->doubleSpinBoxDescentVertSpeed->setDecimals(0);
    ui->doubleSpinBoxDescentVertSpeed->setMinimum(-10000.);
    ui->doubleSpinBoxDescentVertSpeed->setMaximum(-100.);
    ui->doubleSpinBoxDescentVertSpeed->setSingleStep(100.);
  }

  // Update units
  units = new UnitStringTool();
  units->init(unitWidgets, aircraftPerformance.useFuelAsVolume());

  // Create a backup for reset changes
  perfBackup = new AircraftPerf;
  *perfBackup = aircraftPerformance;

  // Copy from object to dialog
  fromPerfToDialog(perf);

  // Update vertical speed descent rule
  vertSpeedChanged();
  updateRange();

  // Fuel by volume or weight changed
  connect(ui->comboBoxFuelUnit, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &AircraftPerfDialog::fuelUnitChanged);

  // Update descent rule
  connect(ui->doubleSpinBoxDescentVertSpeed,
          static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          this, &AircraftPerfDialog::vertSpeedChanged);
  connect(ui->spinBoxDescentSpeed,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::vertSpeedChanged);

  connect(ui->spinBoxUsableFuel,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::updateRange);
  connect(ui->spinBoxCruiseSpeed,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::updateRange);
  connect(ui->spinBoxCruiseFuelFlow,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::updateRange);
  connect(ui->spinBoxReserveFuel,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::updateRange);

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &AircraftPerfDialog::buttonBoxClicked);
}

AircraftPerfDialog::~AircraftPerfDialog()
{
  saveState();

  delete perf;
  delete perfBackup;

  delete units;
  delete ui;
}

atools::fs::perf::AircraftPerf AircraftPerfDialog::getAircraftPerf() const
{
  return *perf;
}

void AircraftPerfDialog::restoreState()
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_EDIT_DIALOG);
  ws.restore({this, ui->tabWidget});

  switch(OptionData::instance().getUnitFuelAndWeight())
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      fuelUnit = perf->useFuelAsVolume() ? VOLUME_GAL : WEIGHT_LBS;
      break;
    case opts::FUEL_WEIGHT_LITER_KG:
      fuelUnit = perf->useFuelAsVolume() ? VOLUME_LITER : WEIGHT_KG;
      break;
  }
}

void AircraftPerfDialog::saveState()
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_EDIT_DIALOG);
  ws.save({this, ui->tabWidget});
}

void AircraftPerfDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    fromDialogToPerf(perf);
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Reset))
  {
    // Reset to backup
    *perf = *perfBackup;
    fromPerfToDialog(perf);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
  {
    // Reset to default values
    *perf = AircraftPerf();
    fromPerfToDialog(perf);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "AIRCRAFTPERFEDIT.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void AircraftPerfDialog::updateRange()
{
  // Check values and display error message =======================
  if(ui->spinBoxUsableFuel->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Usable fuel not set.")));
  else if(ui->spinBoxUsableFuel->value() <= ui->spinBoxReserveFuel->value())
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Usable fuel smaller than reserve.")));
  else if(ui->spinBoxCruiseSpeed->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Cruise speed not set.")));
  else if(ui->spinBoxCruiseFuelFlow->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Cruise fuel flow not set.")));
  else if(ui->spinBoxUsableFuel->value() > 1.)
  {
    // All is valid - display range =======================
    float speedKts = Unit::rev(static_cast<float>(ui->spinBoxCruiseSpeed->value()), Unit::speedKtsF);
    float fuelVolWeight = static_cast<float>(ui->spinBoxUsableFuel->value()) -
                          static_cast<float>(ui->spinBoxReserveFuel->value());
    float fuelFlowVolWeight = static_cast<float>(ui->spinBoxCruiseFuelFlow->value());

    float enduranceHours = fuelVolWeight / fuelFlowVolWeight;

    ui->labelUsableFuelRange->setText(tr("Estimated range with reserve %1, %2.").
                                      arg(Unit::distNm(enduranceHours * speedKts)).
                                      arg(formatter::formatMinutesHoursLong(enduranceHours)));
  }
}

void AircraftPerfDialog::vertSpeedChanged()
{
  float descentRateFtPerNm =
    Unit::rev(static_cast<float>(ui->doubleSpinBoxDescentVertSpeed->value()), Unit::speedVertFpmF) * 60.f /
    Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF);

  // 2,3 NM per 1000 ft

  QString txt = tr("Descent Rule of Thumb: %1 per %2 %3.").
                arg(Unit::distNm(1.f / -descentRateFtPerNm * Unit::rev(1000.f, Unit::altFeetF))).
                arg(QLocale().toString(1000.f, 'f', 0)).
                arg(Unit::getUnitAltStr());

  ui->labelDescentRule->setText(txt);
}

void AircraftPerfDialog::fuelUnitChanged()
{
  // Get previous fuel unit (volume/weight)
  bool volOld = isVol(fuelUnit);

  fromDialogToPerf(perf);

  fuelUnit = static_cast<FuelUnit>(ui->comboBoxFuelUnit->currentIndex());

  // Compare old and new fuel unit
  bool volNew = isVol(fuelUnit);
  if(volNew != volOld)
  {
    // Has changed - convert
    if(volNew)
      perf->fromLbsToGal();
    else
      perf->fromGalToLbs();
  }

  // Change widget suffixes
  units->update(volNew, (isMetric(fuelUnit) ? opts::FUEL_WEIGHT_LITER_KG : opts::FUEL_WEIGHT_GAL_LBS));
  fromPerfToDialog(perf);
}

void AircraftPerfDialog::fromPerfToDialog(const atools::fs::perf::AircraftPerf *aircraftPerf)
{
  {
    // Avoid recursion
    QSignalBlocker blocker(ui->comboBoxFuelUnit);
    ui->comboBoxFuelUnit->setCurrentIndex(fuelUnit);
  }

  ui->comboBoxFuelType->setCurrentIndex(aircraftPerf->isJetFuel());

  bool vol = isVol(fuelUnit);

  // Get pass through or conversion function for fuel units
  auto fuelFuncToDlg = fuelUnitsToDialogFunc(fuelUnit);

  ui->lineEditName->setText(aircraftPerf->getName());
  ui->lineEditType->setText(aircraftPerf->getAircraftType());
  ui->textBrowserDescription->setText(aircraftPerf->getDescription());

  ui->spinBoxUsableFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getUsableFuel(), vol)));
  ui->spinBoxReserveFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getReserveFuel(), vol)));
  ui->spinBoxExtraFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getExtraFuel(), vol)));
  ui->spinBoxTaxiFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getTaxiFuel(), vol)));

  ui->spinBoxClimbSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getClimbSpeed())));
  ui->doubleSpinBoxClimbVertSpeed->setValue(Unit::speedVertFpmF(aircraftPerf->getClimbVertSpeed()));
  ui->spinBoxClimbFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getClimbFuelFlow(), vol)));

  ui->spinBoxCruiseSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getCruiseSpeed())));
  ui->spinBoxCruiseFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getCruiseFuelFlow(), vol)));
  ui->spinBoxContingencyFuel->setValue(roundToInt(aircraftPerf->getContingencyFuel()));

  ui->spinBoxDescentSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getDescentSpeed())));
  ui->doubleSpinBoxDescentVertSpeed->setValue(-Unit::speedVertFpmF(aircraftPerf->getDescentVertSpeed()));
  ui->spinBoxDescentFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getDescentFuelFlow(), vol)));

  ui->spinBoxAlternateSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getAlternateSpeed())));
  ui->spinBoxAlternateFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getAlternateFuelFlow(), vol)));

  ui->spinBoxRunwayLength->setValue(roundToInt(Unit::distShortFeetF(aircraftPerf->getMinRunwayLength())));
  ui->comboBoxRunwayType->setCurrentIndex(aircraftPerf->getRunwayType());
}

void AircraftPerfDialog::fromDialogToPerf(atools::fs::perf::AircraftPerf *aircraftPerf) const
{
  bool vol = isVol(fuelUnit);

  // Get pass through or conversion function for fuel units
  auto fuelFunc = fuelUnitsFromDialogFunc(fuelUnit);

  aircraftPerf->setName(ui->lineEditName->text());
  aircraftPerf->setAircraftType(ui->lineEditType->text());
  aircraftPerf->setFuelAsVolume(vol);
  aircraftPerf->setJetFuel(ui->comboBoxFuelType->currentIndex()); // 0 = avgas
  aircraftPerf->setDescription(ui->textBrowserDescription->toPlainText());

  aircraftPerf->setUsableFuel(fuelFunc(ui->spinBoxUsableFuel->value(), vol));
  aircraftPerf->setReserveFuel(fuelFunc(ui->spinBoxReserveFuel->value(), vol));
  aircraftPerf->setExtraFuel(fuelFunc(ui->spinBoxExtraFuel->value(), vol));
  aircraftPerf->setTaxiFuel(fuelFunc(ui->spinBoxTaxiFuel->value(), vol));

  aircraftPerf->setClimbSpeed(Unit::rev(ui->spinBoxClimbSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setClimbVertSpeed(Unit::rev(static_cast<float>(ui->doubleSpinBoxClimbVertSpeed->value()),
                                            Unit::speedVertFpmF));
  aircraftPerf->setClimbFuelFlow(fuelFunc(ui->spinBoxClimbFuelFlow->value(), vol));

  aircraftPerf->setCruiseSpeed(Unit::rev(ui->spinBoxCruiseSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setCruiseFuelFlow(fuelFunc(ui->spinBoxCruiseFuelFlow->value(), vol));
  aircraftPerf->setContingencyFuel(ui->spinBoxContingencyFuel->value());

  aircraftPerf->setDescentSpeed(Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setDescentVertSpeed(-Unit::rev(static_cast<float>(ui->doubleSpinBoxDescentVertSpeed->value()),
                                               Unit::speedVertFpmF));
  aircraftPerf->setDescentFuelFlow(fuelFunc(ui->spinBoxDescentFuelFlow->value(), vol));

  aircraftPerf->setAlternateSpeed(Unit::rev(ui->spinBoxAlternateSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setAlternateFuelFlow(fuelFunc(ui->spinBoxAlternateFuelFlow->value(), vol));

  aircraftPerf->setMinRunwayLength(Unit::rev(ui->spinBoxRunwayLength->value(), Unit::distShortFeetF));
  aircraftPerf->setRunwayType(ui->comboBoxRunwayType->currentIndex());
}

bool AircraftPerfDialog::isMetric(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case VOLUME_GAL:
      return false;

    case WEIGHT_KG:
    case VOLUME_LITER:
      return true;
  }
  return false;
}

bool AircraftPerfDialog::isVol(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case WEIGHT_KG:
      return false;

    case VOLUME_GAL:
    case VOLUME_LITER:
      return true;
  }
  return false;
}

std::function<float(float value, bool fuelAsVolume)> AircraftPerfDialog::fuelUnitsToDialogFunc(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case VOLUME_GAL:
      return Unit::fromCopy;

    case WEIGHT_KG:
    case VOLUME_LITER:
      return Unit::fromUsToMetric;
  }
  return Unit::fromCopy;
}

std::function<float(float value, bool fuelAsVolume)> AircraftPerfDialog::fuelUnitsFromDialogFunc(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case VOLUME_GAL:
      return Unit::fromCopy;

    case WEIGHT_KG:
    case VOLUME_LITER:
      return Unit::fromMetricToUs;
  }
  return Unit::fromCopy;
}
