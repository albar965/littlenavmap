/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel albar965@mailbox.org
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
using atools::roundToInt;

AircraftPerfDialog::AircraftPerfDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& aircraftPerformance)
  : QDialog(parent), ui(new Ui::AircraftPerfDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  restoreState();

  // Update units
  units = new UnitStringTool();
  units->init({
    ui->comboBoxFuelUnit,
    ui->comboBoxFuelType,
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
  }, aircraftPerformance.useFuelAsVolume());

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

  // Fuel by volume or weight changed
  connect(ui->comboBoxFuelUnit, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &AircraftPerfDialog::fuelUnitChanged);

  // Update descent rule
  connect(ui->spinBoxDescentVertSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &AircraftPerfDialog::vertSpeedChanged);

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &AircraftPerfDialog::buttonBoxClicked);

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
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "AIRCRAFTPERFEDIT.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void AircraftPerfDialog::vertSpeedChanged()
{
  float descentRateFtPerNm = ui->spinBoxDescentVertSpeed->value() * 60.f / ui->spinBoxDescentSpeed->value();

  QString txt = tr("Descent Rule of Thumb: %1 per %2 %3").
                arg(Unit::distNm(1.f / -descentRateFtPerNm * 1000.f)).
                arg(QLocale().toString(1000.f, 'f', 0)).
                arg(Unit::getUnitAltStr());

  ui->labelDescentRule->setText(txt);
}

void AircraftPerfDialog::fuelUnitChanged()
{
  // Avoid recursion
  QSignalBlocker blocker(ui->comboBoxFuelUnit);

  bool fuelAsVolume = ui->comboBoxFuelUnit->currentIndex() == 1;
  if(fuelAsVolume && perf->useFuelAsVolume())
    // Nothing changed
    return;

  fromDialog(perf);
  if(fuelAsVolume)
    perf->fromLbsToGal();
  else
    perf->fromGalToLbs();

  units->update(fuelAsVolume);
  toDialog(perf);
}

void AircraftPerfDialog::toDialog(const atools::fs::perf::AircraftPerf *aircraftPerf)
{
  ui->comboBoxFuelUnit->setCurrentIndex(aircraftPerf->useFuelAsVolume());
  ui->comboBoxFuelType->setCurrentIndex(aircraftPerf->isJetFuel());
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
  ui->spinBoxContingencyFuel->setValue(roundToInt(aircraftPerf->getContingencyFuel()));

  ui->spinBoxDescentSpeed->setValue(roundToInt(Unit::speedVertFpmF(aircraftPerf->getDescentSpeed())));
  ui->spinBoxDescentVertSpeed->setValue(-roundToInt(Unit::speedVertFpmF(aircraftPerf->getDescentVertSpeed())));
  ui->spinBoxDescentFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getDescentFuelFlow(), vol)));
}

void AircraftPerfDialog::fromDialog(atools::fs::perf::AircraftPerf *aircraftPerf) const
{
  bool vol = ui->comboBoxFuelUnit->currentIndex() == 1;

  aircraftPerf->setName(ui->lineEditName->text());
  aircraftPerf->setAircraftType(ui->lineEditType->text());
  aircraftPerf->setFuelAsVolume(ui->comboBoxFuelUnit->currentIndex());
  aircraftPerf->setJetFuel(ui->comboBoxFuelType->currentIndex()); // 0 = avgas
  aircraftPerf->setDescription(ui->textBrowserDescription->toPlainText());

  aircraftPerf->setReserveFuel(Unit::rev(ui->spinBoxReserveFuel->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setExtraFuel(Unit::rev(ui->spinBoxExtraFuel->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setTaxiFuel(Unit::rev(ui->spinBoxTaxiFuel->value(), Unit::fuelLbsGallonF, vol));

  aircraftPerf->setClimbSpeed(Unit::rev(ui->spinBoxClimbSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setClimbVertSpeed(Unit::rev(ui->spinBoxClimbVertSpeed->value(), Unit::speedVertFpmF));
  aircraftPerf->setClimbFuelFlow(Unit::rev(ui->spinBoxClimbFuelFlow->value(), Unit::fuelLbsGallonF, vol));

  aircraftPerf->setCruiseSpeed(Unit::rev(ui->spinBoxCruiseSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setCruiseFuelFlow(Unit::rev(ui->spinBoxCruiseFuelFlow->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setContingencyFuel(ui->spinBoxContingencyFuel->value());

  aircraftPerf->setDescentSpeed(Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setDescentVertSpeed(-Unit::rev(ui->spinBoxDescentVertSpeed->value(), Unit::speedVertFpmF));
  aircraftPerf->setDescentFuelFlow(Unit::rev(ui->spinBoxDescentFuelFlow->value(), Unit::fuelLbsGallonF, vol));
}
