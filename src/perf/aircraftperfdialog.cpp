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

#include "perf/aircraftperfdialog.h"
#include "ui_aircraftperfdialog.h"

#include "common/unit.h"
#include "atools.h"
#include "common/constants.h"
#include "gui/helphandler.h"
#include "fs/perf/aircraftperf.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"
#include "common/unitstringtool.h"

#include <QPushButton>

using atools::fs::perf::AircraftPerf;
using atools::fs::perf::fuelUnitFromString;
using atools::fs::perf::fuelUnitToString;
using atools::roundToInt;

AircraftPerfDialog::AircraftPerfDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& aircraftPerformance)
  : QDialog(parent), ui(new Ui::AircraftPerfDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  restoreState();

  // Update units
  bool fuelAsVol = ui->comboBoxFuelUnit->currentIndex() == 1;
  units = new UnitStringTool();
  units->init({
    ui->comboBoxFuelUnit,
    ui->spinBoxClimbFuelFlow,
    ui->spinBoxCruiseSpeed,
    ui->spinBoxDescentFuelFlow,
    ui->spinBoxCruiseFuelFlow,
    ui->spinBoxContingencyFuel,
    ui->spinBoxClimbSpeed,
    ui->spinBoxExtraFuel,
    ui->spinBoxDescentSpeed,
    ui->spinBoxTaxiFuel,
    ui->spinBoxReserveFuel,
    ui->spinBoxClimbVertSpeed,
    ui->spinBoxDescentSpeed,
    ui->spinBoxDescentVertSpeed
  }, fuelAsVol);

  // Copy performance object
  perf = new AircraftPerf;
  *perf = aircraftPerformance;

  // Create a backup for reset changes
  perfBackup = new AircraftPerf;
  *perfBackup = aircraftPerformance;

  // Copy from object to dialog
  toDialog(perf);

  // Update vertical speed descent rule
  vertSpeedChanged();

  connect(ui->comboBoxFuelUnit, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &AircraftPerfDialog::updateUnits);
  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &AircraftPerfDialog::buttonBoxClicked);

  connect(ui->spinBoxDescentVertSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::vertSpeedChanged);
}

AircraftPerfDialog::~AircraftPerfDialog()
{
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
  ws.restore(this);

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  resize(settings.valueVar(lnm::AIRCRAFT_PERF_EDIT_DIALOG_SIZE).toSize());
}

void AircraftPerfDialog::saveState()
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_EDIT_DIALOG);
  ws.save(this);

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValueVar(lnm::AIRCRAFT_PERF_EDIT_DIALOG_SIZE, size());
}

void AircraftPerfDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    saveState();
    fromDialog(perf);
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Reset))
  {
    // Reset to backup
    *perf = *perfBackup;
    toDialog(perf);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
  {
    // Reset to default values
    *perf = AircraftPerf();
    toDialog(perf);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::HELP_ONLINE_URL + "AIRCRAFTPERFEDIT.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void AircraftPerfDialog::vertSpeedChanged()
{
  QString txt = tr("Descent Rule: %1 %2 per %3 %4").
                arg(Unit::altFeetF(1.f / ui->spinBoxDescentVertSpeed->value() * 1000.f), 0, 'f', 1).
                arg(Unit::getUnitDistStr()).
                arg(QLocale().toString(1000.f, 'f', 0)).
                arg(Unit::getUnitAltStr());

  ui->labelDescentRule->setText(txt);
}

void AircraftPerfDialog::updateUnits()
{
  units->update(ui->comboBoxFuelUnit->currentIndex() == 1);
}

void AircraftPerfDialog::toDialog(const atools::fs::perf::AircraftPerf *aircraftPerf)
{
  ui->comboBoxFuelUnit->setCurrentIndex(static_cast<int>(aircraftPerf->getFuelUnit()));
  bool vol = ui->comboBoxFuelUnit->currentIndex() == 1;

  ui->lineEditName->setText(aircraftPerf->getName());
  ui->lineEditType->setText(aircraftPerf->getAircraftType());
  ui->textBrowserDescription->setText(aircraftPerf->getDescription());

  ui->spinBoxReserveFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getReserveFuel(), vol)));
  ui->spinBoxExtraFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getExtraFuel(), vol)));
  ui->spinBoxTaxiFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getTaxiFuel(), vol)));

  ui->spinBoxClimbSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getClimbSpeed())));
  ui->spinBoxClimbVertSpeed->setValue(roundToInt(Unit::speedVertFpmF(aircraftPerf->getClimbVertSpeed())));
  ui->spinBoxClimbFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getClimbFuelFlow(), vol)));

  ui->spinBoxCruiseSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getCruiseSpeed())));
  ui->spinBoxCruiseFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getCruiseFuelFlow(), vol)));
  ui->spinBoxContingencyFuel->setValue(aircraftPerf->getContingencyFuel());

  ui->spinBoxDescentSpeed->setValue(roundToInt(Unit::speedVertFpmF(aircraftPerf->getDescentSpeed())));
  ui->spinBoxDescentVertSpeed->setValue(roundToInt(Unit::speedVertFpmF(aircraftPerf->getDescentVertSpeed())));
  ui->spinBoxDescentFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getDescentFuelFlow(), vol)));
}

void AircraftPerfDialog::fromDialog(atools::fs::perf::AircraftPerf *aircraftPerf) const
{
  bool vol = ui->comboBoxFuelUnit->currentIndex() == 1;

  aircraftPerf->setName(ui->lineEditName->text());
  aircraftPerf->setAircraftType(ui->lineEditType->text());
  aircraftPerf->setFuelUnit(static_cast<atools::fs::perf::FuelUnit>(ui->comboBoxFuelUnit->currentIndex()));
  aircraftPerf->setDescription(ui->textBrowserDescription->toPlainText());

  aircraftPerf->setReserveFuel(roundToInt(Unit::rev(ui->spinBoxReserveFuel->value(), Unit::fuelLbsGallonF, vol)));
  aircraftPerf->setExtraFuel(roundToInt(Unit::rev(ui->spinBoxExtraFuel->value(), Unit::fuelLbsGallonF, vol)));
  aircraftPerf->setTaxiFuel(roundToInt(Unit::rev(ui->spinBoxTaxiFuel->value(), Unit::fuelLbsGallonF, vol)));

  aircraftPerf->setClimbSpeed(roundToInt(Unit::rev(ui->spinBoxClimbSpeed->value(), Unit::speedKtsF)));
  aircraftPerf->setClimbVertSpeed(roundToInt(Unit::rev(ui->spinBoxClimbVertSpeed->value(), Unit::speedVertFpmF)));
  aircraftPerf->setClimbFuelFlow(roundToInt(Unit::rev(ui->spinBoxClimbFuelFlow->value(), Unit::fuelLbsGallonF, vol)));

  aircraftPerf->setCruiseSpeed(roundToInt(Unit::rev(ui->spinBoxCruiseSpeed->value(), Unit::speedKtsF)));
  aircraftPerf->setCruiseFuelFlow(roundToInt(Unit::rev(ui->spinBoxCruiseFuelFlow->value(), Unit::fuelLbsGallonF, vol)));
  aircraftPerf->setContingencyFuel(ui->spinBoxContingencyFuel->value());

  aircraftPerf->setDescentSpeed(roundToInt(Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF)));
  aircraftPerf->setDescentVertSpeed(roundToInt(Unit::rev(ui->spinBoxDescentVertSpeed->value(), Unit::speedVertFpmF)));
  aircraftPerf->setDescentFuelFlow(
    roundToInt(Unit::rev(ui->spinBoxDescentFuelFlow->value(), Unit::fuelLbsGallonF, vol)));
}
