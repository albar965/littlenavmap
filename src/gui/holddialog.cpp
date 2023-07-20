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

#include "gui/holddialog.h"

#include "ui_holddialog.h"

#include "common/constants.h"
#include "common/maptypes.h"
#include "common/formatter.h"
#include "common/unitstringtool.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "settings/settings.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "common/mapresult.h"
#include "app/navapp.h"

#include <QColor>
#include <QColorDialog>

HoldDialog::HoldDialog(QWidget *parent, const map::MapResult& resultParam, const atools::geo::Pos& positionParam)
  : QDialog(parent), ui(new Ui::HoldDialog), color(Qt::darkBlue)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Copy result
  result = new map::MapResult;
  *result = resultParam;

  // Remove duplicates
  result->clearAllButFirst();

  // Remove all except airport, VOR, NDB and waypoints
  result->clear(~(map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT));

  position = new atools::geo::Pos;
  *position = positionParam;

  ui->buttonBoxHold->button(QDialogButtonBox::Ok)->setDefault(true);
  ui->comboBoxHoldTurnDirection->setFocus();

  connect(ui->buttonBoxHold, &QDialogButtonBox::clicked, this, &HoldDialog::buttonBoxClicked);
  connect(ui->pushButtonHoldColor, &QPushButton::clicked, this, &HoldDialog::colorButtonClicked);
  connect(ui->spinBoxHoldSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &HoldDialog::updateLength);
  connect(ui->doubleSpinBoxHoldTime, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          this, &HoldDialog::updateLength);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->spinBoxHoldSpeed, ui->spinBoxHoldAltitude});

  restoreState();
}

HoldDialog::~HoldDialog()
{
  atools::gui::WidgetState(lnm::HOLD_DIALOG).save(this);

  delete units;
  delete ui;
  delete result;
  delete position;
}

/* A button box button was clicked */
void HoldDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxHold->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxHold->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "HOLD.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxHold->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void HoldDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::HOLD_DIALOG, false);
  widgetState.restore({this,
                       ui->spinBoxHoldCourse,
                       ui->spinBoxHoldSpeed,
                       ui->spinBoxHoldAltitude,
                       ui->doubleSpinBoxHoldTime,
                       ui->comboBoxHoldTurnDirection});
  color = atools::settings::Settings::instance().valueVar(lnm::HOLD_DIALOG_COLOR, color).value<QColor>();

  updateWidgets();
  updateLength();
  updateButtonColor();
}

void HoldDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::HOLD_DIALOG, false);
  widgetState.save({this,
                    ui->spinBoxHoldCourse,
                    ui->spinBoxHoldSpeed,
                    ui->spinBoxHoldAltitude,
                    ui->doubleSpinBoxHoldTime,
                    ui->comboBoxHoldTurnDirection});
  atools::settings::Settings::instance().setValueVar(lnm::HOLD_DIALOG_COLOR, color);
}

void HoldDialog::colorButtonClicked()
{
  QColor col = QColorDialog::getColor(color, parentWidget());
  if(col.isValid())
  {
    color = col;
    updateButtonColor();
  }
}

void HoldDialog::updateButtonColor()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonHoldColor, color);
}

void HoldDialog::updateLength()
{
  float speedKts = Unit::rev(ui->spinBoxHoldSpeed->value(), Unit::speedKtsF);
  float mins = static_cast<float>(ui->doubleSpinBoxHoldTime->value());
  float dist = static_cast<float>(speedKts * mins / 60.);

  ui->labelHoldLength->setText(tr("Straight section length %1.<br/>"
                                  "Total time to complete is %2 minutes.").
                               arg(Unit::distNm(dist)).
                               arg(QLocale().toString(mins * 2.f + 2.f, 'f', 1)));
}

void HoldDialog::updateWidgets()
{
  QString text;

  if(result->hasAirports())
    text = map::airportText(result->airports.constFirst());
  else if(result->hasVor())
    text = map::vorText(result->vors.constFirst());
  else if(result->hasNdb())
    text = map::ndbText(result->ndbs.constFirst());
  else if(result->hasWaypoints())
    text = map::waypointText(result->waypoints.constFirst());
  else if(result->hasUserpoints())
    text = map::userpointText(result->userpoints.constFirst());
  else
    text = tr("Coordinates %1").arg(Unit::coords(*position));

  ui->labelHoldNavaid->setText(tr("<p><b>%1</b></p>").arg(text));
}

void HoldDialog::fillHold(map::HoldingMarker& holdingMarker)
{
  map::MapHolding& holding = holdingMarker.holding;

  // Assign an artifical id to the hold to allow internal identification
  holdingMarker.id = map::getNextUserFeatureId();
  holding.color = color;

  holding.id = -1;
  holding.navIdent.clear();
  holding.position = *position;
  holding.navType = map::NONE;
  holding.speedLimit = holding.length = holding.minAltititude = holding.maxAltititude = 0.f;
  holding.airportIdent.clear();

  if(result->hasAirports())
  {
    holding.navIdent = result->airports.constFirst().displayIdent();
    holding.position = result->airports.constFirst().position;
    holding.magvar = result->airports.constFirst().magvar;
    holding.navType = map::AIRPORT;
  }
  else if(result->hasVor())
  {
    const map::MapVor& vor = result->vors.constFirst();
    holding.navIdent = vor.ident;
    holding.position = vor.position;
    holding.magvar = vor.magvar;
    holding.vorDmeOnly = vor.dmeOnly;
    holding.vorHasDme = vor.hasDme;
    holding.vorTacan = vor.tacan;
    holding.vorVortac = vor.vortac;

    holding.navType = map::VOR;
  }
  else if(result->hasNdb())
  {
    holding.navIdent = result->ndbs.constFirst().ident;
    holding.position = result->ndbs.constFirst().position;
    holding.magvar = result->ndbs.constFirst().magvar;
    holding.navType = map::NDB;
  }
  else if(result->hasWaypoints())
  {
    holding.navIdent = result->waypoints.constFirst().ident;
    holding.position = result->waypoints.constFirst().position;
    holding.magvar = result->waypoints.constFirst().magvar;
    holding.navType = map::WAYPOINT;
  }
  else if(result->hasUserpoints())
  {
    holding.navIdent = result->userpoints.constFirst().ident;
    holding.position = result->userpoints.constFirst().position;
    holding.magvar = NavApp::getMagVar(*position);
    holding.navType = map::USERPOINT;
  }
  else
    // Use calculated declination
    holding.magvar = NavApp::getMagVar(*position);

  holding.position.setAltitude(Unit::rev(ui->spinBoxHoldAltitude->value(), Unit::altFeetF));
  holding.turnLeft = ui->comboBoxHoldTurnDirection->currentIndex() == 1;
  holding.time = static_cast<float>(ui->doubleSpinBoxHoldTime->value());
  holding.speedKts = Unit::rev(ui->spinBoxHoldSpeed->value(), Unit::speedKtsF);
  holding.courseTrue = atools::geo::normalizeCourse(ui->spinBoxHoldCourse->value() + holding.magvar);

  holdingMarker.position = holding.position;
}
