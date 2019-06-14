/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
#include "common/unitstringtool.h"
#include "settings/settings.h"
#include "common/formatter.h"
#include "util/htmlbuilder.h"

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
    ui->spinBoxTaxiFuel,
    ui->spinBoxReserveFuel,
    ui->doubleSpinBoxClimbVertSpeed,
    ui->spinBoxDescentSpeed,
    ui->doubleSpinBoxDescentVertSpeed,
    ui->spinBoxAlternateSpeed,
    ui->spinBoxAlternateFuelFlow,
    ui->spinBoxRunwayLength,
    ui->spinBoxUsableFuel
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
  {
    saveState();
    QDialog::reject();
  }
}

void AircraftPerfDialog::updateRange()
{
  if(ui->spinBoxUsableFuel->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Usable fuel not set.")));
  else if(ui->spinBoxUsableFuel->value() <= ui->spinBoxReserveFuel->value())
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Usable fuel smaller than reserve.")));
  else if(ui->spinBoxUsableFuel->value() > 1.)
  {
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

  ui->spinBoxUsableFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getUsableFuel(), vol)));
  ui->spinBoxReserveFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getReserveFuel(), vol)));
  ui->spinBoxExtraFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getExtraFuel(), vol)));
  ui->spinBoxTaxiFuel->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getTaxiFuel(), vol)));

  ui->spinBoxClimbSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getClimbSpeed())));
  ui->doubleSpinBoxClimbVertSpeed->setValue(Unit::speedVertFpmF(aircraftPerf->getClimbVertSpeed()));
  ui->spinBoxClimbFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getClimbFuelFlow(), vol)));

  ui->spinBoxCruiseSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getCruiseSpeed())));
  ui->spinBoxCruiseFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getCruiseFuelFlow(), vol)));
  ui->spinBoxContingencyFuel->setValue(roundToInt(aircraftPerf->getContingencyFuel()));

  ui->spinBoxDescentSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getDescentSpeed())));
  ui->doubleSpinBoxDescentVertSpeed->setValue(-Unit::speedVertFpmF(aircraftPerf->getDescentVertSpeed()));
  ui->spinBoxDescentFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getDescentFuelFlow(), vol)));

  ui->spinBoxAlternateSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getAlternateSpeed())));
  ui->spinBoxAlternateFuelFlow->setValue(roundToInt(Unit::fuelLbsGallonF(aircraftPerf->getAlternateFuelFlow(), vol)));

  ui->spinBoxRunwayLength->setValue(roundToInt(Unit::distShortFeetF(aircraftPerf->getMinRunwayLength())));
  ui->comboBoxRunwayType->setCurrentIndex(aircraftPerf->getRunwayType());
}

void AircraftPerfDialog::fromDialog(atools::fs::perf::AircraftPerf *aircraftPerf) const
{
  bool vol = ui->comboBoxFuelUnit->currentIndex() == 1;

  aircraftPerf->setName(ui->lineEditName->text());
  aircraftPerf->setAircraftType(ui->lineEditType->text());
  aircraftPerf->setFuelAsVolume(ui->comboBoxFuelUnit->currentIndex());
  aircraftPerf->setJetFuel(ui->comboBoxFuelType->currentIndex()); // 0 = avgas
  aircraftPerf->setDescription(ui->textBrowserDescription->toPlainText());

  aircraftPerf->setUsableFuel(Unit::rev(ui->spinBoxUsableFuel->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setReserveFuel(Unit::rev(ui->spinBoxReserveFuel->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setExtraFuel(Unit::rev(ui->spinBoxExtraFuel->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setTaxiFuel(Unit::rev(ui->spinBoxTaxiFuel->value(), Unit::fuelLbsGallonF, vol));

  aircraftPerf->setClimbSpeed(Unit::rev(ui->spinBoxClimbSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setClimbVertSpeed(Unit::rev(static_cast<float>(ui->doubleSpinBoxClimbVertSpeed->value()),
                                            Unit::speedVertFpmF));
  aircraftPerf->setClimbFuelFlow(Unit::rev(ui->spinBoxClimbFuelFlow->value(), Unit::fuelLbsGallonF, vol));

  aircraftPerf->setCruiseSpeed(Unit::rev(ui->spinBoxCruiseSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setCruiseFuelFlow(Unit::rev(ui->spinBoxCruiseFuelFlow->value(), Unit::fuelLbsGallonF, vol));
  aircraftPerf->setContingencyFuel(ui->spinBoxContingencyFuel->value());

  aircraftPerf->setDescentSpeed(Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setDescentVertSpeed(-Unit::rev(static_cast<float>(ui->doubleSpinBoxDescentVertSpeed->value()),
                                               Unit::speedVertFpmF));
  aircraftPerf->setDescentFuelFlow(Unit::rev(ui->spinBoxDescentFuelFlow->value(), Unit::fuelLbsGallonF, vol));

  aircraftPerf->setAlternateSpeed(Unit::rev(ui->spinBoxAlternateSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setAlternateFuelFlow(Unit::rev(ui->spinBoxAlternateFuelFlow->value(), Unit::fuelLbsGallonF, vol));

  aircraftPerf->setMinRunwayLength(Unit::rev(ui->spinBoxRunwayLength->value(), Unit::distShortFeetF));
  aircraftPerf->setRunwayType(ui->comboBoxRunwayType->currentIndex());
}
