/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "atools.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/unitstringtool.h"
#include "fs/pln/flightplan.h"
#include "fs/weather/weathertypes.h"
#include "grib/gribreader.h"
#include "gui/dialog.h"
#include "gui/griddelegate.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/listwidgetindex.h"
#include "gui/texteditdialog.h"
#include "gui/tools.h"
#include "gui/translator.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "mapgui/mapthemehandler.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "settings/settings.h"
#include "ui_options.h"
#include "util/htmlbuilder.h"
#include "weather/weatherreporter.h"
#include "web/webcontroller.h"

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
#include <QStringBuilder>
#include <QStandardItem>
#include <QInputDialog>

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

  ui->buttonBoxOptions->button(QDialogButtonBox::Ok)->setToolTip(tr("Apply changes and close dialog."));
  ui->buttonBoxOptions->button(QDialogButtonBox::Cancel)->setToolTip(tr("Discard all changes and close dialog."));
  ui->buttonBoxOptions->button(QDialogButtonBox::RestoreDefaults)->setToolTip(tr("Reset all settings back to default.\n"
                                                                                 "Changes only settings that can be done with this dialog."));

  ui->buttonBoxOptions->button(QDialogButtonBox::Apply)->setToolTip(tr("Apply changes.\n"
                                                                       "Move the dialog aside to see changes in main window or map."));

  // Styles cascade to children and mess up UI themes on linux - even if widget is selected by name
#if !defined(Q_OS_LINUX) || defined(DEBUG_INFORMATION)
  ui->splitterOptions->setStyleSheet(
    QString("QSplitter::handle { "
            "background: %1;"
            "image: url(:/littlenavmap/resources/icons/splitterhandhoriz.png);"
            " }").
    arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
#endif

#ifdef Q_OS_MACOS
  // Disable menu tooltips on macOS
  ui->checkBoxOptionsGuiTooltipsMenu->hide();
  ui->checkBoxOptionsGuiTooltipsMenu->setChecked(false);
#else
  // Restart after toolbar change only on macOS
  ui->labelOptionsGuiToolbarSize->hide();
#endif

  if(ui->splitterOptions->handle(1) != nullptr)
  {
    ui->splitterOptions->handle(1)->setToolTip(tr("Resize options list."));
    ui->splitterOptions->handle(1)->setStatusTip(ui->splitterOptions->handle(1)->toolTip());
  }

  zoomHandlerMapThemeKeysTable = new atools::gui::ItemViewZoomHandler(ui->tableWidgetOptionsMapKeys);
  zoomHandlerLabelTree = new atools::gui::ItemViewZoomHandler(ui->treeWidgetOptionsDisplayTextOptions);
  zoomHandlerDatabaseInclude = new atools::gui::ItemViewZoomHandler(ui->tableWidgetOptionsDatabaseInclude);
  zoomHandlerDatabaseExclude = new atools::gui::ItemViewZoomHandler(ui->tableWidgetOptionsDatabaseExclude);
  zoomHandlerDatabaseAddonExclude = new atools::gui::ItemViewZoomHandler(ui->tableWidgetOptionsDatabaseExcludeAddon);

  gridDelegate = new atools::gui::GridDelegate(ui->treeWidgetOptionsDisplayTextOptions);
  listWidgetIndex = new atools::gui::ListWidgetIndex(ui->listWidgetOptionPages, ui->stackedWidgetOptions);
  ui->treeWidgetOptionsDisplayTextOptions->setItemDelegate(gridDelegate);

  // Add option pages with text, icon and tooltip ========================================
  /* *INDENT-OFF* */
  QListWidget *list = ui->listWidgetOptionPages;
  list->addItem(pageListItem(list, tr("Startup and Updates"), tr("Select what should be reloaded on startup and change update settings."), ":/littlenavmap/resources/icons/littlenavmap.svg"));
  list->addItem(pageListItem(list, tr("User Interface"), tr("Change language settings, window options and other user interface behavior."), ":/littlenavmap/resources/icons/statusbar.svg"));
  list->addItem(pageListItem(list, tr("Display and Text"), tr("Change text sizes, user interface font, tooltips and more display options."), ":/littlenavmap/resources/icons/copy.svg"));
  list->addItem(pageListItem(list, tr("Units"), tr("Fuel, distance, speed and coordindate units as well as\noptions for course and heading display."), ":/littlenavmap/resources/icons/units.svg"));
  list->addItem(pageListItem(list, tr("Files"), tr("Edit flight plan file pattern and more file related actions."), ":/littlenavmap/resources/icons/fileopen.svg"));
  list->addItem(pageListItem(list, tr("Map"), tr("Change map window behavior, handling of empty airports, and general display options."), ":/littlenavmap/resources/icons/mapsettings.svg"));
  list->addItem(pageListItem(list, tr("Map Tooltips and Clicks"), tr("Tooltip and map click settings."), ":/littlenavmap/resources/icons/mapclick.svg"));
  list->addItem(pageListItem(list, tr("Map Navigation"), tr("Zoom, click, screen navigation and mousewheel settings."), ":/littlenavmap/resources/icons/mapnavigation.svg"));
  list->addItem(pageListItem(list, tr("Map Display"), tr("Change colors, symbols, texts and font for map display and elevation profile objects."), ":/littlenavmap/resources/icons/mapdisplay.svg"));
  list->addItem(pageListItem(list, tr("Map Flight Plan"), tr("Adjust display style and colors for the flight plan on the map and the elevation profile."), ":/littlenavmap/resources/icons/mapdisplayflightplan.svg"));
  list->addItem(pageListItem(list, tr("Map Aircraft Trail"), tr("Edit display of the user aircraft trail and number of trail points."), ":/littlenavmap/resources/icons/aircrafttrail.svg"));
  list->addItem(pageListItem(list, tr("Map User"), tr("Change colors, symbols and texts for highlights, measurement lines and other user features for map and elevation profile."), ":/littlenavmap/resources/icons/mapdisplay2.svg"));
  list->addItem(pageListItem(list, tr("Map Labels"), tr("Change map display and elevation profile label options for all map features."), ":/littlenavmap/resources/icons/mapdisplaylabels.svg"));
  list->addItem(pageListItem(list, tr("Map Keys"), tr("Enter username, API keys or tokens for map services which require a login."), ":/littlenavmap/resources/icons/mapdisplaykeys.svg"));
  list->addItem(pageListItem(list, tr("Map Online"), tr("Map display online center options."), ":/littlenavmap/resources/icons/airspaceonline.svg"));
  list->addItem(pageListItem(list, tr("Simulator Aircraft"), tr("Update and movement options for the user aircraft."), ":/littlenavmap/resources/icons/aircraft.svg"));
  list->addItem(pageListItem(list, tr("Flight Plan"), tr("Options for flight plan calculation and elevation profile altitude buffer."), ":/littlenavmap/resources/icons/route.svg"));
  list->addItem(pageListItem(list, tr("Weather"), tr("Change weather sources for information and tooltips."), ":/littlenavmap/resources/icons/weather.svg"));
  list->addItem(pageListItem(list, tr("Weather Files"), tr("Change web download addresses or file paths of weather sources."), ":/littlenavmap/resources/icons/weatherurl.svg"));
  list->addItem(pageListItem(list, tr("Online Flying"), tr("Select online flying services like VATSIM, IVAO or custom."), ":/littlenavmap/resources/icons/aircraft_online.svg"));
  list->addItem(pageListItem(list, tr("Web Server"), tr("Change settings for the internal web server."), ":/littlenavmap/resources/icons/web.svg"));
  list->addItem(pageListItem(list, tr("Cache and Files"), tr("Change map cache, select elevation data source and the path for user airspaces."), ":/littlenavmap/resources/icons/filesave.svg"));
  list->addItem(pageListItem(list, tr("Scenery Library Database"), tr("Exclude scenery files from loading and\nadd-on recognition."), ":/littlenavmap/resources/icons/database.svg"));
  /* *INDENT-ON* */

  // Build tree settings to map tab =====================================================
  /* *INDENT-OFF* */
  // Top of map =====================================================
  QTreeWidgetItem *topOfMap = addTopItem(tr("Top of Map"), tr("Select information that is displayed on top of the map."));
  addItem<optsac::DisplayOptionsUserAircraft>(topOfMap, displayOptItemIndexUser, tr("Wind Direction and Speed"), tr("Show wind direction and speed on the top center of the map."), optsac::ITEM_USER_AIRCRAFT_WIND, true);
  addItem<optsac::DisplayOptionsUserAircraft>(topOfMap, displayOptItemIndexUser, tr("Wind Pointer"), tr("Show wind direction pointer on the top center of the map."), optsac::ITEM_USER_AIRCRAFT_WIND_POINTER, true);

  // Map Navigation Aids =====================================================
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

  // Airport =====================================================
  QTreeWidgetItem *airport = addTopItem(tr("Airports"), tr("Select airport labels to display on the map."));
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("Name (Ident)"), tr("Airport name and ident in brackets depending on zoom factor.\n"
                                                                                                    "Ident can be internal, ICAO, FAA, IATA or local depending on avilability."), optsd::ITEM_AIRPORT_NAME, true);
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("Tower Frequency"), QString(), optsd::ITEM_AIRPORT_TOWER, true);
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("ATIS / ASOS / AWOS Frequency"), QString(), optsd::ITEM_AIRPORT_ATIS, true);
  addItem<optsd::DisplayOptionsAirport>(airport, displayOptItemIndexAirport, tr("Runway Information"), tr("Show runway elevation, light indicator \"L\" and length text."), optsd::ITEM_AIRPORT_RUNWAY, true);

  // Airport details =====================================================
  QTreeWidgetItem *airportDetails = addTopItem(tr("Airport Details"), tr("Select airport diagram elements."));
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Runways"), tr("Show runways."), optsd::ITEM_AIRPORT_DETAIL_RUNWAY, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Taxiways"), tr("Show taxiway lines and background."), optsd::ITEM_AIRPORT_DETAIL_TAXI, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Aprons"), tr("Display aprons."), optsd::ITEM_AIRPORT_DETAIL_APRON, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Parking"), tr("Show fuel, tower, helipads, gates and ramp parking."), optsd::ITEM_AIRPORT_DETAIL_PARKING, true);
  addItem<optsd::DisplayOptionsAirport>(airportDetails, displayOptItemIndexAirport, tr("Boundary"), tr("Display a white boundary around and below the airport diagram."), optsd::ITEM_AIRPORT_DETAIL_BOUNDARY);

  // Flight plan =====================================================
  QTreeWidgetItem *route = addTopItem(tr("Flight Plan"), tr("Select display options for the flight plan line."));
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Distance"), tr("Show distance along flight plan leg.\n"
                                                                                          "The label moves to keep it visible while scrolling."), optsd::ROUTE_DISTANCE, true);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Airway"), tr("Show airway.\n"
                                                                                        "The label moves to keep it visible while scrolling."), optsd::ROUTE_AIRWAY);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Magnetic Course"), tr("Show great circle magnetic start course at flight plan leg.\n"
                                                                                                 "Does not consider VOR calibrated declination.\n"
                                                                                                 "The label moves to keep it visible while scrolling."), optsd::ROUTE_MAG_COURSE);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("True Course"), tr("Show great circle true start course at flight plan leg.\n"
                                                                                             "The label moves to keep it visible while scrolling."), optsd::ROUTE_TRUE_COURSE);

  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("Magnetic Start and End Course"), tr("Display great circle initial and final magnetic course\n"
                                                                                                               "at the start and end of flight plan legs.\n"
                                                                                                               "The label is fixed. Course also depends on\n"
                                                                                                               "VOR calibrated declination and\n"
                                                                                                               "is colored blue if related to VOR.\n"
                                                                                                               "Not shown at procedure legs."), optsd::ROUTE_INITIAL_FINAL_MAG_COURSE, true);
  addItem<optsd::DisplayOptionsRoute>(route, displayOptItemIndexRoute, tr("True Start and End Course"), tr("Display great circle initial and final true course at\n"
                                                                                                           "the start and end of flight plan legs.\n"
                                                                                                           "The label is fixed. Not shown at procedure legs."), optsd::ROUTE_INITIAL_FINAL_TRUE_COURSE);

  // Airspace =====================================================
  QTreeWidgetItem *airspaces = addTopItem(tr("Airspaces"), QString());
  addItem<optsd::DisplayOptionsAirspace>(airspaces , displayOptItemIndexAirspace, tr("Name"), tr("Shows the airspace name."), optsd::AIRSPACE_NAME);
  addItem<optsd::DisplayOptionsAirspace>(airspaces , displayOptItemIndexAirspace, tr("Restrictive Name"), tr("Shows the restrictive name like \"P-51\" of an airspace."), optsd::AIRSPACE_RESTRICTIVE_NAME, true);
  addItem<optsd::DisplayOptionsAirspace>(airspaces , displayOptItemIndexAirspace, tr("Type"), tr("Type of airspace like \"Prohibited\"."), optsd::AIRSPACE_TYPE, true);
  addItem<optsd::DisplayOptionsAirspace>(airspaces , displayOptItemIndexAirspace, tr("Altitude"), tr("Display the altitude restrictions of airspaces."), optsd::AIRSPACE_ALTITUDE, true);
  addItem<optsd::DisplayOptionsAirspace>(airspaces , displayOptItemIndexAirspace, tr("COM Frequency"), tr("Airspace COM frequency if available."), optsd::AIRSPACE_COM, true);

  // User aircraft =====================================================
  QTreeWidgetItem *userAircraft = addTopItem(tr("User Aircraft"), tr("Select text labels and other options for the user aircraft."));
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Registration"), tr("Aircraft registration like \"N1000A\" or \"D-MABC\"."), optsac::ITEM_USER_AIRCRAFT_REGISTRATION);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Type"), tr("Show the aircraft type, like B738, B350 or M20T."), optsac::ITEM_USER_AIRCRAFT_TYPE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Airline"), tr("Airline like \"Orbit Airlines\"."), optsac::ITEM_USER_AIRCRAFT_AIRLINE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Flight Number"), tr("Flight number like \"123\"."), optsac::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Transponder Code"), tr("Transponder code prefixed with \"XPDR\" on the map"), optsac::ITEM_USER_AIRCRAFT_TRANSPONDER_CODE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Indicated Airspeed"), tr("Value prefixed with \"IAS\" on the map"), optsac::ITEM_USER_AIRCRAFT_IAS);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Ground Speed"), tr("Value prefixed with \"GS\" on the map"), optsac::ITEM_USER_AIRCRAFT_GS, true);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("True Airspeed"), tr("Value prefixed with \"TAS\" on the map"), optsac::ITEM_USER_AIRCRAFT_TAS);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Climb- and Sinkrate"), QString(), optsac::ITEM_USER_AIRCRAFT_CLIMB_SINK);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Heading"), tr("Aircraft magnetic heading prefixed with \"HDG\" on the map"), optsac::ITEM_USER_AIRCRAFT_HEADING);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Actual Altitude"), tr("Real aircraft altitude prefixed with \"ALT\" on the map"), optsac::ITEM_USER_AIRCRAFT_ALTITUDE, true);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Indicated Altitude"), tr("Indicated aircraft altitude prefixed with \"IND\" on the map"), optsac::ITEM_USER_AIRCRAFT_INDICATED_ALTITUDE);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Altitude above ground"), tr("Actual altitude above ground. Prefixed with \"AGL\" on the map"), optsac::ITEM_USER_AIRCRAFT_ALT_ABOVE_GROUND);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Track Line"), tr("Show the aircraft trail as a black needle at\n"
                                                                                                          "the from of the user aircraft."), optsac::ITEM_USER_AIRCRAFT_TRACK_LINE, true);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Coordinates"), tr("Show aircraft coordinates using the format selected on\n"
                                                                                                           "options page \"Units\"."), optsac::ITEM_USER_AIRCRAFT_COORDINATES);
  addItem<optsac::DisplayOptionsUserAircraft>(userAircraft, displayOptItemIndexUser, tr("Icing"), tr("Show a red label \"ICE\" and icing values in percent\n"
                                                                                                     "when aircraft icing occurs."), optsac::ITEM_USER_AIRCRAFT_ICE);

  // AI =====================================================
  QTreeWidgetItem *aiAircraft = addTopItem(tr("AI, Multiplayer and Online Client Aircraft"), tr("Select text labels for the AI, multiplayer and online client aircraft."));
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Registration, Number or Callsign"), tr("Aircraft registration like \"N1000A\" or \"D-MABC\"."), optsac::ITEM_AI_AIRCRAFT_REGISTRATION, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Type"), tr("Show the AI aircraft type, like B738, B350 or M20T."), optsac::ITEM_AI_AIRCRAFT_TYPE, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Airline"), tr("Airline like \"Orbit Airlines\"."), optsac::ITEM_AI_AIRCRAFT_AIRLINE, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Flight Number"), tr("Flight number like \"123\"."), optsac::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Transponder Code"), tr("Transponder code prefixed with \"XPDR\" on the map"), optsac::ITEM_AI_AIRCRAFT_TRANSPONDER_CODE);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Indicated Airspeed"), tr("Value prefixed with \"IAS\" on the map"), optsac::ITEM_AI_AIRCRAFT_IAS);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Ground Speed"), tr("Value prefixed with \"GS\" on the map"), optsac::ITEM_AI_AIRCRAFT_GS, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("True Airspeed"), tr("Value prefixed with \"TAS\" on the map"), optsac::ITEM_AI_AIRCRAFT_TAS);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Climb- and Sinkrate"), QString(), optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Heading"), tr("Aircraft magnetic heading prefixed with \"HDG\" on the map"), optsac::ITEM_AI_AIRCRAFT_HEADING);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Actual Altitude"), tr("Real aircraft altitude prefixed with \"ALT\" on the map"), optsac::ITEM_AI_AIRCRAFT_ALTITUDE, true);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Indicated Altitude"), tr("Indicated aircraft altitude prefixed with \"IND\" on the map"), optsac::ITEM_AI_AIRCRAFT_INDICATED_ALTITUDE);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Departure and Destination"), tr("Departure and destination airport idents"), optsac::ITEM_AI_AIRCRAFT_DEP_DEST);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Coordinates"), tr("Show aircraft coordinates using the format selected on\n"
                                                                                                     "options page \"Units\"."), optsac::ITEM_AI_AIRCRAFT_COORDINATES);
  addItem<optsac::DisplayOptionsAiAircraft>(aiAircraft, displayOptItemIndexAi, tr("Distance and Bearing from User"), tr("Distance and magnetic bearing from user aircraft\n"
                                                                                                                        "prefixed with \"From User\"."), optsac::ITEM_AI_AIRCRAFT_DIST_BEARING_FROM_USER);

  // Compass rose =====================================================
  QTreeWidgetItem *compassRose = addTopItem(tr("Compass Rose"), tr("Select display options for the compass rose."));
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Direction Labels"), tr("Show N, S, E and W labels."), optsd::ROSE_DIR_LABELS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Degree Tick Marks"), tr("Show tick marks for degrees on ring."), optsd::ROSE_DEGREE_MARKS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Degree Labels"), tr("Show degree labels on ring."), optsd::ROSE_DEGREE_LABELS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Range Rings"), tr("Show range rings and distance labels inside."), optsd::ROSE_RANGE_RINGS, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Heading Line"), tr("Show the dashed heading line for user aircraft."), optsd::ROSE_HEADING_LINE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Track Line"), tr("Show the solid track line for user aircraft."), optsd::ROSE_TRACK_LINE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Track Label"), tr("Show track label for user aircraft."), optsd::ROSE_TRACK_LABEL, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Heading Indicator"), tr("Show the heading for the user aircraft as a\n"
                                                                                                       "small magenta circle."), optsd::ROSE_CRAB_ANGLE, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("Course to Next Waypoint"), tr("Show the course to next waypoint for the user aircraft as\n"
                                                                                                             "a small magenta line."), optsd::ROSE_NEXT_WAYPOINT, true);
  addItem<optsd::DisplayOptionsRose>(compassRose, displayOptItemIndexRose, tr("True Heading"), tr("Show the whole circle and tick marks using true heading."), optsd::ROSE_TRUE_HEADING);

  // Measurment lines =====================================================
  QTreeWidgetItem *measurement = addTopItem(tr("Measurement Lines"), tr("Select display options measurement lines."));
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Distance"), tr("Great circle distance for measurement line."), optsd::MEASUREMENT_DIST, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Magnetic Course"), tr("Show magnetic course for start and end of line."), optsd::MEASUREMENT_MAG, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("True Course"), tr("Show true course for start and end of line."), optsd::MEASUREMENT_TRUE, true);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Radial Number"), tr("Shows the radial prefixed with \"R\" for VOR, VORDME,\n"
                                                                                                                 "VORTAC, TACAN or NDB."), optsd::MEASUREMENT_RADIAL);
  addItem<optsd::DisplayOptionsMeasurement>(measurement, displayOptItemIndexMeasurement, tr("Navaid or airport ident"), tr("Show ident if attached to navaid or airport.\n"
                                                                                                                           "Also show frequency if attached to a radio navaid. "), optsd::MEASUREMENT_LABEL, true);
  /* *INDENT-ON* */

  ui->treeWidgetOptionsDisplayTextOptions->resizeColumnToContents(0);

  // Create widget list for state saver
  // This will take over the actual saving of the settings
  widgets.append(
    {ui->listWidgetOptionPages,
     ui->splitterOptions,
     ui->checkBoxOptionsGuiCenterKml,
     ui->checkBoxOptionsGuiWheel,
     ui->checkBoxOptionsGuiRaiseWindows,
     ui->checkBoxOptionsGuiRaiseDockWindows,
     ui->checkBoxOptionsGuiRaiseMainWindow,
     ui->checkBoxOptionsGuiCenterRoute,
     ui->checkBoxOptionsGuiAvoidOverwrite,
     ui->checkBoxOptionsGuiOverrideLocale,
     ui->checkBoxOptionsGuiHighDpi,
     ui->checkBoxOptionsGuiTooltipsAll,
     ui->checkBoxOptionsGuiTooltipsMenu,
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
     ui->checkBoxOptionsMapTooltipTrail,
     ui->checkBoxOptionsMapClickUserAircraft,
     ui->checkBoxOptionsMapClickAiAircraft,
     ui->checkBoxOptionsMapClickAirport,
     ui->checkBoxOptionsMapClickAirportProcs,
     ui->checkBoxOptionsMapClickNavaid,
     ui->checkBoxOptionsMapClickAirspace,
     ui->checkBoxOptionsMapClickFlightplan,
     ui->checkBoxOptionsMapUndock,
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
     ui->lineEditOptionsWeatherXplane12Path,
     ui->lineEditOptionsWeatherNoaaStationsUrl,
     ui->lineEditOptionsWeatherVatsimUrl,
     ui->lineEditOptionsWeatherIvaoUrl,

     ui->lineEditOptionsWeatherXplaneWind,
     ui->lineEditOptionsWeatherNoaaWindUrl,

     ui->tableWidgetOptionsDatabaseInclude,
     ui->tableWidgetOptionsDatabaseExclude,
     ui->tableWidgetOptionsDatabaseExcludeAddon,

     ui->comboBoxMapScrollZoomDetails,
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
     ui->lineEditCacheMapThemeDir,
     ui->lineEditOptionsRouteFilename,

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
     ui->spinBoxOptionsDisplaySymbolSizeHighlight,
     ui->spinBoxOptionsDisplayTextSizeNavaid,
     ui->spinBoxOptionsDisplayTextSizeAirspace,
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
     ui->spinBoxOptionsDisplayThicknessUserFeature,
     ui->spinBoxOptionsDisplayThicknessMeasurement,
     ui->spinBoxOptionsDisplayThicknessCompassRose,
     ui->spinBoxOptionsDisplaySunShadeDarkness,
     ui->comboBoxOptionsDisplayTrailType,
     ui->comboBoxOptionsDisplayTrailGradient,
     ui->spinBoxOptionsDisplayTextSizeCompassRose,
     ui->spinBoxOptionsDisplayTextSizeUserFeature,
     ui->spinBoxOptionsDisplayTextSizeMeasurement,

     ui->checkBoxOptionsMapHighlightTransparent,
     ui->spinBoxOptionsMapHighlightTransparent,

     ui->checkBoxOptionsMapAirwayText,
     ui->checkBoxOptionsMapUserAircraftText,
     ui->checkBoxOptionsMapAiAircraftText,
     ui->checkBoxOptionsMapAiAircraftHideGround,
     ui->checkBoxOptionsMapAirspaceNoMultZ,
     ui->checkBoxOptionsDisplayTrailGradient,
     ui->spinBoxOptionsDisplayTextSizeAirway,
     ui->spinBoxOptionsDisplayThicknessAirway,
     ui->spinBoxOptionsDisplayThicknessAirspace,

     ui->spinBoxOptionsDisplayTextSizeMora,
     ui->spinBoxOptionsDisplayTransparencyMora,

     ui->spinBoxOptionsDisplayTextSizeAirportMsa,
     ui->spinBoxOptionsDisplayTransparencyAirportMsa,
     ui->spinBoxOptionsDisplayTransparencyAirspace,

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
     ui->checkBoxOptionsSimHighlightActiveTable,
     ui->checkBoxOptionsMapFlightplanDimPassed,
     ui->checkBoxOptionsMapFlightplanHighlightActive,
     ui->checkBoxOptionsMapFlightplanTransparent,
     ui->checkBoxOptionsSimCenterLeg,
     ui->checkBoxOptionsSimCenterLegTable,

     ui->checkBoxOptionsSimDoNotFollowScroll,
     ui->spinBoxOptionsSimDoNotFollowScrollTime,
     ui->checkBoxOptionsSimZoomOnLanding,
     ui->doubleSpinBoxOptionsSimZoomOnLanding,
     ui->checkBoxOptionsSimZoomOnTakeoff,
     ui->spinBoxOptionsSimZoomOnTakeoff,
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
     ui->checkBoxOptionsOnlineRemoveShadow,

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
  connect(ui->checkBoxOptionsGuiTooltipsAll, &QPushButton::clicked, this, &OptionsDialog::updateGuiWidgets);

  // ===========================================================================
  // Weather widgets - ASN
  connect(ui->pushButtonOptionsWeatherAsnPathSelect, &QPushButton::clicked, this, &OptionsDialog::selectActiveSkyPathClicked);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::textEdited, this, &OptionsDialog::updateActiveSkyPathStatus);

  // Weather widgets - X-Plane 11 METAR
  connect(ui->pushButtonOptionsWeatherXplanePathSelect, &QPushButton::clicked, this, &OptionsDialog::selectXplane11PathClicked);
  connect(ui->lineEditOptionsWeatherXplanePath, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplanePath, &QLineEdit::textEdited, this, &OptionsDialog::updateXplane11PathStatus);

  // Weather widgets - X-Plane 12 METAR and Wind
  connect(ui->pushButtonOptionsWeatherXplane12DirSelect, &QPushButton::clicked, this, &OptionsDialog::selectXplane12PathClicked);
  connect(ui->lineEditOptionsWeatherXplane12Path, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplane12Path, &QLineEdit::textEdited, this, &OptionsDialog::updateXplane12PathStatus);

  // ===========================================================================
  // Online weather
  connect(ui->lineEditOptionsWeatherNoaaStationsUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherVatsimUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherIvaoUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherNoaaWindUrl, &QLineEdit::textEdited, this, &OptionsDialog::updateWeatherButtonState);

  connect(ui->lineEditOptionsWeatherXplaneWind, &QLineEdit::editingFinished, this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherXplaneWind, &QLineEdit::textEdited, this, &OptionsDialog::updateXplaneWindStatus);

  connect(ui->pushButtonOptionsWeatherXplaneWindPathSelect, &QPushButton::clicked, this,
          &OptionsDialog::weatherXplane11WindPathSelectClicked);

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
  connect(ui->checkBoxOptionsDisplayTrailGradient, &QCheckBox::toggled, this, &OptionsDialog::updateTrailStates);

  // Map navigation
  connect(ui->radioButtonOptionsMapNavDragMove, &QRadioButton::clicked, this, &OptionsDialog::updateNavOptions);
  connect(ui->radioButtonOptionsMapNavClickCenter, &QRadioButton::clicked, this, &OptionsDialog::updateNavOptions);
  connect(ui->radioButtonOptionsMapNavTouchscreen, &QRadioButton::clicked, this, &OptionsDialog::updateNavOptions);

  connect(ui->pushButtonOptionsDisplaySelectFont, &QPushButton::clicked, this, &OptionsDialog::selectMapFontClicked);
  connect(ui->pushButtonOptionsDisplayResetFont, &QPushButton::clicked, this, &OptionsDialog::resetMapFontClicked);

  connect(ui->pushButtonOptionsMapboxUser, &QPushButton::clicked, this, &OptionsDialog::mapboxUserMapClicked);

  // ===========================================================================
  // Database include path
  connect(ui->pushButtonOptionsDatabaseAddIncludeDir, &QPushButton::clicked, this, &OptionsDialog::addDatabaseIncludeDirClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveInclude, &QPushButton::clicked, this, &OptionsDialog::removeDatabaseIncludePathClicked);
  connect(ui->tableWidgetOptionsDatabaseInclude, &QTableWidget::itemSelectionChanged, this, &OptionsDialog::updateDatabaseButtonState);

  // Database exclude path
  connect(ui->pushButtonOptionsDatabaseAddExcludeDir, &QPushButton::clicked, this, &OptionsDialog::addDatabaseExcludeDirClicked);
  connect(ui->pushButtonOptionsDatabaseAddExcludeFile, &QPushButton::clicked, this, &OptionsDialog::addDatabaseExcludeFileClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveExclude, &QPushButton::clicked, this, &OptionsDialog::removeDatabaseExcludePathClicked);
  connect(ui->tableWidgetOptionsDatabaseExclude, &QTableWidget::itemSelectionChanged, this, &OptionsDialog::updateDatabaseButtonState);

  // Database addon exclude path
  connect(ui->pushButtonOptionsDatabaseAddAddon, &QPushButton::clicked, this, &OptionsDialog::addDatabaseAddOnExcludePathClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveAddon, &QPushButton::clicked, this, &OptionsDialog::removeDatabaseAddOnExcludePathClicked);
  connect(ui->tableWidgetOptionsDatabaseExcludeAddon, &QTableWidget::itemSelectionChanged, this, &OptionsDialog::updateDatabaseButtonState);

  // ===========================================================================
  // Cache
  connect(ui->pushButtonOptionsCacheClearMemory, &QPushButton::clicked, this, &OptionsDialog::clearMemCachedClicked);
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

  connect(ui->pushButtonOptionsMapHighlightFlightPlanColor, &QPushButton::clicked, this,
          &OptionsDialog::mapHighlightFlightplanColorClicked);
  connect(ui->pushButtonOptionsMapHighlightSearchColor, &QPushButton::clicked, this, &OptionsDialog::mapHighlightSearchColorClicked);
  connect(ui->pushButtonOptionsMapHighlightProfileColor, &QPushButton::clicked, this, &OptionsDialog::mapHighlightProfileColorClicked);
  connect(ui->pushButtonOptionsMapMeasurementColor, &QPushButton::clicked, this, &OptionsDialog::mapMeasurementColorClicked);

  connect(ui->checkBoxOptionsMapFlightplanDimPassed, &QCheckBox::toggled, this, &OptionsDialog::updateFlightPlanColorWidgets);
  connect(ui->checkBoxOptionsMapFlightplanHighlightActive, &QCheckBox::toggled, this, &OptionsDialog::updateFlightPlanColorWidgets);
  connect(ui->checkBoxOptionsMapFlightplanTransparent, &QCheckBox::toggled, this, &OptionsDialog::updateFlightPlanColorWidgets);

  connect(ui->checkBoxOptionsMapHighlightTransparent, &QCheckBox::toggled, this, &OptionsDialog::updateHighlightWidgets);

  connect(ui->lineEditOptionsRouteFilename, &QLineEdit::textEdited, this, &OptionsDialog::updateFlightplanExample);
  connect(ui->pushButtonOptionsRouteFilenameShort, &QPushButton::clicked, this, &OptionsDialog::flightplanPatterShortClicked);
  connect(ui->pushButtonOptionsRouteFilenameLong, &QPushButton::clicked, this, &OptionsDialog::flightplanPatterLongClicked);

  connect(ui->radioButtonCacheUseOffineElevation, &QRadioButton::clicked, this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->radioButtonCacheUseOnlineElevation, &QRadioButton::clicked, this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->lineEditCacheOfflineDataPath, &QLineEdit::textEdited, this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->lineEditCacheMapThemeDir, &QLineEdit::textEdited, this, &OptionsDialog::updateCacheMapThemeDir);
  connect(ui->pushButtonCacheOfflineDataSelect, &QPushButton::clicked, this, &OptionsDialog::offlineDataSelectClicked);
  connect(ui->pushButtonCacheMapThemeDir, &QPushButton::clicked, this, &OptionsDialog::mapThemeDirSelectClicked);

  connect(ui->pushButtonOptionsStartupCheckUpdate, &QPushButton::clicked, this, &OptionsDialog::checkUpdateClicked);

  connect(ui->checkBoxOptionsMapEmptyAirports, &QCheckBox::toggled, this, &OptionsDialog::mapEmptyAirportsClicked);

  connect(ui->checkBoxOptionsSimDoNotFollowScroll, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
  connect(ui->checkBoxOptionsSimZoomOnLanding, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
  connect(ui->checkBoxOptionsSimZoomOnTakeoff, &QCheckBox::toggled, this, &OptionsDialog::updateWhileFlyingWidgets);
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

  // Map theme key double clicked ===================================================================
  connect(ui->tableWidgetOptionsMapKeys, &QTableWidget::itemDoubleClicked, this, &OptionsDialog::mapThemeKeyEdited);

  connect(ui->lineEditOptionSearch, &QLineEdit::textEdited, this, &OptionsDialog::searchTextEdited);
}

/* called at program end */
OptionsDialog::~OptionsDialog()
{
  ui->treeWidgetOptionsDisplayTextOptions->setItemDelegate(nullptr);

  ATOOLS_DELETE_LOG(gridDelegate);
  ATOOLS_DELETE_LOG(listWidgetIndex);
  ATOOLS_DELETE_LOG(zoomHandlerLabelTree);
  ATOOLS_DELETE_LOG(zoomHandlerDatabaseInclude);
  ATOOLS_DELETE_LOG(zoomHandlerDatabaseExclude);
  ATOOLS_DELETE_LOG(zoomHandlerDatabaseAddonExclude);
  ATOOLS_DELETE_LOG(zoomHandlerMapThemeKeysTable);
  ATOOLS_DELETE_LOG(units);
  ATOOLS_DELETE_LOG(ui);
  ATOOLS_DELETE_LOG(fontDialog);
}

void OptionsDialog::styleChanged()
{
  // Remember text, clear label and set text again to force update after style changes
  atools::gui::util::labelForcedUpdate(ui->labelMapApiKeys);
  atools::gui::util::labelForcedUpdate(ui->labelCacheGlobePathDownload);
  atools::gui::util::labelForcedUpdate(ui->labelOptionsWebStatus);

  gridDelegate->styleChanged();

  // Update potential search hightlights with new color
  listWidgetIndex->setHighlightColor(NavApp::isCurrentGuiStyleNight() ? QColor(255, 0, 0, 200) : QColor(255, 255, 0, 200));
  listWidgetIndex->find(QString());
  listWidgetIndex->find(ui->lineEditOptionSearch->text());
}

void OptionsDialog::setCacheMapThemeDir(const QString& mapThemesDir)
{
  // Assign value if current is empty and passed value is valid
  QString& cacheMapThemeDir = OptionData::instanceInternal().cacheMapThemeDir;
  if(cacheMapThemeDir.isEmpty() && atools::checkDir(Q_FUNC_INFO, mapThemesDir, true /* warn */))
  {
    Settings::instance().setValue("OptionsDialog/Widget_lineEditCacheMapThemeDir", mapThemesDir);
    Settings::syncSettings();
    cacheMapThemeDir = mapThemesDir;
    ui->lineEditCacheMapThemeDir->setText(mapThemesDir);
  }
}

void OptionsDialog::setCacheOfflineDataPath(const QString& globeDir)
{
  // Assign value if current is empty and passed value is valid
  QString& cacheOfflineElevationPath = OptionData::instanceInternal().cacheOfflineElevationPath;
  if(cacheOfflineElevationPath.isEmpty() && atools::checkDir(Q_FUNC_INFO, globeDir, true /* warn */))
  {
    Settings::instance().setValue("OptionsDialog/Widget_lineEditCacheOfflineDataPath", globeDir);
    Settings::syncSettings();
    cacheOfflineElevationPath = globeDir;
    ui->lineEditCacheOfflineDataPath->setText(globeDir);
  }
}

void OptionsDialog::open()
{
  qDebug() << Q_FUNC_INFO;
  // Fetch keys from handler
  OptionData::instanceInternal().mapThemeKeys = NavApp::getMapThemeHandler()->getMapThemeKeys();

  optionDataToWidgets(OptionData::instanceInternal());
  updateCacheElevationStates();
  updateCacheMapThemeDir();
  updateDatabaseButtonState();
  updateNavOptions();
  updateGuiWidgets();
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
  styleChanged();

  QDialog::open();
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

    NavApp::getMapThemeHandler()->setMapThemeKeys(OptionData::instanceInternal().mapThemeKeys);

    NavApp::updateChannels(OptionData::instance().getUpdateChannels());
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

    NavApp::getMapThemeHandler()->setMapThemeKeys(OptionData::instanceInternal().mapThemeKeys);

    NavApp::updateChannels(OptionData::instance().getUpdateChannels());
    emit optionsChanged();

    // Close dialog
    accept();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrlWeb(this,
                                lnm::helpOnlineUrl + QString("OPTIONS.html#page%1").arg(ui->stackedWidgetOptions->currentIndex() + 1),
                                lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Cancel))
    reject();
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::RestoreDefaults))
  {
    qDebug() << "OptionsDialog::resetDefaultClicked";

    int result = QMessageBox::question(this, QApplication::applicationName(), tr("Reset all options to default?"),
                                       QMessageBox::No | QMessageBox::Yes, QMessageBox::No);

    if(result == QMessageBox::Yes)
    {
      // Do reset - this can be undone with cancel

      // Create data object with default options
      OptionData defaultOpts;

      // Get all keys and remove values for reset in default options
      defaultOpts.mapThemeKeys = NavApp::getMapThemeHandler()->getMapThemeKeys();
      for(auto it = defaultOpts.mapThemeKeys.begin(); it != defaultOpts.mapThemeKeys.end(); ++it)
        it.value().clear();

      // Keep the undock map window state to avoid a messed up layout after restart
      defaultOpts.flags2.setFlag(opts2::MAP_ALLOW_UNDOCK, OptionData::instanceInternal().getFlags2().testFlag(opts2::MAP_ALLOW_UNDOCK));

      optionDataToWidgets(defaultOpts);
      updateWidgetStates();

      updateWebOptionsFromData();
      updateFontFromData();
      updateWebServerStatus();

      updateTooltipOption();
      resetGuiFontClicked();
      resetMapFontClicked();
      flightplanPatterLongClicked();
      NavApp::updateChannels(OptionData::instance().getUpdateChannels());
    }
  }
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
      if(host.endsWith("ivao.aero") || host.endsWith("vatsim.net") || host.endsWith("littlenavmap.org") || host.endsWith("pilotedge.net"))
      {
        qWarning() << Q_FUNC_INFO << "Update of" << ui->spinBoxOptionsOnlineUpdate->value()
                   << "s for url" << url << "host" << host;
        NavApp::closeSplashScreen();
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

  if(atools::fs::weather::testUrl(result, url, QString(), QHash<QString, QString>(), 250))
  {
    bool ok = false;
    for(const QString& str : qAsConst(result))
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
        atools::gui::Dialog::warning(this, tr("<p>Downloaded successfully but the file does not look like a status.txt file.</p>"
                                                "<p><b>One of the keys <i>url0</i> and/or <i>url1</i> is missing.</b></p>"
                                                  "<p>First lines in file:</p><hr/><code>%1</code><hr/><br/>").
                                     arg(result.mid(0, 6).join("<br/>")));
      else
        atools::gui::Dialog::warning(this, tr("<p>Downloaded successfully but the file does not look like a whazzup.txt file.</p>"
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
      ui->doubleSpinBoxOptionsSimZoomOnLanding,
      ui->spinBoxOptionsSimZoomOnTakeoff
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

void OptionsDialog::updateTrailStates()
{
  ui->comboBoxOptionsDisplayTrailGradient->setEnabled(ui->checkBoxOptionsDisplayTrailGradient->isChecked());
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
  updateDatabaseButtonState();
  updateNavOptions();
  updateGuiWidgets();
  updateOnlineWidgetStatus();
  updateWeatherButtonState();
  updateWidgetUnits();
  updateFlightPlanColorWidgets();
  updateHighlightWidgets();
  toolbarSizeClicked();
  updateTrailStates();
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
  saveDisplayOptItemStates(displayOptItemIndexAirspace, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_AIRSPACE);

  Settings& settings = Settings::instance();

  settings.setValue(lnm::OPTIONS_DIALOG_LANGUAGE, guiLanguage);
  settings.setValue(lnm::OPTIONS_DIALOG_FONT, guiFont);
  settings.setValue(lnm::OPTIONS_DIALOG_MAP_FONT, mapFont);

  // Save the path lists
  QStringList paths;
  for(int row = 0; row < ui->tableWidgetOptionsDatabaseInclude->rowCount(); row++)
    paths.append(ui->tableWidgetOptionsDatabaseInclude->item(row, 0)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_INCLUDE, paths);

  paths.clear();
  for(int row = 0; row < ui->tableWidgetOptionsDatabaseExclude->rowCount(); row++)
    paths.append(ui->tableWidgetOptionsDatabaseExclude->item(row, 0)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_EXCLUDE, paths);

  paths.clear();
  for(int row = 0; row < ui->tableWidgetOptionsDatabaseExcludeAddon->rowCount(); row++)
    paths.append(ui->tableWidgetOptionsDatabaseExcludeAddon->item(row, 0)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE, paths);

  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, flightplanColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_OUTLINE_COLOR, flightplanOutlineColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, flightplanProcedureColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, flightplanActiveColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, flightplanPassedColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, trailColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_MEASUREMENT_COLOR, measurementColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_HIGHLIGHT_COLOR, highlightFlightplanColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_SEARCH_HIGHLIGHT_COLOR, highlightSearchColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_PROFILE_HIGHLIGHT_COLOR, highlightProfileColor);

  Settings::syncSettings();
}

void OptionsDialog::restoreState()
{
  Settings& settings = Settings::instance();

  // Reload online network settings from configuration file which can be overloaded by placing a copy
  // in the settings file
  QString networksPath = Settings::getOverloadedPath(lnm::NETWORKS_CONFIG);
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
      maxWidth = std::max(QFontMetrics(item->font()).horizontalAdvance(item->text()), maxWidth);
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
  restoreOptionItemStates(displayOptItemIndexAirspace, lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS_AIRSPACE);

  ui->splitterOptions->setHandleWidth(6);

  addDatabaseTableItems(ui->tableWidgetOptionsDatabaseInclude, settings.valueStrList(lnm::OPTIONS_DIALOG_DB_INCLUDE));
  addDatabaseTableItems(ui->tableWidgetOptionsDatabaseExclude, settings.valueStrList(lnm::OPTIONS_DIALOG_DB_EXCLUDE));
  addDatabaseTableItems(ui->tableWidgetOptionsDatabaseExcludeAddon, settings.valueStrList(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE));

  flightplanColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, QColor(Qt::yellow)).value<QColor>();
  flightplanOutlineColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_OUTLINE_COLOR, QColor(Qt::black)).value<QColor>();
  flightplanProcedureColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, QColor(255, 150, 0)).value<QColor>();
  flightplanActiveColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, QColor(Qt::magenta)).value<QColor>();
  flightplanPassedColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, QColor(Qt::gray)).value<QColor>();
  trailColor = settings.valueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, QColor(Qt::black)).value<QColor>();
  measurementColor = settings.valueVar(lnm::OPTIONS_DIALOG_MEASUREMENT_COLOR, QColor(Qt::black)).value<QColor>();
  highlightFlightplanColor = settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_HIGHLIGHT_COLOR, QColor(Qt::green)).value<QColor>();
  highlightSearchColor = settings.valueVar(lnm::OPTIONS_DIALOG_SEARCH_HIGHLIGHT_COLOR, QColor(Qt::yellow)).value<QColor>();
  highlightProfileColor = settings.valueVar(lnm::OPTIONS_DIALOG_PROFILE_HIGHLIGHT_COLOR, QColor(Qt::cyan)).value<QColor>();

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
  updateTrailStates();

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
  // The overall tooltip state is checked by the Application event handler Application::notify()
  if(OptionData::instance().getFlags().testFlag(opts::ENABLE_TOOLTIPS_ALL))
    NavApp::setTooltipsEnabled();
  else
    NavApp::setTooltipsDisabled({NavApp::getMapWidgetGui()});

#ifdef Q_OS_MACOS
  NavApp::setToolTipsEnabledMainMenu(false);
#else
  // Menu tooltips only on Linux and Windows - also needs to keep them enabled for recent menus
  NavApp::setToolTipsEnabledMainMenu(OptionData::instance().getFlags().testFlag(opts::ENABLE_TOOLTIPS_MENU));
#endif
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
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsMapMeasurementColor, measurementColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsMapHighlightFlightPlanColor, highlightFlightplanColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsMapHighlightSearchColor, highlightSearchColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsMapHighlightProfileColor, highlightProfileColor);
}

template<typename TYPE>
void OptionsDialog::restoreOptionItemStates(const QHash<TYPE, QTreeWidgetItem *>& index, const QString& optionPrefix) const
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
  item->setToolTip(1, description);

  QFont font = item->font(0);
  font.setBold(true);
  item->setFont(0, font);
  return item;
}

template<typename TYPE>
QTreeWidgetItem *OptionsDialog::addItem(QTreeWidgetItem *root, QHash<TYPE, QTreeWidgetItem *>& index,
                                        const QString& text, const QString& description, TYPE type, bool checked) const
{
  QTreeWidgetItem *item = new QTreeWidgetItem(root, {text, description}, static_cast<int>(type));
  item->setToolTip(1, description);
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
                          true /* manual */, false /* startup */, false /* forceDebug */);
}

void OptionsDialog::colorButtonClicked(QColor& color)
{
  QColor col = QColorDialog::getColor(color, mainWindow);
  if(col.isValid())
  {
    color = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanProcedureColorClicked()
{
  colorButtonClicked(flightplanProcedureColor);
}

void OptionsDialog::flightplanColorClicked()
{
  colorButtonClicked(flightplanColor);
}

void OptionsDialog::flightplanOutlineColorClicked()
{
  colorButtonClicked(flightplanOutlineColor);
}

void OptionsDialog::flightplanActiveColorClicked()
{
  colorButtonClicked(flightplanActiveColor);
}

void OptionsDialog::flightplanPassedColorClicked()
{
  colorButtonClicked(flightplanPassedColor);
}

void OptionsDialog::mapHighlightFlightplanColorClicked()
{
  colorButtonClicked(highlightFlightplanColor);
}

void OptionsDialog::mapHighlightSearchColorClicked()
{
  colorButtonClicked(highlightSearchColor);
}

void OptionsDialog::mapHighlightProfileColorClicked()
{
  colorButtonClicked(highlightProfileColor);
}

void OptionsDialog::trailColorClicked()
{
  colorButtonClicked(trailColor);
}

void OptionsDialog::mapMeasurementColorClicked()
{
  colorButtonClicked(measurementColor);
}

void OptionsDialog::updateHighlightWidgets()
{
  ui->spinBoxOptionsMapHighlightTransparent->setEnabled(ui->checkBoxOptionsMapHighlightTransparent->isChecked());
}

void OptionsDialog::updateFlightPlanColorWidgets()
{
  ui->pushButtonOptionsDisplayFlightplanPassedColor->setEnabled(ui->checkBoxOptionsMapFlightplanDimPassed->isChecked());
  ui->pushButtonOptionsDisplayFlightplanActiveColor->setEnabled(ui->checkBoxOptionsMapFlightplanHighlightActive->isChecked());
  ui->pushButtonOptionsDisplayFlightplanOutlineColor->setDisabled(ui->checkBoxOptionsMapFlightplanTransparent->isChecked());
  ui->spinBoxOptionsDisplayTransparencyFlightplan->setEnabled(ui->checkBoxOptionsMapFlightplanTransparent->isChecked());
  updateButtonColors();
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

  bool result = WeatherReporter::testUrl(resultStr, url, QString());

  QGuiApplication::restoreOverrideCursor();

  if(result)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Success. First lines in file:</p><hr/><code>%1</code><hr/><br/>").
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
  bool result = WeatherReporter::testUrl(resultStr, ui->lineEditOptionsWeatherVatsimUrl->text(), QString());
  QGuiApplication::restoreOverrideCursor();

  if(result)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Success. First lines in file:</p><hr/><code>%1</code><hr/><br/>").
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
  bool result = WeatherReporter::testUrl(resultStr, ui->lineEditOptionsWeatherIvaoUrl->text(), QString(), {
    {"accept", "application/json"},
    {"apiKey",
     atools::strFromCryptFile(":/littlenavmap/little_navmap_keys/ivao_weather_api_key.bin",
                              0x2B1A96468EB62460)}
  });

  QGuiApplication::restoreOverrideCursor();

  if(result)
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("<p>Success. First lines in file:</p><hr/><code>%1</code><hr/><br/>").arg(resultStr.join("<br/>")));
  else
    atools::gui::Dialog::warning(this, tr("Failed. Reason:\n%1").arg(resultStr.join("\n")));
}

/* Test NOAA GRIB download URL and show a dialog of the first line */
void OptionsDialog::testWeatherNoaaWindUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  QStringList resultStr;
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  bool result = WeatherReporter::testUrl(resultStr, ui->lineEditOptionsWeatherNoaaWindUrl->text(), QString());
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

/* Show directory dialog to add add-on exclude path */
void OptionsDialog::addDatabaseIncludeDirClicked()
{
  qDebug() << Q_FUNC_INFO;
  QString path = atools::gui::Dialog(this).openDirectoryDialog(tr("Open Directory to include"), lnm::OPTIONS_DIALOG_DB_DIR_DLG,
                                                               NavApp::getCurrentSimulatorBasePath());

  addDatabaseTableItem(ui->tableWidgetOptionsDatabaseInclude, path);
  updateDatabaseButtonState();
}

void OptionsDialog::removeDatabaseIncludePathClicked()
{
  qDebug() << Q_FUNC_INFO;
  removeSelectedDatabaseTableItems(ui->tableWidgetOptionsDatabaseInclude);
  updateDatabaseButtonState();
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludeDirClicked()
{
  qDebug() << Q_FUNC_INFO;
  QString path = atools::gui::Dialog(this).openDirectoryDialog(tr("Open Directory to exclude from Scenery Loading"),
                                                               lnm::OPTIONS_DIALOG_DB_DIR_DLG, NavApp::getCurrentSimulatorBasePath());

  addDatabaseTableItem(ui->tableWidgetOptionsDatabaseExclude, path);
  updateDatabaseButtonState();
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludeFileClicked()
{
  qDebug() << Q_FUNC_INFO;

  const QStringList paths = atools::gui::Dialog(this).openFileDialogMulti(tr("Open Files to exclude from Scenery Loading"),
                                                                          QString(), // filter lnm::OPTIONS_DIALOG_DB_FILE_DLG,
                                                                          NavApp::getCurrentSimulatorBasePath());

  for(const QString& path : paths)
    addDatabaseTableItem(ui->tableWidgetOptionsDatabaseExclude, path);
  updateDatabaseButtonState();
}

void OptionsDialog::removeDatabaseExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;
  removeSelectedDatabaseTableItems(ui->tableWidgetOptionsDatabaseExclude);
  updateDatabaseButtonState();
}

/* Show directory dialog to add add-on exclude path */
void OptionsDialog::addDatabaseAddOnExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openDirectoryDialog(tr("Open Directory to exclude from Add-On Recognition"),
                                                               lnm::OPTIONS_DIALOG_DB_DIR_DLG, NavApp::getCurrentSimulatorBasePath());

  addDatabaseTableItem(ui->tableWidgetOptionsDatabaseExcludeAddon, path);
  updateDatabaseButtonState();
}

void OptionsDialog::removeDatabaseAddOnExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  removeSelectedDatabaseTableItems(ui->tableWidgetOptionsDatabaseExcludeAddon);
  updateDatabaseButtonState();
}

void OptionsDialog::updateDatabaseButtonState()
{
  ui->pushButtonOptionsDatabaseRemoveInclude->setEnabled(ui->tableWidgetOptionsDatabaseInclude->selectionModel()->hasSelection());
  ui->pushButtonOptionsDatabaseRemoveExclude->setEnabled(ui->tableWidgetOptionsDatabaseExclude->selectionModel()->hasSelection());
  ui->pushButtonOptionsDatabaseRemoveAddon->setEnabled(ui->tableWidgetOptionsDatabaseExcludeAddon->selectionModel()->hasSelection());
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

  ui->checkBoxOptionsSimZoomOnTakeoff->setEnabled(!ui->checkBoxOptionsSimCenterLeg->isChecked());
  ui->spinBoxOptionsSimZoomOnTakeoff->setEnabled(ui->checkBoxOptionsSimZoomOnTakeoff->isChecked() &&
                                                 !ui->checkBoxOptionsSimCenterLeg->isChecked());

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
  data.highlightFlightplanColor = highlightFlightplanColor;
  data.highlightSearchColor = highlightSearchColor;
  data.highlightProfileColor = highlightProfileColor;

  data.trailColor = trailColor;
  data.measurementColor = measurementColor;

  data.displayOptionsUserAircraft = optsac::ITEM_USER_AIRCRAFT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsUserAircraft, displayOptItemIndexUser);

  data.displayOptionsAiAircraft = optsac::ITEM_AI_AIRCRAFT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsAiAircraft, displayOptItemIndexAi);

  data.displayOptionsAirport = optsd::AIRPORT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsAirport, displayOptItemIndexAirport);

  data.displayOptionsRose = optsd::ROSE_NONE;
  displayOptWidgetToOptionData(data.displayOptionsRose, displayOptItemIndexRose);

  data.displayOptionsMeasurement = optsd::MEASUREMENT_NONE;
  displayOptWidgetToOptionData(data.displayOptionsMeasurement, displayOptItemIndexMeasurement);

  data.displayOptionsRoute = optsd::ROUTE_NONE;
  displayOptWidgetToOptionData(data.displayOptionsRoute, displayOptItemIndexRoute);

  data.displayOptionsNavAid = optsd::NAVAIDS_NONE;
  displayOptWidgetToOptionData(data.displayOptionsNavAid, displayOptItemIndexNavAid);

  data.displayOptionsAirspace = optsd::AIRSPACE_NONE;
  displayOptWidgetToOptionData(data.displayOptionsAirspace, displayOptItemIndexAirspace);

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
  toFlags(ui->checkBoxOptionsGuiWheel, opts::GUI_REVERSE_WHEEL);
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
  toFlags(ui->checkBoxOptionsMapAiAircraftHideGround, opts::MAP_AI_HIDE_GROUND);
  toFlags(ui->checkBoxOptionsMapAirspaceNoMultZ, opts::MAP_AIRSPACE_NO_MULT_Z);
  toFlags2(ui->checkBoxOptionsMapHighlightTransparent, opts2::MAP_HIGHLIGHT_TRANSPARENT);
  toFlags(ui->checkBoxOptionsDisplayTrailGradient, opts::MAP_TRAIL_GRADIENT);

  toFlags2(ui->checkBoxOptionsMapFlightplanDimPassed, opts2::MAP_ROUTE_DIM_PASSED);
  toFlags2(ui->checkBoxOptionsMapFlightplanHighlightActive, opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE);
  toFlags2(ui->checkBoxOptionsMapFlightplanTransparent, opts2::MAP_ROUTE_TRANSPARENT);
  toFlags2(ui->checkBoxOptionsSimDoNotFollowScroll, opts2::ROUTE_NO_FOLLOW_ON_MOVE);
  toFlags2(ui->checkBoxOptionsSimCenterLeg, opts2::ROUTE_AUTOZOOM);
  toFlags2(ui->checkBoxOptionsSimCenterLegTable, opts2::ROUTE_CENTER_ACTIVE_LEG);
  toFlags2(ui->checkBoxOptionsSimClearSelection, opts2::ROUTE_CLEAR_SELECTION);
  toFlags2(ui->checkBoxOptionsSimHighlightActiveTable, opts2::ROUTE_HIGHLIGHT_ACTIVE_TABLE);

  toFlags2(ui->checkBoxDisplayOnlineNameLookup, opts2::ONLINE_AIRSPACE_BY_NAME);
  toFlags2(ui->checkBoxDisplayOnlineFileLookup, opts2::ONLINE_AIRSPACE_BY_FILE);

  toFlags(ui->checkBoxOptionsOnlineRemoveShadow, opts::ONLINE_REMOVE_SHADOW);
  toFlags(ui->checkBoxOptionsGuiTooltipsAll, opts::ENABLE_TOOLTIPS_ALL);
  toFlags(ui->checkBoxOptionsGuiTooltipsMenu, opts::ENABLE_TOOLTIPS_MENU);

  data.flightplanPattern = ui->lineEditOptionsRouteFilename->text();
  data.cacheOfflineElevationPath = ui->lineEditCacheOfflineDataPath->text();
  data.cacheMapThemeDir = ui->lineEditCacheMapThemeDir->text();

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_VERBOSE, ui->checkBoxOptionsMapTooltipVerbose->isChecked());

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRCRAFT_USER, ui->checkBoxOptionsMapTooltipUserAircraft->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRCRAFT_AI, ui->checkBoxOptionsMapTooltipAiAircraft->isChecked());

  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRPORT, ui->checkBoxOptionsMapTooltipAirport->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_NAVAID, ui->checkBoxOptionsMapTooltipNavaid->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRSPACE, ui->checkBoxOptionsMapTooltipAirspace->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_WIND, ui->checkBoxOptionsMapTooltipWind->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_MARKS, ui->checkBoxOptionsMapTooltipUser->isChecked());
  data.displayTooltipOptions.setFlag(optsd::TOOLTIP_AIRCRAFT_TRAIL, ui->checkBoxOptionsMapTooltipTrail->isChecked());

  data.displayClickOptions.setFlag(optsd::CLICK_AIRCRAFT_USER, ui->checkBoxOptionsMapClickUserAircraft->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRCRAFT_AI, ui->checkBoxOptionsMapClickAiAircraft->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRPORT, ui->checkBoxOptionsMapClickAirport->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRPORT_PROC, ui->checkBoxOptionsMapClickAirportProcs->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_NAVAID, ui->checkBoxOptionsMapClickNavaid->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_AIRSPACE, ui->checkBoxOptionsMapClickAirspace->isChecked());
  data.displayClickOptions.setFlag(optsd::CLICK_FLIGHTPLAN, ui->checkBoxOptionsMapClickFlightplan->isChecked());

  toFlags2(ui->checkBoxOptionsSimZoomOnLanding, opts2::ROUTE_ZOOM_LANDING);
  toFlags2(ui->checkBoxOptionsSimZoomOnTakeoff, opts2::ROUTE_ZOOM_TAKEOFF);
  data.weatherXplane11Path = QDir::toNativeSeparators(ui->lineEditOptionsWeatherXplanePath->text());
  data.weatherXplane12Path = QDir::toNativeSeparators(ui->lineEditOptionsWeatherXplane12Path->text());
  data.weatherActiveSkyPath = QDir::toNativeSeparators(ui->lineEditOptionsWeatherAsnPath->text());
  data.weatherNoaaUrl = ui->lineEditOptionsWeatherNoaaStationsUrl->text();
  data.weatherVatsimUrl = ui->lineEditOptionsWeatherVatsimUrl->text();
  data.weatherIvaoUrl = ui->lineEditOptionsWeatherIvaoUrl->text();

  data.databaseInclude.clear();
  for(int row = 0; row < ui->tableWidgetOptionsDatabaseInclude->rowCount(); row++)
    data.databaseInclude.append(ui->tableWidgetOptionsDatabaseInclude->item(row, 0)->text());

  data.databaseExclude.clear();
  for(int row = 0; row < ui->tableWidgetOptionsDatabaseExclude->rowCount(); row++)
    data.databaseExclude.append(ui->tableWidgetOptionsDatabaseExclude->item(row, 0)->text());

  data.databaseAddonExclude.clear();
  for(int row = 0; row < ui->tableWidgetOptionsDatabaseExcludeAddon->rowCount(); row++)
    data.databaseAddonExclude.append(ui->tableWidgetOptionsDatabaseExcludeAddon->item(row, 0)->text());

  data.mapScrollDetail = static_cast<opts::MapScrollDetail>(ui->comboBoxMapScrollZoomDetails->currentIndex());

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
  data.simZoomOnTakeoffDist = static_cast<float>(ui->spinBoxOptionsSimZoomOnTakeoff->value());
  data.simCleanupTableTime = ui->spinBoxOptionsSimCleanupTableTime->value();

  data.simUpdateBox = ui->spinBoxOptionsSimUpdateBox->value();
  data.simUpdateBoxCenterLegZoom = ui->spinBoxOptionsSimCenterLegZoom->value();
  data.aircraftTrailMaxPoints = ui->spinBoxSimMaxTrackPoints->value();

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
  data.displaySymbolSizeHighlight = ui->spinBoxOptionsDisplaySymbolSizeHighlight->value();
  data.displayTextSizeNavaid = ui->spinBoxOptionsDisplayTextSizeNavaid->value();
  data.displayTextSizeAirspace = ui->spinBoxOptionsDisplayTextSizeAirspace->value();
  data.displayTextSizeUserpoint = ui->spinBoxOptionsDisplayTextSizeUserpoint->value();
  data.displayThicknessAirway = ui->spinBoxOptionsDisplayThicknessAirway->value();
  data.displayThicknessAirspace = ui->spinBoxOptionsDisplayThicknessAirspace->value();
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
  data.displayThicknessUserFeature = ui->spinBoxOptionsDisplayThicknessUserFeature->value();
  data.displayThicknessMeasurement = ui->spinBoxOptionsDisplayThicknessMeasurement->value();
  data.displayThicknessCompassRose = ui->spinBoxOptionsDisplayThicknessCompassRose->value();
  data.displaySunShadingDimFactor = ui->spinBoxOptionsDisplaySunShadeDarkness->value();
  data.displayTrailType = static_cast<opts::DisplayTrailType>(ui->comboBoxOptionsDisplayTrailType->currentIndex());

  data.displayTransparencyMora = ui->spinBoxOptionsDisplayTransparencyMora->value();
  data.displayTextSizeMora = ui->spinBoxOptionsDisplayTextSizeMora->value();

  data.displayTransparencyAirportMsa = ui->spinBoxOptionsDisplayTransparencyAirportMsa->value();
  data.displayTransparencyAirspace = ui->spinBoxOptionsDisplayTransparencyAirspace->value();
  data.displayTextSizeAirportMsa = ui->spinBoxOptionsDisplayTextSizeAirportMsa->value();

  data.mapNavTouchArea = ui->spinBoxOptionsMapNavTouchArea->value();

  data.displayTextSizeUserFeature = ui->spinBoxOptionsDisplayTextSizeUserFeature->value();
  data.displayTextSizeMeasurement = ui->spinBoxOptionsDisplayTextSizeMeasurement->value();
  data.displayTextSizeCompassRose = ui->spinBoxOptionsDisplayTextSizeCompassRose->value();

  data.displayMapHighlightTransparent = ui->spinBoxOptionsMapHighlightTransparent->value();

  data.updateRate = static_cast<opts::UpdateRate>(ui->comboBoxOptionsStartupUpdateRate->currentIndex());
  data.updateChannels = static_cast<opts::UpdateChannels>(ui->comboBoxOptionsStartupUpdateChannels->currentIndex());

  data.unitDist = static_cast<opts::UnitDist>(ui->comboBoxOptionsUnitDistance->currentIndex());
  data.unitShortDist = static_cast<opts::UnitShortDist>(ui->comboBoxOptionsUnitShortDistance->currentIndex());
  data.unitAlt = static_cast<opts::UnitAlt>(ui->comboBoxOptionsUnitAlt->currentIndex());
  data.unitSpeed = static_cast<opts::UnitSpeed>(ui->comboBoxOptionsUnitSpeed->currentIndex());
  data.unitVertSpeed = static_cast<opts::UnitVertSpeed>(ui->comboBoxOptionsUnitVertSpeed->currentIndex());
  data.unitCoords = atools::minmax(opts::COORDS_MIN, opts::COORDS_MAX,
                                   static_cast<opts::UnitCoords>(ui->comboBoxOptionsUnitCoords->currentIndex()));
  data.unitFuelWeight = static_cast<opts::UnitFuelAndWeight>(ui->comboBoxOptionsUnitFuelWeight->currentIndex());
  data.altitudeRuleType = static_cast<opts::AltitudeRule>(ui->comboBoxOptionsRouteAltitudeRuleType->currentIndex());

  data.displayTrailGradientType = static_cast<opts::DisplayTrailGradientType>(ui->comboBoxOptionsDisplayTrailGradient->currentIndex());

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

  data.displayOnlineClearance = displayOnlineRangeToData(ui->spinBoxDisplayOnlineClearance, ui->checkBoxDisplayOnlineClearanceRange);
  data.displayOnlineArea = displayOnlineRangeToData(ui->spinBoxDisplayOnlineArea, ui->checkBoxDisplayOnlineAreaRange);
  data.displayOnlineApproach = displayOnlineRangeToData(ui->spinBoxDisplayOnlineApproach, ui->checkBoxDisplayOnlineApproachRange);
  data.displayOnlineDeparture = displayOnlineRangeToData(ui->spinBoxDisplayOnlineDeparture, ui->checkBoxDisplayOnlineDepartureRange);
  data.displayOnlineFir = displayOnlineRangeToData(ui->spinBoxDisplayOnlineFir, ui->checkBoxDisplayOnlineFirRange);
  data.displayOnlineObserver = displayOnlineRangeToData(ui->spinBoxDisplayOnlineObserver, ui->checkBoxDisplayOnlineObserverRange);
  data.displayOnlineGround = displayOnlineRangeToData(ui->spinBoxDisplayOnlineGround, ui->checkBoxDisplayOnlineGroundRange);
  data.displayOnlineTower = displayOnlineRangeToData(ui->spinBoxDisplayOnlineTower, ui->checkBoxDisplayOnlineTowerRange);

  data.webPort = ui->spinBoxOptionsWebPort->value();
  data.webDocumentRoot = QDir::fromNativeSeparators(ui->lineEditOptionsWebDocroot->text());
  data.webEncrypted = ui->checkBoxOptionsWebEncrypted->isChecked();

  data.weatherXplaneWind = ui->lineEditOptionsWeatherXplaneWind->text();
  data.weatherNoaaWindBaseUrl = ui->lineEditOptionsWeatherNoaaWindUrl->text();

  widgetToMapThemeKeys(data);

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
  measurementColor = data.measurementColor;
  highlightFlightplanColor = data.highlightFlightplanColor;
  highlightSearchColor = data.highlightSearchColor;
  highlightProfileColor = data.highlightProfileColor;

  // Copy values from the tree widget
  displayOptDataToWidget(data.displayOptionsUserAircraft, displayOptItemIndexUser);
  displayOptDataToWidget(data.displayOptionsAiAircraft, displayOptItemIndexAi);
  displayOptDataToWidget(data.displayOptionsAirport, displayOptItemIndexAirport);
  displayOptDataToWidget(data.displayOptionsRose, displayOptItemIndexRose);
  displayOptDataToWidget(data.displayOptionsMeasurement, displayOptItemIndexMeasurement);
  displayOptDataToWidget(data.displayOptionsRoute, displayOptItemIndexRoute);
  displayOptDataToWidget(data.displayOptionsNavAid, displayOptItemIndexNavAid);
  displayOptDataToWidget(data.displayOptionsAirspace, displayOptItemIndexAirspace);

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
  fromFlags(data, ui->checkBoxOptionsGuiWheel, opts::GUI_REVERSE_WHEEL);
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
  fromFlags(data, ui->checkBoxOptionsMapAiAircraftHideGround, opts::MAP_AI_HIDE_GROUND);
  fromFlags(data, ui->checkBoxOptionsMapAirspaceNoMultZ, opts::MAP_AIRSPACE_NO_MULT_Z);
  fromFlags(data, ui->checkBoxOptionsDisplayTrailGradient, opts::MAP_TRAIL_GRADIENT);

  fromFlags2(data, ui->checkBoxOptionsMapHighlightTransparent, opts2::MAP_HIGHLIGHT_TRANSPARENT);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanText, opts2::MAP_ROUTE_TEXT_BACKGROUND);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanDimPassed, opts2::MAP_ROUTE_DIM_PASSED);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanHighlightActive, opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE);
  fromFlags2(data, ui->checkBoxOptionsMapFlightplanTransparent, opts2::MAP_ROUTE_TRANSPARENT);
  fromFlags2(data, ui->checkBoxOptionsSimDoNotFollowScroll, opts2::ROUTE_NO_FOLLOW_ON_MOVE);
  fromFlags2(data, ui->checkBoxOptionsSimZoomOnLanding, opts2::ROUTE_ZOOM_LANDING);
  fromFlags2(data, ui->checkBoxOptionsSimZoomOnTakeoff, opts2::ROUTE_ZOOM_TAKEOFF);
  fromFlags2(data, ui->checkBoxOptionsSimCenterLeg, opts2::ROUTE_AUTOZOOM);
  fromFlags2(data, ui->checkBoxOptionsSimCenterLegTable, opts2::ROUTE_CENTER_ACTIVE_LEG);
  fromFlags2(data, ui->checkBoxOptionsSimClearSelection, opts2::ROUTE_CLEAR_SELECTION);
  fromFlags2(data, ui->checkBoxOptionsSimHighlightActiveTable, opts2::ROUTE_HIGHLIGHT_ACTIVE_TABLE);

  fromFlags2(data, ui->checkBoxDisplayOnlineNameLookup, opts2::ONLINE_AIRSPACE_BY_NAME);
  fromFlags2(data, ui->checkBoxDisplayOnlineFileLookup, opts2::ONLINE_AIRSPACE_BY_FILE);

  fromFlags(data, ui->checkBoxOptionsOnlineRemoveShadow, opts::ONLINE_REMOVE_SHADOW);
  fromFlags(data, ui->checkBoxOptionsGuiTooltipsAll, opts::ENABLE_TOOLTIPS_ALL);
  fromFlags(data, ui->checkBoxOptionsGuiTooltipsMenu, opts::ENABLE_TOOLTIPS_MENU);

  ui->lineEditOptionsRouteFilename->setText(data.flightplanPattern);
  ui->lineEditCacheOfflineDataPath->setText(data.cacheOfflineElevationPath);
  ui->lineEditCacheMapThemeDir->setText(data.cacheMapThemeDir);

  ui->checkBoxOptionsMapTooltipVerbose->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_VERBOSE));
  ui->checkBoxOptionsMapTooltipUserAircraft->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRCRAFT_USER));
  ui->checkBoxOptionsMapTooltipAiAircraft->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRCRAFT_AI));

  ui->checkBoxOptionsMapTooltipAirport->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRPORT));
  ui->checkBoxOptionsMapTooltipNavaid->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_NAVAID));
  ui->checkBoxOptionsMapTooltipAirspace->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRSPACE));
  ui->checkBoxOptionsMapTooltipWind->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_WIND));
  ui->checkBoxOptionsMapTooltipUser->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_MARKS));
  ui->checkBoxOptionsMapTooltipTrail->setChecked(data.displayTooltipOptions.testFlag(optsd::TOOLTIP_AIRCRAFT_TRAIL));

  ui->checkBoxOptionsMapClickAirport->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRPORT));
  ui->checkBoxOptionsMapClickAirportProcs->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRPORT_PROC));
  ui->checkBoxOptionsMapClickNavaid->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_NAVAID));
  ui->checkBoxOptionsMapClickAirspace->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_AIRSPACE));
  ui->checkBoxOptionsMapClickFlightplan->setChecked(data.displayClickOptions.testFlag(optsd::CLICK_FLIGHTPLAN));

  ui->lineEditOptionsWeatherXplanePath->setText(QDir::toNativeSeparators(data.weatherXplane11Path));
  ui->lineEditOptionsWeatherXplane12Path->setText(QDir::toNativeSeparators(data.weatherXplane12Path));
  ui->lineEditOptionsWeatherAsnPath->setText(QDir::toNativeSeparators(data.weatherActiveSkyPath));
  ui->lineEditOptionsWeatherNoaaStationsUrl->setText(data.weatherNoaaUrl);
  ui->lineEditOptionsWeatherVatsimUrl->setText(data.weatherVatsimUrl);
  ui->lineEditOptionsWeatherIvaoUrl->setText(data.weatherIvaoUrl);

  addDatabaseTableItems(ui->tableWidgetOptionsDatabaseInclude, data.databaseInclude);
  addDatabaseTableItems(ui->tableWidgetOptionsDatabaseExclude, data.databaseExclude);
  addDatabaseTableItems(ui->tableWidgetOptionsDatabaseExcludeAddon, data.databaseAddonExclude);

  ui->comboBoxMapScrollZoomDetails->setCurrentIndex(data.mapScrollDetail);

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
  ui->spinBoxOptionsSimZoomOnTakeoff->setValue(static_cast<int>(data.simZoomOnTakeoffDist));
  ui->spinBoxOptionsSimCleanupTableTime->setValue(data.simCleanupTableTime);
  ui->spinBoxOptionsSimUpdateBox->setValue(data.simUpdateBox);
  ui->spinBoxOptionsSimCenterLegZoom->setValue(data.simUpdateBoxCenterLegZoom);
  ui->spinBoxSimMaxTrackPoints->setValue(data.aircraftTrailMaxPoints);
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
  ui->spinBoxOptionsDisplaySymbolSizeHighlight->setValue(data.displaySymbolSizeHighlight);
  ui->spinBoxOptionsDisplayTextSizeUserpoint->setValue(data.displayTextSizeUserpoint);
  ui->spinBoxOptionsDisplayTextSizeNavaid->setValue(data.displayTextSizeNavaid);
  ui->spinBoxOptionsDisplayTextSizeAirspace->setValue(data.displayTextSizeAirspace);
  ui->spinBoxOptionsDisplayThicknessAirway->setValue(data.displayThicknessAirway);
  ui->spinBoxOptionsDisplayThicknessAirspace->setValue(data.displayThicknessAirspace);
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
  ui->spinBoxOptionsDisplayThicknessUserFeature->setValue(data.displayThicknessUserFeature);
  ui->spinBoxOptionsDisplayThicknessMeasurement->setValue(data.displayThicknessMeasurement);
  ui->spinBoxOptionsDisplayThicknessCompassRose->setValue(data.displayThicknessCompassRose);
  ui->spinBoxOptionsDisplaySunShadeDarkness->setValue(data.displaySunShadingDimFactor);
  ui->comboBoxOptionsDisplayTrailType->setCurrentIndex(data.displayTrailType);
  ui->spinBoxOptionsDisplayTransparencyMora->setValue(data.displayTransparencyMora);
  ui->spinBoxOptionsDisplayTextSizeMora->setValue(data.displayTextSizeMora);
  ui->spinBoxOptionsDisplayTransparencyAirportMsa->setValue(data.displayTransparencyAirportMsa);
  ui->spinBoxOptionsDisplayTransparencyAirspace->setValue(data.displayTransparencyAirspace);
  ui->spinBoxOptionsDisplayTextSizeAirportMsa->setValue(data.displayTextSizeAirportMsa);
  ui->spinBoxOptionsMapNavTouchArea->setValue(data.mapNavTouchArea);
  ui->spinBoxOptionsDisplayTextSizeUserFeature->setValue(data.displayTextSizeUserFeature);
  ui->spinBoxOptionsDisplayTextSizeMeasurement->setValue(data.displayTextSizeMeasurement);
  ui->spinBoxOptionsDisplayTextSizeCompassRose->setValue(data.displayTextSizeCompassRose);
  ui->spinBoxOptionsMapHighlightTransparent->setValue(data.displayMapHighlightTransparent);
  ui->comboBoxOptionsStartupUpdateRate->setCurrentIndex(data.updateRate);
  ui->comboBoxOptionsStartupUpdateChannels->setCurrentIndex(data.updateChannels);
  ui->comboBoxOptionsUnitDistance->setCurrentIndex(data.unitDist);
  ui->comboBoxOptionsUnitShortDistance->setCurrentIndex(data.unitShortDist);
  ui->comboBoxOptionsUnitAlt->setCurrentIndex(data.unitAlt);
  ui->comboBoxOptionsUnitSpeed->setCurrentIndex(data.unitSpeed);
  ui->comboBoxOptionsUnitVertSpeed->setCurrentIndex(data.unitVertSpeed);
  ui->comboBoxOptionsUnitCoords->setCurrentIndex(atools::minmax(opts::COORDS_MIN, opts::COORDS_MAX, data.unitCoords));
  ui->comboBoxOptionsUnitFuelWeight->setCurrentIndex(data.unitFuelWeight);
  ui->comboBoxOptionsRouteAltitudeRuleType->setCurrentIndex(data.altitudeRuleType);
  ui->comboBoxOptionsDisplayTrailGradient->setCurrentIndex(data.displayTrailGradientType);

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

  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineClearance, ui->checkBoxDisplayOnlineClearanceRange, data.displayOnlineClearance);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineArea, ui->checkBoxDisplayOnlineAreaRange, data.displayOnlineArea);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineApproach, ui->checkBoxDisplayOnlineApproachRange, data.displayOnlineApproach);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineDeparture, ui->checkBoxDisplayOnlineDepartureRange, data.displayOnlineDeparture);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineFir, ui->checkBoxDisplayOnlineFirRange, data.displayOnlineFir);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineObserver, ui->checkBoxDisplayOnlineObserverRange, data.displayOnlineObserver);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineGround, ui->checkBoxDisplayOnlineGroundRange, data.displayOnlineGround);
  displayOnlineRangeFromData(ui->spinBoxDisplayOnlineTower, ui->checkBoxDisplayOnlineTowerRange, data.displayOnlineTower);

  ui->spinBoxOptionsWebPort->setValue(data.webPort);
  ui->checkBoxOptionsWebEncrypted->setChecked(data.webEncrypted);
  ui->lineEditOptionsWebDocroot->setText(QDir::toNativeSeparators(data.webDocumentRoot));

  ui->lineEditOptionsWeatherXplaneWind->setText(data.weatherXplaneWind);
  ui->lineEditOptionsWeatherNoaaWindUrl->setText(data.weatherNoaaWindBaseUrl);

  mapThemeKeysToWidget(data);
}

void OptionsDialog::mapThemeKeyEdited(QTableWidgetItem *item)
{
  qDebug() << Q_FUNC_INFO;

  if(item->flags().testFlag(Qt::ItemIsEditable))
    ui->tableWidgetOptionsMapKeys->editItem(item);
}

void OptionsDialog::searchTextEdited(const QString& text)
{
  listWidgetIndex->find(text);
}

void OptionsDialog::widgetToMapThemeKeys(OptionData& data)
{
  data.mapThemeKeys.clear();
  for(int row = 0; row < ui->tableWidgetOptionsMapKeys->rowCount(); row++)
    data.mapThemeKeys.insert(ui->tableWidgetOptionsMapKeys->item(row, 0)->text().trimmed(),
                             ui->tableWidgetOptionsMapKeys->item(row, 1)->text().trimmed());
}

void OptionsDialog::mapThemeKeysToWidget(const OptionData& data)
{
  ui->tableWidgetOptionsMapKeys->clear();

  if(data.mapThemeKeys.isEmpty())
  {
    // Add single message for empty list =======================================
    ui->tableWidgetOptionsMapKeys->setSelectionMode(QAbstractItemView::NoSelection);
    ui->tableWidgetOptionsMapKeys->clearSelection();

    ui->tableWidgetOptionsMapKeys->setRowCount(1);
    ui->tableWidgetOptionsMapKeys->setColumnCount(1);
    ui->tableWidgetOptionsMapKeys->setItem(0, 0, new QTableWidgetItem(tr("No keys found in DGML map configuration files.")));
  }
  else
  {
    // Fill table with key/value pairs =======================================
    ui->tableWidgetOptionsMapKeys->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidgetOptionsMapKeys->setRowCount(data.mapThemeKeys.size());
    ui->tableWidgetOptionsMapKeys->setColumnCount(2);
    ui->tableWidgetOptionsMapKeys->setHorizontalHeaderLabels({tr(" API Key / Username / Token "), tr(" Value ")});

    int index = 0;
    for(auto it = data.mapThemeKeys.constBegin(); it != data.mapThemeKeys.constEnd(); ++it)
    {
      // Item for key name ==================
      QTableWidgetItem *item = new QTableWidgetItem(it.key());
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      ui->tableWidgetOptionsMapKeys->setItem(index, 0, item);

      // Editable item for key value ==================
      QString value = it.value();
      item = new QTableWidgetItem(value);
      item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setToolTip(tr("Double click to edit key value"));
      ui->tableWidgetOptionsMapKeys->setItem(index, 1, item);

      index++;
    }
  }
  ui->tableWidgetOptionsMapKeys->resizeColumnsToContents();
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

void OptionsDialog::mapThemeDirSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).
                 openDirectoryDialog(tr("Select directory for map themes"), QString(), ui->lineEditCacheMapThemeDir->text());

  if(!path.isEmpty())
    ui->lineEditCacheMapThemeDir->setText(QDir::toNativeSeparators(path));

  updateCacheMapThemeDir();
}

void OptionsDialog::updateCacheMapThemeDir()
{
  ui->labelCacheMapThemeDir->setText(MapThemeHandler::getStatusTextForDir(ui->lineEditCacheMapThemeDir->text()));
}

void OptionsDialog::offlineDataSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).
                 openDirectoryDialog(tr("Open GLOBE data directory"), QString(), ui->lineEditCacheOfflineDataPath->text());

  if(!path.isEmpty())
    ui->lineEditCacheOfflineDataPath->setText(QDir::toNativeSeparators(path));

  updateCacheElevationStates();
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
      else if(!ElevationProvider::isGlobeDirectoryValid(path))
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
  WeatherReporter *weatherReporter = NavApp::getWeatherReporter();

  if(weatherReporter != nullptr)
  {
    bool hasAs = weatherReporter->hasAnyActiveSkyWeather();
    ui->checkBoxOptionsWeatherInfoAsn->setEnabled(hasAs);
    ui->checkBoxOptionsWeatherTooltipAsn->setEnabled(hasAs);

    ui->pushButtonOptionsWeatherNoaaTest->setEnabled(!ui->lineEditOptionsWeatherNoaaStationsUrl->text().isEmpty());
    ui->pushButtonOptionsWeatherVatsimTest->setEnabled(!ui->lineEditOptionsWeatherVatsimUrl->text().isEmpty());
    ui->pushButtonOptionsWeatherIvaoTest->setEnabled(!ui->lineEditOptionsWeatherIvaoUrl->text().isEmpty());
    ui->pushButtonOptionsWeatherNoaaWindTest->setEnabled(!ui->lineEditOptionsWeatherNoaaWindUrl->text().isEmpty());

    updateActiveSkyPathStatus();
    updateXplane11PathStatus();
    updateXplane12PathStatus();
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
      ui->labelOptionsWeatherAsnPathState->setText(HtmlBuilder::errorMessage(tr("Is not an Active Sky weather snapshot file.")));
    else
      ui->labelOptionsWeatherAsnPathState->setText(
        tr("Weather snapshot file is valid. Using selected for all simulators"));
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

      case WeatherReporter::ASP5:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for ASP5 for %1.").arg(sim);
        break;

      case WeatherReporter::ASXPL11:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for Active Sky XP 11 for %1.").arg(sim);
        break;

      case WeatherReporter::ASXPL12:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for Active Sky XP 12 for %1.").arg(sim);
        break;
    }

    ui->labelOptionsWeatherAsnPathState->setText(text);
  }
}

void OptionsDialog::updateXplane11PathStatus()
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
      ui->labelOptionsWeatherXplanePathState->setText(tr("Weather file is valid. Using selected for X-Plane 11."));
  }
  else
    // No manual path set
    ui->labelOptionsWeatherXplanePathState->setText(tr("Using default weather from X-Plane 11 base path."));
}

void OptionsDialog::updateXplane12PathStatus()
{
  const QString& path = ui->lineEditOptionsWeatherXplane12Path->text();

  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelOptionsWeatherXplane12PathState->setText(HtmlBuilder::errorMessage(tr("Directory does not exist.")));
    else if(!fileinfo.isDir())
      ui->labelOptionsWeatherXplane12PathState->setText(HtmlBuilder::errorMessage(tr("Is not a directory.")));
    else
      ui->labelOptionsWeatherXplane12PathState->setText(tr("Weather directory is valid. Using selected for X-Plane 12."));
  }
  else
    // No manual path set
    ui->labelOptionsWeatherXplane12PathState->setText(tr("Using default weather from X-Plane 12 base path."));
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
      ui->labelOptionsWeatherXplaneWindPathState->setText(HtmlBuilder::errorMessage(tr("Is not a X-Plane 11 wind file.")));
    else
      ui->labelOptionsWeatherXplaneWindPathState->setText(tr("X-Plane 11 wind file is valid."));
  }
  else
    ui->labelOptionsWeatherXplaneWindPathState->setText(tr("Using default X-Plane 11 wind file."));
}

void OptionsDialog::selectActiveSkyPathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Select Active Sky Weather Snapshot File"),
    tr("Active Sky Weather Snapshot Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AS_SNAPSHOT),
    lnm::OPTIONS_DIALOG_AS_FILE_DLG, ui->lineEditOptionsWeatherAsnPath->text());

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherAsnPath->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::selectXplane11PathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = ui->lineEditOptionsWeatherXplanePath->text();

  if(path.isEmpty() || QFileInfo(path).isRelative())
    path = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_11) % path;

  path = atools::gui::Dialog(this).openFileDialog(tr("Select X-Plane 11 METAR File"),
                                                  tr("X-Plane METAR Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_XPLANE_METAR),
                                                  lnm::OPTIONS_DIALOG_XPLANE_DLG, path);

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherXplanePath->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::selectXplane12PathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = ui->lineEditOptionsWeatherXplane12Path->text();

  if(path.isEmpty())
    path = "Output" % atools::SEP % "real weather";

  if(QFileInfo(path).isRelative())
    path = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_12) % path;

  path = atools::gui::Dialog(this).openDirectoryDialog(tr("Select X-Plane 12 Weather Directory"), lnm::OPTIONS_DIALOG_XPLANE12_DLG, path);

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherXplane12Path->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::weatherXplane11WindPathSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = ui->lineEditOptionsWeatherXplaneWind->text();
  if(path.isEmpty() || QFileInfo(path).isRelative())
    path = NavApp::getSimulatorBasePath(atools::fs::FsPaths::XPLANE_11) % path;

  // global_winds.grib
  path = atools::gui::Dialog(this).openFileDialog(tr("Select X-Plane 11 Wind File"),
                                                  tr("X-Plane 11 Wind Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GRIB),
                                                  lnm::OPTIONS_DIALOG_XPLANE_WIND_FILE_DLG, path);

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherXplaneWind->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::updateGuiWidgets()
{
  ui->checkBoxOptionsGuiTooltipsMenu->setEnabled(ui->checkBoxOptionsGuiTooltipsAll->isChecked());
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

/* Opens the disk cache in explorer, finder, whatever */
void OptionsDialog::showDiskCacheClicked()
{
  QUrl url = QUrl::fromLocalFile(Marble::MarbleDirs::localPath() % atools::SEP % "maps" % atools::SEP % "earth");

  if(!QDesktopServices::openUrl(url))
    atools::gui::Dialog::warning(this, tr("Error opening disk cache \"%1\"").arg(url.toDisplayString()));
}

QListWidgetItem *OptionsDialog::pageListItem(QListWidget *parent, const QString& text, const QString& tooltip, const QString& iconPath)
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

      if(urls.isEmpty())
        ui->labelOptionsWebStatus->setText(tr("No valid address found."));
      else if(urls.size() == 1)
        ui->labelOptionsWebStatus->setText(tr("Web Server is running at the address:<br/>%1").arg(urls.constFirst()));
      if(urls.size() > 1)
        ui->labelOptionsWebStatus->setText(tr("Web Server is running at addresses:<br/>%1").arg(urls.join("<br/>")));

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
    else if(!QFileInfo::exists(path + atools::SEP + "index.html"))
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
      // Update options from page before starting and restart
      updateWebOptionsFromGui();

    updateWebServerStatus();
  }
}

void OptionsDialog::updateWebOptionsFromData()
{
  qDebug() << Q_FUNC_INFO;
  WebController *webController = NavApp::getWebController();
  if(webController != nullptr)
  {
    OptionData& data = OptionData::instanceInternal();

    // Detect changed parameters
    QString root = QDir::fromNativeSeparators(QFileInfo(data.getWebDocumentRoot()).canonicalFilePath());
    int port = data.getWebPort();
    bool encrypted = data.isWebEncrypted();
    bool changed = root != webController->getDocumentRoot() || port != webController->getPort() ||
                   encrypted != webController->isEncrypted();

    webController->setDocumentRoot(root);
    webController->setPort(port);
    webController->setEncrypted(encrypted);

    // Restart if it is running and any paramters were changed
    if(changed && webController->isRunning())
      webController->restartServer(false /* force */);
  }
}

void OptionsDialog::updateWebOptionsFromGui()
{
  qDebug() << Q_FUNC_INFO;

  WebController *webController = NavApp::getWebController();
  if(webController != nullptr)
  {
    webController->setDocumentRoot(QDir::fromNativeSeparators(QFileInfo(ui->lineEditOptionsWebDocroot->text()).canonicalFilePath()));
    webController->setPort(ui->spinBoxOptionsWebPort->value());
    webController->setEncrypted(ui->checkBoxOptionsWebEncrypted->isChecked());
    webController->restartServer(true /* force */); // Restart always
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
    QString errorMsg;
    QString example = atools::fs::pln::Flightplan::getFilenamePatternExample(ui->lineEditOptionsRouteFilename->text() + ".lnmpln",
                                                                             ".lnmpln", true /* html */, &errorMsg);

    QString text = tr("Example: \"%1\"").arg(example);

    if(!errorMsg.isEmpty())
      text.append(tr("<br/>") % atools::util::HtmlBuilder::errorMessage(errorMsg));

    ui->labelOptionsRouteFilenameExample->setText(text);
  }
  else
    ui->labelOptionsRouteFilenameExample->setText(
      atools::util::HtmlBuilder::warningMessage(tr("Pattern is empty. Using default \"%1\".").arg(atools::fs::pln::pattern::SHORT)));
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
  NavApp::setStayOnTop(fontDialog);
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
  NavApp::setStayOnTop(fontDialog);
  if(fontDialog->exec())
  {
    QFont selfont = fontDialog->selectedFont();

    // Limit size to keep the user from messing up the UI without an option to change
    bool corrected = false;
    if(selfont.pointSizeF() > 30.)
    {
      selfont.setPointSizeF(30.);
      corrected = true;
    }

    if(selfont.pixelSize() > 30)
    {
      selfont.setPixelSize(30);
      corrected = true;
    }

    if(corrected)
      QMessageBox::warning(this, QApplication::applicationName(),
                           tr("Font too large for user interface. Size was corrected. Maximum is 30 pixels/points."));

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

void OptionsDialog::mapboxUserMapClicked()
{
  qDebug() << Q_FUNC_INFO;
  static const QLatin1String USERNAME_KEY("Mapbox Username"); // Hardcoded and linked to variables in "mapboxuser.dgml"
  static const QLatin1String USERSTYLE_KEY("Mapbox User Style");
  static const QLatin1String TOKEN_KEY("Mapbox Token");

  QString label = tr(
    "<p>Here you can enter a Mapbox User Style URL.</p>"
      "<p>Open the Mapbox Studio, login and click on the three-dot menu button of your style.<br/> "
        "Then click on the copy icon of the \"Style URL\" to add it to the clipboard and enter it below.</p>"
        "<p>A style URL looks like \"mapbox://styles/USERNAME/STYLEID\".</p>"
          "<p><a href=\"https://studio.mapbox.com/\"><b>Click here to open the Mapbox Studio page in your browser</b></a></p>");

  QString label2 = tr(
    "<p>You can also to provide you Mapbox Access Token below if not already done.<br/>"
    "You can find the Token on your Mapbox Account page.</p>"
    "<p><a href=\"https://account.mapbox.com/\"><b>Click here to open the Mapbox Account page in your browser</b></a></p>");

  // Collect all editable items by key ======================
  QTableWidgetItem *userNameItem = nullptr, *userStyleItem = nullptr, *tokenItem = nullptr;
  for(int row = 0; row < ui->tableWidgetOptionsMapKeys->rowCount(); row++)
  {
    QTableWidgetItem *item = ui->tableWidgetOptionsMapKeys->item(row, 0);
    if(item->text() == USERNAME_KEY)
      userNameItem = ui->tableWidgetOptionsMapKeys->item(row, 1);
    else if(item->text() == USERSTYLE_KEY)
      userStyleItem = ui->tableWidgetOptionsMapKeys->item(row, 1);
    else if(item->text() == TOKEN_KEY)
      tokenItem = ui->tableWidgetOptionsMapKeys->item(row, 1);
  }

  if(userNameItem != nullptr && userStyleItem != nullptr && tokenItem != nullptr)
  {
    TextEditDialog dialog(this, QApplication::applicationName() % tr(" - Enter Mapbox Keys"), label, label2,
                          "OPTIONS.html#mapboxtheme");

    // Prefill with present keys ==============
    dialog.setText(QString("mapbox://styles/%1/%2").arg(userNameItem->text()).arg(userStyleItem->text()));
    dialog.setText2(tokenItem->text());

    if(dialog.exec() == QDialog::Accepted)
    {
      QString url = dialog.getText().simplified();
      // now "mapbox://styles/USERNAME/STYLEID"

      if(url.startsWith("mapbox://styles/", Qt::CaseInsensitive))
      {
        url.replace("mapbox://styles/", QString(), Qt::CaseInsensitive);
        // Now "USERNAME/STYLEID"

        QString userName = url.section('/', 0, 0);
        if(!userName.isEmpty())
        {
          QString styleId = url.section('/', 1, 1);
          if(!styleId.isEmpty())
          {
            if(!dialog.getText2().isEmpty())
            {
              // Write values directly into the table widget to allow undoing the changes on cancel
              userNameItem->setText(userName.trimmed());
              userStyleItem->setText(styleId.trimmed());
              tokenItem->setText(dialog.getText2().trimmed());
            }
            else
              QMessageBox::warning(this, QApplication::applicationName(), tr("Mapbox Token is empty."));
          }
          else
            QMessageBox::warning(this, QApplication::applicationName(), tr("Mapbox User Style not found in URL."));
        }
        else
          QMessageBox::warning(this, QApplication::applicationName(), tr("Mapbox Username not found in URL."));
      }
      else
        QMessageBox::warning(this, QApplication::applicationName(), tr("Style URL has to start with \"mapbox://styles/\"."));
    }
  }
  else
    QMessageBox::warning(this, QApplication::applicationName(), tr("One or more Mapbox keys are missing. "
                                                                   "Installation might be incomplete since map themes are missing."));
}

void OptionsDialog::removeSelectedDatabaseTableItems(QTableWidget *widget)
{
  // Create list in reverse order so that deleting can start at the bottom of the list
  for(int row : atools::gui::util::getSelectedIndexesInDeletionOrder(widget->selectionModel()))
    // Item removes itself from the list when deleted
    widget->removeRow(row);

  widget->resizeColumnToContents(0);
}

void OptionsDialog::addDatabaseTableItem(QTableWidget *widget, const QString& path)
{
  if(!path.isEmpty())
  {
    widget->insertRow(widget->rowCount());
    widget->setItem(widget->rowCount() - 1, 0, new QTableWidgetItem(QDir::toNativeSeparators(path)));
    widget->resizeColumnToContents(0);
  }
}

void OptionsDialog::addDatabaseTableItems(QTableWidget *widget, const QStringList& strings)
{
  widget->clear();
  widget->setRowCount(strings.size());
  widget->setColumnCount(1);

  for(int row = 0; row < strings.size(); row++)
    widget->setItem(row, 0, new QTableWidgetItem(strings.at(row)));
  widget->resizeColumnToContents(0);
}
