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

#include "gui/trafficpatterndialog.h"

#include "common/maptypes.h"
#include "common/constants.h"
#include "gui/widgetstate.h"
#include "query/airportquery.h"
#include "geo/calculations.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"
#include "navapp.h"
#include "common/unit.h"
#include "gui/helphandler.h"

#include "ui_trafficpatterndialog.h"

#include <QColorDialog>

using atools::geo::Pos;

TrafficPatternDialog::TrafficPatternDialog(QWidget *parent, const map::MapAirport& mapAirport) :
  QDialog(parent), ui(new Ui::TrafficPatternDialog), airport(mapAirport), color(Qt::darkRed)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  doubleSpinBoxTrafficPatternBaseDistanceSuffix = ui->doubleSpinBoxTrafficPatternBaseDistance->suffix();
  doubleSpinBoxDownwindDistanceSuffix = ui->doubleSpinBoxDownwindDistance->suffix();
  spinBoxTrafficPatternAltitudeSuffix = ui->spinBoxTrafficPatternAltitude->suffix();

  updateWidgetUnits();

  fillRunwayComboBox();
  fillAirportLabel();

  connect(ui->checkBoxTrafficPattern45Degree, &QCheckBox::toggled, this, &TrafficPatternDialog::updateWidgets);
  connect(ui->comboBoxTrafficPatternRunway, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &TrafficPatternDialog::updateRunwayLabel);
  connect(ui->buttonBoxTrafficPattern, &QDialogButtonBox::clicked, this, &TrafficPatternDialog::buttonBoxClicked);
  connect(ui->pushButtonTrafficPatternColor, &QPushButton::clicked, this, &TrafficPatternDialog::colorButtonClicked);

  restoreState();
}

TrafficPatternDialog::~TrafficPatternDialog()
{
  delete ui;
}

/* A button box button was clicked */
void TrafficPatternDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Ok))
  {
    QDialog::accept();
    saveState();
  }
  else if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(
      parentWidget(), lnm::HELP_ONLINE_URL + "TRAFFICPATTERN.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void TrafficPatternDialog::fillAirportLabel()
{
  ui->labelTrafficPatternAirport->setText(tr("%1, elevation %2").
                                          arg(map::airportTextShort(airport)).
                                          arg(Unit::altFeet(airport.position.getAltitude())));
}

void TrafficPatternDialog::fillRunwayComboBox()
{
  const QList<map::MapRunway> *rw = NavApp::getAirportQuerySim()->getRunways(airport.id);
  if(rw != nullptr)
    runways = *rw;

  std::sort(runways.begin(), runways.end(), [](const map::MapRunway& rw1, const map::MapRunway& rw2) -> bool {
    return rw1.length > rw2.length;
  });

  int index = 0;
  for(const map::MapRunway& runway : runways)
  {
    QString atts;
    if(runway.isHard())
      atts = tr("Hard");
    else if(runway.isSoft())
      atts = tr("Soft");
    else if(runway.isWater())
      atts = tr("Water");

    QString primaryName(tr("%1, %2, %3").
                        arg(runway.primaryName).
                        arg(Unit::distShortFeet(runway.length)).
                        arg(atts));
    QString secondaryName(tr("%1, %2, %3").
                          arg(runway.secondaryName).
                          arg(Unit::distShortFeet(runway.length)).
                          arg(atts));

    ui->comboBoxTrafficPatternRunway->addItem(primaryName, QVariantList({index, true}));
    ui->comboBoxTrafficPatternRunway->addItem(secondaryName, QVariantList({index, false}));
    index++;
  }

  updateRunwayLabel(0);
}

void TrafficPatternDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::TRAFFIC_PATTERN_DIALOG, false);
  widgetState.restore({
    ui->doubleSpinBoxDownwindDistance,
    ui->spinBoxTrafficPatternAltitude,
    ui->doubleSpinBoxTrafficPatternBaseDistance,
    ui->checkBoxTrafficPattern45Degree,
    ui->comboBoxTrafficPatternTurnDirection,
    ui->checkBoxTrafficPatternEntryExit
  });
  color = atools::settings::Settings::instance().valueVar(lnm::TRAFFIC_PATTERN_DIALOG_COLOR, color).value<QColor>();

  updateWidgets();
  updateButtonColor();
  updateRunwayLabel(ui->comboBoxTrafficPatternRunway->currentIndex());
}

void TrafficPatternDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::TRAFFIC_PATTERN_DIALOG, false);
  widgetState.save({
    ui->doubleSpinBoxDownwindDistance,
    ui->spinBoxTrafficPatternAltitude,
    ui->doubleSpinBoxTrafficPatternBaseDistance,
    ui->checkBoxTrafficPattern45Degree,
    ui->comboBoxTrafficPatternTurnDirection,
    ui->checkBoxTrafficPatternEntryExit
  });
  atools::settings::Settings::instance().setValueVar(lnm::TRAFFIC_PATTERN_DIALOG_COLOR, color);
}

void TrafficPatternDialog::colorButtonClicked()
{
  QColor col = QColorDialog::getColor(color, parentWidget());
  if(col.isValid())
  {
    color = col;
    updateButtonColor();
  }
}

void TrafficPatternDialog::updateButtonColor()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonTrafficPatternColor, color);
}

void TrafficPatternDialog::updateWidgetUnits()
{
  ui->doubleSpinBoxTrafficPatternBaseDistance->
  setSuffix(Unit::replacePlaceholders(doubleSpinBoxTrafficPatternBaseDistanceSuffix));
  ui->doubleSpinBoxDownwindDistance->setSuffix(Unit::replacePlaceholders(doubleSpinBoxDownwindDistanceSuffix));
  ui->spinBoxTrafficPatternAltitude->setSuffix(Unit::replacePlaceholders(spinBoxTrafficPatternAltitudeSuffix));
}

void TrafficPatternDialog::updateRunwayLabel(int index)
{
  if(index >= 0)
  {
    QVariantList data = ui->comboBoxTrafficPatternRunway->itemData(index).toList();
    int rwIndex = data.at(0).toInt();
    bool primary = data.at(1).toBool();

    const map::MapRunway& rw = runways.at(rwIndex);

    float heading = atools::geo::normalizeCourse((primary ? rw.heading : atools::geo::opposedCourseDeg(
                                                    rw.heading)) - airport.magvar);

    QStringList atts;
    if(rw.isLighted())
      atts.append(tr("Lighted"));

    if(primary && rw.primaryClosed)
      atts.append(tr("Closed"));
    if(!primary && rw.secondaryClosed)
      atts.append(tr("Closed"));

    QString name(tr("<b>%1</b>, %2 x %3, %4°M, %5%6").
                 arg(primary ? rw.primaryName : rw.secondaryName).
                 arg(Unit::distShortFeet(rw.length, false)).
                 arg(Unit::distShortFeet(rw.width)).
                 arg(QLocale().toString(heading, 'f', 0)).
                 arg(map::surfaceName(rw.surface)).
                 arg(atts.isEmpty() ? QString() : ", " + atts.join(", ")));
    ui->labelTrafficPatternRunwayInfo->setText(name);
  }
}

void TrafficPatternDialog::updateWidgets()
{
  ui->doubleSpinBoxTrafficPatternBaseDistance->setEnabled(!ui->checkBoxTrafficPattern45Degree->isChecked());
}

void TrafficPatternDialog::fillTrafficPattern(map::TrafficPattern& pattern)
{
  QVariantList data = ui->comboBoxTrafficPatternRunway->currentData().toList();
  int rwIndex = data.at(0).toInt();
  bool primary = data.at(1).toBool();

  const map::MapRunway& rw = runways.at(rwIndex);

  pattern.airportIcao = airport.ident;
  pattern.runwayName = primary ? rw.primaryName : rw.secondaryName;
  pattern.color = color;
  pattern.turnRight = ui->comboBoxTrafficPatternTurnDirection->currentIndex() == 1;
  pattern.base45Degree = ui->checkBoxTrafficPattern45Degree->isChecked();
  pattern.showEntryExit = ui->checkBoxTrafficPatternEntryExit->isChecked();
  pattern.downwindDistance =
    Unit::rev(static_cast<float>(ui->doubleSpinBoxDownwindDistance->value()), Unit::distNmF);
  pattern.baseDistance =
    Unit::rev(static_cast<float>(ui->doubleSpinBoxTrafficPatternBaseDistance->value()), Unit::distNmF);

  pattern.heading = primary ? rw.heading : atools::geo::opposedCourseDeg(rw.heading);
  pattern.magvar = airport.magvar;
  pattern.runwayLength = rw.length - (primary ? rw.primaryOffset : rw.secondaryOffset);

  float heading = primary ? atools::geo::opposedCourseDeg(rw.heading) : rw.heading;
  Pos pos = rw.position.endpointRhumb(
    atools::geo::feetToMeter(rw.length / 2 - (primary ? rw.primaryOffset : rw.secondaryOffset)), heading);
  pos.setAltitude(Unit::rev(static_cast<float>(ui->spinBoxTrafficPatternAltitude->value()), Unit::altFeetF) +
                  airport.getPosition().getAltitude());
  pattern.position = pos;
}
