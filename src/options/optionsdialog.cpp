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

const int MIN_ONLINE_UPDATE = 120;

using atools::settings::Settings;
using atools::gui::HelpHandler;
using atools::util::HtmlBuilder;

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

  // Add option pages with text, icon and tooltip ========================================
  /* *INDENT-OFF* */
  QListWidget*list = ui->listWidgetOptionPages;
  list->addItem(pageListItem(list, tr("Startup and Updates"), tr("Select what should be reloaded on startup and change update settings."), ":/littlenavmap/resources/icons/littlenavmap.svg"));
  list->addItem(pageListItem(list, tr("User Interface"), tr("Change language settings, window options and other user interface behavior."), ":/littlenavmap/resources/icons/statusbar.svg"));
  list->addItem(pageListItem(list, tr("Display and Text"), tr("Change text sizes, user interface font and other display options."), ":/littlenavmap/resources/icons/copy.svg"));
  list->addItem(pageListItem(list, tr("Map"), tr("General map settings: Zoom, click and tooltip settings."), ":/littlenavmap/resources/icons/mapsettings.svg"));
  list->addItem(pageListItem(list, tr("Map Navigation"), tr("Zoom, click and screen navigation settings."), ":/littlenavmap/resources/icons/mapnavigation.svg"));
  list->addItem(pageListItem(list, tr("Map Display"), tr("Change colors, symbols, texts and font for map display objects."), ":/littlenavmap/resources/icons/mapdisplay.svg"));
  list->addItem(pageListItem(list, tr("Map Display Flight Plan"), tr("Adjust display style and colors for the flight plan on the map and the elevation profile."), ":/littlenavmap/resources/icons/mapdisplayflightplan.svg"));
  list->addItem(pageListItem(list, tr("Map Display User"), tr("Change colors, symbols and texts for marks, measurement lines and other user features."), ":/littlenavmap/resources/icons/mapdisplay2.svg"));
  list->addItem(pageListItem(list, tr("Map Display Labels"), tr("Change label options for marks, user aircraft and more."), ":/littlenavmap/resources/icons/mapdisplaylabels.svg"));
  list->addItem(pageListItem(list, tr("Map Display Online"), tr("Map display online center options."), ":/littlenavmap/resources/icons/airspaceonline.svg"));
  list->addItem(pageListItem(list, tr("Units"), tr("Fuel, distance, speed and coordindate units as well as\noptions for course and heading display."), ":/littlenavmap/resources/icons/units.svg"));
  list->addItem(pageListItem(list, tr("Simulator Aircraft"), tr("Update and movement options for the user aircraft and trail."), ":/littlenavmap/resources/icons/aircraft.svg"));
  list->addItem(pageListItem(list, tr("Flight Plan"), tr("Options for flight plan calculation, saving and loading."), ":/littlenavmap/resources/icons/route.svg"));
  list->addItem(pageListItem(list, tr("Weather"), tr("Change weather sources for information and tooltips."), ":/littlenavmap/resources/icons/weather.svg"));
  list->addItem(pageListItem(list, tr("Weather Files"), tr("Change web download addresses or file paths of weather sources."), ":/littlenavmap/resources/icons/weatherurl.svg"));
  list->addItem(pageListItem(list, tr("Online Flying"), tr("Select online flying services like VATSIM, IVAO or custom."), ":/littlenavmap/resources/icons/aircraft_online.svg"));
  list->addItem(pageListItem(list, tr("Web Server"), tr("Change settings for the internal web server."), ":/littlenavmap/resources/icons/web.svg"));
  list->addItem(pageListItem(list, tr("Cache and Files"), tr("Change map cache, select elevation data source and the path for user airspaces."), ":/littlenavmap/resources/icons/filesave.svg"));
  list->addItem(pageListItem(list, tr("Scenery Library Database"), tr("Exclude scenery files from loading and\nadd-on recognition."), ":/littlenavmap/resources/icons/database.svg"));
  /* *INDENT-ON* */

  // Build tree settings to map tab =====================================================
  /* *INDENT-OFF* */
  QTreeWidgetItem *topOfMap = addTopItem(tr("Top of Map"), tr("Select information that is displayed on top of the map."));
  addItem<optsac::DisplayOptionsUserAircraft>(topOfMap, displayOptItemIndexUser, tr("Wind Direction and Speed"), tr("Show wind direction and speed on the top center of the map."), optsac::ITEM_USER_AIRCRAFT_WIND, true);
  addItem<optsac::DisplayOptionsUserAircraft>(topOfMap, displayOptItemIndexUser, tr("Wind Pointer"), tr("Show wind direction pointer on the top center of the map."), optsac::ITEM_USER_AIRCRAFT_WIND_POINTER, true);

  QTreeWidgetItem *navAids = addTopItem(tr("Map Navigation Aids"), QString());
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Center Cross"), tr("Shows the map center. Useful if \"Click map to center position\"\n"
                                                                                                   "on page \"Map Navigation\" is enabled."), optsd::NAVAIDS_CENTER_CROSS);
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Screen Area"), tr("Highlight click- or touchable areas on screen.\nOnly shown if \"Use map areas\"\n"
                                                                                                  "on page \"Map Navigation\" is enabled as well."), optsd::NAVAIDS_TOUCHSCREEN_REGIONS);
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Screen Areas Marks"), tr("Shows corner marks to highlight the screen areas.\n"
                                                                                                         "Helps if map areas are used for touchscreen navigation.\n"
                                                                                                         "Only shown if \"Use map areas\" on page \"Map Navigation\" is enabled as well."), optsd::NAVAIDS_TOUCHSCREEN_AREAS);
  addItem<optsd::DisplayOptionsNavAid>(navAids , displayOptItemIndexNavAid, tr("Screen Area Icons"), tr("Shows icons for the screen areas.\n"
                                                                                                        "Useful if map areas are used for touchscreen navigation.\n"
                                                                                                        "Only shown if \"Use map areas\" on page \"Map Navigation\" is enabled as well."), optsd::NAVAIDS_TOUCHSCREEN_ICONS);

  QTreeWidgetItem *airport = addTopItem(tr("Airport"), tr("Select airport labels to display on the map."));
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("Name (Ident)"), tr("Airport name and ident in brackets depending on zoom factor.\n"
                                                                                                    "Ident can be internal, ICAO, FAA, IATA or local depending on avilability."), optsd::ITEM_AIRPORT_NAME, true);
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("Tower Frequency"), QString(), optsd::ITEM_AIRPORT_TOWER, true);
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("ATIS / ASOS / AWOS Frequency"), QString(), optsd::ITEM_AIRPORT_ATIS, true);
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("Runway Information"), tr("Show runway length, width and light indicator text."), optsd::ITEM_AIRPORT_RUNWAY, true);
  // addItem(ap, tr("Wind Pointer"), optsd::ITEM_AIRPORT_WIND_POINTER);

  QTreeWidgetItem *airportDetails = addTopItem(tr("Airport Details"), tr("Select airport diagram elements."));
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Runways"), tr("Show runways."), optsd::ITEM_AIRPORT_DETAIL_RUNWAY, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Taxiways"), tr("Show taxiway lines and background."), optsd::ITEM_AIRPORT_DETAIL_TAXI, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Aprons"), tr("Display aprons."), optsd::ITEM_AIRPORT_DETAIL_APRON, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Parking"), tr("Show fuel, tower, helipads, gates and ramp parking."), optsd::ITEM_AIRPORT_DETAIL_PARKING, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Boundary"), tr("Display a white boundary around and below the airport diagram."), optsd::ITEM_AIRPORT_DETAIL_BOUNDARY);


  QTreeWidgetItem *route = addTopItem(tr("Flight Plan"), tr("Select display options for the flight plan line."));
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Distance"), tr("Show distance along flight plan leg."), optsd::ROUTE_DISTANCE, true);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Magnetic great circle course"), tr("Show magnetic great circle start course at flight plan leg."), optsd::ROUTE_MAG_COURSE_GC, true);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("True great circle course"), tr("Show true great circle start course at flight plan leg."), optsd::ROUTE_TRUE_COURSE_GC);

  QTreeWidgetItem *userAircraft = addTopItem(tr("User Aircraft"), tr("Select text labels and other options for the user aircraft."));
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Registration"), QString(), optsac::ITEM_USER_AIRCRAFT_REGISTRATION);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Type"), tr("Show the aircraft type, like B738, B350 or M20T."), optsac::ITEM_USER_AIRCRAFT_TYPE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Airline"), QString(), optsac::ITEM_USER_AIRCRAFT_AIRLINE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Flight Number"), QString(), optsac::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Transponder Code"), tr("Transponder code prefixed with \"XPDR\" on the map"), optsac::ITEM_USER_AIRCRAFT_TRANSPONDER_CODE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Indicated Airspeed"), tr("Value prefixed with \"IAS\" on the map"), optsac::ITEM_USER_AIRCRAFT_IAS);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Ground Speed"), tr("Value prefixed with \"GS\" on the map"), optsac::ITEM_USER_AIRCRAFT_GS, true);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("True Airspeed"), tr("Value prefixed with \"TAS\" on the map"), optsac::ITEM_USER_AIRCRAFT_TAS);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Climb- and Sinkrate"), QString(), optsac::ITEM_USER_AIRCRAFT_CLIMB_SINK);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Heading"), tr("Aircraft heading prefixed with \"HDG\" on the map"), optsac::ITEM_USER_AIRCRAFT_HEADING);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Actual Altitude"), tr("Real aircraft altitude prefixed with \"ALT\" on the map"), optsac::ITEM_USER_AIRCRAFT_ALTITUDE, false);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Indicated Altitude"), tr("Indicated aircraft altitude prefixed with \"IND\" on the map"), optsac::ITEM_USER_AIRCRAFT_INDICATED_ALTITUDE, true);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Track Line"), tr("Show the aircraft track as a black needle protruding from the aircraft nose."), optsac::ITEM_USER_AIRCRAFT_TRACK_LINE, true);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Coordinates"), tr("Show aircraft coordinates using the format selected on options page \"Units\"."), optsac::ITEM_USER_AIRCRAFT_COORDINATES, false);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Icing"), tr("Show a red label \"ICE\" and icing values in percent when aircraft icing occurs."), optsac::ITEM_USER_AIRCRAFT_ICE, false);

  QTreeWidgetItem *aiAircraft = addTopItem(tr("AI, Multiplayer and Online Client Aircraft"), tr("Select text labels for the AI, multiplayer and online client aircraft."));
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Registration, Number or Callsign"), QString(), optsac::ITEM_AI_AIRCRAFT_REGISTRATION, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Type"), QString(), optsac::ITEM_AI_AIRCRAFT_TYPE, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Airline"), QString(), optsac::ITEM_AI_AIRCRAFT_AIRLINE, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Flight Number"), QString(), optsac::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER, false);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Transponder Code"), tr("Transponder code prefixed with \"XPDR\" on the map"), optsac::ITEM_AI_AIRCRAFT_TRANSPONDER_CODE);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Indicated Airspeed"), QString(), optsac::ITEM_AI_AIRCRAFT_IAS, false);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Ground Speed"), QString(), optsac::ITEM_AI_AIRCRAFT_GS, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("True Airspeed"), QString(), optsac::ITEM_AI_AIRCRAFT_TAS, false);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Climb- and Sinkrate"), QString(), optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Heading"), QString(), optsac::ITEM_AI_AIRCRAFT_HEADING);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Altitude"), QString(), optsac::ITEM_AI_AIRCRAFT_ALTITUDE, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Departure and Destination"), QString(), optsac::ITEM_AI_AIRCRAFT_DEP_DEST, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Coordinates"), tr("Show aircraft coordinates using the format selected on options page \"Units\"."), optsac::ITEM_AI_AIRCRAFT_COORDINATES, false);

  QTreeWidgetItem *compassRose = addTopItem(tr("Compass Rose"), tr("Select display options for the compass rose."));
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Direction Labels"), tr("Show N, S, E and W labels."), optsd::ROSE_DIR_LABLES, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Degree Tick Marks"), tr("Show tick marks for degrees on ring."), optsd::ROSE_DEGREE_MARKS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Degree Labels"), tr("Show degree labels on ring."), optsd::ROSE_DEGREE_LABELS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Range Rings"), tr("Show range rings and distance labels inside."), optsd::ROSE_RANGE_RINGS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Heading Line"), tr("Show the dashed heading line for user aircraft."), optsd::ROSE_HEADING_LINE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Track Line"), tr("Show the solid track line for user aircraft."), optsd::ROSE_TRACK_LINE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Track Label"), tr("Show track label for user aircraft."), optsd::ROSE_TRACK_LABEL, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Heading Indicator"), tr("Show the heading for the user aircraft as a small magenta circle."), optsd::ROSE_CRAB_ANGLE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Course to Next Waypoint"), tr("Show the course to next waypoint for the user aircraft as a small magenta line."), optsd::ROSE_NEXT_WAYPOINT, true);

  QTreeWidgetItem *measurement = addTopItem(tr("Measurement Lines"), tr("Select display options measurement lines."));
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Distance"), tr("Great circle distance for measurement line."), optsd::MEASUREMNENT_DIST, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Magnetic Course"), tr("Show magnetic course for start and end of line."), optsd::MEASUREMNENT_MAG, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("True Course"), tr("Show true course for start and end of line."), optsd::MEASUREMNENT_TRUE, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Navaid or airport ident"), tr("Show ident if attached to navaid or airport.\n"
                                                                                                                           "Also show frequency if attached to a radio navaid. "), optsd::MEASUREMNENT_LABEL, true);
  /* *INDENT-ON* */

  ui->treeWidgetOptionsDisplayTextOptions->resizeColumnToContents(0);

  // Create widget list for state saver
  // This will take over the actual saving of the settings
  widgets.append(
    {ui->listWidgetOptionPages,
     ui->splitterOptions,
     ui->checkBoxOptionsGuiCenterKml,
     ui->checkBoxOptionsGuiRaiseWindows,
     ui->checkBoxOptionsGuiRaiseDockWindows,
     ui->checkBoxOptionsGuiRaiseMainWindow,
     ui->checkBoxOptionsGuiCenterRoute,
     ui->checkBoxOptionsGuiAvoidOverwrite,
     ui->checkBoxOptionsGuiOverrideLocale,
     ui->checkBoxOptionsGuiHighDpi,
     ui->checkBoxOptionsGuiTooltips,
     ui->checkBoxOptionsGuiToolbarSize,
     // ui->comboBoxOptionsGuiLanguage, saved directly

     ui->checkBoxDisplayOnlineNameLookup,
     ui->checkBoxDisplayOnlineFileLookup,
     ui->checkBoxOptionsMapEmptyAirports,
     ui->checkBoxOptionsMapEmptyAirports3D,
     ui->checkBoxOptionsMapTooltipVerbose,
     ui->checkBoxOptionsMapTooltipUserAircraft,
     ui->checkBoxOptionsMapTooltipAiAircraft,
     ui->checkBoxOptionsMapTooltipAirport,
     ui->checkBoxOptionsMapTooltipNavaid,
     ui->checkBoxOptionsMapTooltipAirspace,
     ui->checkBoxOptionsMapTooltipWind,
     ui->checkBoxOptionsMapTooltipUser,
     ui->checkBoxOptionsMapClickUserAircraft,
     ui->checkBoxOptionsMapClickAiAircraft,
     ui->checkBoxOptionsMapClickAirport,
     ui->checkBoxOptionsMapClickAirportProcs,
     ui->checkBoxOptionsMapClickNavaid,
     ui->checkBoxOptionsMapClickAirspace,
     ui->checkBoxOptionsMapUndock,
     ui->checkBoxOptionsRouteDeclination,
     ui->checkBoxOptionsRouteEastWestRule,
     ui->comboBoxOptionsRouteAltitudeRuleType,
     ui->checkBoxOptionsStartupLoadKml,
     ui->checkBoxOptionsStartupLoadMapSettings,
     ui->checkBoxOptionsStartupLoadRoute,
     ui->checkBoxOptionsStartupLoadPerf,
     ui->checkBoxOptionsStartupLoadLayout,
     ui->checkBoxOptionsStartupShowSplash,
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
     // ui->lineEditOptionsMapRangeRings, // Saved separately lnm::OPTIONS_DIALOG_RANGE_DISTANCES
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
     ui->spinBoxOptionsSimCenterLegZoom,
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
     ui->spinBoxOptionsGuiToolbarSize,
     ui->spinBoxOptionsMapClickRect,
     ui->spinBoxOptionsMapTooltipRect,
     ui->doubleSpinBoxOptionsMapZoomShowMap,
     ui->doubleSpinBoxOptionsMapZoomShowMapMenu,
     ui->spinBoxOptionsRouteGroundBuffer,

     ui->spinBoxOptionsDisplayTextSizeAircraftAi,
     ui->spinBoxOptionsDisplaySymbolSizeNavaid,
     ui->spinBoxOptionsDisplaySymbolSizeUserpoint,
     ui->spinBoxOptionsDisplayTextSizeNavaid,
     ui->spinBoxOptionsDisplayTextSizeUserpoint,
     ui->spinBoxOptionsDisplayThicknessFlightplan,
     ui->spinBoxOptionsDisplayThicknessFlightplanProfile,
     ui->spinBoxOptionsDisplaySymbolSizeAirport,
     ui->spinBoxOptionsDisplaySymbolSizeAirportWeather,
     ui->spinBoxOptionsDisplaySymbolSizeWindBarbs,
     ui->spinBoxOptionsDisplaySymbolSizeAircraftAi,
     ui->spinBoxOptionsDisplayTextSizeFlightplan,
     ui->spinBoxOptionsDisplayTextSizeFlightplanProfile,
     ui->spinBoxOptionsDisplayTransparencyFlightplan,
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

     ui->checkBoxOptionsMapHighlightTransparent,
     ui->spinBoxOptionsMapHighlightTransparent,

     ui->checkBoxOptionsMapAirwayText,
     ui->checkBoxOptionsMapUserAircraftText,
     ui->checkBoxOptionsMapAiAircraftText,
     ui->spinBoxOptionsDisplayTextSizeAirway,
     ui->spinBoxOptionsDisplayThicknessAirway,

     ui->spinBoxOptionsDisplayTextSizeMora,
     ui->spinBoxOptionsDisplayTransparencyMora,

     ui->spinBoxOptionsDisplayTextSizeAirportMsa,
     ui->spinBoxOptionsDisplayTransparencyAirportMsa,

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
     ui->checkBoxOptionsMapAirportAddon,
     ui->checkBoxOptionsMapNavaidText,
     ui->checkBoxOptionsMapUserpointText,
     ui->checkBoxOptionsMapFlightplanText,
     ui->checkBoxOptionsMapFlightplanDimPassed,
     ui->checkBoxOptionsMapFlightplanTransparent,
     ui->checkBoxOptionsSimCenterLeg,
     ui->checkBoxOptionsSimCenterLegTable,
     ui->checkBoxOptionsSimClearSelection,

     ui->checkBoxOptionsSimDoNotFollowScroll,
     ui->spinBoxOptionsSimDoNotFollowScrollTime,
     ui->checkBoxOptionsSimZoomOnLanding,
     ui->doubleSpinBoxOptionsSimZoomOnLanding,
     ui->checkBoxOptionsSimCenterLegTable,
     ui->spinBoxOptionsSimCleanupTableTime,
     ui->checkBoxOptionsSimClearSelection,

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

  connect(ui->listWidgetOptionPages, &QListWidget::currentItemChanged, this, &OptionsDialog::changePage);

  connect(ui->buttonBoxOptions, &QDialogButtonBox::clicked, this, &OptionsDialog::buttonBoxClicked);

  // ===========================================================================
  // GUI language options
  connect(ui->comboBoxOptionsGuiLanguage, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &OptionsDialog::languageChanged);
  connect(ui->pushButtonOptionsGuiSelectFont, &QPushButton::clicked, this, &OptionsDialog::selectGuiFontClicked);
  connect(ui->pushButtonOptionsGuiResetFont, &QPushButton::clicked, this, &OptionsDialog::resetGuiFontClicked);
  connect(ui->checkBoxOptionsGuiToolbarSize, &QPushButton::clicked, this, &OptionsDialog::toolbarSizeClicked);

  // ===========================================================================
  // Weather widgets - ASN
  connect(ui->pushButtonOptionsWeatherAsnPathSelect, &QPushButton::clicked, this, &OptionsDialog::selectActiveSkyPathClicked);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::textEdited, this, &OptionsDialog::updateActiveSkyPathStatus);

  // Weather widgets - X-Plane METAR
  connect(ui->pushButtonOptionsWeatherXplanePathSelect, &QPushButton::clicked, this, &OptionsDialog::selectXplanePathClicked);
  connect(ui->lineEditOptionsWeatherXplanePath, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplanePath, &QLineEdit::textEdited, this, &OptionsDialog::updateXplanePathStatus);

  // ===========================================================================
  // Online weather
  connect(ui->lineEditOptionsWeatherNoaaStationsUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherVatsimUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherIvaoUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherNoaaWindUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);

  connect(ui->lineEditOptionsWeatherXplaneWind, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplaneWind, &QLineEdit::textEdited, this, &OptionsDialog::updateXplaneWindStatus);

  connect(ui->pushButtonOptionsWeatherXplaneWindPathSelect, &QPushButton::clicked, this,
          &OptionsDialog::weatherXplaneWindPathSelectClicked);

  // ===========================================================================
  // Weather test buttons
  connect(ui->pushButtonOptionsWeatherNoaaTest, &QPushButton::clicked, this, &OptionsDialog::testWeatherNoaaUrlClicked);
  connect(ui->pushButtonOptionsWeatherVatsimTest, &QPushButton::clicked, this, &OptionsDialog::testWeatherVatsimUrlClicked);
  connect(ui->pushButtonOptionsWeatherIvaoTest, &QPushButton::clicked, this, &OptionsDialog::testWeatherIvaoUrlClicked);
  connect(ui->pushButtonOptionsWeatherNoaaWindTest, &QPushButton::clicked, this, &OptionsDialog::testWeatherNoaaWindUrlClicked);

  // ===========================================================================
  // Weather reset buttons
  connect(ui->pushButtonOptionsWeatherNoaaReset, &QPushButton::clicked, this, &OptionsDialog::resetWeatherNoaaUrlClicked);
  connect(ui->pushButtonOptionsWeatherVatsimReset, &QPushButton::clicked, this, &OptionsDialog::resetWeatherVatsimUrlClicked);
  connect(ui->pushButtonOptionsWeatherIvaoReset, &QPushButton::clicked, this, &OptionsDialog::resetWeatherIvaoUrlClicked);
  connect(ui->pushButtonOptionsWeatherNoaaWindReset, &QPushButton::clicked, this, &OptionsDialog::resetWeatherNoaaWindUrlClicked);

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
  connect(ui->pushButtonOptionsDatabaseAddExcludeDir, &QPushButton::clicked, this, &OptionsDialog::addDatabaseExcludeDirClicked);
  connect(ui->pushButtonOptionsDatabaseAddExcludeFile, &QPushButton::clicked, this, &OptionsDialog::addDatabaseExcludeFileClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveExclude, &QPushButton::clicked, this, &OptionsDialog::removeDatabaseExcludePathClicked);
  connect(ui->listWidgetOptionsDatabaseExclude, &QListWidget::currentRowChanged, this, &OptionsDialog::updateDatabaseButtonState);

  // Database addon exclude path
  connect(ui->pushButtonOptionsDatabaseAddAddon, &QPushButton::clicked, this, &OptionsDialog::addDatabaseAddOnExcludePathClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveAddon, &QPushButton::clicked, this, &OptionsDialog::removeDatabaseAddOnExcludePathClicked);
  connect(ui->listWidgetOptionsDatabaseAddon, &QListWidget::currentRowChanged, this, &OptionsDialog::updateDatabaseButtonState);

  // ===========================================================================
  // Cache
  connect(ui->pushButtonOptionsCacheClearMemory, &QPushButton::clicked, this, &OptionsDialog::clearMemCachedClicked);
  connect(ui->pushButtonOptionsCacheClearDisk, &QPushButton::clicked, this, &OptionsDialog::clearDiskCachedClicked);
  connect(ui->pushButtonOptionsCacheShow, &QPushButton::clicked, this, &OptionsDialog::showDiskCacheClicked);

  connect(ui->checkBoxOptionsSimUpdatesConstant, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);

  // ===========================================================================
  // Map settings
  connect(ui->pushButtonOptionsDisplayFlightplanColor, &QPushButton::clicked, this, &OptionsDialog::flightplanColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanOutlineColor, &QPushButton::clicked, this, &OptionsDialog::flightplanOutlineColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanProcedureColor, &QPushButton::clicked, this,
          &OptionsDialog::flightplanProcedureColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanActiveColor, &QPushButton::clicked, this, &OptionsDialog::flightplanActiveColorClicked);
  connect(ui->pushButtonOptionsDisplayTrailColor, &QPushButton::clicked, this, &OptionsDialog::trailColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanPassedColor, &QPushButton::clicked, this, &OptionsDialog::flightplanPassedColorClicked);

  connect(ui->checkBoxOptionsMapFlightplanDimPassed, &QCheckBox::toggled, this, &OptionsDialog::updateFlightPlanColorWidgets);
  connect(ui->checkBoxOptionsMapFlightplanTransparent, &QCheckBox::toggled, this, &OptionsDialog::updateFlightPlanColorWidgets);

  connect(ui->checkBoxOptionsMapHighlightTransparent, &QCheckBox::toggled, this, &OptionsDialog::updateHighlightWidgets);

  connect(ui->lineEditOptionsRouteFilename, &QLineEdit::textEdited, this, &OptionsDialog::updateFlightplanExample);
  connect(ui->pushButtonOptionsRouteFilenameShort, &QPushButton::clicked, this, &OptionsDialog::flightplanPatterShortClicked);
  connect(ui->pushButtonOptionsRouteFilenameLong, &QPushButton::clicked, this, &OptionsDialog::flightplanPatterLongClicked);

  connect(ui->radioButtonCacheUseOffineElevation, &QRadioButton::clicked, this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->radioButtonCacheUseOnlineElevation, &QRadioButton::clicked, this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->lineEditCacheOfflineDataPath, &QLineEdit::textEdited, this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->pushButtonCacheOfflineDataSelect, &QPushButton::clicked, this, &OptionsDialog::offlineDataSelectClicked);

  connect(ui->pushButtonCacheUserAirspacePathSelect, &QPushButton::clicked, this, &OptionsDialog::userAirspacePathSelectClicked);
  connect(ui->lineEditCacheUserAirspacePath, &QLineEdit::textEdited, this, &OptionsDialog::updateCacheUserAirspaceStates);

  connect(ui->pushButtonOptionsStartupCheckUpdate, &QPushButton::clicked, this, &OptionsDialog::checkUpdateClicked);

  connect(ui->checkBoxOptionsMapEmptyAirports, &QCheckBox::toggled, this, &OptionsDialog::mapEmptyAirportsClicked);

  connect(ui->checkBoxOptionsSimDoNotFollowScroll, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
  connect(ui->checkBoxOptionsSimZoomOnLanding, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
  connect(ui->checkBoxOptionsSimCenterLegTable, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
  connect(ui->checkBoxOptionsSimClearSelection, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
  connect(ui->checkBoxOptionsSimCenterLeg, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);

  // Online tab =======================================================================
  connect(ui->radioButtonOptionsOnlineNone, &QRadioButton::clicked, this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineVatsim, &QRadioButton::clicked, this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineIvao, &QRadioButton::clicked, this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlinePilotEdge, &QRadioButton::clicked, this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineCustomStatus, &QRadioButton::clicked, this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineCustom, &QRadioButton::clicked, this, &OptionsDialog::updateOnlineWidgetStatus);

  connect(ui->lineEditOptionsOnlineStatusUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->lineEditOptionsOnlineWhazzupUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateOnlineWidgetStatus);

  connect(ui->pushButtonOptionsOnlineTestStatusUrl, &QPushButton::clicked, this, &OptionsDialog::onlineTestStatusUrlClicked);
  connect(ui->pushButtonOptionsOnlineTestWhazzupUrl, &QPushButton::clicked, this, &OptionsDialog::onlineTestWhazzupUrlClicked);

  // Online map display =======================================================================
  connect(ui->checkBoxDisplayOnlineGroundRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineApproachRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineObserverRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineFirRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineAreaRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineDepartureRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineTowerRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);
  connect(ui->checkBoxDisplayOnlineClearanceRange, &QCheckBox::toggled, this, &OptionsDialog::onlineDisplayRangeClicked);

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
  updateLinks();
  updateFlightPlanColorWidgets();
  updateHighlightWidgets();
  toolbarSizeClicked();

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

  ui->labelDisplayOnlineClearance->setEnabled(!ui->checkBoxDisplayOnlineClearanceRange->isChecked());
  ui->labelDisplayOnlineArea->setEnabled(!ui->checkBoxDisplayOnlineAreaRange->isChecked());
  ui->labelDisplayOnlineApproach->setEnabled(!ui->checkBoxDisplayOnlineApproachRange->isChecked());
  ui->labelDisplayOnlineDeparture->setEnabled(!ui->checkBoxDisplayOnlineDepartureRange->isChecked());
  ui->labelDisplayOnlineFir->setEnabled(!ui->checkBoxDisplayOnlineFirRange->isChecked());
  ui->labelDisplayOnlineObserver->setEnabled(!ui->checkBoxDisplayOnlineObserverRange->isChecked());
  ui->labelDisplayOnlineGround->setEnabled(!ui->checkBoxDisplayOnlineGroundRange->isChecked());
  ui->labelDisplayOnlineTower->setEnabled(!ui->checkBoxDisplayOnlineTowerRange->isChecked());
}

void OptionsDialog::checkOfficialOnlineUrls()
{
  if(ui->spinBoxOptionsOnlineUpdate->value() < MIN_ONLINE_UPDATE)
  {
    QUrl url;
    if(ui->radioButtonOptionsOnlineCustom->isChecked())
      // Custom whazzup.txt
      url = QUrl(ui->lineEditOptionsOnlineWhazzupUrl->text());
    else if(ui->radioButtonOptionsOnlineCustomStatus->isChecked())
      // Custom status.txt
      url = QUrl(ui->lineEditOptionsOnlineStatusUrl->text());

    if(!url.isEmpty() && !url.isLocalFile())
    {
      QString host = url.host().toLower();
      if(host.endsWith("ivao.aero") || host.endsWith("vatsim.net") || host.endsWith("littlenavmap.org") ||
         host.endsWith("pilotedge.net"))
      {
        qWarning() << Q_FUNC_INFO << "Update of" << ui->spinBoxOptionsOnlineUpdate->value()
                   << "s for url" << url << "host" << host;
        NavApp::deleteSplashScreen();
        QMessageBox::warning(this, QApplication::applicationName(),
                             tr("Do not use an update period smaller than %1 seconds "
                                "for official networks like VATSIM, IVAO or PilotEdge.\n\n"
                                "Resetting update period back to %1 seconds.").arg(MIN_ONLINE_UPDATE));

        // Reset both widget and data
        ui->spinBoxOptionsOnlineUpdate->setValue(MIN_ONLINE_UPDATE);
        OptionData::instanceInternal().onlineCustomReload = MIN_ONLINE_UPDATE;
      }
    }
  }
}

void OptionsDialog::onlineTestStatusUrlClicked()
{
  onlineTestUrl(ui->lineEditOptionsOnlineStatusUrl->text(), true /* statusFile */);
}

void OptionsDialog::onlineTestWhazzupUrlClicked()
{
  onlineTestUrl(ui->lineEditOptionsOnlineWhazzupUrl->text(), false /* statusFile */);
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
        ok |= str.simplified().startsWith("url0") || str.simplified().startsWith("url1");
      else
        ok |= str.simplified().startsWith("!GENERAL") || str.simplified().startsWith("!CLIENTS");
    }

    if(ok)
      QMessageBox::information(this, QApplication::applicationName(),
                               tr("<p>Success. First lines in file:</p><hr/><code>%1</code><hr/><br/>").
                               arg(result.mid(0, 6).join("<br/>")));
    else
    {
      if(statusFile)
        atools::gui::Dialog::warning(this,
                                     tr(
                                       "<p>Downloaded successfully but the file does not look like a status.txt file.</p>"
                                         "<p><b>One of the keys <i>url0</i> and/or <i>url1</i> is missing.</b></p>"
                                           "<p>First lines in file:</p><hr/><code>%1</code><hr/><br/>").
                                     arg(result.mid(0, 6).join("<br/>")));
      else
        atools::gui::Dialog::warning(this,
                                     tr(
                                       "<p>Downloaded successfully but the file does not look like a whazzup.txt file.</p>"
                                         "<p><b>One of the sections <i>!GENERAL</i> and/or <i>!CLIENTS</i> is missing.</b></p>"
                                           "<p>First lines in file:</p><hr/><code>%1</code><hr/><br/>").
                                     arg(result.mid(0, 6).join("<br/>")));
    }
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
    ui->labelOptionsOnlineStatusUrl->setEnabled(false);
    ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(false);
    ui->labelOptionsOnlineWhazzupUrl->setEnabled(false);
    ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(false);
    ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(false);
    ui->spinBoxOptionsOnlineUpdate->setEnabled(false);
    ui->labelOptionsOnlineUpdate->setEnabled(false);
    ui->comboBoxOptionsOnlineFormat->setEnabled(false);
    ui->labelOptionsOnlineFormat->setEnabled(false);
  }
  else
  {
    if(ui->radioButtonOptionsOnlineCustomStatus->isChecked())
    {
      ui->lineEditOptionsOnlineStatusUrl->setEnabled(true);
      ui->labelOptionsOnlineStatusUrl->setEnabled(true);
      ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(false);
      ui->labelOptionsOnlineWhazzupUrl->setEnabled(false);
      ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(QUrl(ui->lineEditOptionsOnlineStatusUrl->text()).isValid());
      ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(false);
    }
    else if(ui->radioButtonOptionsOnlineCustom->isChecked())
    {
      ui->lineEditOptionsOnlineStatusUrl->setEnabled(false);
      ui->labelOptionsOnlineStatusUrl->setEnabled(false);
      ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(true);
      ui->labelOptionsOnlineWhazzupUrl->setEnabled(true);
      ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(false);
      ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(QUrl(ui->lineEditOptionsOnlineWhazzupUrl->text()).isValid());
    }
    ui->spinBoxOptionsOnlineUpdate->setEnabled(true);
    ui->labelOptionsOnlineUpdate->setEnabled(true);
    ui->comboBoxOptionsOnlineFormat->setEnabled(true);
    ui->labelOptionsOnlineFormat->setEnabled(true);
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
      ui->spinBoxDisplayOnlineClearance,
      ui->spinBoxDisplayOnlineArea,
      ui->spinBoxDisplayOnlineApproach,
      ui->spinBoxDisplayOnlineDeparture,
      ui->spinBoxDisplayOnlineFir,
      ui->spinBoxDisplayOnlineObserver,
      ui->spinBoxDisplayOnlineGround,
      ui->spinBoxDisplayOnlineTower,
      ui->doubleSpinBoxOptionsSimZoomOnLanding
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
    // Test if user uses a too low update rate for well known URLs of official networks
    checkOfficialOnlineUrls();

    widgetsToOptionData();
    saveState();

    updateTooltipOption();

    emit optionsChanged();

    // Update dialog internal stuff
    updateWidgetStates();

    updateWebOptionsFromData();
    updateWebServerStatus();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Ok))
  {
    // Test if user uses a too low update rate for well known URLs of official networks
    checkOfficialOnlineUrls();

    widgetsToOptionData();
    saveState();
    updateWidgetUnits();
    updateWebOptionsFromData();

    updateTooltipOption();

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

      updateTooltipOption();
    }
  }
}

void OptionsDialog::updateWidgetStates()
{
  updateWhileFlyingWidgets(false);
  updateButtonColors();
  updateGuiFontLabel();
  updateMapFontLabel();
  onlineDisplayRangeClicked();
  eastWestRuleClicked();
  mapEmptyAirportsClicked(false);
  updateCacheElevationStates();
  updateCacheUserAirspaceStates();
  updateDatabaseButtonState();
  updateNavOptions();
  updateOnlineWidgetStatus();
  updateWeatherButtonState();
  updateWidgetUnits();
  updateFlightPlanColorWidgets();
  updateHighlightWidgets();
  toolbarSizeClicked();
}

void OptionsDialog::saveState()
{
  optionDataToWidgets(OptionData::instanceInternal());

  // Save widgets to settings
  atools::gui::WidgetState state(lnm::OPTIONS_DIALOG_WIDGET, false /* save visibility */, true /* block signals */);
  state.save(widgets);
  state.save(this);

  saveDisplayOptItemStates(displayOptItemIndexUser, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_USER_AIRCRAFT);
  saveDisplayOptItemStates(displayOptItemIndexAi, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_AI_AIRCRAFT);
  saveDisplayOptItemStates(displayOptItemIndexAirport, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_AIRPORT);
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
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_OUTLINE_COLOR, flightplanOutlineColor);
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
  od.onlineVatsimTransceiverUrl = networkSettings.value("vatsim/transceiverurl").toString();
  od.onlineVatsimReload = networkSettings.value("vatsim/update") == "auto" ? -1 :
                          networkSettings.value("vatsim/update").toInt();
  od.onlineVatsimTransceiverReload = networkSettings.value("vatsim/updatetransceiver") == "auto" ? -1 :
                                     networkSettings.value("vatsim/updatetransceiver").toInt();

  od.onlineIvaoWhazzupUrl = networkSettings.value("ivao/whazzupurl").toString();
  od.onlineIvaoReload = networkSettings.value("ivao/update") == "auto" ? -1 :
                        networkSettings.value("ivao/update").toInt();

  od.onlinePilotEdgeStatusUrl = networkSettings.value("pilotedge/statusurl").toString();
  od.onlinePilotEdgeReload = networkSettings.value("pilotedge/update") == "auto" ? -1 :
                             networkSettings.value("pilotedge/update").toInt();

  // Disable selection based on what was found in the file
  ui->radioButtonOptionsOnlineIvao->setVisible(!od.onlineIvaoWhazzupUrl.isEmpty());
  ui->radioButtonOptionsOnlinePilotEdge->setVisible(!od.onlinePilotEdgeStatusUrl.isEmpty());
  ui->radioButtonOptionsOnlineVatsim->setVisible(!od.onlineVatsimStatusUrl.isEmpty());

  atools::gui::WidgetState state(lnm::OPTIONS_DIALOG_WIDGET, false /*save visibility*/, true /*block signals*/);
  if(!state.contains(ui->splitterOptions))
  {
    // First start - splitter not saved yet

    // Get list widget max width by looking at all items
    int maxWidth = 0;
    for(int i = 0; i < ui->listWidgetOptionPages->count(); i++)
    {
      QListWidgetItem *item = ui->listWidgetOptionPages->item(i);
      maxWidth = std::max(QFontMetrics(item->font()).width(item->text()), maxWidth);
    }

    // Adjust splitter size to a reasonable value by setting maximum for the widget on the left
    ui->listWidgetOptionPages->setMaximumWidth(maxWidth * 4 / 3);

    // Save splitter size - user can resize freely after next start
    state.save(ui->splitterOptions);
  }

  state.restore(this);
  state.restore(widgets);

  restoreOptionItemStates(displayOptItemIndexUser, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_USER_AIRCRAFT);
  restoreOptionItemStates(displayOptItemIndexAi, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_AI_AIRCRAFT);
  restoreOptionItemStates(displayOptItemIndexAirport, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_AIRPORT);
  restoreOptionItemStates(displayOptItemIndexRose, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_COMPASS_ROSE);
  restoreOptionItemStates(displayOptItemIndexMeasurement, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_MEASUREMENT);
  restoreOptionItemStates(displayOptItemIndexRoute, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_ROUTE);
  restoreOptionItemStates(displayOptItemIndexNavAid, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_NAVAID);

  ui->splitterOptions->setHandleWidth(6);

  if(settings.contains(lnm::OPTIONS_DIALOG_DB_EXCLUDE))
    ui->listWidgetOptionsDatabaseExclude->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_EXCLUDE));
  if(settings.contains(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE))
    ui->listWidgetOptionsDatabaseAddon->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE));

  flightplanColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, QColor(Qt::yellow)).value<QColor>();
  flightplanOutlineColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_OUTLINE_COLOR, QColor(Qt::black)).value<QColor>();
  flightplanProcedureColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, QColor(255, 150, 0)).value<QColor>();
  flightplanActiveColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, QColor(Qt::magenta)).value<QColor>();
  flightplanPassedColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, QColor(Qt::gray)).value<QColor>();
  trailColor = settings.valueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, QColor(Qt::black)).value<QColor>();

  guiLanguage = settings.valueStr(lnm::OPTIONS_DIALOG_LANGUAGE, QLocale().name());
  guiFont = settings.valueStr(lnm::OPTIONS_DIALOG_FONT, QString());
  mapFont = settings.valueStr(lnm::OPTIONS_DIALOG_MAP_FONT, QString());

  widgetsToOptionData();
  updateWidgetUnits();
  mapEmptyAirportsClicked(false);
  updateWhileFlyingWidgets(false);
  updateButtonColors();
  updateGuiFontLabel();
  updateMapFontLabel();
  onlineDisplayRangeClicked();
  eastWestRuleClicked();

  updateWebServerStatus();
  updateWebDocrootStatus();
  updateFlightplanExample();
  toolbarSizeClicked();

  if(ui->listWidgetOptionPages->selectedItems().isEmpty())
    ui->listWidgetOptionPages->selectionModel()->select(ui->listWidgetOptionPages->model()->index(0, 0),
                                                        QItemSelectionModel::ClearAndSelect);

  ui->stackedWidgetOptions->setCurrentIndex(ui->listWidgetOptionPages->currentRow());
}

void OptionsDialog::updateTooltipOption()
{
  if(OptionData::instance().getFlags2().testFlag(opts2::DISABLE_TOOLTIPS))
    NavApp::setTooltipsDisabled({NavApp::getMapWidgetGui()});
  else
    NavApp::setTooltipsEnabled();
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
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanOutlineColor, flightplanOutlineColor);
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
    index.value(dispOpt)->setCheckState(0, type & dispOpt ? Qt::Checked : Qt::Unchecked);
}

QTreeWidgetItem *OptionsDialog::addTopItem(const QString& text, const QString& description)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidgetOptionsDisplayTextOptions->invisibleRootItem(), {text, description});
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate | Qt::ItemIsEnabled);
  return item;
}

template<typename TYPE>
QTreeWidgetItem *OptionsDialog::addItem(QTreeWidgetItem *root, QHash<TYPE, QTreeWidgetItem *>& index,
                                        const QString& text, const QString& description, TYPE type, bool checked) const
{
  QTreeWidgetItem *item = new QTreeWidgetItem(root, {text, description}, static_cast<int>(type));
  item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
  index.insert(type, item);
  return item;
}

void OptionsDialog::checkUpdateClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Trigger async check and get a dialog even if nothing was found
  NavApp::checkForUpdates(ui->comboBoxOptionsStartupUpdateChannels->currentIndex(),
                          true /* manually triggered */, false /* forceDebug */);
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

void OptionsDialog::flightplanOutlineColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanOutlineColor, mainWindow);
  if(col.isValid())
  {
    flightplanOutlineColor = col;
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

void OptionsDialog::updateHighlightWidgets()
{
  ui->spinBoxOptionsMapHighlightTransparent->setEnabled(ui->checkBoxOptionsMapHighlightTransparent->isChecked());
}

void OptionsDialog::updateFlightPlanColorWidgets()
{
  ui->pushButtonOptionsDisplayFlightplanPassedColor->setEnabled(ui->checkBoxOptionsMapFlightplanDimPassed->isChecked());
  ui->pushButtonOptionsDisplayFlightplanOutlineColor->setDisabled(ui->checkBoxOptionsMapFlightplanTransparent->isChecked());
  ui->spinBoxOptionsDisplayTransparencyFlightplan->setEnabled(ui->checkBoxOptionsMapFlightplanTransparent->isChecked());
  updateButtonColors();
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

void OptionsDialog::resetWeatherNoaaUrlClicked()
{
  ui->lineEditOptionsWeatherNoaaStationsUrl->setText(OptionData::WEATHER_NOAA_DEFAULT_URL);
}

void OptionsDialog::resetWeatherVatsimUrlClicked()
{
  ui->lineEditOptionsWeatherVatsimUrl->setText(OptionData::WEATHER_VATSIM_DEFAULT_URL);
}

void OptionsDialog::resetWeatherIvaoUrlClicked()
{
  ui->lineEditOptionsWeatherIvaoUrl->setText(OptionData::WEATHER_IVAO_DEFAULT_URL);
}

void OptionsDialog::resetWeatherNoaaWindUrlClicked()
{
  ui->lineEditOptionsWeatherNoaaWindUrl->setText(OptionData::WEATHER_NOAA_WIND_BASE_DEFAULT_URL);
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludeDirClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Scenery Loading"),
    lnm::OPTIONS_DIALOG_DB_DIR_DLG,
    NavApp::getCurrentSimulatorBasePath());

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
    NavApp::getCurrentSimulatorBasePath());

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
    NavApp::getCurrentSimulatorBasePath());

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

void OptionsDialog::mapEmptyAirportsClicked(bool state)
{
  Q_UNUSED(state)
  ui->checkBoxOptionsMapEmptyAirports3D->setEnabled(ui->checkBoxOptionsMapEmptyAirports->isChecked());
}

void OptionsDialog::updateWhileFlyingWidgets(bool)
{
  ui->spinBoxOptionsSimUpdateBox->setDisabled(ui->checkBoxOptionsSimUpdatesConstant->isChecked());
  ui->labelOptionsSimUpdateBox->setDisabled(ui->checkBoxOptionsSimUpdatesConstant->isChecked());

  ui->spinBoxOptionsSimCenterLegZoom->setEnabled(ui->checkBoxOptionsSimCenterLeg->isChecked());
  ui->labelOptionsSimCenterLegZoom->setEnabled(ui->checkBoxOptionsSimCenterLeg->isChecked());

  ui->spinBoxOptionsSimDoNotFollowScrollTime->setEnabled(ui->checkBoxOptionsSimDoNotFollowScroll->isChecked());
  ui->doubleSpinBoxOptionsSimZoomOnLanding->setEnabled(ui->checkBoxOptionsSimZoomOnLanding->isChecked());
  ui->spinBoxOptionsSimCleanupTableTime->setEnabled(ui->checkBoxOptionsSimCenterLegTable->isChecked() ||
                                                    ui->checkBoxOptionsSimClearSelection->isChecked());
}

/* Copy widget states to OptionData object */
void OptionsDialog::widgetsToOptionData()
{
  OptionData& data = OptionData::instanceInternal();

  data.guiLanguage = guiLanguage;
  data.guiFont = guiFont;
  data.mapFont = mapFont;

  data.flightplanColor = flightplanColor;
  data.flightplanOutlineColor = flightplanOutlineColor;
  data.flightplanProcedureColor = flightplanProcedureColor;
  data.flightplanActiveColor = flightplanActiveColor;
  data.flightplanPassedColor = flightplanPassedColor;
  data.trailColor = trailColor;

  data.displayOptionsUserAircraft = optsac::ITEM_USER_AIRCRAFT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsUserAircraft, displayOptItemIndexUser);

  data.displayOptionsAiAircraft = optsac::ITEM_AI_AIRCRAFT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsAiAircraft, displayOptItemIndexAi);

  data.displayOptionsAirport = optsd::AIRPORT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsAirport, displayOptItemIndexAirport);

  data.displayOptionsRose = optsd::ROSE_NONE;
  displayOptWidgetToOptionData(data.displayOptionsRose, displayOptItemIndexRose);

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
  toFlags(ui->checkBoxOptionsStartupLoadPerf, opts::STARTUP_LOAD_PERF);
  toFlags(ui->checkBoxOptionsStartupLoadLayout, opts::STARTUP_LOAD_LAYOUT);
  toFlags(ui->checkBoxOptionsStartupShowSplash, opts::STARTUP_SHOW_SPLASH);
  toFlags(ui->checkBoxOptionsStartupLoadSearch, opts::STARTUP_LOAD_SEARCH);
  toFlags(ui->checkBoxOptionsStartupLoadInfoContent, opts::STARTUP_LOAD_INFO);
  toFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  toFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  toFlags(ui->radioButtonOptionsStartupShowFlightplan, opts::STARTUP_SHOW_ROUTE);
  toFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
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
  toFlags2(ui->checkBoxOptionsGuiTooltips, opts2::DISABLE_TOOLTIPS);
  toFlags2(ui->checkBoxOptionsGuiToolbarSize, opts2::OVERRIDE_TOOLBAR_SIZE);

  toFlags(ui->radioButtonCacheUseOffineElevation, opts::CACHE_USE_OFFLINE_ELEVATION);
  toFlags(ui->radioButtonCacheUseOnlineElevation, opts::CACHE_USE_ONLINE_ELEVATION);

  toFlags2(ui->checkBoxOptionsMapEmptyAirports3D, opts2::MAP_EMPTY_AIRPORTS_3D);

  toFlags2(ui->checkBoxOptionsMapAirportText, opts2::MAP_AIRPORT_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapAirportAddon, opts2::MAP_AIRPORT_HIGHLIGHT_ADDON);
  toFlags2(ui->checkBoxOptionsMapNavaidText, opts2::MAP_NAVAID_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapUserpointText, opts2::MAP_USERPOINT_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapAirwayText, opts2::MAP_AIRWAY_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapFlightplanText, opts2::MAP_ROUTE_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapUserAircraftText, opts2::MAP_USER_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapAiAircraftText, opts2::MAP_AI_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapHighlightTransparent, opts2::MAP_HIGHLIGHT_TRANSPARENT);

  toFlags2(ui->checkBoxOptionsMapFlightplanDimPassed, opts2::MAP_ROUTE_DIM_PASSED);
  toFlags2(ui->checkBoxOptionsMapFlightplanTransparent, opts2::MAP_ROUTE_TRANSPARENT);
  toFlags2(ui->checkBoxOptionsSimDoNotFollowScroll, opts2::ROUTE_NO_FOLLOW_ON_MOVE);
  toFlags2(ui->checkBoxOptionsSimCenterLeg, opts2::ROUTE_AUTOZOOM);
  toFlags2(ui->checkBoxOptionsSimCenterLegTable, opts2::ROUTE_CENTER_ACTIVE_LEG);
  toFlags2(ui->checkBoxOptionsSimClearSelection, opts2::ROUTE_CLEAR_SELECTION);
  toFlags2(ui->checkBoxOptionsSimZoomOnLanding, opts2::ROUTE_ZOOM_LANDING);

  toFlags2(ui->checkBoxDisplayOnlineNameLookup, opts2::ONLINE_AIRSPACE_BY_NAME);
  toFlags2(ui->checkBoxDisplayOnlineFileLookup, opts2::ONLINE_AIRSPACE_BY_FILE);

  data.flightplanPattern = ui->lineEditOptionsRouteFilename->text();
  data.cacheOfflineElevationPath = ui->lineEditCacheOfflineDataPath->text();
  data.cacheUserAirspacePath = ui->lineEditCacheUserAirspacePath->text();
  data.cacheUserAirspaceExtensions = ui->lineEditCacheUserAirspaceExtensions->text();

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_VERBOSE, ui->checkBoxOptionsMapTooltipVerbose->isChecked());

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRCRAFT_USER, ui->checkBoxOptionsMapTooltipUserAircraft->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRCRAFT_AI, ui->checkBoxOptionsMapTooltipAiAircraft->isChecked());

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRPORT, ui->checkBoxOptionsMapTooltipAirport->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_NAVAID, ui->checkBoxOptionsMapTooltipNavaid->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRSPACE, ui->checkBoxOptionsMapTooltipAirspace->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_WIND, ui->checkBoxOptionsMapTooltipWind->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_MARKS, ui->checkBoxOptionsMapTooltipUser->isChecked());

  data.displayClickOptions.setFlag(optsd::CLICK_AIRCRAFT_USER, ui->checkBoxOptionsMapClickUserAircraft->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRCRAFT_AI, ui->checkBoxOptionsMapClickAiAircraft->isChecked());

  data.displayClickOptions.setFlag(optsd::CLICK_AIRPORT, ui->checkBoxOptionsMapClickAirport->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRPORT_PROC, ui->checkBoxOptionsMapClickAirportProcs->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_NAVAID, ui->checkBoxOptionsMapClickNavaid->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRSPACE, ui->checkBoxOptionsMapClickAirspace->isChecked());

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

  data.simNoFollowOnScrollTime = ui->spinBoxOptionsSimDoNotFollowScrollTime->value();
  data.simZoomOnLandingDist = static_cast<float>(ui->doubleSpinBoxOptionsSimZoomOnLanding->value());
  data.simCleanupTableTime = ui->spinBoxOptionsSimCleanupTableTime->value();

  data.simUpdateBox = ui->spinBoxOptionsSimUpdateBox->value();
  data.simUpdateBoxCenterLegZoom = ui->spinBoxOptionsSimCenterLegZoom->value();
  data.aircraftTrackMaxPoints = ui->spinBoxSimMaxTrackPoints->value();

  data.cacheSizeDisk = ui->spinBoxOptionsCacheDiskSize->value();
  data.cacheSizeMemory = ui->spinBoxOptionsCacheMemorySize->value();
  data.guiInfoTextSize = ui->spinBoxOptionsGuiInfoText->value();
  data.guiPerfReportTextSize = ui->spinBoxOptionsGuiAircraftPerf->value();
  data.guiRouteTableTextSize = ui->spinBoxOptionsGuiRouteText->value();
  data.guiSearchTableTextSize = ui->spinBoxOptionsGuiSearchText->value();
  data.guiInfoSimSize = ui->spinBoxOptionsGuiSimInfoText->value();
  data.guiToolbarSize = ui->spinBoxOptionsGuiToolbarSize->value();

  data.guiStyleMapDimming = ui->spinBoxOptionsGuiThemeMapDimming->value();

  data.mapClickSensitivity = ui->spinBoxOptionsMapClickRect->value();
  data.mapTooltipSensitivity = ui->spinBoxOptionsMapTooltipRect->value();

  data.mapZoomShowClick = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMap->value());
  data.mapZoomShowMenu = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMapMenu->value());

  data.routeGroundBuffer = ui->spinBoxOptionsRouteGroundBuffer->value();

  data.displayTextSizeAircraftAi = ui->spinBoxOptionsDisplayTextSizeAircraftAi->value();
  data.displaySymbolSizeNavaid = ui->spinBoxOptionsDisplaySymbolSizeNavaid->value();
  data.displaySymbolSizeUserpoint = ui->spinBoxOptionsDisplaySymbolSizeUserpoint->value();
  data.displayTextSizeNavaid = ui->spinBoxOptionsDisplayTextSizeNavaid->value();
  data.displayTextSizeUserpoint = ui->spinBoxOptionsDisplayTextSizeUserpoint->value();
  data.displayThicknessAirway = ui->spinBoxOptionsDisplayThicknessAirway->value();
  data.displayTextSizeAirway = ui->spinBoxOptionsDisplayTextSizeAirway->value();
  data.displayThicknessFlightplan = ui->spinBoxOptionsDisplayThicknessFlightplan->value();
  data.displayThicknessFlightplanProfile = ui->spinBoxOptionsDisplayThicknessFlightplanProfile->value();
  data.displaySymbolSizeAirport = ui->spinBoxOptionsDisplaySymbolSizeAirport->value();
  data.displaySymbolSizeAirportWeather = ui->spinBoxOptionsDisplaySymbolSizeAirportWeather->value();
  data.displaySymbolSizeWindBarbs = ui->spinBoxOptionsDisplaySymbolSizeWindBarbs->value();
  data.displaySymbolSizeAircraftAi = ui->spinBoxOptionsDisplaySymbolSizeAircraftAi->value();
  data.displayTextSizeFlightplan = ui->spinBoxOptionsDisplayTextSizeFlightplan->value();
  data.displayTextSizeFlightplanProfile = ui->spinBoxOptionsDisplayTextSizeFlightplanProfile->value();
  data.displayTransparencyFlightplan = ui->spinBoxOptionsDisplayTransparencyFlightplan->value();
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

  data.displayTransparencyAirportMsa = ui->spinBoxOptionsDisplayTransparencyAirportMsa->value();
  data.displayTextSizeAirportMsa = ui->spinBoxOptionsDisplayTextSizeAirportMsa->value();

  data.mapNavTouchArea = ui->spinBoxOptionsMapNavTouchArea->value();

  data.displayTextSizeRangeDistance = ui->spinBoxOptionsDisplayTextSizeRangeDistance->value();
  data.displayTextSizeCompassRose = ui->spinBoxOptionsDisplayTextSizeCompassRose->value();

  data.displayMapHighlightTransparent = ui->spinBoxOptionsMapHighlightTransparent->value();

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
  data.onlineCustomReload = ui->spinBoxOptionsOnlineUpdate->value();
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
  flightplanOutlineColor = data.flightplanOutlineColor;
  flightplanProcedureColor = data.flightplanProcedureColor;
  flightplanActiveColor = data.flightplanActiveColor;
  flightplanPassedColor = data.flightplanPassedColor;
  trailColor = data.trailColor;

  // Copy values from the tree widget
  displayOptDataToWidget(data.displayOptionsUserAircraft, displayOptItemIndexUser);
  displayOptDataToWidget(data.displayOptionsAiAircraft, displayOptItemIndexAi);
  displayOptDataToWidget(data.displayOptionsAirport, displayOptItemIndexAirport);
  displayOptDataToWidget(data.displayOptionsRose, displayOptItemIndexRose);
  displayOptDataToWidget(data.displayOptionsMeasurement, displayOptItemIndexMeasurement);
  displayOptDataToWidget(data.displayOptionsRoute, displayOptItemIndexRoute);
  displayOptDataToWidget(data.displayOptionsNavAid, displayOptItemIndexNavAid);

  // Copy from check and radio buttons
  fromFlags(data, ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  fromFlags(data, ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  fromFlags(data, ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  fromFlags(data, ui->checkBoxOptionsStartupLoadPerf, opts::STARTUP_LOAD_PERF);
  fromFlags(data, ui->checkBoxOptionsStartupLoadLayout, opts::STARTUP_LOAD_LAYOUT);
  fromFlags(data, ui->checkBoxOptionsStartupShowSplash, opts::STARTUP_SHOW_SPLASH);
  fromFlags(data, ui->checkBoxOptionsStartupLoadTrail, opts::STARTUP_LOAD_TRAIL);
  fromFlags(data, ui->checkBoxOptionsStartupLoadInfoContent, opts::STARTUP_LOAD_INFO);
  fromFlags(data, ui->checkBoxOptionsStartupLoadSearch, opts::STARTUP_LOAD_SEARCH);
  fromFlags(data, ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  fromFlags(data, ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  fromFlags(data, ui->radioButtonOptionsStartupShowFlightplan, opts::STARTUP_SHOW_ROUTE);
  fromFlags(data, ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
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
  fromFlags2(data, ui->checkBoxOptionsGuiTooltips, opts2::DISABLE_TOOLTIPS);
  fromFlags2(data, ui->checkBoxOptionsGuiToolbarSize, opts2::OVERRIDE_TOOLBAR_SIZE);

  fromFlags(data, ui->radioButtonCacheUseOffineElevation, opts::CACHE_USE_OFFLINE_ELEVATION);
  fromFlags(data, ui->radioButtonCacheUseOnlineElevation, opts::CACHE_USE_ONLINE_ELEVATION);

  fromFlags2(data, ui->checkBoxOptionsMapEmptyAirports3D, opts2::MAP_EMPTY_AIRPORTS_3D);

  fromFlags2(data, ui->checkBoxOptionsMapAirportText, opts2::MAP_AIRPORT_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapAirportAddon, opts2::MAP_AIRPORT_HIGHLIGHT_ADDON);
  fromFlags2(data, ui->checkBoxOptionsMapNavaidText, opts2::MAP_NAVAID_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapUserpointText, opts2::MAP_USERPOINT_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapAirwayText, opts2::MAP_AIRWAY_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapUserAircraftText, opts2::MAP_USER_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapAiAircraftText, opts2::MAP_AI_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapHighlightTransparent, opts2::MAP_HIGHLIGHT_TRANSPARENT);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanText, opts2::MAP_ROUTE_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanDimPassed, opts2::MAP_ROUTE_DIM_PASSED);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanTransparent, opts2::MAP_ROUTE_TRANSPARENT);
  fromFlags2(data, ui->checkBoxOptionsSimDoNotFollowScroll, opts2::ROUTE_NO_FOLLOW_ON_MOVE);
  fromFlags2(data, ui->checkBoxOptionsSimZoomOnLanding, opts2::ROUTE_ZOOM_LANDING);
  fromFlags2(data, ui->checkBoxOptionsSimCenterLeg, opts2::ROUTE_AUTOZOOM);
  fromFlags2(data, ui->checkBoxOptionsSimCenterLegTable, opts2::ROUTE_CENTER_ACTIVE_LEG);
  fromFlags2(data, ui->checkBoxOptionsSimClearSelection, opts2::ROUTE_CLEAR_SELECTION);

  fromFlags2(data, ui->checkBoxDisplayOnlineNameLookup, opts2::ONLINE_AIRSPACE_BY_NAME);
  fromFlags2(data, ui->checkBoxDisplayOnlineFileLookup, opts2::ONLINE_AIRSPACE_BY_FILE);

  ui->lineEditOptionsRouteFilename->setText(data.flightplanPattern);
  ui->lineEditCacheOfflineDataPath->setText(data.cacheOfflineElevationPath);
  ui->lineEditCacheUserAirspacePath->setText(data.cacheUserAirspacePath);
  ui->lineEditCacheUserAirspaceExtensions->setText(data.cacheUserAirspaceExtensions);

  ui->checkBoxOptionsMapTooltipVerbose->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_VERBOSE));
  ui->checkBoxOptionsMapTooltipUserAircraft->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRCRAFT_USER));
  ui->checkBoxOptionsMapTooltipAiAircraft->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRCRAFT_AI));

  ui->checkBoxOptionsMapTooltipAirport->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRPORT));
  ui->checkBoxOptionsMapTooltipNavaid->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_NAVAID));
  ui->checkBoxOptionsMapTooltipAirspace->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRSPACE));
  ui->checkBoxOptionsMapTooltipWind->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_WIND));
  ui->checkBoxOptionsMapTooltipUser->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_MARKS));

  ui->checkBoxOptionsMapClickAirport->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRPORT));
  ui->checkBoxOptionsMapClickAirportProcs->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRPORT_PROC));
  ui->checkBoxOptionsMapClickNavaid->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_NAVAID));
  ui->checkBoxOptionsMapClickAirspace->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRSPACE));

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

  ui->spinBoxOptionsSimDoNotFollowScrollTime->setValue(data.simNoFollowOnScrollTime);
  ui->doubleSpinBoxOptionsSimZoomOnLanding->setValue(data.simZoomOnLandingDist);
  ui->spinBoxOptionsSimCleanupTableTime->setValue(data.simCleanupTableTime);
  ui->spinBoxOptionsSimUpdateBox->setValue(data.simUpdateBox);
  ui->spinBoxOptionsSimCenterLegZoom->setValue(data.simUpdateBoxCenterLegZoom);
  ui->spinBoxSimMaxTrackPoints->setValue(data.aircraftTrackMaxPoints);
  ui->spinBoxOptionsCacheDiskSize->setValue(data.cacheSizeDisk);
  ui->spinBoxOptionsCacheMemorySize->setValue(data.cacheSizeMemory);
  ui->spinBoxOptionsGuiInfoText->setValue(data.guiInfoTextSize);
  ui->spinBoxOptionsGuiAircraftPerf->setValue(data.guiPerfReportTextSize);
  ui->spinBoxOptionsGuiRouteText->setValue(data.guiRouteTableTextSize);
  ui->spinBoxOptionsGuiSearchText->setValue(data.guiSearchTableTextSize);
  ui->spinBoxOptionsGuiSimInfoText->setValue(data.guiInfoSimSize);
  ui->spinBoxOptionsGuiToolbarSize->setValue(data.guiToolbarSize);
  ui->spinBoxOptionsGuiThemeMapDimming->setValue(data.guiStyleMapDimming);
  ui->spinBoxOptionsMapClickRect->setValue(data.mapClickSensitivity);
  ui->spinBoxOptionsMapTooltipRect->setValue(data.mapTooltipSensitivity);
  ui->doubleSpinBoxOptionsMapZoomShowMap->setValue(data.mapZoomShowClick);
  ui->doubleSpinBoxOptionsMapZoomShowMapMenu->setValue(data.mapZoomShowMenu);
  ui->spinBoxOptionsRouteGroundBuffer->setValue(data.routeGroundBuffer);
  ui->spinBoxOptionsDisplayTextSizeAircraftAi->setValue(data.displayTextSizeAircraftAi);
  ui->spinBoxOptionsDisplaySymbolSizeNavaid->setValue(data.displaySymbolSizeNavaid);
  ui->spinBoxOptionsDisplaySymbolSizeUserpoint->setValue(data.displaySymbolSizeUserpoint);
  ui->spinBoxOptionsDisplayTextSizeUserpoint->setValue(data.displayTextSizeUserpoint);
  ui->spinBoxOptionsDisplayThicknessAirway->setValue(data.displayThicknessAirway);
  ui->spinBoxOptionsDisplayTextSizeAirway->setValue(data.displayTextSizeAirway);
  ui->spinBoxOptionsDisplayThicknessFlightplan->setValue(data.displayThicknessFlightplan);
  ui->spinBoxOptionsDisplayThicknessFlightplanProfile->setValue(data.displayThicknessFlightplanProfile);
  ui->spinBoxOptionsDisplaySymbolSizeAirport->setValue(data.displaySymbolSizeAirport);
  ui->spinBoxOptionsDisplaySymbolSizeAirportWeather->setValue(data.displaySymbolSizeAirportWeather);
  ui->spinBoxOptionsDisplaySymbolSizeWindBarbs->setValue(data.displaySymbolSizeWindBarbs);
  ui->spinBoxOptionsDisplaySymbolSizeAircraftAi->setValue(data.displaySymbolSizeAircraftAi);
  ui->spinBoxOptionsDisplayTextSizeFlightplan->setValue(data.displayTextSizeFlightplan);
  ui->spinBoxOptionsDisplayTextSizeFlightplanProfile->setValue(data.displayTextSizeFlightplanProfile);
  ui->spinBoxOptionsDisplayTransparencyFlightplan->setValue(data.displayTransparencyFlightplan);
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
  ui->spinBoxOptionsDisplayTransparencyAirportMsa->setValue(data.displayTransparencyAirportMsa);
  ui->spinBoxOptionsDisplayTextSizeAirportMsa->setValue(data.displayTextSizeAirportMsa);
  ui->spinBoxOptionsMapNavTouchArea->setValue(data.mapNavTouchArea);
  ui->spinBoxOptionsDisplayTextSizeRangeDistance->setValue(data.displayTextSizeRangeDistance);
  ui->spinBoxOptionsDisplayTextSizeCompassRose->setValue(data.displayTextSizeCompassRose);
  ui->spinBoxOptionsMapHighlightTransparent->setValue(data.displayMapHighlightTransparent);
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
  ui->spinBoxOptionsOnlineUpdate->setValue(data.onlineCustomReload);
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
                                     QString() /* lnm::OPTIONS_DIALOG_GLOBE_FILE_DLG */,
                                     ui->lineEditCacheOfflineDataPath->text());

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
  ui->labelOptionsMapNavTouchscreenArea->setEnabled(ui->radioButtonOptionsMapNavTouchscreen->isChecked());
}

void OptionsDialog::clearMemCachedClicked()
{
  qDebug() << Q_FUNC_INFO;

  NavApp::getMapWidgetGui()->clearVolatileTileCache();
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
    NavApp::getMapWidgetGui()->model()->clearPersistentTileCache();
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
    else if(!QFileInfo::exists(path + QDir::separator() + "index.html"))
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

void OptionsDialog::updateLinks()
{
  // Check if text was already replaced to avoid warning
  if(ui->labelCacheGlobePathDownload->text().contains("%1"))
  {
    QString url =
      atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineInstallGlobeUrl, lnm::helpLanguageOnline()).toString();
    ui->labelCacheGlobePathDownload->setText(ui->labelCacheGlobePathDownload->text().arg(url));
  }
}

void OptionsDialog::updateFlightplanExample()
{
  if(!ui->lineEditOptionsRouteFilename->text().isEmpty())
  {
    QString example = atools::fs::pln::Flightplan::getFilenamePattern(ui->lineEditOptionsRouteFilename->text(),
                                                                      "IFR", "Frankfurt Am Main", "EDDF",
                                                                      "Fiumicino", "LIRF", ".lnmpln", 30000, false);

    QString text = tr("Example: \"%1\"").arg(atools::cleanFilename(example, atools::MAX_FILENAME_CHARS));

    // Check if the cleaned filename differs from user input
    if(example != atools::cleanFilename(example, atools::MAX_FILENAME_CHARS))
      text.append(tr("<br/>") +
                  atools::util::HtmlBuilder::errorMessage({tr("Pattern contains invalid characters, "
                                                              "double spaces or is longer than %1 characters.").
                                                           arg(atools::MAX_FILENAME_CHARS),
                                                           tr("Not allowed are:  "
                                                              "\\  /  :  \'  *  &amp;  &gt;  &lt;  ?  $  |")}));

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
    // Fall back to application font
    font = QGuiApplication::font();

  atools::gui::fontDescription(font, ui->labelOptionsDisplayFont);
}

void OptionsDialog::toolbarSizeClicked()
{
  ui->spinBoxOptionsGuiToolbarSize->setEnabled(ui->checkBoxOptionsGuiToolbarSize->isChecked());
}

void OptionsDialog::updateGuiFontLabel()
{
  ui->labelOptionsGuiFont->setText(atools::gui::fontDescription(QApplication::font()));
}

void OptionsDialog::resetMapFontClicked()
{
  // Set to GUI font and add bold - no matter if this defaults to system or not
  QFont font(QGuiApplication::font());
  font.setBold(true);
  mapFont = font.toString();

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
    QFont selfont = fontDialog->selectedFont();

    // Limit size to keep the user from messing up the UI without an option to change
    if(selfont.pointSizeF() > 30.)
      selfont.setPointSizeF(30.);
    if(selfont.pixelSize() > 30)
      selfont.setPixelSize(30);

    guiFont = selfont.toString();
    qDebug() << Q_FUNC_INFO << guiFont;

    // the user clicked OK and font is set to the font the user selected
    if(QApplication::font() != selfont)
      QApplication::setFont(selfont);
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
