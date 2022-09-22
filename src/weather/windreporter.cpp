/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "weather/windreporter.h"

#include "navapp.h"
#include "ui_mainwindow.h"
#include "grib/windquery.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "options/optiondata.h"
#include "common/unit.h"
#include "perf/aircraftperfcontroller.h"
#include "route/route.h"
#include "mapgui/maplayer.h"
#include "gui/dialog.h"

#include <QToolButton>
#include <QMessageBox>
#include <QDir>

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
static int queryMaxRows = 5000;

namespace windinternal {

WindSliderAction::WindSliderAction(QObject *parent)
  : QWidgetAction(parent)
{
  sliderValue = minValue();
  setSliderValue(sliderValue);
}

int WindSliderAction::getAltitudeFt() const
{
  return sliderValue * WIND_SLIDER_STEP_ALT_FT;
}

void WindSliderAction::setAltitudeFt(int altitude)
{
  setSliderValue(altitude / WIND_SLIDER_STEP_ALT_FT);
}

QWidget *WindSliderAction::createWidget(QWidget *parent)
{
  QSlider *slider = new QSlider(Qt::Horizontal, parent);
  slider->setToolTip(tr("Altitude for wind level display"));
  slider->setStatusTip(slider->toolTip());
  slider->setMinimum(minValue());
  slider->setMaximum(maxValue());

  slider->setTickPosition(QSlider::TicksBothSides);
  slider->setTickInterval(5000 / WIND_SLIDER_STEP_ALT_FT);
  slider->setPageStep(1000 / WIND_SLIDER_STEP_ALT_FT);
  slider->setSingleStep(500 / WIND_SLIDER_STEP_ALT_FT);
  slider->setTracking(true);
  slider->setValue(sliderValue);

  connect(slider, &QSlider::valueChanged, this, &WindSliderAction::setSliderValue);
  connect(slider, &QSlider::valueChanged, this, &WindSliderAction::valueChanged);
  connect(slider, &QSlider::sliderReleased, this, &WindSliderAction::sliderReleased);

  // Add to list (register)
  sliders.append(slider);
  return slider;
}

void WindSliderAction::deleteWidget(QWidget *widget)
{
  QSlider *slider = dynamic_cast<QSlider *>(widget);
  if(slider != nullptr)
  {
    disconnect(slider, &QSlider::valueChanged, this, &WindSliderAction::setSliderValue);
    disconnect(slider, &QSlider::valueChanged, this, &WindSliderAction::valueChanged);
    disconnect(slider, &QSlider::sliderReleased, this, &WindSliderAction::sliderReleased);
    sliders.removeAll(slider);
    delete widget;
  }
}

int WindSliderAction::minValue() const
{
  return MIN_WIND_ALT / WIND_SLIDER_STEP_ALT_FT;
}

int WindSliderAction::maxValue() const
{
  return MAX_WIND_ALT / WIND_SLIDER_STEP_ALT_FT;
}

void WindSliderAction::setSliderValue(int value)
{
  sliderValue = value;
  for(QSlider *slider : sliders)
  {
    slider->blockSignals(true);
    slider->setValue(sliderValue);
    slider->blockSignals(false);
  }
}

// =======================================================================================

/*
 * Wrapper for label action.
 */
class WindLabelAction
  : public QWidgetAction
{
public:
  WindLabelAction(QObject *parent) : QWidgetAction(parent)
  {
  }

  void setText(const QString& textParam);

protected:
  /* Create a delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* List of created/registered labels */
  QVector<QLabel *> labels;
  QString text;
};

void WindLabelAction::setText(const QString& textParam)
{
  text = textParam;
  // Set text to all registered labels
  for(QLabel *label : labels)
    label->setText(text);
}

QWidget *WindLabelAction::createWidget(QWidget *parent)
{
  QLabel *label = new QLabel(parent);
  label->setMargin(4);
  label->setText(text);
  label->setEnabled(isEnabled());
  labels.append(label);
  return label;
}

void WindLabelAction::deleteWidget(QWidget *widget)
{
  labels.removeAll(dynamic_cast<QLabel *>(widget));
  delete widget;
}

} // namespace internal

// =======================================================================================

WindReporter::WindReporter(QObject *parent, atools::fs::FsPaths::SimulatorType type)
  : QObject(parent), simType(type)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  verbose = settings.getAndStoreValue(lnm::OPTIONS_WEATHER_DEBUG, false).toBool();

  // Real wind ==================
  windQueryOnline = new atools::grib::WindQuery(parent, verbose);
  connect(windQueryOnline, &atools::grib::WindQuery::windDataUpdated, this, &WindReporter::windDownloadFinished);
  connect(windQueryOnline, &atools::grib::WindQuery::windDownloadFailed, this, &WindReporter::windDownloadFailed);
  connect(windQueryOnline, &atools::grib::WindQuery::windDownloadSslErrors, this, &WindReporter::windDownloadSslErrors);
  connect(windQueryOnline, &atools::grib::WindQuery::windDownloadProgress, this, &WindReporter::windDownloadProgress);

  // Layers from custom settings ==================
  windQueryManual = new atools::grib::WindQuery(parent, verbose);
  windQueryManual->initFromFixedModel(0.f, 0.f, 0.f);

  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionMapShowWindDisabled, &QAction::triggered, this, &WindReporter::sourceActionTriggered);
  connect(ui->actionMapShowWindNOAA, &QAction::triggered, this, &WindReporter::sourceActionTriggered);
  connect(ui->actionMapShowWindSimulator, &QAction::triggered, this, &WindReporter::sourceActionTriggered);
}

WindReporter::~WindReporter()
{
  qDebug() << Q_FUNC_INFO << "delete windQueryOnline";
  delete windQueryOnline;
  windQueryOnline = nullptr;
  qDebug() << Q_FUNC_INFO << "delete windQueryManual";
  delete windQueryManual;
  windQueryManual = nullptr;
  qDebug() << Q_FUNC_INFO << "delete actionGroup";
  delete actionGroup;
  actionGroup = nullptr;
  qDebug() << Q_FUNC_INFO << "delete windlevelToolButton";
  delete windlevelToolButton;
  windlevelToolButton = nullptr;
}

void WindReporter::preDatabaseLoad()
{

}

void WindReporter::postDatabaseLoad(atools::fs::FsPaths::SimulatorType type)
{
  if(type != simType)
  {
    // Simulator has changed - reload files
    simType = type;
    updateDataSource();
  }
}

void WindReporter::optionsChanged()
{
  updateDataSource();
}

void WindReporter::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::MAP_WIND_LEVEL, sliderActionAltitude->getAltitudeFt());
  atools::settings::Settings::instance().setValue(lnm::MAP_WIND_SELECTION, currentWindSelection);
  atools::settings::Settings::instance().setValue(lnm::MAP_WIND_LEVEL_ROUTE, showFlightplanWaypoints);
  atools::settings::Settings::instance().setValue(lnm::MAP_WIND_SOURCE, currentSource);
}

void WindReporter::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    atools::settings::Settings& settings = atools::settings::Settings::instance();

    // Defaults also set if keys are missing
    currentWindSelection = static_cast<wind::WindSelection>(settings.valueInt(lnm::MAP_WIND_SELECTION, wind::NONE));
    showFlightplanWaypoints = settings.valueBool(lnm::MAP_WIND_LEVEL_ROUTE, false);
    currentSource = static_cast<wind::WindSource>(settings.valueInt(lnm::MAP_WIND_SOURCE, wind::NOAA));
    sliderActionAltitude->setAltitudeFt(settings.valueInt(lnm::MAP_WIND_LEVEL, 10000));
  }
  valuesToAction();

  // Download wind data with a delay after startup
  QTimer::singleShot(2000, this, &WindReporter::updateDataSource);
  updateToolButtonState();
  updateSliderLabel();
}

void WindReporter::updateDataSource()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  actionToValues();
  downloadErrorReported = false;

  if(ui->actionMapShowWindSimulator->isChecked() && atools::fs::FsPaths::isAnyXplane(simType))
  {
    if(simType == atools::fs::FsPaths::XPLANE_11)
    {
      // Load GRIB file only if X-Plane is enabled - will call windDownloadFinished later
      QString path = OptionData::instance().getWeatherXplaneWind();
      if(path.isEmpty())
        path = NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "global_winds.grib";

      windQueryOnline->initFromPath(path, atools::fs::weather::WEATHER_XP11);
    }
    else if(simType == atools::fs::FsPaths::XPLANE_12)
    {
      QString path = OptionData::instance().getWeatherXplane12Path();
      if(path.isEmpty())
        // Use default base path
        path = NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "Output" + QDir::separator() + "real weather";

      windQueryOnline->initFromPath(path, atools::fs::weather::WEATHER_XP12);
    }
  }
  else if(ui->actionMapShowWindNOAA->isChecked())
    // Download from NOAA - will call windDownloadFinished later
    windQueryOnline->initFromUrl(OptionData::instance().getWeatherNoaaWindBaseUrl());
  else
  {
    windQueryOnline->deinit();
    updateToolButtonState();
    updateSliderLabel();

    emit windUpdated();
    emit windDisplayUpdated();
  }
}

void WindReporter::windDownloadFinished()
{
  qDebug() << Q_FUNC_INFO;
  updateToolButtonState();
  updateSliderLabel();

  if(!isWindManual())
  {
    QDateTime from, to;
    windQueryOnline->getValidity(from, to);

    QString validText = from.isValid() && to.isValid() ? tr(" Forecast from %1 to %2 UTC.").
                        arg(QLocale().toString(from, QLocale::ShortFormat)).
                        arg(QLocale().toString(to, QLocale::ShortFormat)) : QString();

    QString msg;
    switch(currentSource)
    {
      case wind::NO_SOURCE:
        break;

      case wind::SIMULATOR:
        msg = tr("Winds aloft updated from simulator.%1").arg(validText);
        break;

      case wind::NOAA:
        msg = tr("Winds aloft downloaded from NOAA.%1").arg(validText);
        break;
    }
    NavApp::setStatusMessage(msg, true /* addToLog */);
  }
  emit windUpdated();
  emit windDisplayUpdated();
}

void WindReporter::windDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QString downloadUrl)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << "bytesReceived" << bytesReceived << "bytesTotal" << bytesTotal
             << "downloadUrl" << downloadUrl;

  QApplication::processEvents(QEventLoop::WaitForMoreEvents);
}

void WindReporter::windDownloadSslErrors(const QStringList& errors, const QString& downloadUrl)
{
  qWarning() << Q_FUNC_INFO;
  NavApp::closeSplashScreen();

  int result = atools::gui::Dialog(NavApp::getQMainWindow()).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_WIND,
                                  tr("<p>Errors while trying to establish an encrypted "
                                       "connection to download winds aloft:</p>"
                                       "<p>URL: %1</p>"
                                         "<p>Error messages:<br/>%2</p>"
                                           "<p>Continue?</p>").
                                  arg(downloadUrl).
                                  arg(atools::strJoin(errors, tr("<br/>"))),
                                  tr("Do not &show this again and ignore errors."),
                                  QMessageBox::Cancel | QMessageBox::Yes,
                                  QMessageBox::Cancel, QMessageBox::Yes);

  windQueryOnline->setIgnoreSslErrors(result == QMessageBox::Yes);
}

void WindReporter::windDownloadFailed(const QString& error, int errorCode)
{
  qDebug() << Q_FUNC_INFO << error << errorCode;

  if(!downloadErrorReported)
  {
    // Get rid of splash in case this happens on startup
    NavApp::closeSplashScreen();
    QMessageBox::warning(NavApp::getQMainWidget(), QApplication::applicationName(),
                         tr("Error downloading or reading wind data: %1 (%2)").arg(error).arg(errorCode));
    downloadErrorReported = true;
  }
  updateToolButtonState();
  updateSliderLabel();
}

void WindReporter::addToolbarButton()
{
  // Create and add toolbar button =====================================
  Ui::MainWindow *ui = NavApp::getMainUi();
  windlevelToolButton = new QToolButton(ui->toolbarMapOptions);
  windlevelToolButton->setIcon(QIcon(":/littlenavmap/resources/icons/wind.svg"));
  windlevelToolButton->setPopupMode(QToolButton::InstantPopup);
  windlevelToolButton->setToolTip(tr("Wind forecast altitude levels to display"));
  windlevelToolButton->setStatusTip(windlevelToolButton->toolTip());
  windlevelToolButton->setCheckable(true);

  // Add tear off menu to button =======
  windlevelToolButton->setMenu(new QMenu(windlevelToolButton));
  QMenu *buttonMenu = windlevelToolButton->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  // Insert before show route
  ui->toolbarMapOptions->insertWidget(ui->actionMapShowSunShading, windlevelToolButton);
  ui->menuHighAltitudeWindLevels->clear();

  // Create and add flight plan action =====================================
  actionFlightplanWaypoints = new QAction(tr("Wind at &Flight Plan Waypoints"), buttonMenu);
  actionFlightplanWaypoints->setToolTip(tr("Show wind at flight plan waypoints"));
  actionFlightplanWaypoints->setStatusTip(actionFlightplanWaypoints->toolTip());
  actionFlightplanWaypoints->setCheckable(true);
  buttonMenu->addAction(actionFlightplanWaypoints);
  ui->menuHighAltitudeWindLevels->addAction(actionFlightplanWaypoints);
  connect(actionFlightplanWaypoints, &QAction::triggered, this, &WindReporter::toolbarActionFlightplanTriggered);

  ui->menuHighAltitudeWindLevels->addSeparator();
  buttonMenu->addSeparator();

  actionGroup = new QActionGroup(buttonMenu);

  // Create and add level actions =====================================
  // Create and add none action =====================================
  actionNone = new QAction(tr("&No Wind Barbs"), buttonMenu);
  actionNone->setToolTip(tr("Do not show wind barbs"));
  actionNone->setStatusTip(actionNone->toolTip());
  actionNone->setData(wind::NONE);
  actionNone->setCheckable(true);
  actionNone->setActionGroup(actionGroup);
  buttonMenu->addAction(actionNone);
  ui->menuHighAltitudeWindLevels->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  // Create and add ground/AGL action =====================================
  actionAgl = new QAction(tr("Wind at &Ground (only NOAA)"), buttonMenu);
  actionAgl->setToolTip(tr("Show wind for 80 m / 260 ft above ground"));
  actionAgl->setStatusTip(actionAgl->toolTip());
  actionAgl->setData(wind::AGL);
  actionAgl->setCheckable(true);
  actionAgl->setActionGroup(actionGroup);
  buttonMenu->addAction(actionAgl);
  ui->menuHighAltitudeWindLevels->addAction(actionAgl);
  connect(actionAgl, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  // Create and add flight plan action =====================================
  actionFlightplan = new QAction(tr("Wind at Flight Plan &Cruise Altitude"), buttonMenu);
  actionFlightplan->setToolTip(tr("Show wind at flight plan cruise altitude"));
  actionFlightplan->setStatusTip(actionFlightplan->toolTip());
  actionFlightplan->setData(wind::FLIGHTPLAN);
  actionFlightplan->setCheckable(true);
  actionFlightplan->setActionGroup(actionGroup);
  buttonMenu->addAction(actionFlightplan);
  ui->menuHighAltitudeWindLevels->addAction(actionFlightplan);
  connect(actionFlightplan, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  // Create and add flight plan action =====================================
  actionSelected = new QAction(tr("Wind for &Selected Altitude"), buttonMenu);
  actionSelected->setToolTip(tr("Show wind at selected altitude"));
  actionSelected->setStatusTip(actionSelected->toolTip());
  actionSelected->setData(wind::SELECTED);
  actionSelected->setCheckable(true);
  actionSelected->setActionGroup(actionGroup);
  buttonMenu->addAction(actionSelected);
  ui->menuHighAltitudeWindLevels->addAction(actionSelected);
  connect(actionSelected, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  // Add label and sliders for min/max altitude selection ==============================================
  buttonMenu->addSeparator();

  // Create and add the wrapped actions ================
  labelActionWindAltitude = new windinternal::WindLabelAction(buttonMenu);
  buttonMenu->addAction(labelActionWindAltitude);

  sliderActionAltitude = new windinternal::WindSliderAction(buttonMenu);
  buttonMenu->addAction(sliderActionAltitude);

  connect(sliderActionAltitude, &windinternal::WindSliderAction::valueChanged, this, &WindReporter::altSliderChanged);
  connect(sliderActionAltitude, &windinternal::WindSliderAction::sliderReleased, this, &WindReporter::altSliderChanged);
}

void WindReporter::altSliderChanged()
{
  updateSliderLabel();
  emit windDisplayUpdated();
}

void WindReporter::updateSliderLabel()
{
  // Get note text if slider is disabled =============
  QString disabledText;
  if(currentWindSelection == wind::NONE)
    disabledText = tr(" (wind display disabled)");
  else if(currentWindSelection == wind::AGL)
    disabledText = tr(" (showing for ground level)");
  else if(currentWindSelection == wind::FLIGHTPLAN)
    disabledText = tr(" (showing for flight plan)");

  labelActionWindAltitude->setText(tr("At %1%2").arg(Unit::altFeet(sliderActionAltitude->getAltitudeFt())).arg(disabledText));
}

bool WindReporter::isWindShown() const
{
  return currentWindSelection != wind::NONE;
}

bool WindReporter::isRouteWindShown() const
{
  return showFlightplanWaypoints;
}

bool WindReporter::hasOnlineWindData() const
{
  return windQueryOnline->hasWindData();
}

bool WindReporter::isWindManual() const
{
  return NavApp::getAircraftPerfController()->isWindManual();
}

void WindReporter::sourceActionTriggered()
{
  if(!ignoreUpdates)
  {
    updateDataSource();
    updateToolButtonState();
    updateSliderLabel();

    emit windUpdated();
    emit windDisplayUpdated();
  }
}

void WindReporter::toolbarActionFlightplanTriggered()
{
  if(!ignoreUpdates)
  {
    actionToValues();
    updateToolButtonState();
    updateSliderLabel();

    emit windDisplayUpdated();
  }
}

void WindReporter::toolbarActionTriggered()
{
  if(!ignoreUpdates)
  {
    actionToValues();
    updateToolButtonState();
    updateSliderLabel();

    emit windDisplayUpdated();
  }
}

void WindReporter::updateToolButtonState()
{
  bool hasWind = hasAnyWindData();

  // Disable whole button if no wind available
  windlevelToolButton->setEnabled(hasWind);

  // Either selection will show button depressed independent if enabled or not
  windlevelToolButton->setChecked(!actionNone->isChecked() || actionFlightplanWaypoints->isChecked());

  // Actions that need real wind or manual wind
  actionNone->setEnabled(hasWind);
  actionFlightplan->setEnabled(hasWind);
  actionAgl->setEnabled(hasWind);
  actionSelected->setEnabled(hasWind);

  // Label and slider need action checked
  bool enabled = currentWindSelection == wind::SELECTED;
  labelActionWindAltitude->setEnabled(enabled && hasWind);
  sliderActionAltitude->setEnabled(enabled && hasWind);

  actionFlightplanWaypoints->setEnabled(!NavApp::getRoute().isFlightplanEmpty() && hasWind);
}

void WindReporter::valuesToAction()
{
  ignoreUpdates = true;
  switch(currentWindSelection)
  {
    case wind::NONE:
      actionNone->setChecked(true);
      break;

    case wind::AGL:
      actionAgl->setChecked(true);
      break;

    case wind::FLIGHTPLAN:
      actionFlightplan->setChecked(true);
      break;

    case wind::SELECTED:
      actionSelected->setChecked(true);
      break;
  }

  actionFlightplanWaypoints->setChecked(showFlightplanWaypoints);

  switch(currentSource)
  {
    case wind::NO_SOURCE:
      NavApp::getMainUi()->actionMapShowWindDisabled->setChecked(true);
      break;
    case wind::NOAA:
      NavApp::getMainUi()->actionMapShowWindNOAA->setChecked(true);
      break;
    case wind::SIMULATOR:
      NavApp::getMainUi()->actionMapShowWindSimulator->setChecked(true);
      break;
  }
  ignoreUpdates = false;
}

QString WindReporter::getLevelText() const
{
  switch(currentWindSelection)
  {
    case wind::NONE:
      return tr("None");

    case wind::AGL:
      return tr("Ground");

    case wind::FLIGHTPLAN:
      return tr("Flight plan cruise altitude");

    case wind::SELECTED:
      return Unit::altFeet(sliderActionAltitude->getAltitudeFt());
  }
  return QString();
}

QString WindReporter::getSourceText() const
{
  if(isWindManual())
    return tr("Manual");

  switch(currentSource)
  {
    case wind::NO_SOURCE:
      return tr("Disabled");

    case wind::NOAA:
      return tr("NOAA");

    case wind::SIMULATOR:
      return tr("Simulator");
  }
  return QString();
}

wind::WindSource WindReporter::getSource() const
{
  return currentSource;
}

void WindReporter::resetSettingsToDefault()
{
  currentWindSelection = wind::NONE;
  showFlightplanWaypoints = false;

  sliderActionAltitude->setAltitudeFt(10000);

  valuesToAction();
  updateToolButtonState();
  updateSliderLabel();
  emit windUpdated();
  emit windDisplayUpdated();
}

void WindReporter::debugDumpContainerSizes() const
{
  if(windQueryManual != nullptr)
    windQueryManual->debugDumpContainerSizes();
  if(windQueryOnline != nullptr)
    windQueryOnline->debugDumpContainerSizes();
  qDebug() << Q_FUNC_INFO << "windPosCache.list.size()" << windPosCache.list.size();

}

void WindReporter::actionToValues()
{
  // Get setting from action data
  QAction *action = actionGroup->checkedAction();
  if(action != nullptr)
    currentWindSelection = static_cast<wind::WindSelection>(action->data().toInt());
  else
    currentWindSelection = wind::NONE;

  /// Fetch source
  if(NavApp::getMainUi()->actionMapShowWindDisabled->isChecked())
    currentSource = wind::NO_SOURCE;
  else if(NavApp::getMainUi()->actionMapShowWindNOAA->isChecked())
    currentSource = wind::NOAA;
  else if(NavApp::getMainUi()->actionMapShowWindSimulator->isChecked())
    currentSource = wind::SIMULATOR;

  showFlightplanWaypoints = actionFlightplanWaypoints->isChecked();
}

float WindReporter::getAltitudeFt() const
{
  switch(currentWindSelection)
  {
    case wind::NONE:
      return map::INVALID_ALTITUDE_VALUE;

    case wind::AGL:
      return 260.f;

    case wind::FLIGHTPLAN:
      return NavApp::getRouteCruiseAltFt();

    case wind::SELECTED:
      return static_cast<float>(sliderActionAltitude->getAltitudeFt());
  }
  return map::INVALID_ALTITUDE_VALUE;
}

const atools::grib::WindPosList *WindReporter::getWindForRect(const Marble::GeoDataLatLonBox& rect,
                                                              const MapLayer *mapLayer, bool lazy, bool& overflow)
{
  atools::grib::WindQuery *windQuery = currentWindQuery();
  if(windQuery->hasWindData() && getAltitudeFt() < map::INVALID_ALTITUDE_VALUE)
  {
    // Update
    windPosCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                             [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
    {
      return curLayer->hasSameQueryParametersWind(newLayer);
    });

    if((windPosCache.list.isEmpty() && !lazy) || cachedLevel != sliderActionAltitude->getAltitudeFt()) // Force update if level has changed
    {
      windPosCache.clear();
      for(const Marble::GeoDataLatLonBox& box : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
      {
        atools::geo::Rect geoRect(box.west(Marble::GeoDataCoordinates::Degree), box.north(Marble::GeoDataCoordinates::Degree),
                                  box.east(Marble::GeoDataCoordinates::Degree), box.south(Marble::GeoDataCoordinates::Degree));

        atools::grib::WindPosVector windPosVector;
        windQuery->getWindForRect(windPosVector, geoRect, getAltitudeFt());
        windPosCache.list.append(windPosVector.toList());
        cachedLevel = sliderActionAltitude->getAltitudeFt();
      }
    }
    overflow = windPosCache.validate(queryMaxRows);
    return &windPosCache.list;
  }
  return nullptr;
}

atools::grib::WindPos WindReporter::getWindForPos(const atools::geo::Pos& pos, float altFeet)
{
  atools::grib::WindQuery *windQuery = currentWindQuery();
  atools::grib::WindPos wp;
  if(windQuery->hasWindData())
  {
    wp.pos = pos;
    wp.wind = windQuery->getWindForPos(pos.alt(altFeet));
  }
  return wp;
}

atools::grib::WindPos WindReporter::getWindForPos(const atools::geo::Pos& pos)
{
  return getWindForPos(pos, pos.getAltitude());
}

atools::grib::Wind WindReporter::getWindForPosRoute(const atools::geo::Pos& pos)
{
  return (isWindManual() ? windQueryManual : windQueryOnline)->getWindForPos(pos);
}

atools::grib::Wind WindReporter::getWindForLineRoute(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2)
{
  return (isWindManual() ? windQueryManual : windQueryOnline)->getWindAverageForLine(pos1, pos2);
}

atools::grib::Wind WindReporter::getWindForLineRoute(const atools::geo::Line& line)
{
  return getWindForLineRoute(line.getPos1(), line.getPos2());
}

atools::grib::Wind WindReporter::getWindForLineStringRoute(const atools::geo::LineString& line)
{
  return (isWindManual() ? windQueryManual : windQueryOnline)->getWindAverageForLineString(line);
}

atools::grib::WindPosVector WindReporter::getWindStackForPos(const atools::geo::Pos& pos, QVector<int> altitudesFt)
{
  atools::grib::WindPosVector winds;
  atools::grib::WindQuery *windQuery = currentWindQuery();

  if(windQuery->hasWindData() && getAltitudeFt() < map::INVALID_ALTITUDE_VALUE)
  {
    float curAlt = getAltitudeFt();
    atools::grib::WindPos wp;

    // Collect wind for all levels
    for(int i = 0; i < altitudesFt.size(); i++)
    {
      // Treat 0 level as AGL
      float alt = altitudesFt.at(i) == 0 ? 260.f : altitudesFt.at(i);
      float altNext = i < altitudesFt.size() - 1 ? altitudesFt.at(i + 1) : 100000.f;

      // Get wind for layer/altitude
      wp.pos = pos.alt(alt);
      if(currentSource != wind::NOAA && altitudesFt.at(i) == 0)
        wp.wind = {map::INVALID_COURSE_VALUE, map::INVALID_SPEED_VALUE}
      ;
      else
        wp.wind = windQuery->getWindForPos(wp.pos);
      winds.append(wp);

      if((currentWindSelection == wind::FLIGHTPLAN || currentWindSelection == wind::SELECTED) && curAlt > alt && curAlt < altNext)
      {
        // Insert flight plan altitude if selected in GUI
        wp.pos = pos.alt(curAlt);
        wp.wind = windQuery->getWindForPos(wp.pos);
        winds.append(wp);
      }
    }
  }
  return winds;
}

atools::grib::WindPosVector WindReporter::getWindStackForPos(const atools::geo::Pos& pos)
{
  return getWindStackForPos(pos, levelsTooltip);
}

void WindReporter::updateManualRouteWinds()
{
  windQueryManual->initFromFixedModel(NavApp::getAircraftPerfController()->getWindDir(),
                                      NavApp::getAircraftPerfController()->getWindSpeed(),
                                      NavApp::getRoute().getCruisingAltitudeFeet());
}

#ifdef DEBUG_INFORMATION
QString WindReporter::getDebug(const atools::geo::Pos& pos)
{
  atools::grib::WindQuery *windQuery = isWindManual() ? windQueryManual : windQueryOnline;
  return windQuery->getDebug(pos);
}

#endif
