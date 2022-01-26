/*****************************************************************************
* Copyright 2015-2021 Alexander Barthel alex@littlenavmap.org
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

#include "info/aircraftprogressconfig.h"
#include "gui/treedialog.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "atools.h"

#include <QDebug>

AircraftProgressConfig::AircraftProgressConfig(QWidget *parentWidget)
  : parent(parentWidget)
{

}

// All available ids. Keep this updated with pid::ProgressConfId
const static QVector<pid::ProgressConfId> ALLIDS({
  pid::DATE_TIME, pid::LOCAL_TIME, pid::DATE_TIME_REAL, pid::LOCAL_TIME_REAL, pid::FLOWN, pid::DEST_DIST_TIME_ARR, pid::DEST_FUEL,
  pid::DEST_GROSS_WEIGHT, pid::TOD_DIST_TIME_ARR, pid::TOD_FUEL, pid::TOD_TO_DESTINATION, pid::TOC_DIST_TIME_ARR, pid::TOC_FUEL,
  pid::TOC_FROM_DESTINATION, pid::NEXT_LEG_TYPE, pid::NEXT_INSTRUCTIONS, pid::NEXT_RELATED, pid::NEXT_RESTRICTION, pid::NEXT_DIST_TIME_ARR,
  pid::NEXT_ALTITUDE, pid::NEXT_FUEL, pid::NEXT_COURSE_TO_WP, pid::NEXT_LEG_COURSE, pid::NEXT_HEADING, pid::NEXT_CROSS_TRACK_DIST,
  pid::NEXT_REMARKS, pid::AIRCRAFT_HEADING, pid::AIRCRAFT_TRACK, pid::AIRCRAFT_FUEL_FLOW, pid::AIRCRAFT_ENDURANCE, pid::AIRCRAFT_FUEL,
  pid::AIRCRAFT_GROSS_WEIGHT, pid::AIRCRAFT_ICE, pid::ALT_INDICATED, pid::ALT_ACTUAL, pid::ALT_ABOVE_GROUND, pid::ALT_GROUND_ELEVATION,
  pid::ALT_AUTOPILOT_ALT, pid::SPEED_INDICATED, pid::SPEED_GROUND, pid::SPEED_TRUE, pid::SPEED_MACH, pid::SPEED_VERTICAL,
  pid::DESCENT_DEVIATION, pid::DESCENT_ANGLE_SPEED, pid::ENV_WIND_DIR_SPEED, pid::ENV_TAT, pid::ENV_SAT, pid::ENV_ISA_DEV,
  pid::ENV_SEA_LEVEL_PRESS, pid::ENV_CONDITIONS, pid::ENV_VISIBILITY, pid::POS_COORDINATES});

// Default ids which are enabled with out settings
const static QVector<pid::ProgressConfId> DEFAULTIDS({
  pid::DATE_TIME, pid::LOCAL_TIME, pid::FLOWN, pid::DEST_DIST_TIME_ARR, pid::DEST_FUEL, pid::DEST_GROSS_WEIGHT, pid::TOD_DIST_TIME_ARR,
  pid::TOD_FUEL, pid::TOD_TO_DESTINATION, pid::NEXT_LEG_TYPE, pid::NEXT_INSTRUCTIONS, pid::NEXT_RELATED, pid::NEXT_RESTRICTION,
  pid::NEXT_DIST_TIME_ARR, pid::NEXT_ALTITUDE, pid::NEXT_FUEL, pid::NEXT_COURSE_TO_WP, pid::NEXT_LEG_COURSE, pid::NEXT_HEADING,
  pid::NEXT_CROSS_TRACK_DIST, pid::NEXT_REMARKS, pid::AIRCRAFT_HEADING, pid::AIRCRAFT_TRACK, pid::AIRCRAFT_FUEL_FLOW,
  pid::AIRCRAFT_ENDURANCE, pid::AIRCRAFT_ICE, pid::ALT_INDICATED, pid::ALT_ACTUAL, pid::ALT_ABOVE_GROUND, pid::ALT_GROUND_ELEVATION,
  pid::ALT_AUTOPILOT_ALT, pid::SPEED_INDICATED, pid::SPEED_GROUND, pid::SPEED_TRUE, pid::SPEED_MACH, pid::SPEED_VERTICAL,
  pid::DESCENT_DEVIATION, pid::DESCENT_ANGLE_SPEED, pid::ENV_WIND_DIR_SPEED, pid::ENV_TAT, pid::ENV_SAT, pid::ENV_ISA_DEV,
  pid::ENV_SEA_LEVEL_PRESS, pid::ENV_CONDITIONS, pid::ENV_VISIBILITY});

void AircraftProgressConfig::progressConfiguration()
{
  atools::gui::TreeDialog treeDialog(parent, QApplication::applicationName() + tr(" - Aircraft Progress Configuration"),
                                     tr("Select the fields to show in the aircraft progress tab.\n"
                                        "Note that some fields are only shown if certain conditions apply."),
                                     lnm::INFOWINDOW_PROGRESS_FIELD_DIALOG, "INFO.html#progress-field-configuration",
                                     true /* showExplandCollapse */);

  treeDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  treeDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());
  treeDialog.setHeader({tr("Field or Header"), tr("Description")});

  /* *INDENT-OFF* */
  // =====================================================================================================================
  /*: This and all following texts have to match the ones in HtmlInfoBuilder::aircraftProgressText() */
  QTreeWidgetItem *rootItem = treeDialog.getRootItem();
  treeDialog.addItem2(rootItem, pid::DATE_TIME_REAL,  tr("Real Date and Time"), tr("Real date and UTC time."), QString());
  treeDialog.addItem2(rootItem, pid::LOCAL_TIME_REAL, tr("Real Local Time"), tr("Real local date and time."), QString());
  treeDialog.addItem2(rootItem, pid::DATE_TIME,       tr("Simulator Date and Time"), tr("Simulator date and UTC time."), QString());
  treeDialog.addItem2(rootItem, pid::LOCAL_TIME,      tr("Simulator Local Time"), tr("Simulator local date and time."), QString());
  treeDialog.addItem2(rootItem, pid::FLOWN,           tr("Flown"), tr("Flown distance since takeoff."), QString());

  // Destination ==========================================================================================================
  QTreeWidgetItem *destItem = treeDialog.addTopItem1(tr("Destination"));
  treeDialog.addItem2(destItem, pid::DEST_DIST_TIME_ARR, tr("Distance, Time and Arrival"), tr("Distance to, time to and arrival time in UTC at destination."), QString());
  treeDialog.addItem2(destItem, pid::DEST_FUEL,          tr("Fuel"), tr("Estimated fuel at destination.\n"
                                                                        "Shows orange warning if below reserve and red error text if insufficient."), QString());
  treeDialog.addItem2(destItem, pid::DEST_GROSS_WEIGHT,  tr("Gross Weight"), tr("Estimated aircraft gross weight at destination."), QString());

  // TOC ==========================================================================================================
  QTreeWidgetItem *tocItem = treeDialog.addTopItem1(tr("Top of Climb"));
  treeDialog.addItem2(tocItem, pid::TOC_DIST_TIME_ARR,  tr("Distance, Time and Arrival"), tr("Distance to, time to and arrival time in UTC at the top of climb."), QString());
  treeDialog.addItem2(tocItem, pid::TOC_FUEL,           tr("Fuel"), tr("Estimated fuel at top of climb."), QString());
  treeDialog.addItem2(tocItem, pid::TOC_FROM_DESTINATION, tr("From Departure"), tr("Distance from departure to top of climb."), QString());

  // TOD ==========================================================================================================
  QTreeWidgetItem *todItem = treeDialog.addTopItem1(tr("Top of Descent"));
  treeDialog.addItem2(todItem, pid::TOD_DIST_TIME_ARR,  tr("Distance, Time and Arrival"), tr("Distance to, time to and arrival time in UTC at the top of descent."), QString());
  treeDialog.addItem2(todItem, pid::TOD_FUEL,           tr("Fuel"), tr("Estimated fuel at top of descent."), QString());
  treeDialog.addItem2(todItem, pid::TOD_TO_DESTINATION, tr("To Destination"), tr("Distance from top of descent to destination."), QString());

  // Next ==========================================================================================================
  QTreeWidgetItem *nextItem = treeDialog.addTopItem1(tr("Next Waypoint"));
  treeDialog.addItem2(nextItem, pid::NEXT_LEG_TYPE,         tr("Leg Type"), tr("Type of a procedure leg describing the flying path."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_INSTRUCTIONS,     tr("Instructions"), tr("Procedure instructions showing how to fly,\n"
                                                                                   "like turn direction or required overfly."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_RELATED,          tr("Related Navaid"), tr("A related navaid for a procedure fix describing\n"
                                                                                     "the fix position relative to the navaid."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_RESTRICTION,      tr("Restriction"), tr("Altitude, speed or vertical angle restriction."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_DIST_TIME_ARR,    tr("Distance, Time and Arrival"), tr("Distance to, time to and arrival time in UTC at the next waypoint."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_ALTITUDE,         tr("Altitude"), tr("Calculated altitude at the next waypoint."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_FUEL,             tr("Fuel"), tr("Estimated fuel at the next waypoint."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_COURSE_TO_WP,     tr("Course to waypoint"), tr("Direct course to the next waypoint."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_LEG_COURSE,       tr("Leg Course"), tr("Course of the current active flight plan leg."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_HEADING,          tr("Heading"), tr("Heading to fly to the next waypoint considering wind."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_CROSS_TRACK_DIST, tr("Cross Track Distance"), tr("Distance to flight plan leg.\n"
                                                                                           "► means aircraft is left of flight plan leg (fly right) and ◄ means right of leg."), QString());
  treeDialog.addItem2(nextItem, pid::NEXT_REMARKS,              tr("Remarks"), tr("User entered remarks for an user flight plan position."), QString());

  // Aircraft ==========================================================================================================
  QTreeWidgetItem *aircraftItem = treeDialog.addTopItem1(tr("Aircraft"));
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_HEADING,      tr("Heading"), tr("Aircraft heading."), QString());
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_TRACK,        tr("Track"), tr("Flown aircraft track considering wind."), QString());
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_FUEL_FLOW,    tr("Fuel Flow"), tr("Current fuel flow for all engines."), QString());
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_FUEL,         tr("Fuel"), tr("Current fuel amount on board."), QString());
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_GROSS_WEIGHT, tr("Gross Weight"), tr("Current aircraft gross weight."), QString());
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_ENDURANCE,    tr("Endurance"), tr("Estimated endurance based on current fuel flow and groundspeed\n"
                                                                                    "considering reserves and contingency. Only shown if airborne.\n"
                                                                                    "Shows orange warning if below reserve and red error text if insufficient\n"
                                                                                    "when no flightplan is used."), QString());
  treeDialog.addItem2(aircraftItem, pid::AIRCRAFT_ICE,          tr("Ice"), tr("Aircraft icing, if any."), QString());

  // Altitude ==========================================================================================================
  QTreeWidgetItem *altitudeItem = treeDialog.addTopItem1(tr("Altitude"));
  treeDialog.addItem2(altitudeItem, pid::ALT_INDICATED,        tr("Indicated"), tr("Indicated altitude considering aircraft barometer setting."), QString());
  treeDialog.addItem2(altitudeItem, pid::ALT_ACTUAL,           tr("Actual"), tr("Actual altitude."), QString());
  treeDialog.addItem2(altitudeItem, pid::ALT_ABOVE_GROUND,     tr("Above Ground"), tr("Altitude above ground as reported by simulator."), QString());
  treeDialog.addItem2(altitudeItem, pid::ALT_GROUND_ELEVATION, tr("Ground Elevation"), tr("Ground elevation above normal as reported by simulator."), QString());
  treeDialog.addItem2(altitudeItem, pid::ALT_AUTOPILOT_ALT,    tr("Autopilot Selected"), tr("Preselected altitude in autopilot."), QString());

  // Speed ==========================================================================================================
  QTreeWidgetItem *speedItem = treeDialog.addTopItem1(tr("Speed"));
  treeDialog.addItem2(speedItem, pid::SPEED_INDICATED, tr("Indicated"), tr("Aircraft indicated airspeed.\n"
                                                                           "Shows orange and red if faster than 250 kts below %L1 ft.").arg(250).arg(10000), QString());
  treeDialog.addItem2(speedItem, pid::SPEED_GROUND,    tr("Ground"), tr("Aircraft groundspeed."), QString());
  treeDialog.addItem2(speedItem, pid::SPEED_TRUE,      tr("True Airspeed"), tr("Aircraft true airspeed."), QString());
  treeDialog.addItem2(speedItem, pid::SPEED_MACH,      tr("Mach"), tr("Aircraft mach number."), QString());
  treeDialog.addItem2(speedItem, pid::SPEED_VERTICAL,  tr("Vertical"), tr("Aircraft vertical speed."), QString());

  // Descent ==========================================================================================================
  QTreeWidgetItem *descentItem = treeDialog.addTopItem1(tr("Descent Path"));
  treeDialog.addItem2(descentItem, pid::DESCENT_DEVIATION,   tr("Deviation"), tr("Vertical altitude deviation from descent path.\n"
                                                                                 "▼ means above (increase sink rate) and ▲ means below (decrease sink rate)."), QString());
  treeDialog.addItem2(descentItem, pid::DESCENT_ANGLE_SPEED, tr("Angle and Speed"), tr("Vertical flight path angle needed to keep the vertical path angle.\n"
                                                                                       "Changes to \"Required angle\" if mandatory in approach procedures."), QString());

  // Environment ==========================================================================================================
  QTreeWidgetItem *envItem = treeDialog.addTopItem1(tr("Environment"));
  treeDialog.addItem2(envItem, pid::ENV_WIND_DIR_SPEED,  tr("Wind Direction and Speed"), tr("Wind direction and speed at aircraft.\n"
                                                                                            "Second line shows headwind indicated by ▼ and tailwind by ▲\n"
                                                                                            "as well as crosswind (► or ◄)."), QString());
  treeDialog.addItem2(envItem, pid::ENV_TAT,             tr("Total Air Temperature"), tr("Total air temperature (TAT).\n"
                                                                                         "Also indicated air temperature (IAT) or ram air temperature."), QString());
  treeDialog.addItem2(envItem, pid::ENV_SAT,             tr("Static Air Temperature"), tr("Static air temperature (SAT).\n"
                                                                                          "Also outside air temperature (OAT) or true air temperature."), QString());
  treeDialog.addItem2(envItem, pid::ENV_ISA_DEV,         tr("ISA Deviation"), tr("Deviation from standard temperature model."), QString());
  treeDialog.addItem2(envItem, pid::ENV_SEA_LEVEL_PRESS, tr("Sea Level Pressure"), tr("Barometric pressure at sea level."), QString());
  treeDialog.addItem2(envItem, pid::ENV_CONDITIONS,      tr("Conditions"), tr("Rain, snow or other weather conditions."), QString());
  treeDialog.addItem2(envItem, pid::ENV_VISIBILITY,      tr("Visibility"), tr("Horizontal visibility at aircraft."), QString());

  // Coordinates ==========================================================================================================
  treeDialog.addItem2(rootItem, pid::POS_COORDINATES, tr("Coordinates"), tr("Aircraft coordinates."), QString());
  /* *INDENT-ON* */

  treeDialog.restoreState(false /* restoreCheckState */, true /* restoreExpandState */);

  // Check all items from enabled
  treeDialog.setAllChecked(false);
  for(pid::ProgressConfId id : enabledIds)
    treeDialog.setItemChecked(id);

  if(treeDialog.exec() == QDialog::Accepted)
  {
    treeDialog.saveState(false /* saveCheckState */, true /* saveExpandState */);

    enabledIds.clear();
    for(pid::ProgressConfId id : ALLIDS)
    {
      if(treeDialog.isItemChecked(id))
        enabledIds.append(id);
    }
    updateBits();
  }
}

void AircraftProgressConfig::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::INFOWINDOW_PROGRESS_FIELDS, atools::numVectorToStrList(enabledIds).join(";"));
}

void AircraftProgressConfig::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  if(settings.contains(lnm::INFOWINDOW_PROGRESS_FIELDS))
  {
    // Load from settings
    QString idStr = settings.valueStr(lnm::INFOWINDOW_PROGRESS_FIELDS);
    enabledIds = atools::strListToNumVector<pid::ProgressConfId>(idStr.split(";", QString::SkipEmptyParts));
  }
  else
    // Not saved yet - use defaults
    enabledIds = DEFAULTIDS;

  updateBits();
}

void AircraftProgressConfig::updateBits()
{
  // Update bitfield for HtmlBuilder
  enabledBits.fill(false, pid::LAST + 1);
  for(pid::ProgressConfId id : enabledIds)
    enabledBits.setBit(id, true);
}
