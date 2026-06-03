/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "marker/rangemarkerdialog.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/mapmarkers.h"
#include "common/mapresult.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "fs/perf/aircraftperf.h"
#include "fs/util/coordinates.h"
#include "gui/helphandler.h"
#include "gui/tools.h"
#include "gui/widgetstate.h"
#include "perf/aircraftperfcontroller.h"
#include "settings/settings.h"
#include "ui_rangemarkerdialog.h"

#include <QRegularExpressionValidator>

using atools::geo::Pos;

// Also adjust text in options dialog if changing these numbers
const double MIN_RANGE_RING_SIZE_NM = 0.01;
const double MAX_RANGE_RING_SIZE_METER = atools::geo::EARTH_CIRCUMFERENCE_METER / 2. * 0.98;
const int MAX_RANGE_RINGS = 10;

const QList<double> RangeMarkerDialog::MAP_RANGERINGS_DEFAULT({50., 100., 200., 500.});

const QString RangeMarkerDialog::DEFAULT_COLOR_STR = QColor(Qt::darkRed).name(QColor::HexArgb);

QValidator::State RangeRingValidator::validate(QString& input, int&) const
{
  int numRing = 0;
  QStringList split = input.simplified().split(tr(" ", "Range ring separator"));
  if(split.isEmpty())
    // Nothing entered - do not keep user from editing
    return Intermediate;

  QValidator::State state = Acceptable;
  for(const QString& str : std::as_const(split))
  {
    QString val = str.trimmed();
    if(!val.isEmpty())
    {
      bool ok;
      double num = QLocale().toDouble(val, &ok);
      if(!ok || num > atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2))
        // No number or radius too large - keep user from adding more characters
        state = Invalid;

      if(num < MIN_RANGE_RING_SIZE_NM && state == Acceptable)
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

RangeMarkerDialog::RangeMarkerDialog(QWidget *parent, const map::RangeMarker& markerParam, const map::MapResult& result, bool editMode)
  : MarkerDialog(parent, tr("Range Rings"), markerParam, result, editMode), ui(new Ui::RangeMarkerDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  // Assign buttons for base class functions
  setDoNotShowCheckbox(ui->checkBoxRangeMarkerDoNotShow);
  setPushButtonColor(ui->pushButtonRangeMarkerColor);
  setLabelHeader(ui->labelRangeMarkerHeader);

  // Default is OK
  ui->buttonBoxRangeMarker->button(QDialogButtonBox::Ok)->setDefault(true);

  // Focus line input
  ui->lineEditRangeMarkerRadii->setFocus();

  // Change label depending on order
  if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY)
    ui->labelRangeMarkerLatLon->setText(tr("&Longitude and Latitude:"));
  else
    ui->labelRangeMarkerLatLon->setText(tr("&Latitude and Longitude:"));

  connect(ui->buttonBoxRangeMarker, &QDialogButtonBox::clicked, this, &RangeMarkerDialog::buttonBoxClicked);
  connect(ui->pushButtonRangeMarkerColor, &QPushButton::clicked, this, &MarkerDialog::colorButtonClicked);
  connect(ui->lineEditRangeMarkerLatLon, &QLineEdit::textChanged, this, &RangeMarkerDialog::coordinatesEdited);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->labelRangeMarkerRadii, ui->lineEditRangeMarkerRadii});

  // Set radii label text
  ui->labelRangeMarkerRadii->setText(ui->labelRangeMarkerRadii->text().
                                     arg(MAX_RANGE_RINGS).
                                     arg(Unit::distNmF(MIN_RANGE_RING_SIZE_NM), 0, 'f', 2).
                                     arg(atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2)));

  rangeRingValidator = new RangeRingValidator;
  ui->lineEditRangeMarkerRadii->setValidator(rangeRingValidator);

  // Aircraft endurance label
  float enduranceHours, enduranceNm;
  NavApp::getAircraftPerfController()->getEnduranceFull(enduranceHours, enduranceNm);

  const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerfController()->getAircraftPerformance();

  QString header = atools::strJoin({perf.getName(), perf.getAircraftType()}, tr(" - "), tr(" - "), tr(".\n"));
  if(!header.isEmpty())
    header.prepend(tr("Aircraft Performance: "));

  if(enduranceNm < map::INVALID_DISTANCE_VALUE)
    ui->labelRangeMarkerAircraft->setText(tr("%1Estimated range with reserve: %2").arg(header).arg(Unit::distNm(enduranceNm)));
  else
    ui->labelRangeMarkerAircraft->setText(tr("%1Estimated range not valid.").arg(header));

  ui->lineEditRangeMarkerLatLon->setText(Unit::coords(marker->position));
  coordinatesEdited(QStringLiteral());

  restoreState();
}

RangeMarkerDialog::~RangeMarkerDialog()
{
  // Save dialog position and size
  atools::gui::WidgetState(lnm::RANGE_MARKER_DIALOG).save(QList<const QObject *>({this, ui->checkBoxRangeMarkerDoNotShow}));

  delete rangeRingValidator;
  delete units;
  delete ui;
}

void RangeMarkerDialog::buttonBoxClicked(QAbstractButton *button)
{
  atools::gui::WidgetState(lnm::RANGE_MARKER_DIALOG).save(this);

  if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Help))
    // Keep dialog open
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "RANGERINGS.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Cancel))
    QDialog::reject();
  else if(button == ui->buttonBoxRangeMarker->button(QDialogButtonBox::Reset))
  {
    // Reset all back to default
    ui->lineEditRangeMarkerLatLon->setText(Unit::coords(marker->position));
    coordinatesEdited(QStringLiteral());
    ui->lineEditRangeMarkerRadii->setText(rangeDoubleToString(MAP_RANGERINGS_DEFAULT));
    ui->lineEditRangeMarkerLabel->clear();

    marker->color = QColor(DEFAULT_COLOR_STR);
    updateButtonColor();
  }
}

void RangeMarkerDialog::restoreState()
{

  atools::gui::WidgetState widgetState(lnm::RANGE_MARKER_DIALOG);

  // Load dialog position and size
  widgetState.restore(this);

  if(isEditMode())
  {
    widgetState.restore(ui->checkBoxRangeMarkerDoNotShow);
    markerToWidgets();
  }
  else
  {
    textPreFilled = !marker->text.isEmpty();

    if(textPreFilled)
      // Text already given by navaid - do note restore line edit
      widgetState.restore({ui->buttonGroupRangeMarker, ui->lineEditRangeMarkerRadii, ui->checkBoxRangeMarkerDoNotShow});
    else
      // Also restore default text for line edit
      widgetState.restore({ui->buttonGroupRangeMarker, ui->lineEditRangeMarkerRadii, ui->lineEditRangeMarkerLabel,
                           ui->checkBoxRangeMarkerDoNotShow});

    atools::settings::Settings& settings = atools::settings::Settings::instance();
    // Color is save directly in marker
    marker->color = settings.valueStr(lnm::RANGE_MARKER_DIALOG_COLOR, DEFAULT_COLOR_STR);

    if(textPreFilled)
      // Pre-filled text to widget
      ui->lineEditRangeMarkerLabel->setText(marker->text);

    if(settings.contains(lnm::RANGE_MARKER_DIALOG_RADII))
      ui->lineEditRangeMarkerRadii->
      setText(rangeDoubleToString(atools::strListToDoubleList(settings.valueStrList(lnm::RANGE_MARKER_DIALOG_RADII))));

    fillMarkerFromResultAndWidgets();
  }

  updateButtonColor();
  updateHeader();
}

void RangeMarkerDialog::saveState() const
{
  atools::gui::WidgetState widgetState(lnm::RANGE_MARKER_DIALOG);

  if(textPreFilled)
    // Do not save pre-filled text from navaid
    widgetState.save({this, ui->buttonGroupRangeMarker, ui->lineEditRangeMarkerRadii, ui->checkBoxRangeMarkerDoNotShow});
  else
    widgetState.save({this, ui->buttonGroupRangeMarker, ui->lineEditRangeMarkerRadii, ui->lineEditRangeMarkerLabel,
                      ui->checkBoxRangeMarkerDoNotShow});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValue(lnm::RANGE_MARKER_DIALOG_COLOR, marker->color.name(QColor::HexArgb));
  settings.setValue(lnm::RANGE_MARKER_DIALOG_RADII, atools::doubleListToStrList(rangeStringToDouble(ui->lineEditRangeMarkerRadii->text())));
}

QString RangeMarkerDialog::rangeDoubleToString(const QList<double>& ranges) const
{
  QLocale locale;
  // Do not print group separator - can cause issues if space is used
  locale.setNumberOptions(QLocale::OmitGroupSeparator);

  QStringList txt;
  for(double value : ranges)
  {
    if(value >= MIN_RANGE_RING_SIZE_NM && value <= atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2))
      txt.append(locale.toString(value, 'g', 6));
  }

  if(txt.isEmpty())
    return rangeDoubleToString(MAP_RANGERINGS_DEFAULT);
  else
    return txt.join(tr(" ", "Range ring number separator"));
}

const QList<double> RangeMarkerDialog::rangeStringToDouble(const QString& rangeStr) const
{
  QList<double> retval;
  for(const QString& str : rangeStr.simplified().split(tr(" ", "Range ring separator")))
  {
    QString val = str.trimmed();
    if(!val.isEmpty())
    {
      bool ok;
      double num = QLocale().toDouble(val, &ok);
      if(ok && num >= MIN_RANGE_RING_SIZE_NM && num <= atools::roundToPrecision(Unit::distMeterF(MAX_RANGE_RING_SIZE_METER), 2))
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

  // Disable ok for invalid coordinates
  ui->buttonBoxRangeMarker->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelRangeMarkerCoordStatus->setText(message);
}

void RangeMarkerDialog::markerToWidgets()
{
  ui->lineEditRangeMarkerRadii->setText(rangeDoubleToString(marker->ranges));
  ui->radioButtonRangeMarkerRadii->setChecked(!marker->aircraftRange);
  ui->radioButtonRangeMarkerAircraft->setChecked(marker->aircraftRange);
  ui->lineEditRangeMarkerLabel->setText(marker->text);
  ui->lineEditRangeMarkerLatLon->setText(Unit::coords(marker->position));
}

void RangeMarkerDialog::fillMarkerFromResultAndWidgets()
{
  // Get navaid information from result
  atools::geo::Pos position;
  getResult()->getParams({map::AIRPORT, map::VOR, map::NDB, map::WAYPOINT, map::USERPOINT}, nullptr, &position, &marker->nav);
  if(!(marker->nav.magvar < map::INVALID_MAGVAR))
    marker->nav.magvar = NavApp::getMagVar(position, 0.f);

  if(position.isValidRange())
    // Snap to navaid position
    marker->position = position;

  if(marker->text.isEmpty() && !textPreFilled)
    // Set text if not filled by navaid
    marker->text = marker->getIdent();

  widgetsToMarker();
}

void RangeMarkerDialog::widgetsToMarker()
{
  // When navType is set (AIRPORT, VOR, NDB, WAYPOINT), the position should come from the object itself
  // to avoid coordinate transformation and rounding errors from the text field
  if(marker->nav.type == map::NONE)
  {
    bool hemisphere = false;
    marker->position = atools::fs::util::fromAnyFormat(ui->lineEditRangeMarkerLatLon->text(), &hemisphere);

    if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY && !hemisphere)
      // Swap coordinates for lat lon formats if no hemisphere (N, S, E, W) is given
      atools::fs::util::maybeSwapOrdinates(marker->position, ui->lineEditRangeMarkerLatLon->text());
  }

  // Cannot be moved
  marker->attachedToNavaid = false;

  // Normal range rings and not aircraft
  marker->aircraftRange = false;

  if(!ui->lineEditRangeMarkerLabel->text().isEmpty() && marker->text != ui->lineEditRangeMarkerLabel->text())
  {
    // Label in widget is not empty and different to marker - set manually
    marker->text = ui->lineEditRangeMarkerLabel->text();
    marker->manualLabel = true;
  }
  else if(ui->lineEditRangeMarkerLabel->text().isEmpty())
  {
    // Label is empty - generate label from navaid
    marker->text = getResult()->markerLabel();
    marker->manualLabel = false;
  }

  // Ignore aircraft option if dialog was not opened (i.e. Ctrl+Click into map)
  if(ui->radioButtonRangeMarkerRadii->isChecked())
  {
    marker->ranges.clear();
    for(double dist : rangeStringToDouble(ui->lineEditRangeMarkerRadii->text()))
      marker->ranges.append(Unit::rev(dist, Unit::distNmF));
    marker->aircraftRange = false;
  }
  else
  {
    float enduranceHours, enduranceNm;
    NavApp::getAircraftPerfController()->getEnduranceFull(enduranceHours, enduranceNm);
    const atools::fs::perf::AircraftPerf& perf = NavApp::getAircraftPerfController()->getAircraftPerformance();

    marker->text.clear();
    if(!perf.getName().isEmpty())
      marker->text.append(atools::elideTextShort(perf.getName(), 20));
    else if(!perf.getAircraftType().isEmpty())
      marker->text.append(perf.getAircraftType());

    // Do not create anything if range is not valid
    marker->ranges.clear();
    if(enduranceNm < map::INVALID_DISTANCE_VALUE)
      marker->ranges.append(std::round(enduranceNm));

    marker->manualLabel = true;
    marker->aircraftRange = true;
  }
}
