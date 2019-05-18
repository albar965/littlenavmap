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

#include "weather/windreporter.h"

#include "navapp.h"
#include "ui_mainwindow.h"
#include "grib/windquery.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "options/optiondata.h"
#include "common/unit.h"
#include "geo/rect.h"
#include "mapgui/maplayer.h"

#include <QToolButton>
#include <QDebug>
#include <QMessageBox>
#include <QDir>

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
static int queryMaxRows = 5000;

WindReporter::WindReporter(QObject *parent, atools::fs::FsPaths::SimulatorType type)
  : QObject(parent), simType(type)
{
  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_WEATHER_DEBUG, false).toBool();

  query = new atools::grib::WindQuery(parent, verbose);
  connect(query, &atools::grib::WindQuery::windDataUpdated, this, &WindReporter::windDownloadFinished);
  connect(query, &atools::grib::WindQuery::windDownloadFailed, this, &WindReporter::windDownloadFailed);

  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionMapShowWindNOAA, &QAction::triggered, this, &WindReporter::sourceActionTriggered);
  connect(ui->actionMapShowWindSimulator, &QAction::triggered, this, &WindReporter::sourceActionTriggered);
}

WindReporter::~WindReporter()
{
  delete query;
  delete actionGroup;
  delete windlevelToolButton;
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
  atools::settings::Settings::instance().setValue(lnm::MAP_WIND_LEVEL, currentLevel);
  atools::settings::Settings::instance().setValue(lnm::MAP_WIND_SOURCE, currentSource);
}

void WindReporter::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_MAP_SETTINGS)
  {
    atools::settings::Settings& settings = atools::settings::Settings::instance();
    currentLevel = settings.valueInt(lnm::MAP_WIND_LEVEL, wr::NONE);
    currentSource = static_cast<wr::WindSource>(settings.valueInt(lnm::MAP_WIND_SOURCE, wr::NOAA));
  }
  valuesToAction();
  updateDataSource();
}

void WindReporter::updateDataSource()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  actionToValues();

  if(ui->actionMapShowWindSimulator->isChecked() && simType == atools::fs::FsPaths::XPLANE11)
  {
    // Load GRIB file only if X-Plane is enabled
    QString path = OptionData::instance().getWeatherXplaneWind();
    if(path.isEmpty())
      path = NavApp::getSimulatorBasePath(NavApp::getCurrentSimulatorDb()) + QDir::separator() + "global_winds.grib";

    if(QFileInfo::exists(path))
      query->initFromFile(path);
  }
  else if(ui->actionMapShowWindNOAA->isChecked())
    // Download from NOAA
    query->initFromUrl(OptionData::instance().getWeatherNoaaWindBaseUrl());
  else
    query->deinit();
}

void WindReporter::windDownloadFinished()
{
  qDebug() << Q_FUNC_INFO;
  emit windUpdated();
}

void WindReporter::windDownloadFailed(const QString& error, int errorCode)
{
  qDebug() << Q_FUNC_INFO << error << errorCode;

  // Get rid of splash in case this happens on startup
  NavApp::deleteSplashScreen();
  QMessageBox::warning(NavApp::getQMainWidget(), QApplication::applicationName(),
                       tr("Error downloading wind data: %1 (%2").arg(error).arg(errorCode));
}

void WindReporter::addToolbarButton()
{
  // Create and add toolbar button =====================================
  Ui::MainWindow *ui = NavApp::getMainUi();
  QToolButton *button = new QToolButton(ui->toolbarMapOptions);
  windlevelToolButton = button;
  button->setIcon(QIcon(":/littlenavmap/resources/icons/wind.svg"));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolTip(tr("Wind altitude levels to display"));
  button->setStatusTip(button->toolTip());
  button->setCheckable(true);

  actionGroup = new QActionGroup(button);

  // Insert before show route
  ui->toolbarMapOptions->insertWidget(ui->actionMapShowSunShading, button);
  ui->menuHighAltitudeWindLevels->clear();

  // Create and add none action =====================================
  actionNone = new QAction(tr("None"), button);
  actionNone->setToolTip(tr("Do not show wind barbs"));
  actionNone->setStatusTip(actionNone->toolTip());
  actionNone->setData(wr::NONE);
  actionNone->setCheckable(true);
  actionNone->setActionGroup(actionGroup);
  button->addAction(actionNone);
  ui->menuHighAltitudeWindLevels->addAction(actionNone);
  connect(actionNone, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  // Create and add ground/AGL action =====================================
  actionAgl = new QAction(tr("Ground"), button);
  actionAgl->setToolTip(tr("Show wind for 80 m / 250 ft above ground"));
  actionAgl->setStatusTip(actionAgl->toolTip());
  actionAgl->setData(wr::AGL);
  actionAgl->setCheckable(true);
  actionAgl->setActionGroup(actionGroup);
  button->addAction(actionAgl);
  ui->menuHighAltitudeWindLevels->addAction(actionAgl);
  connect(actionAgl, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  // Create and add flight plan action =====================================
  actionFlightplan = new QAction(tr("At Flight Plan Cruise Altitude"), button);
  actionFlightplan->setToolTip(tr("Show wind at flight plan cruise altitude"));
  actionFlightplan->setStatusTip(actionFlightplan->toolTip());
  actionFlightplan->setData(wr::FLIGHTPLAN);
  actionFlightplan->setCheckable(true);
  actionFlightplan->setActionGroup(actionGroup);
  button->addAction(actionFlightplan);
  ui->menuHighAltitudeWindLevels->addAction(actionFlightplan);
  connect(actionFlightplan, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);

  ui->menuHighAltitudeWindLevels->addSeparator();

  // Create and add level actions =====================================
  for(int level : levels)
  {
    QAction *action = new QAction(tr("At %1").arg(Unit::altFeet(level)), button);
    action->setToolTip(tr("Show wind at %1 altitude").arg(Unit::altFeet(level)));
    action->setData(level);
    action->setCheckable(true);
    action->setStatusTip(action->toolTip());
    action->setActionGroup(actionGroup);
    button->addAction(action);
    actionLevelVector.append(action);
    ui->menuHighAltitudeWindLevels->addAction(action);
    connect(action, &QAction::triggered, this, &WindReporter::toolbarActionTriggered);
  }
}

bool WindReporter::isWindShown() const
{
  return currentLevel != wr::NONE;
}

bool WindReporter::isWindEnabled() const
{
  return query->hasWindData();
}

void WindReporter::sourceActionTriggered()
{
  if(!ignoreUpdates)
    updateDataSource();
}

void WindReporter::toolbarActionTriggered()
{
  if(!ignoreUpdates)
  {
    actionToValues();
    windlevelToolButton->setChecked(!actionNone->isChecked());
    emit windUpdated();
  }
}

void WindReporter::valuesToAction()
{
  ignoreUpdates = true;
  switch(currentLevel)
  {
    case wr::NONE:
      actionNone->setChecked(true);
      break;

    case wr::AGL:
      actionAgl->setChecked(true);
      break;

    case wr::FLIGHTPLAN:
      actionFlightplan->setChecked(true);
      break;

    default:
      for(QAction *action : actionLevelVector)
      {
        if(action->data().toInt() == currentLevel)
          action->setChecked(true);
      }
  }

  windlevelToolButton->setChecked(!actionNone->isChecked());

  qDebug() << Q_FUNC_INFO << "source" << currentSource;
  switch(currentSource)
  {
    case wr::NOAA:
      NavApp::getMainUi()->actionMapShowWindNOAA->setChecked(true);
      break;
    case wr::SIMULATOR:
      NavApp::getMainUi()->actionMapShowWindSimulator->setChecked(true);
      break;
  }
  ignoreUpdates = false;
}

void WindReporter::actionToValues()
{
  QAction *action = actionGroup->checkedAction();
  if(action != nullptr)
    currentLevel = action->data().toInt();
  else
    currentLevel = wr::NONE;

  if(NavApp::getMainUi()->actionMapShowWindNOAA->isChecked())
    currentSource = wr::NOAA;
  else if(NavApp::getMainUi()->actionMapShowWindSimulator->isChecked())
    currentSource = wr::SIMULATOR;

  qDebug() << Q_FUNC_INFO << currentLevel;
}

float WindReporter::getAltitude() const
{
  switch(currentLevel)
  {
    case wr::NONE:
      return 0.f;

    case wr::AGL:
      return 260.f;

    case wr::FLIGHTPLAN:
      return NavApp::getRouteCruiseAltFt();

    default:
      if(actionGroup->checkedAction())
        return actionGroup->checkedAction()->data().toInt();
      else
        return 0.f;
  }
}

const atools::grib::WindPosList *WindReporter::getWindForRect(const Marble::GeoDataLatLonBox& rect,
                                                              const MapLayer *mapLayer, bool lazy)
{
  if(query->hasWindData())
  {
    // Update
    windPosCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                             [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
    {
      return curLayer->hasSameQueryParametersWind(newLayer);
    });

    if((windPosCache.list.isEmpty() && !lazy) || cachedLevel != currentLevel) // Force update if level has changed
    {
      windPosCache.clear();
      for(const Marble::GeoDataLatLonBox& box :
          query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
      {
        atools::geo::Rect r(box.west(Marble::GeoDataCoordinates::Degree),
                            box.north(Marble::GeoDataCoordinates::Degree),
                            box.east(Marble::GeoDataCoordinates::Degree),
                            box.south(Marble::GeoDataCoordinates::Degree));

        atools::grib::WindPosVector windPosVector;
        query->getWindForRect(windPosVector, r, getAltitude());
        windPosCache.list.append(windPosVector.toList());
        cachedLevel = currentLevel;
      }
    }
    windPosCache.validate(queryMaxRows);
    return &windPosCache.list;
  }
  return nullptr;
}

void WindReporter::getWindForRoute(atools::grib::WindPosVector& winds, atools::grib::WindPos& average,
                                   const Route& route)
{
  // TODO
}

atools::grib::WindPos WindReporter::getWindForPos(const atools::geo::Pos& pos, float altFeet)
{
  atools::grib::WindPos wp;
  wp.init();
  if(query->hasWindData())
  {
    wp.pos = pos;
    wp.wind = query->getWindForPos(pos.alt(altFeet));
  }
  return wp;
}

atools::grib::WindPosVector WindReporter::getWindStackForPos(const atools::geo::Pos& pos,
                                                             QVector<int> altitudesFt)
{
  atools::grib::WindPosVector winds;

  if(query->hasWindData())
  {
    float curAlt = getAltitude();
    atools::grib::WindPos wp;

    // Collect wind for all levels
    for(int i = 0; i < altitudesFt.size(); i++)
    {
      float alt = altitudesFt.at(i);
      float altNext = i < altitudesFt.size() - 1 ? altitudesFt.at(i + 1) : 100000.f;

      // Get wind for layer/altitude
      wp.pos = pos.alt(alt);
      wp.wind = query->getWindForPos(wp.pos);
      winds.append(wp);

      if(currentLevel == wr::FLIGHTPLAN && curAlt > alt && curAlt < altNext)
      {
        // Insert flight plan altitude if selected in GUI
        wp.pos = pos.alt(curAlt);
        wp.wind = query->getWindForPos(wp.pos);
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
