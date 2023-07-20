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

#include "gui/rangemarkerdialog.h"

#include "common/constants.h"
#include "gui/widgetutil.h"
#include "geo/calculations.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"
#include "gui/helphandler.h"
#include "common/unitstringtool.h"
#include "fs/util/coordinates.h"
#include "common/unit.h"
#include "common/maptypes.h"
#include "common/formatter.h"
#include "atools.h"
#include "geo/pos.h"
#include "perf/aircraftperfcontroller.h"
#include "fs/perf/aircraftperf.h"
#include "app/navapp.h"

#include "ui_rangemarkerdialog.h"

#include <QColorDialog>
#include <QRegularExpressionValidator>

using atools::geo::Pos;

// Also adjust text in options dialog if changing these numbers
const float MIN_RANGE_RING_SIZE = 0.01f;
const float MAX_RANGE_RING_SIZE_METER = atools::geo::EARTH_CIRCUMFERENCE_METER / 2.f * 0.98f;

const int MAX_RANGE_RINGS = 10;
const QVector<float> RangeMarkerDialog::MAP_RANGERINGS_DEFAULT({50.f, 100.f, 200.f, 500.f});

RangeRingValidator::RangeRingValidator()
{
}

QValidator::State RangeRingValidator::validate(QString& input, int&) const
{
  int numRing = 0;
  QStringList split = input.simplified().split(tr(" ", "Range ring separator"));
  if(split.isEmpty())
    // Nothing entered - do not keep user from editing
    return Intermediate;

  QValidator::State state = Acceptable;
  for(const QString& str : split)
  {
    QString val = str.trimmed();
    if(!val.isEmpty())
    {
      bool ok;
      float num = QLocale().toFloat(val, &ok);
      if(!ok || num > atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2))
        // No number or radius too large - keep user from adding more characters
        state = Invalid;

      if(num < MIN_RANGE_RING_SIZE && state == Acceptable)
        // Zero entered - do not keep user from editing
        state = Intermediate;

      numRing++;
    }
  }

  if(numRing > MAX_RANGE_RINGS)
    // Too many rings - keep user from adding more characters
    state = Invalid;

  return state;
}

// ------------------------------------------------------------------------

RangeMarkerDialog::RangeMarkerDialog(QWidget *parent, const atools::geo::Pos& pos)
  : QDialog(parent), ui(new Ui::RangeMarkerDialog), color(Qt::darkRed)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  position = new atools::geo::Pos(pos);

  // Default is OK
  ui->buttonBoxRangeMarker->button(QDialogButtonBox::Ok)->setDefault(true);

  // Change label depending on order
  if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY)
    ui->labelRangeMarkerLatLon->setText(tr("&Longitude and Latitude:"));
  else
    ui->labelRangeMarkerLatLon->setText(tr("&Latitude and Longitude:"));

  connect(ui->buttonBoxRangeMarker, &QDialogButtonBox::clicked, this, &RangeMarkerDialog::buttonBoxClicked);
  connect(ui->pushButtonRangeMarkerColor, &QPushButton::clicked, this, &RangeMarkerDialog::colorButtonClicked);
  connect(ui->lineEditRangeMarkerLatLon, &QLineEdit::textChanged, this, &RangeMarkerDialog::coordinatesEdited);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->labelRangeMarkerRadii, ui->lineEditRangeMarkerRadii});

  // Set radii label text
  ui->labelRangeMarkerRadii->setText(ui->labelRangeMarkerRadii->text().
                                     arg(MAX_RANGE_RINGS).
                                     arg(Unit::distNmF(MIN_RANGE_RING_SIZE), 0, 'f', 2).
                                     arg(atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2)));

  rangeRingValidator = new RangeRingValidator;
  ui->lineEditRangeMarkerRadii->setValidator(rangeRingValidator);

  /// Header text
  ui->labelRangeMarkerHeader->setText(tr("<p><b>%1</b></p>").arg(tr("Coordinates at click spot: %1").arg(Unit::coords(*position))));

  // Aircraft endurance label
  float enduranceHours, enduranceNm;
  NavApp::getAircraftPerfController()->getEnduranceFull(enduranceHours, enduranceNm);

  const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerfController()->getAircraftPerformance();

  QStringList headerList({perf.getName(), perf.getAircraftType()});
  QString header = atools::strJoin(headerList, tr(" - "), tr(" - "), tr(".\n"));
  if(!header.isEmpty())
    header.prepend(tr("Aircraft Performance: "));

  if(enduranceNm < map::INVALID_DISTANCE_VALUE)
    ui->labelRangeMarkerAircraft->setText(tr("%1Estimated range with reserve: %2").arg(header).arg(Unit::distNm(enduranceNm)));
  else
    ui->labelRangeMarkerAircraft->setText(tr("%1Estimated range not valid.").arg(header));

  restoreState();

  ui->lineEditRangeMarkerLatLon->setText(Unit::coords(*position));
  coordinatesEdited(QString());
}

RangeMarkerDialog::~RangeMarkerDialog()
{
  atools::gui::WidgetState(lnm::RANGE_MARKER_DIALOG).save({this, ui->checkBoxRangeMarkerDoNotShow});

  delete rangeRingValidator;
  delete units;
  delete ui;
  delete position;
}

/* A button box button was clicked */
void RangeMarkerDialog::buttonBoxClicked(QAbstractButton *button)
{
  saveState();

  if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Ok))
    QDialog::accept();
  else if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Help))
    // Keep dialog open
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "RANGERINGS.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Close))
    QDialog::reject();
  else if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Reset))
  {
    // Reset all back to default
    ui->lineEditRangeMarkerLatLon->setText(Unit::coords(*position));
    coordinatesEdited(QString());
    color = Qt::red;
    updateButtonColor();
    ui->lineEditRangeMarkerRadii->setText(rangeFloatToString(MAP_RANGERINGS_DEFAULT));
    ui->lineEditRangeMarkerLabel->clear();
  }
}

void RangeMarkerDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::RANGE_MARKER_DIALOG, false);
  widgetState.restore({this, ui->buttonGroupRangeMarker, ui->lineEditRangeMarkerRadii, ui->lineEditRangeMarkerLabel,
                       ui->checkBoxRangeMarkerDoNotShow});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  color = settings.valueVar(lnm::RANGE_MARKER_DIALOG_COLOR, QColor(Qt::red)).value<QColor>();

  if(settings.contains(lnm::RANGE_MARKER_DIALOG_RADII))
    ui->lineEditRangeMarkerRadii->
    setText(rangeFloatToString(atools::strListToFloatVector(settings.valueStrList(lnm::RANGE_MARKER_DIALOG_RADII))));

  updateButtonColor();
}

void RangeMarkerDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::RANGE_MARKER_DIALOG, false);
  widgetState.save({this, ui->buttonGroupRangeMarker, ui->lineEditRangeMarkerRadii, ui->lineEditRangeMarkerLabel,
                    ui->checkBoxRangeMarkerDoNotShow});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValueVar(lnm::RANGE_MARKER_DIALOG_COLOR, color);
  settings.setValue(lnm::RANGE_MARKER_DIALOG_RADII, atools::floatVectorToStrList(rangeStringToFloat(ui->lineEditRangeMarkerRadii->text())));
}

void RangeMarkerDialog::colorButtonClicked()
{
  QColor col = QColorDialog::getColor(color, parentWidget());
  if(col.isValid())
  {
    color = col;
    updateButtonColor();
  }
}

void RangeMarkerDialog::updateButtonColor()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonRangeMarkerColor, color);
}

void RangeMarkerDialog::fillRangeMarker(map::RangeMarker& marker, bool dialogOpened)
{
  bool hemisphere = false;
  *position = atools::fs::util::fromAnyFormat(ui->lineEditRangeMarkerLatLon->text(), &hemisphere);

  if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY && !hemisphere)
    // Swap coordinates for lat lon formats if no hemisphere (N, S, E, W) is given
    atools::fs::util::maybeSwapOrdinates(*position, ui->lineEditRangeMarkerLatLon->text());

  marker.id = map::getNextUserFeatureId();
  marker.navType = map::NONE;
  marker.position = *position;
  marker.color = color;

  if(dialogOpened)
    marker.text = ui->lineEditRangeMarkerLabel->text();

  // Ignore aircraft option if dialog was not opened (i.e. Ctrl+Click into map)
  if(ui->radioButtonRangeMarkerRadii->isChecked() || !dialogOpened)
  {
    for(float dist : rangeStringToFloat(ui->lineEditRangeMarkerRadii->text()))
      marker.ranges.append(Unit::rev(dist, Unit::distNmF));
  }
  else
  {
    float enduranceHours, enduranceNm;
    NavApp::getAircraftPerfController()->getEnduranceFull(enduranceHours, enduranceNm);

    const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerfController()->getAircraftPerformance();

    if(!perf.getName().isEmpty())
      marker.text.append(atools::elideTextShort(perf.getName(), 20));
    else if(!perf.getAircraftType().isEmpty())
      marker.text.append(perf.getAircraftType());

    // Do not create anything if range is not valid
    if(enduranceNm < map::INVALID_DISTANCE_VALUE)
      marker.ranges.append(std::round(enduranceNm));
  }
}

bool RangeMarkerDialog::isNoShowShiftClickEnabled()
{
  return ui->checkBoxRangeMarkerDoNotShow->isChecked();
}

QString RangeMarkerDialog::rangeFloatToString(const QVector<float>& ranges) const
{
  QLocale locale;
  // Do not print group separator - can cause issues if space is used
  locale.setNumberOptions(QLocale::OmitGroupSeparator);

  QStringList txt;
  for(float value : ranges)
  {
    if(value >= MIN_RANGE_RING_SIZE && value <= atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2))
      txt.append(locale.toString(value, 'g', 6));
  }

  if(txt.isEmpty())
    return rangeFloatToString(MAP_RANGERINGS_DEFAULT);
  else
    return txt.join(tr(" ", "Range ring number separator"));
}

QVector<float> RangeMarkerDialog::rangeStringToFloat(const QString& rangeStr) const
{
  QVector<float> retval;
  for(const QString& str : rangeStr.simplified().split(tr(" ", "Range ring separator")))
  {
    QString val = str.trimmed();
    if(!val.isEmpty())
    {
      bool ok;
      float num = QLocale().toFloat(val, &ok);
      if(ok && num >= MIN_RANGE_RING_SIZE && num <= atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2))
        retval.append(num);
    }
  }

  if(retval.isEmpty())
    retval = MAP_RANGERINGS_DEFAULT;
  else
    std::sort(retval.begin(), retval.end());

  return retval;
}

void RangeMarkerDialog::coordinatesEdited(const QString&)
{
  QString message;
  atools::geo::Pos pos;
  bool valid = formatter::checkCoordinates(message, ui->lineEditRangeMarkerLatLon->text(), &pos);
  ui->buttonBoxRangeMarker->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelRangeMarkerCoordStatus->setText(message);
}
