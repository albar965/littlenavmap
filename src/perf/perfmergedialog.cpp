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

#include "perfmergedialog.h"
#include "ui_perfmergedialog.h"

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

PerfMergeDialog::PerfMergeDialog(QWidget *parent, const AircraftPerf& sourcePerfLbs, AircraftPerf& destPerf,
                                 bool showAll)
  : QDialog(parent), ui(new Ui::PerfMergeDialog), to(destPerf), showAllWidgets(showAll)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  from = new atools::fs::perf::AircraftPerf(sourcePerfLbs);

  if(to.useFuelAsVolume())
    from->fromLbsToGal();

  ui->setupUi(this);

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &PerfMergeDialog::buttonBoxClicked);

  // Push buttons to set all combo boxes to same index
  connect(ui->pushButtonCopy, &QPushButton::clicked, this, &PerfMergeDialog::copyClicked);
  connect(ui->pushButtonMerge, &QPushButton::clicked, this, &PerfMergeDialog::mergeClicked);
  connect(ui->pushButtonIgnore, &QPushButton::clicked, this, &PerfMergeDialog::ignoreClicked);

  if(!showAllWidgets)
  {
    // Hide all widgets which are not relevant for collected performance data
    ui->comboBoxContingencyFuel->hide();
    ui->labelContingencyFuel->hide();
    ui->labelContingencyFuelIcon->hide();
    ui->labelContingencyFuelValue->hide();
    ui->labelContingencyFuelValue2->hide();

    ui->comboBoxExtraFuel->hide();
    ui->labelExtraFuel->hide();
    ui->labelExtraFuelIcon->hide();
    ui->labelExtraFuelValue->hide();
    ui->labelExtraFuelValue2->hide();

    ui->comboBoxReserveFuel->hide();
    ui->labelReserveFuel->hide();
    ui->labelReserveFuelIcon->hide();
    ui->labelReserveFuelValue->hide();
    ui->labelReserveFuelValue2->hide();

    ui->comboBoxUsableFuel->hide();
    ui->labelUsableFuel->hide();
    ui->labelUsableFuelIcon->hide();
    ui->labelUsableFuelValue->hide();
    ui->labelUsableFuelValue2->hide();

    ui->lineAlternate->hide();
    ui->comboBoxAlternateSpeed->hide();
    ui->labelAlternateSpeed->hide();
    ui->labelAlternateSpeedIcon->hide();
    ui->labelAlternateSpeedValue->hide();
    ui->labelAlternateSpeedValue2->hide();

    ui->comboBoxAlternateFuelFlow->hide();
    ui->labelAlternateFuelFlow->hide();
    ui->labelAlternateFuelFlowIcon->hide();
    ui->labelAlternateFuelFlowValue->hide();
    ui->labelAlternateFuelFlowValue2->hide();
  }

  restoreState();
  updateWidgetValues();
}

PerfMergeDialog::~PerfMergeDialog()
{
  atools::gui::WidgetState(lnm::AIRCRAFT_PERF_MERGE_DIALOG).save(this);

  delete from;
  delete ui;
}

void PerfMergeDialog::restoreState()
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_MERGE_DIALOG);
  ws.restore({this,
              ui->comboBoxName,
              ui->comboBoxType,
              ui->comboBoxClimbFuelFlow,
              ui->comboBoxClimbSpeed,
              ui->comboBoxClimbVertSpeed,
              ui->comboBoxCruiseFuelFlow,
              ui->comboBoxCruiseSpeed,
              ui->comboBoxDescentFuelFlow,
              ui->comboBoxDescentSpeed,
              ui->comboBoxDescentVertSpeed,
              ui->comboBoxTaxiFuel,
              ui->comboBoxAlternateFuelFlow,
              ui->comboBoxAlternateSpeed,
              ui->comboBoxContingencyFuel,
              ui->comboBoxExtraFuel,
              ui->comboBoxReserveFuel,
              ui->comboBoxUsableFuel});

  // Reset values after change from beta 2.4.1 to 2.4.2
  if(ui->comboBoxName->currentIndex() < 0 || ui->comboBoxName->currentIndex() > 1)
    ui->comboBoxName->setCurrentIndex(0);
  if(ui->comboBoxType->currentIndex() < 0 || ui->comboBoxType->currentIndex() > 1)
    ui->comboBoxType->setCurrentIndex(0);
}

void PerfMergeDialog::saveState()
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_MERGE_DIALOG);
  ws.save({this,
           ui->comboBoxName,
           ui->comboBoxType,
           ui->comboBoxClimbFuelFlow,
           ui->comboBoxClimbSpeed,
           ui->comboBoxClimbVertSpeed,
           ui->comboBoxCruiseFuelFlow,
           ui->comboBoxCruiseSpeed,
           ui->comboBoxDescentFuelFlow,
           ui->comboBoxDescentSpeed,
           ui->comboBoxDescentVertSpeed,
           ui->comboBoxTaxiFuel,
           ui->comboBoxAlternateFuelFlow,
           ui->comboBoxAlternateSpeed,
           ui->comboBoxContingencyFuel,
           ui->comboBoxExtraFuel,
           ui->comboBoxReserveFuel,
           ui->comboBoxUsableFuel});
}

void PerfMergeDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    saveState();
    process();
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "AIRCRAFTPERFMERGE.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
  {
    saveState();
    QDialog::reject();
  }
}

void PerfMergeDialog::process()
{
  // change flag is reset by proc method if values differ
  changed = false;

  to.setName(procStr(ui->comboBoxName, from->getName(), to.getName()));
  to.setAircraftType(procStr(ui->comboBoxType, from->getAircraftType(), to.getAircraftType()));

  to.setClimbFuelFlow(procNum(ui->comboBoxClimbFuelFlow, from->getClimbFuelFlow(), to.getClimbFuelFlow()));
  to.setClimbSpeed(procNum(ui->comboBoxClimbSpeed, from->getClimbSpeed(), to.getClimbSpeed()));
  to.setClimbVertSpeed(procNum(ui->comboBoxClimbVertSpeed, from->getClimbVertSpeed(), to.getClimbVertSpeed()));
  to.setCruiseFuelFlow(procNum(ui->comboBoxCruiseFuelFlow, from->getCruiseFuelFlow(), to.getCruiseFuelFlow()));
  to.setCruiseSpeed(procNum(ui->comboBoxCruiseSpeed, from->getCruiseSpeed(), to.getCruiseSpeed()));
  to.setDescentFuelFlow(procNum(ui->comboBoxDescentFuelFlow, from->getDescentFuelFlow(), to.getDescentFuelFlow()));
  to.setDescentSpeed(procNum(ui->comboBoxDescentSpeed, from->getDescentSpeed(), to.getDescentSpeed()));
  to.setDescentVertSpeed(procNum(ui->comboBoxDescentVertSpeed, from->getDescentVertSpeed(), to.getDescentVertSpeed()));
  to.setTaxiFuel(procNum(ui->comboBoxTaxiFuel, from->getTaxiFuel(), to.getTaxiFuel()));

  if(showAllWidgets)
  {
    to.setAlternateFuelFlow(procNum(ui->comboBoxAlternateFuelFlow, from->getAlternateFuelFlow(),
                                    to.getAlternateFuelFlow()));
    to.setAlternateSpeed(procNum(ui->comboBoxAlternateSpeed, from->getAlternateSpeed(), to.getAlternateSpeed()));
    to.setContingencyFuel(procNum(ui->comboBoxContingencyFuel, from->getContingencyFuel(), to.getContingencyFuel()));
    to.setExtraFuel(procNum(ui->comboBoxExtraFuel, from->getExtraFuel(), to.getExtraFuel()));
    to.setReserveFuel(procNum(ui->comboBoxReserveFuel, from->getReserveFuel(), to.getReserveFuel()));
    to.setUsableFuel(procNum(ui->comboBoxUsableFuel, from->getUsableFuel(), to.getUsableFuel()));
  }
}

void PerfMergeDialog::updateWidgetValues()
{
  // Show error if fuel type mismatch =====================================================
  QString err;
  if(from->isJetFuel() != to.isJetFuel())
    err = "<p>" + atools::util::HtmlBuilder::errorMessage(tr("Fuel type does not match.")) + "</p>";

  // Update header =====================================================
  if(showAllWidgets)
    ui->labelAircraft->setText(tr("<p>From <b>%1</b>, type <b>%2</b>, fuel type <b>%3</b><br/>"
                                  "to <b>%4</b>, type <b>%5</b>, fuel type <b>%6</b></p>%7").
                               arg(from->getName()).
                               arg(from->getAircraftType()).
                               arg(from->isAvgas() ? tr("Avgas") : tr("Jetfuel")).
                               arg(to.getName()).
                               arg(to.getAircraftType()).
                               arg(to.isAvgas() ? tr("Avgas") : tr("Jetfuel")).
                               arg(err));
  else
    ui->labelAircraft->setText(tr("<p>Merge current background collected performance to<br/>"
                                  "<b>%1</b>, type <b>%2</b>, fuel type <b>%3</b></p>%4").
                               arg(to.getName()).
                               arg(to.getAircraftType()).
                               arg(to.isAvgas() ? tr("Avgas") : tr("Jetfuel")).
                               arg(err));

  // Update labels with current values from and to =============================================
  ui->labelNameValue->setText(from->getName());
  ui->labelNameValue2->setText(to.getName());
  ui->labelTypeValue->setText(from->getAircraftType());
  ui->labelTypeValue2->setText(to.getAircraftType());

  ui->labelClimbFuelFlowValue->setText(Unit::ffLbsAndGal(from->getClimbFuelFlowLbs(),
                                                         from->getClimbFuelFlowGal()));
  ui->labelClimbFuelFlowValue2->setText(Unit::ffLbsAndGal(to.getClimbFuelFlowLbs(), to.getClimbFuelFlowGal()));

  ui->labelClimbSpeedValue->setText(Unit::speedKts(from->getClimbSpeed()));
  ui->labelClimbSpeedValue2->setText(Unit::speedKts(to.getClimbSpeed()));

  ui->labelClimbVertSpeedValue->setText(Unit::speedVertFpm(from->getClimbVertSpeed()));
  ui->labelClimbVertSpeedValue2->setText(Unit::speedVertFpm(to.getClimbVertSpeed()));

  ui->labelCruiseFuelFlowValue->setText(Unit::ffLbsAndGal(from->getCruiseFuelFlowLbs(),
                                                          from->getCruiseFuelFlowGal()));
  ui->labelCruiseFuelFlowValue2->setText(Unit::ffLbsAndGal(to.getCruiseFuelFlowLbs(), to.getCruiseFuelFlowGal()));

  ui->labelCruiseSpeedValue->setText(Unit::speedKts(from->getCruiseSpeed()));
  ui->labelCruiseSpeedValue2->setText(Unit::speedKts(to.getCruiseSpeed()));

  ui->labelDescentFuelFlowValue->setText(Unit::ffLbsAndGal(from->getDescentFuelFlowLbs(),
                                                           from->getDescentFuelFlowGal()));
  ui->labelDescentFuelFlowValue2->setText(Unit::ffLbsAndGal(to.getDescentFuelFlowLbs(), to.getDescentFuelFlowGal()));

  ui->labelDescentSpeedValue->setText(Unit::speedKts(from->getDescentSpeed()));
  ui->labelDescentSpeedValue2->setText(Unit::speedKts(to.getDescentSpeed()));

  ui->labelDescentVertSpeedValue->setText(Unit::speedVertFpm(from->getDescentVertSpeed()));
  ui->labelDescentVertSpeedValue2->setText(Unit::speedVertFpm(to.getDescentVertSpeed()));

  ui->labelTaxiFuelValue->setText(Unit::fuelLbsAndGal(from->getTaxiFuelLbs(), from->getTaxiFuelGal()));
  ui->labelTaxiFuelValue2->setText(Unit::fuelLbsAndGal(to.getTaxiFuelLbs(), to.getTaxiFuelGal()));

  if(showAllWidgets)
  {
    ui->labelAlternateFuelFlowValue->setText(Unit::ffLbsAndGal(from->getAlternateFuelFlowLbs(),
                                                               from->getAlternateFuelFlowGal()));
    ui->labelAlternateFuelFlowValue2->setText(Unit::ffLbsAndGal(to.getAlternateFuelFlowLbs(),
                                                                to.getAlternateFuelFlowGal()));

    ui->labelAlternateSpeedValue->setText(Unit::speedKts(from->getAlternateSpeed()));
    ui->labelAlternateSpeedValue2->setText(Unit::speedKts(to.getAlternateSpeed()));

    ui->labelContingencyFuelValue->setText(QLocale().toString(from->getContingencyFuel(), 'f',
                                                              0) + tr(" percent"));
    ui->labelContingencyFuelValue2->setText(QLocale().toString(to.getContingencyFuel(), 'f', 0) + tr(" percent"));

    ui->labelExtraFuelValue->setText(Unit::fuelLbsAndGal(from->getExtraFuelLbs(), from->getExtraFuelGal()));
    ui->labelExtraFuelValue2->setText(Unit::fuelLbsAndGal(to.getExtraFuelLbs(), to.getExtraFuelGal()));

    ui->labelReserveFuelValue->setText(Unit::fuelLbsAndGal(from->getReserveFuelLbs(), from->getReserveFuelGal()));
    ui->labelReserveFuelValue2->setText(Unit::fuelLbsAndGal(to.getReserveFuelLbs(), to.getReserveFuelGal()));

    ui->labelUsableFuelValue->setText(Unit::fuelLbsAndGal(from->getUsableFuelLbs(), from->getUsableFuelGal()));
    ui->labelUsableFuelValue2->setText(Unit::fuelLbsAndGal(to.getUsableFuelLbs(), to.getUsableFuelGal()));
  }
}

void PerfMergeDialog::ignoreClicked()
{
  updateComboBoxWidgetsNum(NUM_IGNORE);
  updateComboBoxWidgetsStr(STR_IGNORE);
}

void PerfMergeDialog::copyClicked()
{
  updateComboBoxWidgetsNum(NUM_COPY);
  updateComboBoxWidgetsStr(STR_COPY);
}

void PerfMergeDialog::mergeClicked()
{
  updateComboBoxWidgetsNum(NUM_MERGE);
  updateComboBoxWidgetsStr(STR_COPY);
}

void PerfMergeDialog::updateComboBoxWidgetsStr(ComboBoxIndexStr idx)
{
  /* Ignore copy merge */
  ui->comboBoxName->setCurrentIndex(idx);
  ui->comboBoxType->setCurrentIndex(idx);
}

void PerfMergeDialog::updateComboBoxWidgetsNum(ComboBoxIndexNum idx)
{
  ui->comboBoxClimbFuelFlow->setCurrentIndex(idx);
  ui->comboBoxClimbSpeed->setCurrentIndex(idx);
  ui->comboBoxClimbVertSpeed->setCurrentIndex(idx);
  ui->comboBoxCruiseFuelFlow->setCurrentIndex(idx);
  ui->comboBoxCruiseSpeed->setCurrentIndex(idx);
  ui->comboBoxDescentFuelFlow->setCurrentIndex(idx);
  ui->comboBoxDescentSpeed->setCurrentIndex(idx);
  ui->comboBoxDescentVertSpeed->setCurrentIndex(idx);
  ui->comboBoxTaxiFuel->setCurrentIndex(idx);

  if(showAllWidgets)
  {
    ui->comboBoxAlternateFuelFlow->setCurrentIndex(idx);
    ui->comboBoxAlternateSpeed->setCurrentIndex(idx);
    ui->comboBoxContingencyFuel->setCurrentIndex(idx);
    ui->comboBoxExtraFuel->setCurrentIndex(idx);
    ui->comboBoxReserveFuel->setCurrentIndex(idx);
    ui->comboBoxUsableFuel->setCurrentIndex(idx);
  }
}

float PerfMergeDialog::procNum(QComboBox *combo, float fromValue, float toValue)
{
  ComboBoxIndexNum idx = static_cast<ComboBoxIndexNum>(combo->currentIndex());
  if(idx != NUM_IGNORE)
  {
    changed = idx == NUM_COPY || idx == NUM_MERGE;

    if(idx == NUM_COPY)
      return fromValue;
    else if(idx == NUM_MERGE)
      return (toValue + fromValue) / 2.f;
  }
  return toValue;
}

QString PerfMergeDialog::procStr(QComboBox *combo, QString fromValue, QString toValue)
{
  ComboBoxIndexStr idx = static_cast<ComboBoxIndexStr>(combo->currentIndex());
  if(idx != STR_IGNORE)
  {
    changed = idx == STR_COPY;

    if(idx == STR_COPY)
      return fromValue;
  }
  return toValue;
}
