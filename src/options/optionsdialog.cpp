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

#include "options/optionsdialog.h"

#include "navapp.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/unit.h"
#include "atools.h"
#include "gui/tools.h"
#include "ui_options.h"
#include "weather/weatherreporter.h"
#include "web/webcontroller.h"
#include "gui/widgetstate.h"
#include "gui/dialog.h"
#include "gui/widgetutil.h"
#include "settings/settings.h"
#include "mapgui/mapwidget.h"
#include "gui/helphandler.h"
#include "grib/gribreader.h"
#include "util/updatecheck.h"
#include "util/htmlbuilder.h"
#include "common/unitstringtool.h"
#include "gui/translator.h"
#include "fs/pln/flightplan.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDesktopServices>
#include <QColorDialog>
#include <QGuiApplication>
#include <QMainWindow>
#include <QUrl>
#include <QSettings>
#include <QFontDialog>
#include <QFontDatabase>

#include <marble/MarbleModel.h>
#include <marble/MarbleDirs.h>

const int MAX_RANGE_RING_SIZE = 4000;
const int MAX_RANGE_RINGS = 10;

using atools::settings::Settings;
using atools::gui::HelpHandler;
using atools::util::HtmlBuilder;

/* Validates the space separated list of range ring sizes */
class RangeRingValidator :
  public QValidator
{
public:
  RangeRingValidator();

private:
  virtual QValidator::State validate(QString& input, int& pos) const override;

  bool ringStrToVector(const QString& str) const;

  QRegularExpressionValidator regexpVal;
};

RangeRingValidator::RangeRingValidator()
{
  // Multiple numbers starting 1-9 max 4 digits separated by space
  regexpVal.setRegularExpression(QRegularExpression("^([1-9]\\d{0,3} )*[1-9]\\d{1,3}$"));
}

QValidator::State RangeRingValidator::validate(QString& input, int& pos) const
{
  State state = regexpVal.validate(input, pos);
  if(state == Invalid)
    return Invalid;
  else
  {
    // Half valid check number of rings and distance
    if(!ringStrToVector(input))
      return Invalid;
  }

  return state;
}

/* Check for maximum of 10 rings with a maximum size of 4000 nautical miles.
 * @return true if string is ok
 */
bool RangeRingValidator::ringStrToVector(const QString& input) const
{
  int numRing = 0;
  for(const QString& str : input.split(" "))
  {
    QString val = str.trimmed();
    if(!val.isEmpty())
    {
      bool ok;
      int num = val.toInt(&ok);
      if(!ok || num > MAX_RANGE_RING_SIZE)
        return false;

      numRing++;
    }
  }
  return numRing <= MAX_RANGE_RINGS;
}

// ------------------------------------------------------------------------

OptionsDialog::OptionsDialog(QMainWindow *parentWindow)
  : QDialog(parentWindow), ui(new Ui::Options), mainWindow(parentWindow)
{
  qDebug() << Q_FUNC_INFO;
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Styles cascade to children and mess up UI themes on linux - even if widget is selected by name
#ifndef Q_OS_LINUX
  ui->splitterOptions->setStyleSheet(
    QString("QSplitter::handle { "
            "background: %1;"
            "image: url(:/littlenavmap/resources/icons/splitterhandhoriz.png);"
            " }").
    arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
#endif

  if(ui->splitterOptions->handle(1) != nullptr)
  {
    ui->splitterOptions->handle(1)->setToolTip(tr("Resize options list."));
    ui->splitterOptions->handle(1)->setStatusTip(ui->splitterOptions->handle(1)->toolTip());
  }

  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsStartup);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsUserInterface);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsMapGeneral);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsMapNav);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsMapDisplay);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsMapDisplay2);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsMapDisplayOnline);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsUnits);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsSimAircraft);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsFlightPlan);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsWeather);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsWeatherUrls);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsOnline);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsWebServer);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsCacheFiles);
  // ui->stackedWidgetOptions->removeWidget(ui->stackedWidgetOptionsScenery);

  /* *INDENT-OFF* */
  QListWidget*list = ui->listWidgetOptionPages;
  list->addItem(pageListItem(list, tr("Startup and Updates"), tr("Select what should be reloaded on startup and change update settings."), ":/littlenavmap/resources/icons/littlenavmap.svg"));
  list->addItem(pageListItem(list, tr("User Interface"), tr("Change language settings, window options and other user interface behavior."), ":/littlenavmap/resources/icons/statusbar.svg"));
  list->addItem(pageListItem(list, tr("Display and Text"), tr("Change text sizes, user interface font and other display options."), ":/littlenavmap/resources/icons/copy.svg"));
  list->addItem(pageListItem(list, tr("Map"), tr("General map settings: Zoom, click and tooltip settings."), ":/littlenavmap/resources/icons/mapsettings.svg"));
  list->addItem(pageListItem(list, tr("Map Navigation"), tr("Zoom, click and screen navigation settings."), ":/littlenavmap/resources/icons/mapnavigation.svg"));
  list->addItem(pageListItem(list, tr("Map Display"), tr("Change colors, symbols, texts and font for map display objects."), ":/littlenavmap/resources/icons/mapdisplay.svg"));
  list->addItem(pageListItem(list, tr("Map Display 2"), tr("Change colors, symbols and texts for marks, user aircraft and more."), ":/littlenavmap/resources/icons/mapdisplay2.svg"));
  list->addItem(pageListItem(list, tr("Map Display Online"), tr("Map display online center options."), ":/littlenavmap/resources/icons/airspaceonline.svg"));
  list->addItem(pageListItem(list, tr("Units"), tr("Fuel, distance, speed and coordindate units as well as\noptions for course and heading display."), ":/littlenavmap/resources/icons/units.svg"));
  list->addItem(pageListItem(list, tr("Simulator Aircraft"), tr("Update and movement options for the user aircraft and trail."), ":/littlenavmap/resources/icons/aircraft.svg"));
  list->addItem(pageListItem(list, tr("Flight Plan"), tr("Options for flight plan calculation, saving and loading."), ":/littlenavmap/resources/icons/route.svg"));
  list->addItem(pageListItem(list, tr("Weather"), tr("Change weather sources for information and tooltips."), ":/littlenavmap/resources/icons/weather.svg"));
  list->addItem(pageListItem(list, tr("Weather URLs"), tr("Change web download addresses of weather sources."), ":/littlenavmap/resources/icons/weatherurl.svg"));
  list->addItem(pageListItem(list, tr("Online Flying"), tr("Select online flying services like VATSIM, IVAO or custom."), ":/littlenavmap/resources/icons/aircraft_online.svg"));
  list->addItem(pageListItem(list, tr("Web Server"), tr("Change settings for the internal web server."), ":/littlenavmap/resources/icons/web.svg"));
  list->addItem(pageListItem(list, tr("Cache and Files"), tr("Change map cache, select elevation data source and the path for user airspaces."), ":/littlenavmap/resources/icons/filesave.svg"));
  list->addItem(pageListItem(list, tr("Scenery Library Database"), tr("Exclude scenery files from loading and\nadd-on recognition."), ":/littlenavmap/resources/icons/database.svg"));
  /* *INDENT-ON* */

  // Build tree settings to map tab =====================================================
  /* *INDENT-OFF* */
  QTreeWidgetItem *root = ui->treeWidgetOptionsDisplayTextOptions->invisibleRootItem();

  QTreeWidgetItem *topOfMap = addTopItem(root, tr("Top of Map"), tr("Select information that is displayed on top of the map."));
  addItem<optsd::DisplayOptions>(topOfMap, displayOptItemIndex, tr("Wind Direction and Speed"), tr("Show wind direction and speed on the top center of the map."), optsd::ITEM_USER_AIRCRAFT_WIND, true);
  addItem<optsd::DisplayOptions>(topOfMap, displayOptItemIndex, tr("Wind Pointer"), tr("Show wind direction pointer on the top center of the map."), optsd::ITEM_USER_AIRCRAFT_WIND_POINTER, true);

  QTreeWidgetItem *navAids = addTopItem(root, tr("Map Navigation Aids"), QString());
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Center Cross"), tr("Shows the map center. Useful if \"Click map to center position\" on page \"Map Navigation\" is enabled."), optsd::NAVAIDS_CENTER_CROSS, false);
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Screen Area"), tr("Highlight click- or touchable areas on screen.\nOnly shown if \"Use map areas\" on page \"Map Navigation\" is enabled as well."), optsd::NAVAIDS_TOUCHSCREEN_REGIONS, false);
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Screen Areas Marks"), tr("Shows corner marks to highlight the screen areas. Helps if map areas are used for touchscreen navigation.\nOnly shown if \"Use map areas\" on page \"Map Navigation\" is enabled as well."), optsd::NAVAIDS_TOUCHSCREEN_AREAS, false);
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Screen Area Icons"), tr("Shows icons for the screen areas. Useful if map areas are used for touchscreen navigation.\nOnly shown if \"Use map areas\" on page \"Map Navigation\" is enabled as well."), optsd::NAVAIDS_TOUCHSCREEN_ICONS, false);

  QTreeWidgetItem *airport = addTopItem(root, tr("Airport"), tr("Select airport labels to display on the map."));
  addItem<optsd::DisplayOptions>(airport, displayOptItemIndex, tr("Name (Ident)"), QString(), optsd::ITEM_AIRPORT_NAME, true);
  addItem<optsd::DisplayOptions>(airport, displayOptItemIndex, tr("Tower Frequency"), QString(), optsd::ITEM_AIRPORT_TOWER, true);
  addItem<optsd::DisplayOptions>(airport, displayOptItemIndex, tr("ATIS / ASOS / AWOS Frequency"), QString(), optsd::ITEM_AIRPORT_ATIS, true);
  addItem<optsd::DisplayOptions>(airport, displayOptItemIndex, tr("Runway Information"), tr("Show runway length, width and light inidcator text."), optsd::ITEM_AIRPORT_RUNWAY, true);
  // addItem(ap, tr("Wind Pointer"), optsd::ITEM_AIRPORT_WIND_POINTER, false);

  QTreeWidgetItem *route = addTopItem(root, tr("Flight Plan"), tr("Select display options for the flight plan line."));
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Distance"), tr("Show distance along flight plan leg."), optsd::ROUTE_DISTANCE, true);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Magnetic great circle course"), tr("Show magnetic great circle start course at flight plan leg."), optsd::ROUTE_MAG_COURSE_GC, false);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("True great circle course"), tr("Show true great circle start course at flight plan leg."), optsd::ROUTE_TRUE_COURSE_GC, false);

  QTreeWidgetItem *userAircraft = addTopItem(root, tr("User Aircraft"), tr("Select text labels and other options for the user aircraft."));
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Registration"), QString(), optsd::ITEM_USER_AIRCRAFT_REGISTRATION);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Type"), tr("Show the aircraft type, like B738, B350 or M20T."), optsd::ITEM_USER_AIRCRAFT_TYPE);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Airline"), QString(), optsd::ITEM_USER_AIRCRAFT_AIRLINE);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Flight Number"), QString(), optsd::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Indicated Airspeed"), QString(), optsd::ITEM_USER_AIRCRAFT_IAS);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Ground Speed"), QString(), optsd::ITEM_USER_AIRCRAFT_GS, true);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("True Airspeed"), QString(), optsd::ITEM_USER_AIRCRAFT_TAS);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Climb- and Sinkrate"), QString(), optsd::ITEM_USER_AIRCRAFT_CLIMB_SINK);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Heading"), QString(), optsd::ITEM_USER_AIRCRAFT_HEADING);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Altitude"), QString(), optsd::ITEM_USER_AIRCRAFT_ALTITUDE, true);
  addItem<optsd::DisplayOptions>(userAircraft, displayOptItemIndex, tr("Track Line"), tr("Show the aircraft track as a black needle protruding from the aircraft nose."), optsd::ITEM_USER_AIRCRAFT_TRACK_LINE, true);

  QTreeWidgetItem *aiAircraft = addTopItem(root, tr("AI, Multiplayer and Online Client Aircraft"), tr("Select text labels for the AI, multiplayer and online client aircraft."));
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Registration, Number or Callsign"), QString(), optsd::ITEM_AI_AIRCRAFT_REGISTRATION, true);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Type"), QString(), optsd::ITEM_AI_AIRCRAFT_TYPE, true);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Airline"), QString(), optsd::ITEM_AI_AIRCRAFT_AIRLINE, true);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Flight Number"), QString(), optsd::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Indicated Airspeed"), QString(), optsd::ITEM_AI_AIRCRAFT_IAS);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Ground Speed"), QString(), optsd::ITEM_AI_AIRCRAFT_GS, true);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("True Airspeed"), QString(), optsd::ITEM_AI_AIRCRAFT_TAS);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Climb- and Sinkrate"), QString(), optsd::ITEM_AI_AIRCRAFT_CLIMB_SINK);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Heading"), QString(), optsd::ITEM_AI_AIRCRAFT_HEADING);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Altitude"), QString(), optsd::ITEM_AI_AIRCRAFT_ALTITUDE, true);
  addItem<optsd::DisplayOptions>(aiAircraft, displayOptItemIndex, tr("Departure and Destination"), QString(), optsd::ITEM_AI_AIRCRAFT_DEP_DEST, true);

  QTreeWidgetItem *compassRose = addTopItem(root, tr("Compass Rose"), tr("Select display options for the compass rose."));
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Direction Labels"), tr("Show N, S, E and W labels."), optsd::ROSE_DIR_LABLES, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Degree Tick Marks"), tr("Show tick marks for degrees on ring."), optsd::ROSE_DEGREE_MARKS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Degree Labels"), tr("Show degree labels on ring."), optsd::ROSE_DEGREE_LABELS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Range Rings"), tr("Show range rings and distance labels inside."), optsd::ROSE_RANGE_RINGS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Heading Line"), tr("Show the dashed heading line for user aircraft."), optsd::ROSE_HEADING_LINE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Track Line"), tr("Show the solid track line for user aircraft."), optsd::ROSE_TRACK_LINE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Track Label"), tr("Show track label for user aircraft."), optsd::ROSE_TRACK_LABEL, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Crab Angle Indicator"), tr("Show the crab angle for the user aircraft as a small magenta circle."), optsd::ROSE_CRAB_ANGLE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Course to Next Waypoint"), tr("Show the course to next waypoint for the user aircraft as a small magenta line."), optsd::ROSE_NEXT_WAYPOINT, true);

  QTreeWidgetItem *measurement = addTopItem(root, tr("Measurement Lines"), tr("Select display options measurement lines."));
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Distance"), tr("Great circle distance for measurement line."), optsd::MEASUREMNENT_DIST, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Magnetic Course"), tr("Show magnetic course for start and end of line."), optsd::MEASUREMNENT_MAG, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("True Course"), tr("Show true course for start and end of line."), optsd::MEASUREMNENT_TRUE, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Navaid or airport ident"), tr("Show ident if attached to navaid or airport.\n"
                                                                                                                           "Also show frequency if attached to a radio navaid. "), optsd::MEASUREMNENT_LABEL, true);
  /* *INDENT-ON* */

  rangeRingValidator = new RangeRingValidator;

  // Create widget list for state saver
  // This will take over the actual saving of the settings
  widgets.append(
    {ui->listWidgetOptionPages,
     ui->splitterOptions,
     ui->checkBoxOptionsGuiCenterKml,
     ui->checkBoxOptionsGuiProposeFilename,
     ui->checkBoxOptionsGuiRaiseWindows,
     ui->checkBoxOptionsGuiRaiseDockWindows,
     ui->checkBoxOptionsGuiRaiseMainWindow,
     ui->checkBoxOptionsGuiCenterRoute,
     ui->checkBoxOptionsGuiAvoidOverwrite,
     ui->checkBoxOptionsGuiOverrideLocale,
     ui->checkBoxOptionsGuiHighDpi,
     // ui->comboBoxOptionsGuiLanguage, saved directly

     ui->checkBoxDisplayOnlineNameLookup,
     ui->checkBoxDisplayOnlineFileLookup,
     ui->checkBoxOptionsMapEmptyAirports,
     ui->checkBoxOptionsMapEmptyAirports3D,
     ui->checkBoxOptionsMapTooltipAirport,
     ui->checkBoxOptionsMapTooltipNavaid,
     ui->checkBoxOptionsMapTooltipAirspace,
     ui->checkBoxOptionsMapTooltipWind,
     ui->checkBoxOptionsMapClickAirport,
     ui->checkBoxOptionsMapClickAirportProcs,
     ui->checkBoxOptionsMapClickNavaid,
     ui->checkBoxOptionsMapClickAirspace,
     ui->checkBoxOptionsMapUndock,
     ui->checkBoxOptionsRouteDeclination,
     ui->checkBoxOptionsRouteEastWestRule,
     ui->comboBoxOptionsRouteAltitudeRuleType,
     ui->checkBoxOptionsRouteExportUserWpt,
     ui->checkBoxOptionsStartupLoadKml,
     ui->checkBoxOptionsStartupLoadMapSettings,
     ui->checkBoxOptionsStartupLoadRoute,
     ui->checkBoxOptionsStartupLoadTrail,
     ui->checkBoxOptionsStartupLoadSearch,
     ui->checkBoxOptionsStartupLoadInfoContent,
     ui->checkBoxOptionsWeatherInfoAsn,
     ui->checkBoxOptionsWeatherInfoNoaa,
     ui->checkBoxOptionsWeatherInfoVatsim,
     ui->checkBoxOptionsWeatherInfoIvao,
     ui->checkBoxOptionsWeatherInfoFs,
     ui->checkBoxOptionsWeatherTooltipAsn,
     ui->checkBoxOptionsWeatherTooltipNoaa,
     ui->checkBoxOptionsWeatherTooltipVatsim,
     ui->checkBoxOptionsWeatherTooltipIvao,
     ui->checkBoxOptionsWeatherTooltipFs,
     ui->lineEditOptionsMapRangeRings,
     ui->lineEditOptionsWeatherAsnPath,
     ui->lineEditOptionsWeatherXplanePath,
     ui->lineEditOptionsWeatherNoaaStationsUrl,
     ui->lineEditOptionsWeatherVatsimUrl,
     ui->lineEditOptionsWeatherIvaoUrl,

     ui->lineEditOptionsWeatherXplaneWind,
     ui->lineEditOptionsWeatherNoaaWindUrl,

     ui->listWidgetOptionsDatabaseAddon,
     ui->listWidgetOptionsDatabaseExclude,
     ui->comboBoxMapScrollDetails,
     ui->radioButtonOptionsSimUpdateFast,
     ui->radioButtonOptionsSimUpdateLow,
     ui->radioButtonOptionsSimUpdateMedium,

     ui->radioButtonOptionsMapNavDragMove,
     ui->radioButtonOptionsMapNavClickCenter,
     ui->radioButtonOptionsMapNavTouchscreen,

     ui->checkBoxOptionsSimUpdatesConstant,
     ui->spinBoxOptionsSimUpdateBox,
     ui->spinBoxSimMaxTrackPoints,
     ui->radioButtonOptionsStartupShowHome,
     ui->radioButtonOptionsStartupShowLast,
     ui->radioButtonOptionsStartupShowFlightplan,

     ui->spinBoxOptionsCacheDiskSize,
     ui->spinBoxOptionsCacheMemorySize,
     ui->radioButtonCacheUseOffineElevation,
     ui->radioButtonCacheUseOnlineElevation,
     ui->lineEditCacheOfflineDataPath,
     ui->lineEditOptionsRouteFilename,
     ui->lineEditCacheUserAirspacePath,
     ui->lineEditCacheUserAirspaceExtensions,

     ui->spinBoxOptionsGuiInfoText,
     ui->spinBoxOptionsGuiAircraftPerf,
     ui->spinBoxOptionsGuiRouteText,
     ui->spinBoxOptionsGuiSearchText,
     ui->spinBoxOptionsGuiSimInfoText,
     ui->spinBoxOptionsGuiThemeMapDimming,
     ui->spinBoxOptionsMapClickRect,
     ui->spinBoxOptionsMapTooltipRect,
     ui->doubleSpinBoxOptionsMapZoomShowMap,
     ui->doubleSpinBoxOptionsMapZoomShowMapMenu,
     ui->spinBoxOptionsRouteGroundBuffer,

     ui->spinBoxOptionsDisplayTextSizeAircraftAi,
     ui->spinBoxOptionsDisplaySymbolSizeNavaid,
     ui->spinBoxOptionsDisplayTextSizeNavaid,
     ui->spinBoxOptionsDisplayThicknessFlightplan,
     ui->spinBoxOptionsDisplaySymbolSizeAirport,
     ui->spinBoxOptionsDisplaySymbolSizeAirportWeather,
     ui->spinBoxOptionsDisplaySymbolSizeWindBarbs,
     ui->spinBoxOptionsDisplaySymbolSizeAircraftAi,
     ui->spinBoxOptionsDisplayTextSizeFlightplan,
     ui->spinBoxOptionsDisplayTextSizeAircraftUser,
     ui->spinBoxOptionsDisplaySymbolSizeAircraftUser,
     ui->spinBoxOptionsDisplayTextSizeAirport,
     ui->spinBoxOptionsDisplayThicknessTrail,
     ui->spinBoxOptionsDisplayThicknessRangeDistance,
     ui->spinBoxOptionsDisplayThicknessCompassRose,
     ui->spinBoxOptionsDisplaySunShadeDarkness,
     ui->comboBoxOptionsDisplayTrailType,
     ui->spinBoxOptionsDisplayTextSizeCompassRose,
     ui->spinBoxOptionsDisplayTextSizeRangeDistance,

     ui->spinBoxOptionsDisplayTextSizeMora,
     ui->spinBoxOptionsDisplayTransparencyMora,
     ui->spinBoxOptionsMapNavTouchArea,

     ui->comboBoxOptionsStartupUpdateChannels,
     ui->comboBoxOptionsStartupUpdateRate,

     ui->comboBoxOptionsUnitDistance,
     ui->comboBoxOptionsUnitAlt,
     ui->comboBoxOptionsUnitSpeed,
     ui->comboBoxOptionsUnitVertSpeed,
     ui->comboBoxOptionsUnitShortDistance,
     ui->comboBoxOptionsUnitCoords,
     ui->comboBoxOptionsUnitFuelWeight,

     ui->checkBoxOptionsMapZoomAvoidBlurred,

     ui->checkBoxOptionsMapAirportText,
     ui->checkBoxOptionsMapNavaidText,
     ui->checkBoxOptionsMapFlightplanText,
     ui->checkBoxOptionsMapAirportBoundary,
     ui->checkBoxOptionsMapAirportDiagram,
     ui->checkBoxOptionsMapFlightplanDimPassed,
     ui->checkBoxOptionsSimDoNotFollowOnScroll,
     ui->checkBoxOptionsSimCenterLeg,
     ui->checkBoxOptionsSimCenterLegTable,
     ui->checkBoxOptionsSimClearSelection,
     ui->spinBoxSimDoNotFollowOnScrollTime,

     ui->radioButtonOptionsOnlineNone,
     ui->radioButtonOptionsOnlineVatsim,
     ui->radioButtonOptionsOnlineIvao,
     ui->radioButtonOptionsOnlinePilotEdge,
     ui->radioButtonOptionsOnlineCustomStatus,
     ui->radioButtonOptionsOnlineCustom,

     ui->lineEditOptionsOnlineStatusUrl,
     ui->lineEditOptionsOnlineWhazzupUrl,
     ui->spinBoxOptionsOnlineUpdate,
     ui->comboBoxOptionsOnlineFormat,

     ui->checkBoxDisplayOnlineGroundRange,
     ui->checkBoxDisplayOnlineApproachRange,
     ui->checkBoxDisplayOnlineObserverRange,
     ui->checkBoxDisplayOnlineFirRange,
     ui->checkBoxDisplayOnlineAreaRange,
     ui->checkBoxDisplayOnlineDepartureRange,
     ui->checkBoxDisplayOnlineTowerRange,
     ui->checkBoxDisplayOnlineClearanceRange,
     ui->spinBoxDisplayOnlineClearance,
     ui->spinBoxDisplayOnlineArea,
     ui->spinBoxDisplayOnlineApproach,
     ui->spinBoxDisplayOnlineDeparture,
     ui->spinBoxDisplayOnlineFir,
     ui->spinBoxDisplayOnlineObserver,
     ui->spinBoxDisplayOnlineGround,
     ui->spinBoxDisplayOnlineTower,

     ui->checkBoxOptionsUnitFuelOther,
     ui->checkBoxOptionsUnitTrueCourse,

     ui->spinBoxOptionsWebPort,
     ui->checkBoxOptionsWebEncrypted,
     ui->lineEditOptionsWebDocroot});

  ui->lineEditOptionsMapRangeRings->setValidator(rangeRingValidator);

  connect(ui->listWidgetOptionPages, &QListWidget::currentItemChanged, this, &OptionsDialog::changePage);

  connect(ui->buttonBoxOptions, &QDialogButtonBox::clicked, this, &OptionsDialog::buttonBoxClicked);

  // ===========================================================================
  // GUI language options
  connect(ui->comboBoxOptionsGuiLanguage, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &OptionsDialog::languageChanged);
  connect(ui->pushButtonOptionsGuiSelectFont, &QPushButton::clicked, this, &OptionsDialog::selectGuiFontClicked);
  connect(ui->pushButtonOptionsGuiResetFont, &QPushButton::clicked, this, &OptionsDialog::resetGuiFontClicked);

  // ===========================================================================
  // Weather widgets - ASN
  connect(ui->pushButtonOptionsWeatherAsnPathSelect, &QPushButton::clicked,
          this, &OptionsDialog::selectActiveSkyPathClicked);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::editingFinished,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::textEdited,
          this, &OptionsDialog::updateActiveSkyPathStatus);

  // Weather widgets - X-Plane METAR
  connect(ui->pushButtonOptionsWeatherXplanePathSelect, &QPushButton::clicked,
          this, &OptionsDialog::selectXplanePathClicked);
  connect(ui->lineEditOptionsWeatherXplanePath, &QLineEdit::editingFinished,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplanePath, &QLineEdit::textEdited,
          this, &OptionsDialog::updateXplanePathStatus);

  // ===========================================================================
  // Online weather
  connect(ui->lineEditOptionsWeatherNoaaStationsUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherVatsimUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherIvaoUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherNoaaWindUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateWeatherButtonState);

  connect(ui->lineEditOptionsWeatherXplaneWind, &QLineEdit::editingFinished,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplaneWind, &QLineEdit::textEdited,
          this, &OptionsDialog::updateXplaneWindStatus);

  // ===========================================================================
  // Weather test buttons
  connect(ui->pushButtonOptionsWeatherNoaaTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherNoaaUrlClicked);
  connect(ui->pushButtonOptionsWeatherVatsimTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherVatsimUrlClicked);
  connect(ui->pushButtonOptionsWeatherIvaoTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherIvaoUrlClicked);

  connect(ui->pushButtonOptionsWeatherNoaaWindTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherNoaaWindUrlClicked);
  connect(ui->pushButtonOptionsWeatherXplaneWindPathSelect, &QPushButton::clicked,
          this, &OptionsDialog::weatherXplaneWindPathSelectClicked);

  // ===========================================================================
  // Map
  connect(ui->checkBoxOptionsMapClickAirport, &QCheckBox::toggled, this, &OptionsDialog::mapClickAirportProcsToggled);

  // Map navigation
  connect(ui->radioButtonOptionsMapNavDragMove, &QRadioButton::clicked, this, &OptionsDialog::updateNavOptions);
  connect(ui->radioButtonOptionsMapNavClickCenter, &QRadioButton::clicked, this, &OptionsDialog::updateNavOptions);
  connect(ui->radioButtonOptionsMapNavTouchscreen, &QRadioButton::clicked, this, &OptionsDialog::updateNavOptions);

  connect(ui->pushButtonOptionsDisplaySelectFont, &QPushButton::clicked, this, &OptionsDialog::selectMapFontClicked);
  connect(ui->pushButtonOptionsDisplayResetFont, &QPushButton::clicked, this, &OptionsDialog::resetMapFontClicked);

  // ===========================================================================
  // Database exclude path
  connect(ui->pushButtonOptionsDatabaseAddExcludeDir, &QPushButton::clicked,
          this, &OptionsDialog::addDatabaseExcludeDirClicked);
  connect(ui->pushButtonOptionsDatabaseAddExcludeFile, &QPushButton::clicked,
          this, &OptionsDialog::addDatabaseExcludeFileClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveExclude, &QPushButton::clicked,
          this, &OptionsDialog::removeDatabaseExcludePathClicked);
  connect(ui->listWidgetOptionsDatabaseExclude, &QListWidget::currentRowChanged,
          this, &OptionsDialog::updateDatabaseButtonState);

  // Database addon exclude path
  connect(ui->pushButtonOptionsDatabaseAddAddon, &QPushButton::clicked,
          this, &OptionsDialog::addDatabaseAddOnExcludePathClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveAddon, &QPushButton::clicked,
          this, &OptionsDialog::removeDatabaseAddOnExcludePathClicked);
  connect(ui->listWidgetOptionsDatabaseAddon, &QListWidget::currentRowChanged,
          this, &OptionsDialog::updateDatabaseButtonState);

  // ===========================================================================
  // Cache
  connect(ui->pushButtonOptionsCacheClearMemory, &QPushButton::clicked,
          this, &OptionsDialog::clearMemCachedClicked);
  connect(ui->pushButtonOptionsCacheClearDisk, &QPushButton::clicked,
          this, &OptionsDialog::clearDiskCachedClicked);
  connect(ui->pushButtonOptionsCacheShow, &QPushButton::clicked,
          this, &OptionsDialog::showDiskCacheClicked);

  connect(ui->checkBoxOptionsSimUpdatesConstant, &QCheckBox::toggled,
          this, &OptionsDialog::simUpdatesConstantClicked);

  connect(ui->pushButtonOptionsDisplayFlightplanColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanProcedureColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanProcedureColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanActiveColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanActiveColorClicked);
  connect(ui->pushButtonOptionsDisplayTrailColor, &QPushButton::clicked,
          this, &OptionsDialog::trailColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanPassedColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanPassedColorClicked);

  connect(ui->lineEditOptionsRouteFilename, &QLineEdit::textEdited,
          this, &OptionsDialog::updateFlightplanExample);
  connect(ui->pushButtonOptionsRouteFilenameShort, &QPushButton::clicked,
          this, &OptionsDialog::flightplanPatterShortClicked);
  connect(ui->pushButtonOptionsRouteFilenameLong, &QPushButton::clicked,
          this, &OptionsDialog::flightplanPatterLongClicked);

  connect(ui->radioButtonCacheUseOffineElevation, &QRadioButton::clicked,
          this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->radioButtonCacheUseOnlineElevation, &QRadioButton::clicked,
          this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->lineEditCacheOfflineDataPath, &QLineEdit::textEdited,
          this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->pushButtonCacheOfflineDataSelect, &QPushButton::clicked,
          this, &OptionsDialog::offlineDataSelectClicked);

  connect(ui->pushButtonCacheUserAirspacePathSelect, &QPushButton::clicked,
          this, &OptionsDialog::userAirspacePathSelectClicked);
  connect(ui->lineEditCacheUserAirspacePath, &QLineEdit::textEdited,
          this, &OptionsDialog::updateCacheUserAirspaceStates);

  connect(ui->pushButtonOptionsStartupCheckUpdate, &QPushButton::clicked,
          this, &OptionsDialog::checkUpdateClicked);

  connect(ui->checkBoxOptionsMapEmptyAirports, &QCheckBox::toggled,
          this, &OptionsDialog::mapEmptyAirportsClicked);

  connect(ui->checkBoxOptionsSimDoNotFollowOnScroll, &QCheckBox::toggled, this,
          &OptionsDialog::simNoFollowAircraftOnScrollClicked);

  // Online tab =======================================================================
  connect(ui->radioButtonOptionsOnlineNone, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineVatsim, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineIvao, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlinePilotEdge, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineCustomStatus, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineCustom, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);

  connect(ui->lineEditOptionsOnlineStatusUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->lineEditOptionsOnlineWhazzupUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateOnlineWidgetStatus);

  connect(ui->pushButtonOptionsOnlineTestStatusUrl, &QPushButton::clicked,
          this, &OptionsDialog::onlineTestStatusUrlClicked);
  connect(ui->pushButtonOptionsOnlineTestWhazzupUrl, &QPushButton::clicked,
          this, &OptionsDialog::onlineTestWhazzupUrlClicked);

  // Online map display =======================================================================
  connect(ui->checkBoxDisplayOnlineGroundRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineApproachRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineObserverRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineFirRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineAreaRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineDepartureRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineTowerRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineClearanceRange, &QCheckBox::toggled,
          this, &OptionsDialog::onlineDisplayRangeClicked);

  // Web server =======================================================================
  connect(ui->pushButtonOptionsWebSelectDocroot, &QPushButton::clicked, this, &OptionsDialog::selectWebDocrootClicked);
  connect(ui->lineEditOptionsWebDocroot, &QLineEdit::textEdited, this, &OptionsDialog::updateWebDocrootStatus);
  connect(ui->pushButtonOptionsWebStart, &QPushButton::clicked, this, &OptionsDialog::startStopWebServerClicked);

  // Flight plan =======================================================================
  connect(ui->checkBoxOptionsRouteEastWestRule, &QPushButton::clicked, this, &OptionsDialog::eastWestRuleClicked);
}

/* called at program end */
OptionsDialog::~OptionsDialog()
{
  delete rangeRingValidator;
  delete units;
  delete ui;
  delete fontDialog;
}

void OptionsDialog::open()
{
  qDebug() << Q_FUNC_INFO;
  optionDataToWidgets(OptionData::instanceInternal());
  updateCacheElevationStates();
  updateCacheUserAirspaceStates();
  updateDatabaseButtonState();
  updateNavOptions();
  updateOnlineWidgetStatus();
  updateWeatherButtonState();
  updateWebDocrootStatus();
  updateWebServerStatus();
  eastWestRuleClicked();
  updateWidgetUnits();
  mapClickAirportProcsToggled();
  updateFlightplanExample();

  QDialog::open();
}

void OptionsDialog::reject()
{
  qDebug() << Q_FUNC_INFO;

  // Need to catch this here since dialog is owned by main window and kept alive
  updateFontFromData();
  updateWebOptionsFromData();

  QDialog::reject();
}

void OptionsDialog::eastWestRuleClicked()
{
  ui->comboBoxOptionsRouteAltitudeRuleType->setEnabled(ui->checkBoxOptionsRouteEastWestRule->isChecked());
}

void OptionsDialog::onlineDisplayRangeClicked()
{
  ui->spinBoxDisplayOnlineClearance->setEnabled(!ui->checkBoxDisplayOnlineClearanceRange->isChecked());
  ui->spinBoxDisplayOnlineArea->setEnabled(!ui->checkBoxDisplayOnlineAreaRange->isChecked());
  ui->spinBoxDisplayOnlineApproach->setEnabled(!ui->checkBoxDisplayOnlineApproachRange->isChecked());
  ui->spinBoxDisplayOnlineDeparture->setEnabled(!ui->checkBoxDisplayOnlineDepartureRange->isChecked());
  ui->spinBoxDisplayOnlineFir->setEnabled(!ui->checkBoxDisplayOnlineFirRange->isChecked());
  ui->spinBoxDisplayOnlineObserver->setEnabled(!ui->checkBoxDisplayOnlineObserverRange->isChecked());
  ui->spinBoxDisplayOnlineGround->setEnabled(!ui->checkBoxDisplayOnlineGroundRange->isChecked());
  ui->spinBoxDisplayOnlineTower->setEnabled(!ui->checkBoxDisplayOnlineTowerRange->isChecked());
}

void OptionsDialog::onlineTestStatusUrlClicked()
{
  onlineTestUrl(ui->lineEditOptionsOnlineStatusUrl->text(), true);
}

void OptionsDialog::onlineTestWhazzupUrlClicked()
{
  onlineTestUrl(ui->lineEditOptionsOnlineWhazzupUrl->text(), false);
}

void OptionsDialog::onlineTestUrl(const QString& url, bool statusFile)
{
  qDebug() << Q_FUNC_INFO << url;
  QStringList result;

  if(atools::fs::weather::testUrl(url, QString(), result, 250))
  {
    bool ok = false;
    for(const QString& str : result)
    {
      if(statusFile)
        ok |= str.simplified().startsWith("url0");
      else
        ok |= str.simplified().startsWith("!GENERAL") || str.simplified().startsWith("!CLIENTS");
    }

    if(ok)
      QMessageBox::information(this, QApplication::applicationName(),
                               tr("<p>Success. First lines in file:</p><hr/><code>%1</code><hr/><br/>").
                               arg(result.mid(0, 6).join("<br/>")));
    else
      atools::gui::Dialog::warning(this,
                                   tr(
                                     "<p>Downloaded successfully but the file does not look like a whazzup.txt file.</p>"
                                       "<p><b>One of the sections <i>!GENERAL</i> and/or <i>!CLIENTS</i> is missing.</b></p>"
                                         "<p>First lines in file:</p><hr/><code>%1</code><hr/><br/>").
                                   arg(result.mid(0, 6).join("<br/>")));
  }
  else
    atools::gui::Dialog::warning(this, tr("Failed. Reason:\n%1").arg(result.join("\n")));
}

void OptionsDialog::updateOnlineWidgetStatus()
{
  qDebug() << Q_FUNC_INFO;

  if(ui->radioButtonOptionsOnlineNone->isChecked() || ui->radioButtonOptionsOnlineVatsim->isChecked() ||
     ui->radioButtonOptionsOnlineIvao->isChecked() || ui->radioButtonOptionsOnlinePilotEdge->isChecked())
  {
    ui->lineEditOptionsOnlineStatusUrl->setEnabled(false);
    ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(false);
    ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(false);
    ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(false);
    ui->spinBoxOptionsOnlineUpdate->setEnabled(false);
    ui->comboBoxOptionsOnlineFormat->setEnabled(false);
  }
  else
  {
    if(ui->radioButtonOptionsOnlineCustomStatus->isChecked())
    {
      ui->lineEditOptionsOnlineStatusUrl->setEnabled(true);
      ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(false);
      ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(QUrl(ui->lineEditOptionsOnlineStatusUrl->text()).isValid());
      ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(false);
    }
    else if(ui->radioButtonOptionsOnlineCustom->isChecked())
    {
      ui->lineEditOptionsOnlineStatusUrl->setEnabled(false);
      ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(true);
      ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(false);
      ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(QUrl(ui->lineEditOptionsOnlineWhazzupUrl->text()).isValid());
    }
    ui->spinBoxOptionsOnlineUpdate->setEnabled(true);
    ui->comboBoxOptionsOnlineFormat->setEnabled(true);
  }
}

void OptionsDialog::updateWidgetUnits()
{
  if(units == nullptr)
  {
    units = new UnitStringTool();
    units->init({
      ui->doubleSpinBoxOptionsMapZoomShowMap,
      ui->doubleSpinBoxOptionsMapZoomShowMapMenu,
      ui->spinBoxOptionsRouteGroundBuffer,
      ui->labelOptionsMapRangeRings,
      ui->spinBoxDisplayOnlineClearance,
      ui->spinBoxDisplayOnlineArea,
      ui->spinBoxDisplayOnlineApproach,
      ui->spinBoxDisplayOnlineDeparture,
      ui->spinBoxDisplayOnlineFir,
      ui->spinBoxDisplayOnlineObserver,
      ui->spinBoxDisplayOnlineGround,
      ui->spinBoxDisplayOnlineTower
    });
  }
  else
    units->update();
}

bool OptionsDialog::isOverrideRegion()
{
  return Settings::instance().valueBool(lnm::OPTIONS_GUI_OVERRIDE_LOCALE, false);
}

QString OptionsDialog::getLocale()
{
  return Settings::instance().valueStr(lnm::OPTIONS_DIALOG_LANGUAGE, QLocale().name());
}

QString OptionsDialog::selectCacheUserAirspace()
{
  userAirspacePathSelectClicked();
  return ui->lineEditCacheUserAirspacePath->text();
}

void OptionsDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << "Clicked" << button->text();

  if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Apply))
  {
    widgetsToOptionData();
    saveState();
    emit optionsChanged();

    // Update dialog internal stuff
    updateWidgetStates();

    updateWebOptionsFromData();
    updateWebServerStatus();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Ok))
  {
    widgetsToOptionData();
    saveState();
    updateWidgetUnits();
    updateWebOptionsFromData();
    emit optionsChanged();
    accept();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "OPTIONS.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Cancel))
    reject();
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::RestoreDefaults))
  {
    qDebug() << "OptionsDialog::resetDefaultClicked";

    int result = QMessageBox::question(this, QApplication::applicationName(),
                                       tr("Reset all options to default?"),
                                       QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

    if(result == QMessageBox::Yes)
    {
      optionDataToWidgets(OptionData());
      updateWidgetStates();

      updateWebOptionsFromData();
      updateFontFromData();
      updateWebServerStatus();
    }
  }
}

void OptionsDialog::updateWidgetStates()
{
  simNoFollowAircraftOnScrollClicked(false);
  updateButtonColors();
  updateGuiFontLabel();
  updateMapFontLabel();
  onlineDisplayRangeClicked();
  eastWestRuleClicked();
  mapEmptyAirportsClicked(false);
  simUpdatesConstantClicked(false);
  updateCacheElevationStates();
  updateCacheUserAirspaceStates();
  updateDatabaseButtonState();
  updateNavOptions();
  updateOnlineWidgetStatus();
  updateWeatherButtonState();
  updateWidgetUnits();
}

void OptionsDialog::saveState()
{
  optionDataToWidgets(OptionData::instanceInternal());

  // Save widgets to settings
  atools::gui::WidgetState(lnm::OPTIONS_DIALOG_WIDGET,
                           false /* save visibility */, true /* block signals */).save(widgets);
  saveDisplayOptItemStates(displayOptItemIndex, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS);
  saveDisplayOptItemStates(displayOptItemIndexRose, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_COMPASS_ROSE);
  saveDisplayOptItemStates(displayOptItemIndexMeasurement, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_MEASUREMENT);
  saveDisplayOptItemStates(displayOptItemIndexRoute, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_ROUTE);
  saveDisplayOptItemStates(displayOptItemIndexNavAid, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_NAVAID);

  Settings& settings = Settings::instance();

  settings.setValue(lnm::OPTIONS_DIALOG_LANGUAGE, guiLanguage);
  settings.setValue(lnm::OPTIONS_DIALOG_FONT, guiFont);
  settings.setValue(lnm::OPTIONS_DIALOG_MAP_FONT, mapFont);

  // Save the path lists
  QStringList paths;
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    paths.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_EXCLUDE, paths);

  paths.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    paths.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE, paths);

  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, flightplanColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, flightplanProcedureColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, flightplanActiveColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, flightplanPassedColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, trailColor);

  settings.syncSettings();
}

void OptionsDialog::restoreState()
{
  Settings& settings = Settings::instance();

  // Reload online network settings from configuration file which can be overloaded by placing a copy
  // in the settings file
  QString networksPath = settings.getOverloadedPath(lnm::NETWORKS_CONFIG);
  qInfo() << Q_FUNC_INFO << "Loading networks.cfg from" << networksPath;

  QSettings networkSettings(networksPath, QSettings::IniFormat);
  OptionData& od = OptionData::instanceInternal();

  od.onlineVatsimStatusUrl = networkSettings.value("vatsim/statusurl").toString();
  od.onlineIvaoStatusUrl = networkSettings.value("ivao/statusurl").toString();
  od.onlinePilotEdgeStatusUrl = networkSettings.value("pilotedge/statusurl").toString();

  int update;
  if(networkSettings.value("options/update").toString().trimmed().toLower() == "auto")
    update = -1;
  else
    update = networkSettings.value("options/update").toInt();

  if(update < 60 && update != -1)
    update = 60;
  od.onlineReloadSecondsConfig = update;

  // Disable selection based on what was found in the file
  ui->radioButtonOptionsOnlineIvao->setVisible(!od.onlineIvaoStatusUrl.isEmpty());
  ui->radioButtonOptionsOnlinePilotEdge->setVisible(!od.onlinePilotEdgeStatusUrl.isEmpty());
  ui->radioButtonOptionsOnlineVatsim->setVisible(!od.onlineVatsimStatusUrl.isEmpty());

  atools::gui::WidgetState(lnm::OPTIONS_DIALOG_WIDGET,
                           false /*save visibility*/, true /*block signals*/).restore(widgets);
  restoreOptionItemStates(displayOptItemIndex, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS);
  restoreOptionItemStates(displayOptItemIndexRose, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_COMPASS_ROSE);
  restoreOptionItemStates(displayOptItemIndexMeasurement, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_MEASUREMENT);
  restoreOptionItemStates(displayOptItemIndexRoute, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_ROUTE);
  restoreOptionItemStates(displayOptItemIndexNavAid, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_NAVAID);

  ui->splitterOptions->setHandleWidth(6);

  if(settings.contains(lnm::OPTIONS_DIALOG_DB_EXCLUDE))
    ui->listWidgetOptionsDatabaseExclude->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_EXCLUDE));
  if(settings.contains(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE))
    ui->listWidgetOptionsDatabaseAddon->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE));

  flightplanColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, QColor(Qt::yellow)).value<QColor>();
  flightplanProcedureColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, QColor(255, 150, 0)).value<QColor>();
  flightplanActiveColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, QColor(Qt::magenta)).value<QColor>();
  flightplanPassedColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, QColor(Qt::gray)).value<QColor>();
  trailColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, QColor(Qt::black)).value<QColor>();

  guiLanguage = settings.valueStr(lnm::OPTIONS_DIALOG_LANGUAGE, QLocale().name());
  guiFont = settings.valueStr(lnm::OPTIONS_DIALOG_FONT, QString());
  mapFont = settings.valueStr(lnm::OPTIONS_DIALOG_MAP_FONT, QString());

  widgetsToOptionData();
  updateWidgetUnits();
  simUpdatesConstantClicked(false);
  mapEmptyAirportsClicked(false);
  simNoFollowAircraftOnScrollClicked(false);
  updateButtonColors();
  updateGuiFontLabel();
  updateMapFontLabel();
  onlineDisplayRangeClicked();
  eastWestRuleClicked();

  updateWebServerStatus();
  updateWebDocrootStatus();
  updateFlightplanExample();

  if(ui->listWidgetOptionPages->selectedItems().isEmpty())
    ui->listWidgetOptionPages->selectionModel()->select(ui->listWidgetOptionPages->model()->index(0, 0),
                                                        QItemSelectionModel::ClearAndSelect);

  ui->stackedWidgetOptions->setCurrentIndex(ui->listWidgetOptionPages->currentRow());
}

void OptionsDialog::languageChanged(int)
{
  guiLanguage = ui->comboBoxOptionsGuiLanguage->currentData().value<QLocale>().name();
  qDebug() << Q_FUNC_INFO << guiLanguage;
}

void OptionsDialog::udpdateLanguageComboBox(const QString& lang)
{
  if(ui->comboBoxOptionsGuiLanguage->count() == 0)
  {
    // Fill combo box with all available locale =========================
    QVector<QLocale> locales = atools::gui::Translator::findTranslationFiles();
    ui->comboBoxOptionsGuiLanguage->clear();
    for(int i = 0; i < locales.size(); i++)
    {
      // Usedata for item is locale object
      const QLocale& locale = locales.at(i);
      ui->comboBoxOptionsGuiLanguage->addItem(tr("%1, %2").
                                              arg(locale.nativeLanguageName()).
                                              arg(locale.nativeCountryName()), locale);
    }
  }

  // Now try to find the best entry for the given language ==============================
  QLocale system = lang.isEmpty() ? QLocale() : QLocale(lang);
  int currentIndexLang = -1, currentIndexLangCountry = -1, englishLocale = -1;
  for(int i = 0; i < ui->comboBoxOptionsGuiLanguage->count(); i++)
  {
    const QLocale& locale = ui->comboBoxOptionsGuiLanguage->itemData(i).value<QLocale>();

    // Check if language and country match - this is the most precise match
    if(system.language() == locale.language() && system.country() == locale.country())
      currentIndexLangCountry = i;

    // Check if language matches
    if(system.language() == locale.language())
      currentIndexLang = i;

    // Get index for default English
    if(locale.language() == QLocale::English)
      englishLocale = i;
  }
  qDebug() << Q_FUNC_INFO << "currentIndexLangCountry" << currentIndexLangCountry
           << "currentIndexLang" << currentIndexLang;

  // Use the best match to change the current combo box entry
  if(currentIndexLangCountry != -1)
  {
    // Language and country like "pt_BR"
    ui->comboBoxOptionsGuiLanguage->setCurrentIndex(currentIndexLangCountry);
    guiLanguage = ui->comboBoxOptionsGuiLanguage->itemData(currentIndexLangCountry).value<QLocale>().name();
  }
  else if(currentIndexLang != -1)
  {
    // Language only like "de"
    ui->comboBoxOptionsGuiLanguage->setCurrentIndex(currentIndexLang);
    guiLanguage = ui->comboBoxOptionsGuiLanguage->itemData(currentIndexLang).value<QLocale>().name();
  }
  else if(englishLocale != -1)
  {
    // English as fallback
    ui->comboBoxOptionsGuiLanguage->setCurrentIndex(englishLocale);
    guiLanguage = ui->comboBoxOptionsGuiLanguage->itemData(englishLocale).value<QLocale>().name();
  }
}

void OptionsDialog::updateButtonColors()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanColor, flightplanColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanProcedureColor, flightplanProcedureColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanActiveColor, flightplanActiveColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanPassedColor, flightplanPassedColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayTrailColor, trailColor);
}

template<typename TYPE>
void OptionsDialog::restoreOptionItemStates(const QHash<TYPE, QTreeWidgetItem *>& index,
                                            const QString& optionPrefix) const
{
  const Settings& settings = Settings::instance();
  for(const TYPE& dispOpt : index.keys())
  {
    QString optName = optionPrefix + "_" + QString::number(dispOpt);
    if(settings.contains(optName))
      index.value(dispOpt)->setCheckState(0, settings.valueBool(optName, false) ? Qt::Checked : Qt::Unchecked);
  }
}

template<typename TYPE>
void OptionsDialog::saveDisplayOptItemStates(const QHash<TYPE, QTreeWidgetItem *>& index,
                                             const QString& optionPrefix) const
{
  Settings& settings = Settings::instance();

  for(const TYPE& dispOpt : index.keys())
  {
    QTreeWidgetItem *item = index.value(dispOpt);

    QString optName = optionPrefix + "_" + QString::number(dispOpt);

    settings.setValue(optName, item->checkState(0) == Qt::Checked);
  }
}

template<typename TYPE>
void OptionsDialog::displayOptWidgetToOptionData(TYPE& type, const QHash<TYPE, QTreeWidgetItem *>& index) const
{
  for(const TYPE& dispOpt : index.keys())
  {
    if(index.value(dispOpt)->checkState(0) == Qt::Checked)
      type |= dispOpt;
  }
}

template<typename TYPE>
void OptionsDialog::displayOptDataToWidget(const TYPE& type, const QHash<TYPE, QTreeWidgetItem *>& index) const
{
  for(const TYPE& dispOpt : index.keys())
  {
    index.value(dispOpt)->setCheckState(
      0, type & dispOpt ? Qt::Checked : Qt::Unchecked);
  }
}

QTreeWidgetItem *OptionsDialog::addTopItem(QTreeWidgetItem *root, const QString& text, const QString& tooltip)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(root, {text});
  item->setToolTip(0, tooltip);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate | Qt::ItemIsEnabled);
  return item;
}

template<typename TYPE>
QTreeWidgetItem *OptionsDialog::addItem(QTreeWidgetItem *root, QHash<TYPE, QTreeWidgetItem *>& index,
                                        const QString& text, const QString& tooltip, TYPE type, bool checked) const
{
  QTreeWidgetItem *item = new QTreeWidgetItem(root, {text}, static_cast<int>(type));
  item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
  item->setToolTip(0, tooltip);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
  index.insert(type, item);
  return item;
}

void OptionsDialog::checkUpdateClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Trigger async check and get a dialog even if nothing was found
  NavApp::checkForUpdates(ui->comboBoxOptionsStartupUpdateChannels->currentIndex(), true /* manually triggered */);
}

void OptionsDialog::flightplanProcedureColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanProcedureColor, mainWindow);
  if(col.isValid())
  {
    flightplanProcedureColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanColor, mainWindow);
  if(col.isValid())
  {
    flightplanColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanActiveColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanActiveColor, mainWindow);
  if(col.isValid())
  {
    flightplanActiveColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanPassedColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanPassedColor, mainWindow);
  if(col.isValid())
  {
    flightplanPassedColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::trailColorClicked()
{
  QColor col = QColorDialog::getColor(trailColor, mainWindow);
  if(col.isValid())
  {
    trailColor = col;
    updateButtonColors();
  }
}

/* Test NOAA weather URL and show a dialog with the result */
void OptionsDialog::testWeatherNoaaUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  QStringList resultStr;

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  QDateTime datetime = QDateTime::currentDateTimeUtc();
  datetime.setTime(QTime(datetime.time().hour(), 0, 0));
  QString url = ui->lineEditOptionsWeatherNoaaStationsUrl->text().arg(datetime.time().hour(), 2, 10, QChar('0'));

  bool result = WeatherReporter::testUrl(url, QString(), resultStr);

  QGuiApplication::restoreOverrideCursor();

  if(result)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Success. First METARs in file:</p><hr/><code>%1</code><hr/><br/>").
                             arg(resultStr.join("<br/>")));
  else
    atools::gui::Dialog::warning(this, tr("Failed. Reason:\n%1").arg(resultStr.join("\n")));
}

/* Test Vatsim weather URL and show a dialog with the result */
void OptionsDialog::testWeatherVatsimUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  QStringList resultStr;

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  bool result = WeatherReporter::testUrl(ui->lineEditOptionsWeatherVatsimUrl->text(), QString(), resultStr);
  QGuiApplication::restoreOverrideCursor();

  if(result)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Success. First METARs in file:</p><hr/><code>%1</code><hr/><br/>").
                             arg(resultStr.join("<br/>")));
  else
    atools::gui::Dialog::warning(this, tr("Failed. Reason:\n%1").arg(resultStr.join("\n")));
}

/* Test IVAO weather download URL and show a dialog of the first line */
void OptionsDialog::testWeatherIvaoUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  QStringList resultStr;

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  bool result = WeatherReporter::testUrl(ui->lineEditOptionsWeatherIvaoUrl->text(), QString(), resultStr);
  QGuiApplication::restoreOverrideCursor();

  if(result)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Success. First METARs in file:</p><hr/><code>%1</code><hr/><br/>").
                             arg(resultStr.join("<br/>")));
  else
    atools::gui::Dialog::warning(this, tr("Failed. Reason:\n%1").arg(resultStr.join("\n")));
}

/* Test NOAA GRIB download URL and show a dialog of the first line */
void OptionsDialog::testWeatherNoaaWindUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  QStringList resultStr;
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  bool result = WeatherReporter::testUrl(ui->lineEditOptionsWeatherNoaaWindUrl->text(), QString(), resultStr);
  QGuiApplication::restoreOverrideCursor();
  if(result)
    QMessageBox::information(this, QApplication::applicationName(), tr("Success."));
  else
    atools::gui::Dialog::warning(this, tr("Failed. Reason:\n%1").arg(resultStr.join("\n")));
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludeDirClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Scenery Loading"),
    lnm::OPTIONS_DIALOG_DB_DIR_DLG,
    atools::fs::FsPaths::getBasePath(NavApp::getCurrentSimulatorDb()));

  if(!path.isEmpty())
  {
    ui->listWidgetOptionsDatabaseExclude->addItem(QDir::toNativeSeparators(path));
    updateDatabaseButtonState();
  }
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludeFileClicked()
{
  qDebug() << Q_FUNC_INFO;

  QStringList paths = atools::gui::Dialog(this).openFileDialogMulti(
    tr("Open Files to exclude from Scenery Loading"),
    QString(), // filter
    lnm::OPTIONS_DIALOG_DB_FILE_DLG,
    atools::fs::FsPaths::getBasePath(NavApp::getCurrentSimulatorDb()));

  if(!paths.isEmpty())
  {
    for(const QString& path : paths)
      ui->listWidgetOptionsDatabaseExclude->addItem(QDir::toNativeSeparators(path));
    updateDatabaseButtonState();
  }
}

void OptionsDialog::removeDatabaseExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Create list in reverse order so that deleting can start at the bottom of the list
  for(int idx : atools::gui::util::getSelectedIndexesInDeletionOrder(
        ui->listWidgetOptionsDatabaseExclude->selectionModel()))
    // Item removes itself from the list when deleted
    delete ui->listWidgetOptionsDatabaseExclude->item(idx);

  updateDatabaseButtonState();
}

/* Show directory dialog to add add-on exclude path */
void OptionsDialog::addDatabaseAddOnExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Add-On Recognition"),
    lnm::OPTIONS_DIALOG_DB_DIR_DLG,
    atools::fs::FsPaths::getBasePath(NavApp::getCurrentSimulatorDb()));

  if(!path.isEmpty())
    ui->listWidgetOptionsDatabaseAddon->addItem(QDir::toNativeSeparators(path));
  updateDatabaseButtonState();
}

void OptionsDialog::removeDatabaseAddOnExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Create list in reverse order so that deleting can start at the bottom of the list
  for(int idx : atools::gui::util::getSelectedIndexesInDeletionOrder(
        ui->listWidgetOptionsDatabaseAddon->selectionModel()))
    // Item removes itself from the list when deleted
    delete ui->listWidgetOptionsDatabaseAddon->item(idx);

  updateDatabaseButtonState();
}

void OptionsDialog::updateDatabaseButtonState()
{
  ui->pushButtonOptionsDatabaseRemoveExclude->setEnabled(
    ui->listWidgetOptionsDatabaseExclude->currentRow() != -1);
  ui->pushButtonOptionsDatabaseRemoveAddon->setEnabled(
    ui->listWidgetOptionsDatabaseAddon->currentRow() != -1);
}

void OptionsDialog::simUpdatesConstantClicked(bool state)
{
  Q_UNUSED(state)
  ui->spinBoxOptionsSimUpdateBox->setDisabled(ui->checkBoxOptionsSimUpdatesConstant->isChecked());
}

void OptionsDialog::mapEmptyAirportsClicked(bool state)
{
  Q_UNUSED(state)
  ui->checkBoxOptionsMapEmptyAirports3D->setEnabled(ui->checkBoxOptionsMapEmptyAirports->isChecked());
}

void OptionsDialog::simNoFollowAircraftOnScrollClicked(bool state)
{
  Q_UNUSED(state)
  ui->spinBoxSimDoNotFollowOnScrollTime->setEnabled(ui->checkBoxOptionsSimDoNotFollowOnScroll->isChecked());
}

/* Convert the range ring string to an int vector */
QVector<int> OptionsDialog::ringStrToVector(const QString& string) const
{
  QVector<int> rings;
  for(const QString& str : string.split(" "))
  {
    QString val = str.trimmed();

    if(!val.isEmpty())
    {
      bool ok;
      int num = val.toInt(&ok);
      if(ok && num <= MAX_RANGE_RING_SIZE)
        rings.append(num);
    }
  }
  return rings;
}

/* Copy widget states to OptionData object */
void OptionsDialog::widgetsToOptionData()
{
  OptionData& data = OptionData::instanceInternal();

  data.guiLanguage = guiLanguage;
  data.guiFont = guiFont;
  data.mapFont = mapFont;

  data.flightplanColor = flightplanColor;
  data.flightplanProcedureColor = flightplanProcedureColor;
  data.flightplanActiveColor = flightplanActiveColor;
  data.flightplanPassedColor = flightplanPassedColor;
  data.trailColor = trailColor;

  data.displayOptions = optsd::ITEM_NONE;
  displayOptWidgetToOptionData(data.displayOptions, displayOptItemIndex);

  data.displayOptionsMeasurement = optsd::MEASUREMNENT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsMeasurement, displayOptItemIndexMeasurement);

  data.displayOptionsRoute = optsd::ROUTE_NONE;
  displayOptWidgetToOptionData(data.displayOptionsRoute, displayOptItemIndexRoute);

  data.displayOptionsNavAid = optsd::NAVAIDS_NONE;
  displayOptWidgetToOptionData(data.displayOptionsNavAid, displayOptItemIndexNavAid);

  toFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  toFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  toFlags(ui->checkBoxOptionsStartupLoadTrail, opts::STARTUP_LOAD_TRAIL);
  toFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  toFlags(ui->checkBoxOptionsStartupLoadSearch, opts::STARTUP_LOAD_SEARCH);
  toFlags(ui->checkBoxOptionsStartupLoadInfoContent, opts::STARTUP_LOAD_INFO);
  toFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  toFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  toFlags(ui->radioButtonOptionsStartupShowFlightplan, opts::STARTUP_SHOW_ROUTE);
  toFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  toFlags2(ui->checkBoxOptionsGuiProposeFilename, opts2::PROPOSE_FILENAME);
  toFlags2(ui->checkBoxOptionsGuiRaiseWindows, opts2::RAISE_WINDOWS);
  toFlags2(ui->checkBoxOptionsGuiRaiseDockWindows, opts2::RAISE_DOCK_WINDOWS);
  toFlags2(ui->checkBoxOptionsGuiRaiseMainWindow, opts2::RAISE_MAIN_WINDOW);
  toFlags2(ui->checkBoxOptionsUnitFuelOther, opts2::UNIT_FUEL_SHOW_OTHER);
  toFlags2(ui->checkBoxOptionsUnitTrueCourse, opts2::UNIT_TRUE_COURSE);
  toFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  toFlags(ui->checkBoxOptionsGuiAvoidOverwrite, opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN);
  toFlags(ui->checkBoxOptionsGuiOverrideLocale, opts::GUI_OVERRIDE_LOCALE);
  toFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  toFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_ALTITUDE_RULE);
  toFlags(ui->checkBoxOptionsRouteDeclination, opts::ROUTE_IGNORE_VOR_DECLINATION);
  toFlags(ui->checkBoxOptionsRouteExportUserWpt, opts::ROUTE_GARMIN_USER_WPT);
  toFlagsWeather(ui->checkBoxOptionsWeatherInfoAsn, optsw::WEATHER_INFO_ACTIVESKY);
  toFlagsWeather(ui->checkBoxOptionsWeatherInfoNoaa, optsw::WEATHER_INFO_NOAA);
  toFlagsWeather(ui->checkBoxOptionsWeatherInfoVatsim, optsw::WEATHER_INFO_VATSIM);
  toFlagsWeather(ui->checkBoxOptionsWeatherInfoIvao, optsw::WEATHER_INFO_IVAO);
  toFlagsWeather(ui->checkBoxOptionsWeatherInfoFs, optsw::WEATHER_INFO_FS);
  toFlagsWeather(ui->checkBoxOptionsWeatherTooltipAsn, optsw::WEATHER_TOOLTIP_ACTIVESKY);
  toFlagsWeather(ui->checkBoxOptionsWeatherTooltipNoaa, optsw::WEATHER_TOOLTIP_NOAA);
  toFlagsWeather(ui->checkBoxOptionsWeatherTooltipVatsim, optsw::WEATHER_TOOLTIP_VATSIM);
  toFlagsWeather(ui->checkBoxOptionsWeatherTooltipIvao, optsw::WEATHER_TOOLTIP_IVAO);
  toFlagsWeather(ui->checkBoxOptionsWeatherTooltipFs, optsw::WEATHER_TOOLTIP_FS);
  toFlags(ui->checkBoxOptionsSimUpdatesConstant, opts::SIM_UPDATE_MAP_CONSTANTLY);

  toFlags2(ui->checkBoxOptionsMapZoomAvoidBlurred, opts2::MAP_AVOID_BLURRED_MAP);
  toFlags2(ui->checkBoxOptionsMapUndock, opts2::MAP_ALLOW_UNDOCK);
  toFlags2(ui->checkBoxOptionsGuiHighDpi, opts2::HIGH_DPI_DISPLAY_SUPPORT);

  toFlags(ui->radioButtonCacheUseOffineElevation, opts::CACHE_USE_OFFLINE_ELEVATION);
  toFlags(ui->radioButtonCacheUseOnlineElevation, opts::CACHE_USE_ONLINE_ELEVATION);

  toFlags2(ui->checkBoxOptionsMapEmptyAirports3D, opts2::MAP_EMPTY_AIRPORTS_3D);

  toFlags2(ui->checkBoxOptionsMapAirportText, opts2::MAP_AIRPORT_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapNavaidText, opts2::MAP_NAVAID_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapFlightplanText, opts2::MAP_ROUTE_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapAirportBoundary, opts2::MAP_AIRPORT_BOUNDARY);
  toFlags2(ui->checkBoxOptionsMapAirportDiagram, opts2::MAP_AIRPORT_DIAGRAM);
  toFlags2(ui->checkBoxOptionsMapFlightplanDimPassed, opts2::MAP_ROUTE_DIM_PASSED);
  toFlags2(ui->checkBoxOptionsSimDoNotFollowOnScroll, opts2::ROUTE_NO_FOLLOW_ON_MOVE);
  toFlags2(ui->checkBoxOptionsSimCenterLeg, opts2::ROUTE_AUTOZOOM);
  toFlags2(ui->checkBoxOptionsSimCenterLegTable, opts2::ROUTE_CENTER_ACTIVE_LEG);
  toFlags2(ui->checkBoxOptionsSimClearSelection, opts2::ROUTE_CLEAR_SELECTION);

  toFlags2(ui->checkBoxDisplayOnlineNameLookup, opts2::ONLINE_AIRSPACE_BY_NAME);
  toFlags2(ui->checkBoxDisplayOnlineFileLookup, opts2::ONLINE_AIRSPACE_BY_FILE);

  data.flightplanPattern = ui->lineEditOptionsRouteFilename->text();
  data.cacheOfflineElevationPath = ui->lineEditCacheOfflineDataPath->text();
  data.cacheUserAirspacePath = ui->lineEditCacheUserAirspacePath->text();
  data.cacheUserAirspaceExtensions = ui->lineEditCacheUserAirspaceExtensions->text();

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRPORT, ui->checkBoxOptionsMapTooltipAirport->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_NAVAID, ui->checkBoxOptionsMapTooltipNavaid->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRSPACE, ui->checkBoxOptionsMapTooltipAirspace->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_WIND, ui->checkBoxOptionsMapTooltipWind->isChecked());

  data.displayClickOptions.setFlag(optsd::CLICK_AIRPORT, ui->checkBoxOptionsMapClickAirport->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRPORT_PROC, ui->checkBoxOptionsMapClickAirportProcs->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_NAVAID, ui->checkBoxOptionsMapClickNavaid->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRSPACE, ui->checkBoxOptionsMapClickAirspace->isChecked());

  data.mapRangeRings = ringStrToVector(ui->lineEditOptionsMapRangeRings->text());

  data.weatherXplanePath = QDir::toNativeSeparators(ui->lineEditOptionsWeatherXplanePath->text());
  data.weatherActiveSkyPath = QDir::toNativeSeparators(ui->lineEditOptionsWeatherAsnPath->text());
  data.weatherNoaaUrl = ui->lineEditOptionsWeatherNoaaStationsUrl->text();
  data.weatherVatsimUrl = ui->lineEditOptionsWeatherVatsimUrl->text();
  data.weatherIvaoUrl = ui->lineEditOptionsWeatherIvaoUrl->text();

  data.databaseAddonExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    data.databaseAddonExclude.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());

  data.databaseExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    data.databaseExclude.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());

  data.mapScrollDetail = static_cast<opts::MapScrollDetail>(ui->comboBoxMapScrollDetails->currentIndex());

  // Details when moving map =========================================
  if(ui->radioButtonOptionsSimUpdateFast->isChecked())
    data.simUpdateRate = opts::FAST;
  else if(ui->radioButtonOptionsSimUpdateLow->isChecked())
    data.simUpdateRate = opts::LOW;
  else if(ui->radioButtonOptionsSimUpdateMedium->isChecked())
    data.simUpdateRate = opts::MEDIUM;

  // Map navigation mode =========================================
  if(ui->radioButtonOptionsMapNavDragMove->isChecked())
    data.mapNavigation = opts::MAP_NAV_CLICK_DRAG_MOVE;
  else if(ui->radioButtonOptionsMapNavClickCenter->isChecked())
    data.mapNavigation = opts::MAP_NAV_CLICK_CENTER;
  else if(ui->radioButtonOptionsMapNavTouchscreen->isChecked())
    data.mapNavigation = opts::MAP_NAV_TOUCHSCREEN;

  data.simNoFollowAircraftOnScroll = ui->spinBoxSimDoNotFollowOnScrollTime->value();
  data.simUpdateBox = ui->spinBoxOptionsSimUpdateBox->value();
  data.aircraftTrackMaxPoints = ui->spinBoxSimMaxTrackPoints->value();

  data.cacheSizeDisk = ui->spinBoxOptionsCacheDiskSize->value();
  data.cacheSizeMemory = ui->spinBoxOptionsCacheMemorySize->value();
  data.guiInfoTextSize = ui->spinBoxOptionsGuiInfoText->value();
  data.guiPerfReportTextSize = ui->spinBoxOptionsGuiAircraftPerf->value();
  data.guiRouteTableTextSize = ui->spinBoxOptionsGuiRouteText->value();
  data.guiSearchTableTextSize = ui->spinBoxOptionsGuiSearchText->value();
  data.guiInfoSimSize = ui->spinBoxOptionsGuiSimInfoText->value();

  data.guiStyleMapDimming = ui->spinBoxOptionsGuiThemeMapDimming->value();

  data.mapClickSensitivity = ui->spinBoxOptionsMapClickRect->value();
  data.mapTooltipSensitivity = ui->spinBoxOptionsMapTooltipRect->value();

  data.mapZoomShowClick = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMap->value());
  data.mapZoomShowMenu = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMapMenu->value());

  data.routeGroundBuffer = ui->spinBoxOptionsRouteGroundBuffer->value();

  data.displayTextSizeAircraftAi = ui->spinBoxOptionsDisplayTextSizeAircraftAi->value();
  data.displaySymbolSizeNavaid = ui->spinBoxOptionsDisplaySymbolSizeNavaid->value();
  data.displayTextSizeNavaid = ui->spinBoxOptionsDisplayTextSizeNavaid->value();
  data.displayThicknessFlightplan = ui->spinBoxOptionsDisplayThicknessFlightplan->value();
  data.displaySymbolSizeAirport = ui->spinBoxOptionsDisplaySymbolSizeAirport->value();
  data.displaySymbolSizeAirportWeather = ui->spinBoxOptionsDisplaySymbolSizeAirportWeather->value();
  data.displaySymbolSizeWindBarbs = ui->spinBoxOptionsDisplaySymbolSizeWindBarbs->value();
  data.displaySymbolSizeAircraftAi = ui->spinBoxOptionsDisplaySymbolSizeAircraftAi->value();
  data.displayTextSizeFlightplan = ui->spinBoxOptionsDisplayTextSizeFlightplan->value();
  data.displayTextSizeAircraftUser = ui->spinBoxOptionsDisplayTextSizeAircraftUser->value();
  data.displaySymbolSizeAircraftUser = ui->spinBoxOptionsDisplaySymbolSizeAircraftUser->value();
  data.displayTextSizeAirport = ui->spinBoxOptionsDisplayTextSizeAirport->value();
  data.displayThicknessTrail = ui->spinBoxOptionsDisplayThicknessTrail->value();
  data.displayThicknessRangeDistance = ui->spinBoxOptionsDisplayThicknessRangeDistance->value();
  data.displayThicknessCompassRose = ui->spinBoxOptionsDisplayThicknessCompassRose->value();
  data.displaySunShadingDimFactor = ui->spinBoxOptionsDisplaySunShadeDarkness->value();
  data.displayTrailType = static_cast<opts::DisplayTrailType>(ui->comboBoxOptionsDisplayTrailType->currentIndex());

  data.displayTransparencyMora = ui->spinBoxOptionsDisplayTransparencyMora->value();
  data.displayTextSizeMora = ui->spinBoxOptionsDisplayTextSizeMora->value();
  data.mapNavTouchArea = ui->spinBoxOptionsMapNavTouchArea->value();

  data.displayTextSizeRangeDistance = ui->spinBoxOptionsDisplayTextSizeRangeDistance->value();
  data.displayTextSizeCompassRose = ui->spinBoxOptionsDisplayTextSizeCompassRose->value();

  data.updateRate = static_cast<opts::UpdateRate>(ui->comboBoxOptionsStartupUpdateRate->currentIndex());
  data.updateChannels = static_cast<opts::UpdateChannels>(ui->comboBoxOptionsStartupUpdateChannels->currentIndex());

  data.unitDist = static_cast<opts::UnitDist>(ui->comboBoxOptionsUnitDistance->currentIndex());
  data.unitShortDist = static_cast<opts::UnitShortDist>(ui->comboBoxOptionsUnitShortDistance->currentIndex());
  data.unitAlt = static_cast<opts::UnitAlt>(ui->comboBoxOptionsUnitAlt->currentIndex());
  data.unitSpeed = static_cast<opts::UnitSpeed>(ui->comboBoxOptionsUnitSpeed->currentIndex());
  data.unitVertSpeed = static_cast<opts::UnitVertSpeed>(ui->comboBoxOptionsUnitVertSpeed->currentIndex());
  data.unitCoords = static_cast<opts::UnitCoords>(ui->comboBoxOptionsUnitCoords->currentIndex());
  data.unitFuelWeight = static_cast<opts::UnitFuelAndWeight>(ui->comboBoxOptionsUnitFuelWeight->currentIndex());
  data.altitudeRuleType = static_cast<opts::AltitudeRule>(ui->comboBoxOptionsRouteAltitudeRuleType->currentIndex());

  if(ui->radioButtonOptionsOnlineNone->isChecked())
    data.onlineNetwork = opts::ONLINE_NONE;
  else if(ui->radioButtonOptionsOnlineVatsim->isChecked())
    data.onlineNetwork = opts::ONLINE_VATSIM;
  else if(ui->radioButtonOptionsOnlineIvao->isChecked())
    data.onlineNetwork = opts::ONLINE_IVAO;
  else if(ui->radioButtonOptionsOnlinePilotEdge->isChecked())
    data.onlineNetwork = opts::ONLINE_PILOTEDGE;
  else if(ui->radioButtonOptionsOnlineCustomStatus->isChecked())
    data.onlineNetwork = opts::ONLINE_CUSTOM_STATUS;
  else if(ui->radioButtonOptionsOnlineCustom->isChecked())
    data.onlineNetwork = opts::ONLINE_CUSTOM;

  data.onlineStatusUrl = ui->lineEditOptionsOnlineStatusUrl->text();
  data.onlineWhazzupUrl = ui->lineEditOptionsOnlineWhazzupUrl->text();
  data.onlineReloadSeconds = ui->spinBoxOptionsOnlineUpdate->value();
  data.onlineFormat = static_cast<opts::OnlineFormat>(ui->comboBoxOptionsOnlineFormat->currentIndex());

  data.displayOnlineClearance = displayOnlineRangeToData(ui->spinBoxDisplayOnlineClearance,
                                                         ui->checkBoxDisplayOnlineClearanceRange);
  data.displayOnlineArea = displayOnlineRangeToData(ui->spinBoxDisplayOnlineArea, ui->checkBoxDisplayOnlineAreaRange);
  data.displayOnlineApproach = displayOnlineRangeToData(ui->spinBoxDisplayOnlineApproach,
                                                        ui->checkBoxDisplayOnlineApproachRange);
  data.displayOnlineDeparture = displayOnlineRangeToData(ui->spinBoxDisplayOnlineDeparture,
                                                         ui->checkBoxDisplayOnlineDepartureRange);
  data.displayOnlineFir = displayOnlineRangeToData(ui->spinBoxDisplayOnlineFir, ui->checkBoxDisplayOnlineFirRange);
  data.displayOnlineObserver = displayOnlineRangeToData(ui->spinBoxDisplayOnlineObserver,
                                                        ui->checkBoxDisplayOnlineObserverRange);
  data.displayOnlineGround = displayOnlineRangeToData(ui->spinBoxDisplayOnlineGround,
                                                      ui->checkBoxDisplayOnlineGroundRange);
  data.displayOnlineTower =
    displayOnlineRangeToData(ui->spinBoxDisplayOnlineTower, ui->checkBoxDisplayOnlineTowerRange);

  data.webPort = ui->spinBoxOptionsWebPort->value();
  data.webDocumentRoot = QDir::fromNativeSeparators(ui->lineEditOptionsWebDocroot->text());
  data.webEncrypted = ui->checkBoxOptionsWebEncrypted->isChecked();

  data.weatherXplaneWind = ui->lineEditOptionsWeatherXplaneWind->text();
  data.weatherNoaaWindBaseUrl = ui->lineEditOptionsWeatherNoaaWindUrl->text();

  data.valid = true;
}

int OptionsDialog::displayOnlineRangeToData(const QSpinBox *spinBox, const QCheckBox *checkButton)
{
  return checkButton->isChecked() ? -1 : spinBox->value();
}

void OptionsDialog::displayOnlineRangeFromData(QSpinBox *spinBox, QCheckBox *checkButton, int value)
{
  spinBox->setValue(value);
  checkButton->setChecked(value == -1);
}

/* Copy OptionData object to widget */
void OptionsDialog::optionDataToWidgets(const OptionData& data)
{
  guiLanguage = data.guiLanguage;
  guiFont = data.guiFont;
  mapFont = data.mapFont;
  udpdateLanguageComboBox(data.guiLanguage);
  updateGuiFontLabel();
  updateMapFontLabel();

  flightplanColor = data.flightplanColor;
  flightplanProcedureColor = data.flightplanProcedureColor;
  flightplanActiveColor = data.flightplanActiveColor;
  flightplanPassedColor = data.flightplanPassedColor;
  trailColor = data.trailColor;
  displayOptDataToWidget(data.displayOptions, displayOptItemIndex);
  displayOptDataToWidget(data.displayOptionsRose, displayOptItemIndexRose);
  displayOptDataToWidget(data.displayOptionsMeasurement, displayOptItemIndexMeasurement);
  displayOptDataToWidget(data.displayOptionsRoute, displayOptItemIndexRoute);
  displayOptDataToWidget(data.displayOptionsNavAid, displayOptItemIndexNavAid);

  fromFlags(data, ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  fromFlags(data, ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  fromFlags(data, ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  fromFlags(data, ui->checkBoxOptionsStartupLoadTrail, opts::STARTUP_LOAD_TRAIL);
  fromFlags(data, ui->checkBoxOptionsStartupLoadInfoContent, opts::STARTUP_LOAD_INFO);
  fromFlags(data, ui->checkBoxOptionsStartupLoadSearch, opts::STARTUP_LOAD_SEARCH);
  fromFlags(data, ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  fromFlags(data, ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  fromFlags(data, ui->radioButtonOptionsStartupShowFlightplan, opts::STARTUP_SHOW_ROUTE);
  fromFlags(data, ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  fromFlags2(data, ui->checkBoxOptionsGuiProposeFilename, opts2::PROPOSE_FILENAME);
  fromFlags2(data, ui->checkBoxOptionsGuiRaiseWindows, opts2::RAISE_WINDOWS);
  fromFlags2(data, ui->checkBoxOptionsGuiRaiseDockWindows, opts2::RAISE_DOCK_WINDOWS);
  fromFlags2(data, ui->checkBoxOptionsGuiRaiseMainWindow, opts2::RAISE_MAIN_WINDOW);
  fromFlags2(data, ui->checkBoxOptionsUnitFuelOther, opts2::UNIT_FUEL_SHOW_OTHER);
  fromFlags2(data, ui->checkBoxOptionsUnitTrueCourse, opts2::UNIT_TRUE_COURSE);
  fromFlags(data, ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  fromFlags(data, ui->checkBoxOptionsGuiAvoidOverwrite, opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN);
  fromFlags(data, ui->checkBoxOptionsGuiOverrideLocale, opts::GUI_OVERRIDE_LOCALE);
  fromFlags(data, ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  fromFlags(data, ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_ALTITUDE_RULE);
  fromFlags(data, ui->checkBoxOptionsRouteDeclination, opts::ROUTE_IGNORE_VOR_DECLINATION);
  fromFlags(data, ui->checkBoxOptionsRouteExportUserWpt, opts::ROUTE_GARMIN_USER_WPT);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherInfoAsn, optsw::WEATHER_INFO_ACTIVESKY);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherInfoNoaa, optsw::WEATHER_INFO_NOAA);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherInfoVatsim, optsw::WEATHER_INFO_VATSIM);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherInfoIvao, optsw::WEATHER_INFO_IVAO);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherInfoFs, optsw::WEATHER_INFO_FS);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherTooltipAsn, optsw::WEATHER_TOOLTIP_ACTIVESKY);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherTooltipNoaa, optsw::WEATHER_TOOLTIP_NOAA);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherTooltipVatsim, optsw::WEATHER_TOOLTIP_VATSIM);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherTooltipIvao, optsw::WEATHER_TOOLTIP_IVAO);
  fromFlagsWeather(data, ui->checkBoxOptionsWeatherTooltipFs, optsw::WEATHER_TOOLTIP_FS);
  fromFlags(data, ui->checkBoxOptionsSimUpdatesConstant, opts::SIM_UPDATE_MAP_CONSTANTLY);

  fromFlags2(data, ui->checkBoxOptionsMapZoomAvoidBlurred, opts2::MAP_AVOID_BLURRED_MAP);
  fromFlags2(data, ui->checkBoxOptionsMapUndock, opts2::MAP_ALLOW_UNDOCK);
  fromFlags2(data, ui->checkBoxOptionsGuiHighDpi, opts2::HIGH_DPI_DISPLAY_SUPPORT);

  fromFlags(data, ui->radioButtonCacheUseOffineElevation, opts::CACHE_USE_OFFLINE_ELEVATION);
  fromFlags(data, ui->radioButtonCacheUseOnlineElevation, opts::CACHE_USE_ONLINE_ELEVATION);

  fromFlags2(data, ui->checkBoxOptionsMapEmptyAirports3D, opts2::MAP_EMPTY_AIRPORTS_3D);

  fromFlags2(data, ui->checkBoxOptionsMapAirportText, opts2::MAP_AIRPORT_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapNavaidText, opts2::MAP_NAVAID_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanText, opts2::MAP_ROUTE_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapAirportBoundary, opts2::MAP_AIRPORT_BOUNDARY);
  fromFlags2(data, ui->checkBoxOptionsMapAirportDiagram, opts2::MAP_AIRPORT_DIAGRAM);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanDimPassed, opts2::MAP_ROUTE_DIM_PASSED);
  fromFlags2(data, ui->checkBoxOptionsSimDoNotFollowOnScroll, opts2::ROUTE_NO_FOLLOW_ON_MOVE);
  fromFlags2(data, ui->checkBoxOptionsSimCenterLeg, opts2::ROUTE_AUTOZOOM);
  fromFlags2(data, ui->checkBoxOptionsSimCenterLegTable, opts2::ROUTE_CENTER_ACTIVE_LEG);
  fromFlags2(data, ui->checkBoxOptionsSimClearSelection, opts2::ROUTE_CLEAR_SELECTION);

  fromFlags2(data, ui->checkBoxDisplayOnlineNameLookup, opts2::ONLINE_AIRSPACE_BY_NAME);
  fromFlags2(data, ui->checkBoxDisplayOnlineFileLookup, opts2::ONLINE_AIRSPACE_BY_FILE);

  ui->lineEditOptionsRouteFilename->setText(data.flightplanPattern);
  ui->lineEditCacheOfflineDataPath->setText(data.cacheOfflineElevationPath);
  ui->lineEditCacheUserAirspacePath->setText(data.cacheUserAirspacePath);
  ui->lineEditCacheUserAirspaceExtensions->setText(data.cacheUserAirspaceExtensions);

  ui->checkBoxOptionsMapTooltipAirport->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRPORT));
  ui->checkBoxOptionsMapTooltipNavaid->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_NAVAID));
  ui->checkBoxOptionsMapTooltipAirspace->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRSPACE));
  ui->checkBoxOptionsMapTooltipWind->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_WIND));

  ui->checkBoxOptionsMapClickAirport->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRPORT));
  ui->checkBoxOptionsMapClickAirportProcs->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRPORT_PROC));
  ui->checkBoxOptionsMapClickNavaid->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_NAVAID));
  ui->checkBoxOptionsMapClickAirspace->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRSPACE));

  QString txt;
  for(int val : data.mapRangeRings)
  {
    if(val > 0)
    {
      if(!txt.isEmpty())
        txt += " ";
      txt += QString::number(val);
    }
  }
  ui->lineEditOptionsMapRangeRings->setText(txt);
  ui->lineEditOptionsWeatherXplanePath->setText(QDir::toNativeSeparators(data.weatherXplanePath));
  ui->lineEditOptionsWeatherAsnPath->setText(QDir::toNativeSeparators(data.weatherActiveSkyPath));
  ui->lineEditOptionsWeatherNoaaStationsUrl->setText(data.weatherNoaaUrl);
  ui->lineEditOptionsWeatherVatsimUrl->setText(data.weatherVatsimUrl);
  ui->lineEditOptionsWeatherIvaoUrl->setText(data.weatherIvaoUrl);

  ui->listWidgetOptionsDatabaseAddon->clear();
  for(const QString& str : data.databaseAddonExclude)
    ui->listWidgetOptionsDatabaseAddon->addItem(str);

  ui->listWidgetOptionsDatabaseExclude->clear();
  for(const QString& str : data.databaseExclude)
    ui->listWidgetOptionsDatabaseExclude->addItem(str);

  ui->comboBoxMapScrollDetails->setCurrentIndex(data.mapScrollDetail);

  // Details when moving map =========================================

  switch(data.simUpdateRate)
  {
    case opts::FAST:
      ui->radioButtonOptionsSimUpdateFast->setChecked(true);
      break;
    case opts::MEDIUM:
      ui->radioButtonOptionsSimUpdateMedium->setChecked(true);
      break;
    case opts::LOW:
      ui->radioButtonOptionsSimUpdateLow->setChecked(true);
      break;
  }

  // Map navigation mode =========================================
  switch(data.mapNavigation)
  {
    case opts::MAP_NAV_CLICK_DRAG_MOVE:
      ui->radioButtonOptionsMapNavDragMove->setChecked(true);
      break;

    case opts::MAP_NAV_CLICK_CENTER:
      ui->radioButtonOptionsMapNavClickCenter->setChecked(true);
      break;

    case opts::MAP_NAV_TOUCHSCREEN:
      ui->radioButtonOptionsMapNavTouchscreen->setChecked(true);
      break;
  }

  ui->spinBoxSimDoNotFollowOnScrollTime->setValue(data.simNoFollowAircraftOnScroll);
  ui->spinBoxOptionsSimUpdateBox->setValue(data.simUpdateBox);
  ui->spinBoxSimMaxTrackPoints->setValue(data.aircraftTrackMaxPoints);

  ui->spinBoxOptionsCacheDiskSize->setValue(data.cacheSizeDisk);
  ui->spinBoxOptionsCacheMemorySize->setValue(data.cacheSizeMemory);
  ui->spinBoxOptionsGuiInfoText->setValue(data.guiInfoTextSize);
  ui->spinBoxOptionsGuiAircraftPerf->setValue(data.guiPerfReportTextSize);
  ui->spinBoxOptionsGuiRouteText->setValue(data.guiRouteTableTextSize);
  ui->spinBoxOptionsGuiSearchText->setValue(data.guiSearchTableTextSize);
  ui->spinBoxOptionsGuiSimInfoText->setValue(data.guiInfoSimSize);

  ui->spinBoxOptionsGuiThemeMapDimming->setValue(data.guiStyleMapDimming);

  ui->spinBoxOptionsMapClickRect->setValue(data.mapClickSensitivity);
  ui->spinBoxOptionsMapTooltipRect->setValue(data.mapTooltipSensitivity);
  ui->doubleSpinBoxOptionsMapZoomShowMap->setValue(data.mapZoomShowClick);
  ui->doubleSpinBoxOptionsMapZoomShowMapMenu->setValue(data.mapZoomShowMenu);
  ui->spinBoxOptionsRouteGroundBuffer->setValue(data.routeGroundBuffer);

  ui->spinBoxOptionsDisplayTextSizeAircraftAi->setValue(data.displayTextSizeAircraftAi);
  ui->spinBoxOptionsDisplaySymbolSizeNavaid->setValue(data.displaySymbolSizeNavaid);
  ui->spinBoxOptionsDisplayTextSizeNavaid->setValue(data.displayTextSizeNavaid);
  ui->spinBoxOptionsDisplayThicknessFlightplan->setValue(data.displayThicknessFlightplan);
  ui->spinBoxOptionsDisplaySymbolSizeAirport->setValue(data.displaySymbolSizeAirport);
  ui->spinBoxOptionsDisplaySymbolSizeAirportWeather->setValue(data.displaySymbolSizeAirportWeather);
  ui->spinBoxOptionsDisplaySymbolSizeWindBarbs->setValue(data.displaySymbolSizeWindBarbs);
  ui->spinBoxOptionsDisplaySymbolSizeAircraftAi->setValue(data.displaySymbolSizeAircraftAi);
  ui->spinBoxOptionsDisplayTextSizeFlightplan->setValue(data.displayTextSizeFlightplan);
  ui->spinBoxOptionsDisplayTextSizeAircraftUser->setValue(data.displayTextSizeAircraftUser);
  ui->spinBoxOptionsDisplaySymbolSizeAircraftUser->setValue(data.displaySymbolSizeAircraftUser);
  ui->spinBoxOptionsDisplayTextSizeAirport->setValue(data.displayTextSizeAirport);
  ui->spinBoxOptionsDisplayThicknessTrail->setValue(data.displayThicknessTrail);
  ui->spinBoxOptionsDisplayThicknessRangeDistance->setValue(data.displayThicknessRangeDistance);
  ui->spinBoxOptionsDisplayThicknessCompassRose->setValue(data.displayThicknessCompassRose);
  ui->spinBoxOptionsDisplaySunShadeDarkness->setValue(data.displaySunShadingDimFactor);
  ui->comboBoxOptionsDisplayTrailType->setCurrentIndex(data.displayTrailType);

  ui->spinBoxOptionsDisplayTransparencyMora->setValue(data.displayTransparencyMora);
  ui->spinBoxOptionsDisplayTextSizeMora->setValue(data.displayTextSizeMora);
  ui->spinBoxOptionsMapNavTouchArea->setValue(data.mapNavTouchArea);

  ui->spinBoxOptionsDisplayTextSizeRangeDistance->setValue(data.displayTextSizeRangeDistance);
  ui->spinBoxOptionsDisplayTextSizeCompassRose->setValue(data.displayTextSizeCompassRose);

  ui->comboBoxOptionsStartupUpdateRate->setCurrentIndex(data.updateRate);
  ui->comboBoxOptionsStartupUpdateChannels->setCurrentIndex(data.updateChannels);

  ui->comboBoxOptionsUnitDistance->setCurrentIndex(data.unitDist);
  ui->comboBoxOptionsUnitShortDistance->setCurrentIndex(data.unitShortDist);
  ui->comboBoxOptionsUnitAlt->setCurrentIndex(data.unitAlt);
  ui->comboBoxOptionsUnitSpeed->setCurrentIndex(data.unitSpeed);
  ui->comboBoxOptionsUnitVertSpeed->setCurrentIndex(data.unitVertSpeed);
  ui->comboBoxOptionsUnitCoords->setCurrentIndex(data.unitCoords);
  ui->comboBoxOptionsUnitFuelWeight->setCurrentIndex(data.unitFuelWeight);
  ui->comboBoxOptionsRouteAltitudeRuleType->setCurrentIndex(data.altitudeRuleType);

  switch(data.onlineNetwork)
  {
    case opts::ONLINE_NONE:
      ui->radioButtonOptionsOnlineNone->setChecked(true);
      break;
    case opts::ONLINE_VATSIM:
      ui->radioButtonOptionsOnlineVatsim->setChecked(true);
      break;
    case opts::ONLINE_IVAO:
      ui->radioButtonOptionsOnlineIvao->setChecked(true);
      break;
    case opts::ONLINE_PILOTEDGE:
      ui->radioButtonOptionsOnlinePilotEdge->setChecked(true);
      break;
    case opts::ONLINE_CUSTOM_STATUS:
      ui->radioButtonOptionsOnlineCustomStatus->setChecked(true);
      break;
    case opts::ONLINE_CUSTOM:
      ui->radioButtonOptionsOnlineCustom->setChecked(true);
      break;
  }
  ui->lineEditOptionsOnlineStatusUrl->setText(data.onlineStatusUrl);
  ui->lineEditOptionsOnlineWhazzupUrl->setText(data.onlineWhazzupUrl);
  ui->spinBoxOptionsOnlineUpdate->setValue(data.onlineReloadSeconds);
  ui->comboBoxOptionsOnlineFormat->setCurrentIndex(data.onlineFormat);

  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineClearance, ui->checkBoxDisplayOnlineClearanceRange,
                             data.displayOnlineClearance);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineArea, ui->checkBoxDisplayOnlineAreaRange, data.displayOnlineArea);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineApproach, ui->checkBoxDisplayOnlineApproachRange,
                             data.displayOnlineApproach);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineDeparture, ui->checkBoxDisplayOnlineDepartureRange,
                             data.displayOnlineDeparture);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineFir, ui->checkBoxDisplayOnlineFirRange, data.displayOnlineFir);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineObserver, ui->checkBoxDisplayOnlineObserverRange,
                             data.displayOnlineObserver);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineGround, ui->checkBoxDisplayOnlineGroundRange,
                             data.displayOnlineGround);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineTower, ui->checkBoxDisplayOnlineTowerRange,
                             data.displayOnlineTower);

  ui->spinBoxOptionsWebPort->setValue(data.webPort);
  ui->checkBoxOptionsWebEncrypted->setChecked(data.webEncrypted);
  ui->lineEditOptionsWebDocroot->setText(QDir::toNativeSeparators(data.webDocumentRoot));

  ui->lineEditOptionsWeatherXplaneWind->setText(data.weatherXplaneWind);
  ui->lineEditOptionsWeatherNoaaWindUrl->setText(data.weatherNoaaWindBaseUrl);
}

void OptionsDialog::toFlagsWeather(QCheckBox *checkBox, optsw::FlagsWeather flag)
{
  if(checkBox->isChecked())
    OptionData::instanceInternal().flagsWeather |= flag;
  else
    OptionData::instanceInternal().flagsWeather &= ~flag;
}

void OptionsDialog::fromFlagsWeather(const OptionData& data, QCheckBox *checkBox, optsw::FlagsWeather flag)
{
  checkBox->setChecked(data.flagsWeather & flag);
}

/* Add flag from checkbox to OptionData flags */
void OptionsDialog::toFlags(QCheckBox *checkBox, opts::Flags flag)
{
  if(checkBox->isChecked())
    OptionData::instanceInternal().flags |= flag;
  else
    OptionData::instanceInternal().flags &= ~flag;
}

/* Add flag from radio button to OptionData flags */
void OptionsDialog::toFlags(QRadioButton *radioButton, opts::Flags flag)
{
  if(radioButton->isChecked())
    OptionData::instanceInternal().flags |= flag;
  else
    OptionData::instanceInternal().flags &= ~flag;
}

void OptionsDialog::fromFlags(const OptionData& data, QCheckBox *checkBox, opts::Flags flag)
{
  checkBox->setChecked(data.flags & flag);
}

void OptionsDialog::fromFlags(const OptionData& data, QRadioButton *radioButton, opts::Flags flag)
{
  radioButton->setChecked(data.flags & flag);
}

/* Add flag from checkbox to OptionData flags */
void OptionsDialog::toFlags2(QCheckBox *checkBox, opts2::Flags2 flag)
{
  if(checkBox->isChecked())
    OptionData::instanceInternal().flags2 |= flag;
  else
    OptionData::instanceInternal().flags2 &= ~flag;
}

/* Add flag from radio button to OptionData flags */
void OptionsDialog::toFlags2(QRadioButton *radioButton, opts2::Flags2 flag)
{
  if(radioButton->isChecked())
    OptionData::instanceInternal().flags2 |= flag;
  else
    OptionData::instanceInternal().flags2 &= ~flag;
}

void OptionsDialog::fromFlags2(const OptionData& data, QCheckBox *checkBox, opts2::Flags2 flag)
{
  checkBox->setChecked(data.flags2 & flag);
}

void OptionsDialog::fromFlags2(const OptionData& data, QRadioButton *radioButton, opts2::Flags2 flag)
{
  radioButton->setChecked(data.flags2 & flag);
}

void OptionsDialog::offlineDataSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).
                 openDirectoryDialog(tr("Open GLOBE data directory"),
                                     lnm::OPTIONS_DIALOG_GLOBE_FILE_DLG, ui->lineEditCacheOfflineDataPath->text());

  if(!path.isEmpty())
    ui->lineEditCacheOfflineDataPath->setText(QDir::toNativeSeparators(path));

  updateCacheElevationStates();
}

void OptionsDialog::userAirspacePathSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString defaultPath = ui->lineEditCacheUserAirspacePath->text();

  if(defaultPath.isEmpty())
    defaultPath = atools::documentsDir();

  QString path = atools::gui::Dialog(mainWindow).openDirectoryDialog(
    tr("Select Directory for User Airspaces"), lnm::DATABASE_USER_AIRSPACE_PATH, defaultPath);

  if(!path.isEmpty())
  {
    ui->lineEditCacheUserAirspacePath->setText(QDir::toNativeSeparators(path));
    OptionData::instanceInternal().cacheUserAirspacePath = ui->lineEditCacheUserAirspacePath->text();
  }

  updateCacheUserAirspaceStates();
}

void OptionsDialog::updateCacheUserAirspaceStates()
{
  const QString& path = ui->lineEditCacheUserAirspacePath->text();
  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelCacheUserAirspacePathStatus->setText(HtmlBuilder::errorMessage(tr("Directory does not exist.")));
    else if(!fileinfo.isDir())
      ui->labelCacheUserAirspacePathStatus->setText(HtmlBuilder::errorMessage(tr(("Is not a directory."))));
    else
      ui->labelCacheUserAirspacePathStatus->setText(tr("Directory is valid."));
  }
  else
    ui->labelCacheUserAirspacePathStatus->setText(tr("No directory selected."));
}

void OptionsDialog::updateCacheElevationStates()
{
  ui->lineEditCacheOfflineDataPath->setDisabled(ui->radioButtonCacheUseOnlineElevation->isChecked());
  ui->pushButtonCacheOfflineDataSelect->setDisabled(ui->radioButtonCacheUseOnlineElevation->isChecked());

  if(ui->radioButtonCacheUseOffineElevation->isChecked())
  {
    const QString& path = ui->lineEditCacheOfflineDataPath->text();
    if(!path.isEmpty())
    {
      QFileInfo fileinfo(path);
      if(!fileinfo.exists())
        ui->labelCacheGlobePathState->setText(HtmlBuilder::errorMessage(tr("Directory does not exist.")));
      else if(!fileinfo.isDir())
        ui->labelCacheGlobePathState->setText(HtmlBuilder::errorMessage(tr(("Is not a directory."))));
      else if(!NavApp::getElevationProvider()->isGlobeDirectoryValid(path))
        ui->labelCacheGlobePathState->setText(HtmlBuilder::errorMessage(tr("No valid GLOBE data found.")));
      else
        ui->labelCacheGlobePathState->setText(tr("Directory and files are valid."));
    }
    else
      ui->labelCacheGlobePathState->setText(tr("No directory selected."));
  }
  else
    ui->labelCacheGlobePathState->clear();
}

void OptionsDialog::updateWeatherButtonState()
{
  WeatherReporter *wr = NavApp::getWeatherReporter();

  if(wr != nullptr)
  {
    bool hasAs = wr->getCurrentActiveSkyType() != WeatherReporter::NONE;
    ui->checkBoxOptionsWeatherInfoAsn->setEnabled(hasAs);
    ui->checkBoxOptionsWeatherTooltipAsn->setEnabled(hasAs);

    ui->pushButtonOptionsWeatherNoaaTest->setEnabled(!ui->lineEditOptionsWeatherNoaaStationsUrl->text().isEmpty());
    ui->pushButtonOptionsWeatherVatsimTest->setEnabled(!ui->lineEditOptionsWeatherVatsimUrl->text().isEmpty());
    ui->pushButtonOptionsWeatherIvaoTest->setEnabled(!ui->lineEditOptionsWeatherIvaoUrl->text().isEmpty());
    ui->pushButtonOptionsWeatherNoaaWindTest->setEnabled(!ui->lineEditOptionsWeatherNoaaWindUrl->text().isEmpty());

    updateActiveSkyPathStatus();
    updateXplanePathStatus();
    updateXplaneWindStatus();
  }
}

/* Checks the path to the ASN weather file and its contents. Display an error message in the label */
void OptionsDialog::updateActiveSkyPathStatus()
{
  const QString& path = ui->lineEditOptionsWeatherAsnPath->text();

  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelOptionsWeatherAsnPathState->setText(HtmlBuilder::errorMessage(tr("File does not exist.")));
    else if(!fileinfo.isFile())
      ui->labelOptionsWeatherAsnPathState->setText(HtmlBuilder::errorMessage(tr("Is not a file.")));
    else if(!WeatherReporter::validateActiveSkyFile(path))
      ui->labelOptionsWeatherAsnPathState->setText(HtmlBuilder::errorMessage(
                                                     tr("Is not an Active Sky weather snapshot file.")));
    else
      ui->labelOptionsWeatherAsnPathState->setText(
        tr("Weather snapshot file is valid. Using this one for all simulators"));
  }
  else
  {
    // No manual path set
    QString text;
    WeatherReporter *wr = NavApp::getWeatherReporter();
    QString sim = atools::fs::FsPaths::typeToShortName(wr->getSimType());

    switch(wr->getCurrentActiveSkyType())
    {
      case WeatherReporter::NONE:
        text = tr("No Active Sky weather snapshot found. Active Sky METARs are not available.");
        break;
      case WeatherReporter::MANUAL:
        text = tr("Will use default weather snapshot after confirming change.");
        break;
      case WeatherReporter::ASN:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for Active Sky Next for %1.").arg(sim);
        break;
      case WeatherReporter::AS16:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for AS16 for %1.").arg(sim);
        break;

      case WeatherReporter::ASP4:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for ASP4 for %1.").arg(sim);
        break;

      case WeatherReporter::ASXPL:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for Active Sky XP for %1.").arg(sim);
        break;
    }

    ui->labelOptionsWeatherAsnPathState->setText(text);
  }
}

void OptionsDialog::updateXplanePathStatus()
{
  const QString& path = ui->lineEditOptionsWeatherXplanePath->text();

  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelOptionsWeatherXplanePathState->setText(HtmlBuilder::errorMessage(tr("File does not exist.")));
    else if(!fileinfo.isFile())
      ui->labelOptionsWeatherXplanePathState->setText(HtmlBuilder::errorMessage(tr("Is not a file.")));
    else
      ui->labelOptionsWeatherXplanePathState->setText(
        tr("Weather file is valid. Using this one for X-Plane."));
  }
  else
    // No manual path set
    ui->labelOptionsWeatherXplanePathState->setText(tr("Using default weather from X-Plane base path."));
}

/* Checks the path to the X-Plane wind GRIB file. Display an error message in the label */
void OptionsDialog::updateXplaneWindStatus()
{
  const QString& path = ui->lineEditOptionsWeatherXplaneWind->text();
  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelOptionsWeatherXplaneWindPathState->setText(HtmlBuilder::errorMessage(tr("File does not exist.")));
    else if(!fileinfo.isFile())
      ui->labelOptionsWeatherXplaneWindPathState->setText(HtmlBuilder::errorMessage(tr("Is not a file.")));
    else if(!atools::grib::GribReader::validateGribFile(path))
      ui->labelOptionsWeatherXplaneWindPathState->setText(HtmlBuilder::errorMessage(tr("Is not a X-Plane wind file.")));
    else
      ui->labelOptionsWeatherXplaneWindPathState->setText(tr("X-Plane wind file is valid."));
  }
  else
    ui->labelOptionsWeatherXplaneWindPathState->setText(tr("Using default X-Plane wind file."));
}

void OptionsDialog::selectActiveSkyPathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Open Active Sky Weather Snapshot File"),
    tr("Active Sky Weather Snapshot Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AS_SNAPSHOT),
    lnm::OPTIONS_DIALOG_AS_FILE_DLG, ui->lineEditOptionsWeatherAsnPath->text());

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherAsnPath->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::selectXplanePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Open X-Plane METAR File"),
    tr("X-Plane METAR Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_XPLANE_METAR),
    lnm::OPTIONS_DIALOG_XPLANE_DLG, ui->lineEditOptionsWeatherXplanePath->text());

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherXplanePath->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::weatherXplaneWindPathSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  // global_winds.grib
  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Open X-Plane Wind File"),
    tr("X-Plane Wind Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GRIB),
    lnm::OPTIONS_DIALOG_XPLANE_WIND_FILE_DLG, ui->lineEditOptionsWeatherXplaneWind->text());

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherXplaneWind->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::updateNavOptions()
{
  ui->spinBoxOptionsMapNavTouchArea->setEnabled(ui->radioButtonOptionsMapNavTouchscreen->isChecked());
}

void OptionsDialog::clearMemCachedClicked()
{
  qDebug() << Q_FUNC_INFO;

  NavApp::getMapWidget()->clearVolatileTileCache();
  NavApp::setStatusMessage(tr("Memory cache cleared."));
}

void OptionsDialog::clearDiskCachedClicked()
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton result =
    QMessageBox::question(this, QApplication::applicationName(),
                          tr("Clear the disk cache?\n"
                             "All files in the directory \"%1\" will be deleted.\n"
                             "This process will run in background and can take a while.").
                          arg(Marble::MarbleDirs::localPath()),
                          QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

  if(result == QMessageBox::Yes)
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    NavApp::getMapWidget()->model()->clearPersistentTileCache();
    NavApp::setStatusMessage(tr("Disk cache cleared."));
    QGuiApplication::restoreOverrideCursor();
  }
}

/* Opens the disk cache in explorer, finder, whatever */
void OptionsDialog::showDiskCacheClicked()
{
  const QString& localPath = Marble::MarbleDirs::localPath();

  QUrl url = QUrl::fromLocalFile(localPath);

  if(!QDesktopServices::openUrl(url))
    atools::gui::Dialog::warning(this, tr("Error opening help URL \"%1\"").arg(url.toDisplayString()));
}

QListWidgetItem *OptionsDialog::pageListItem(QListWidget *parent, const QString& text,
                                             const QString& tooltip, const QString& iconPath)
{
  QListWidgetItem *item = new QListWidgetItem(text, parent);
  if(!tooltip.isEmpty())
    item->setToolTip(tooltip);
  if(!iconPath.isEmpty())
    item->setIcon(QIcon(iconPath));
  return item;
}

void OptionsDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
  if(!current)
    current = previous;
  ui->stackedWidgetOptions->setCurrentIndex(ui->listWidgetOptionPages->row(current));
}

void OptionsDialog::updateWebServerStatus()
{
  WebController *webController = NavApp::getWebController();
  if(webController != nullptr)
  {
    if(webController->isRunning())
    {
      QStringList urls = webController->getUrlStr();

      if(urls.size() > 1)
        ui->labelOptionsWebStatus->setText(tr("Web Server is running at<ul><li>%1</li></ul>").
                                           arg(webController->getUrlStr().join("</li><li>")));
      else
        ui->labelOptionsWebStatus->setText(tr("Web Server is running at %1").arg(urls.join(tr(", "))));

      ui->pushButtonOptionsWebStart->setText(tr("&Stop Web Server"));
    }
    else
    {
      ui->labelOptionsWebStatus->setText(tr("Web Server is not running."));
      ui->pushButtonOptionsWebStart->setText(tr("&Start Web Server"));
    }
  }
}

void OptionsDialog::updateWebDocrootStatus()
{
  const QString& path = ui->lineEditOptionsWebDocroot->text();

  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelOptionWebDocrootStatus->setText(HtmlBuilder::errorMessage(tr("Error: Directory does not exist.")));
    else if(!fileinfo.isDir())
      ui->labelOptionWebDocrootStatus->setText(HtmlBuilder::errorMessage(tr("Error: Is not a directory.")));
    else if(!QFileInfo(path + QDir::separator() + "index.html").exists())
      ui->labelOptionWebDocrootStatus->setText(HtmlBuilder::warningMessage(tr("Warning: No file \"index.html\" found.")));
    else
      ui->labelOptionWebDocrootStatus->setText(tr("Document root is valid."));
  }
  else
  {
    // Use default path
    WebController *webController = NavApp::getWebController();
    if(webController != nullptr)
      ui->labelOptionWebDocrootStatus->setText(tr("Using default document root \"%1\".").arg(
                                                 webController->getAbsoluteWebrootFilePath()));
    else
      // Might happen only at startup
      ui->labelOptionWebDocrootStatus->setText(tr("Not initialized."));
  }
}

void OptionsDialog::selectWebDocrootClicked()
{
  qDebug() << Q_FUNC_INFO;
  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Document Root Directory"), lnm::OPTIONS_DIALOG_WEB_DOCROOT_DLG, ui->lineEditOptionsWebDocroot->text());

  if(!path.isEmpty())
    ui->lineEditOptionsWebDocroot->setText(QDir::toNativeSeparators(path));

  updateWebDocrootStatus();
  updateWebServerStatus();
}

void OptionsDialog::startStopWebServerClicked()
{
  qDebug() << Q_FUNC_INFO;

  WebController *webController = NavApp::getWebController();
  if(webController != nullptr)
  {
    if(webController->isRunning())
      webController->stopServer();
    else
      // Update options from page before starting
      updateWebOptionsFromGui();

    updateWebServerStatus();
  }
}

void OptionsDialog::updateWebOptionsFromData()
{
  WebController *webController = NavApp::getWebController();
  if(webController != nullptr)
  {
    OptionData& data = OptionData::instanceInternal();

    webController->setDocumentRoot(QDir::fromNativeSeparators(QFileInfo(data.getWebDocumentRoot()).canonicalFilePath()));
    webController->setPort(data.getWebPort());
    webController->setEncrypted(data.isWebEncrypted());
    webController->restartServer(false /* force */);
  }
}

void OptionsDialog::updateWebOptionsFromGui()
{
  WebController *webController = NavApp::getWebController();
  if(webController != nullptr)
  {
    webController->setDocumentRoot(QDir::fromNativeSeparators(QFileInfo(ui->lineEditOptionsWebDocroot->text()).
                                                              canonicalFilePath()));
    webController->setPort(ui->spinBoxOptionsWebPort->value());
    webController->setEncrypted(ui->checkBoxOptionsWebEncrypted->isChecked());
    webController->restartServer(true /* force */);
  }
}

void OptionsDialog::mapClickAirportProcsToggled()
{
  ui->checkBoxOptionsMapClickAirportProcs->setEnabled(ui->checkBoxOptionsMapClickAirport->isChecked());
}

void OptionsDialog::flightplanPatterShortClicked()
{
  ui->lineEditOptionsRouteFilename->setText(atools::fs::pln::pattern::SHORT);
  updateFlightplanExample();
}

void OptionsDialog::flightplanPatterLongClicked()
{
  ui->lineEditOptionsRouteFilename->setText(atools::fs::pln::pattern::LONG);
  updateFlightplanExample();
}

void OptionsDialog::updateFlightplanExample()
{
  if(!ui->lineEditOptionsRouteFilename->text().isEmpty())
  {
    QString example = atools::fs::pln::Flightplan::getFilenamePattern(ui->lineEditOptionsRouteFilename->text(),
                                                                      "IFR", "Frankfurt Am Main", "EDDF",
                                                                      "Fiumicino", "LIRF", ".lnmpln", 30000, false);

    QString text = tr("Example: \"%1\"").arg(atools::cleanFilename(example));

    // Check if the cleaned filename differs from user input
    if(example != atools::cleanFilename(example))
      text.append(tr("<br/>") +
                  atools::util::HtmlBuilder::errorMessage({tr("Pattern contains invalid characters or double spaces."),
                                                           tr("Not allowed are:  "
                                                              "\\  /  :  \'  *  &amp;  &gt;  &lt;  ?  $")}));

    ui->labelOptionsRouteFilenameExample->setText(text);
  }
  else
    ui->labelOptionsRouteFilenameExample->setText(
      atools::util::HtmlBuilder::warningMessage(tr("Pattern is empty. Using default \"%1\".").
                                                arg(atools::fs::pln::pattern::SHORT)));
}

void OptionsDialog::updateMapFontLabel()
{
  QFont font;
  if(!mapFont.isEmpty())
    // Use set font
    font.fromString(mapFont);
  else if(!guiFont.isEmpty())
    // Fall back to GUI font
    font.fromString(guiFont);
  else
    // Fall back to system font
    font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);

  atools::gui::fontDescription(font, ui->labelOptionsDisplayFont);
}

void OptionsDialog::updateGuiFontLabel()
{
  ui->labelOptionsGuiFont->setText(atools::gui::fontDescription(QApplication::font()));
}

void OptionsDialog::resetMapFontClicked()
{
  // Set to GUI font - no matter if this defaults to system or not
  mapFont = guiFont;
  updateMapFontLabel();
}

void OptionsDialog::buildFontDialog()
{
  if(fontDialog == nullptr)
  {
    fontDialog = new QFontDialog(this);
    fontDialog->setWindowTitle(tr("%1 - Select font").arg(QApplication::applicationName()));
  }
}

void OptionsDialog::selectMapFontClicked()
{
  QFont font;
  if(!mapFont.isEmpty())
    // Use set font
    font.fromString(mapFont);
  else if(!guiFont.isEmpty())
    // Fall back to GUI font
    font.fromString(guiFont);
  else
    // Fall back to system font
    font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);

  buildFontDialog();
  fontDialog->setCurrentFont(font);
  if(fontDialog->exec())
  {
    mapFont = fontDialog->selectedFont().toString();
    updateMapFontLabel();
  }
}

void OptionsDialog::resetGuiFontClicked()
{
  // Empty description means system font
  guiFont.clear();

  // Set GUI back to system font
  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  qDebug() << Q_FUNC_INFO << font;
  QApplication::setFont(font);

  updateGuiFontLabel();
}

void OptionsDialog::selectGuiFontClicked()
{
  QFont font;
  if(guiFont.isEmpty())
    // Empty description means system font
    font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  else
    font.fromString(guiFont);

  buildFontDialog();
  fontDialog->setCurrentFont(font);
  if(fontDialog->exec())
  {
    QFont font = fontDialog->selectedFont();

    // Limit size to keep the user from messing up the UI without an option to change
    if(font.pointSizeF() > 30.)
      font.setPointSizeF(30.);
    if(font.pixelSize() > 30)
      font.setPixelSize(30);

    guiFont = font.toString();
    qDebug() << Q_FUNC_INFO << guiFont;

    // the user clicked OK and font is set to the font the user selected
    if(QApplication::font() != font)
      QApplication::setFont(font);
    updateGuiFontLabel();
  }
}

void OptionsDialog::updateFontFromData()
{
  OptionData& data = OptionData::instanceInternal();
  QFont font;

  if(data.guiFont.isEmpty())
    // Empty description means system font
    font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  else
    font.fromString(data.guiFont);

  if(QApplication::font() != font)
    QApplication::setFont(font);
}
