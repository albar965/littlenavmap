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

#include "common/maptypes.h"

#include "atools.h"
#include "geo/calculations.h"
#include "common/unit.h"
#include "options/optiondata.h"
#include "navapp.h"
#include "common/proctypes.h"

#include <QDataStream>
#include <QHash>
#include <QObject>
#include <QRegularExpression>

namespace map {

static QHash<QString, QString> surfaceMap;

/* Short size name for gate and full name for others */
static QHash<QString, QString> parkingMapGate;

/* Short size name for parking and full name for others */
static QHash<QString, QString> parkingMapRamp;

/* Full name for all parking including type */
static QHash<QString, QString> parkingTypeMap;

static QHash<QString, QString> parkingNameMap;

static QHash<QString, QString> parkingDatabaseNameMap;

static QHash<QString, QString> navTypeNamesVor;

static QHash<QString, QString> navTypeNamesVorLong;

static QHash<QString, QString> navTypeNamesNdb;

static QHash<QString, QString> navTypeNamesWaypoint;

static QHash<QString, QString> navTypeNames;

static QHash<QString, QString> comTypeNames;

static QHash<map::MapAirspaceTypes, QString> airspaceTypeNameMap;

static QHash<map::MapAirspaceFlags, QString> airspaceFlagNameMap;

static QHash<map::MapAirspaceTypes, QString> airspaceRemarkMap;

void initTranslateableTexts()
{
  surfaceMap = QHash<QString, QString>(
    {
      {"C", QObject::tr("Concrete")},
      {"G", QObject::tr("Grass")},
      {"W", QObject::tr("Water")},
      {"A", QObject::tr("Asphalt")},
      {"CE", QObject::tr("Cement")},
      {"CL", QObject::tr("Clay")},
      {"SN", QObject::tr("Snow")},
      {"I", QObject::tr("Ice")},
      {"D", QObject::tr("Dirt")},
      {"CR", QObject::tr("Coral")},
      {"GR", QObject::tr("Gravel")},
      {"OT", QObject::tr("Oil treated")},
      {"SM", QObject::tr("Steel Mats")},
      {"B", QObject::tr("Bituminous")},
      {"BR", QObject::tr("Brick")},
      {"M", QObject::tr("Macadam")},
      {"PL", QObject::tr("Planks")},
      {"S", QObject::tr("Sand")},
      {"SH", QObject::tr("Shale")},
      {"T", QObject::tr("Tarmac")},
      {"TR", QObject::tr("Transparent")},
      {"UNKNOWN", QObject::tr("Unknown")},
      {"INVALID", QObject::tr("Invalid")}
    });

  /* Short size name for gate and full name for others */
  parkingMapGate = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"UNKNOWN", QObject::tr("Unknown")},
      {"RGA", QObject::tr("Ramp GA")},
      {"RGAS", QObject::tr("Ramp GA Small")},
      {"RGAM", QObject::tr("Ramp GA Medium")},
      {"RGAL", QObject::tr("Ramp GA Large")},
      {"RC", QObject::tr("Ramp Cargo")},
      {"RM", QObject::tr("Ramp Mil")},
      {"RMC", QObject::tr("Ramp Mil Cargo")},
      {"RMCB", QObject::tr("Ramp Mil Combat")},
      {"T", QObject::tr("Tie down")},
      {"H", QObject::tr("Hangar")},
      {"G", QString()},
      {"GS", QObject::tr("Small")},
      {"GM", QObject::tr("Medium")},
      {"GH", QObject::tr("Heavy")},
      {"DGA", QObject::tr("Dock GA")},
      {"FUEL", QObject::tr("Fuel")},
      {"V", QObject::tr("Vehicles")}
    });

  /* Short size name for parking and full name for others */
  parkingMapRamp = QHash<QString, QString>(
    {
      {"UNKNOWN", QObject::tr("Unknown")},
      {"RGA", QObject::tr("Ramp GA")},
      {"RGAS", QObject::tr("Small")},
      {"RGAM", QObject::tr("Medium")},
      {"RGAL", QObject::tr("Large")},
      {"RC", QObject::tr("Ramp Cargo")},
      {"RM", QObject::tr("Ramp Mil")},
      {"RMC", QObject::tr("Ramp Mil Cargo")},
      {"RMCB", QObject::tr("Ramp Mil Combat")},
      {"T", QObject::tr("Tie down")},
      {"H", QObject::tr("Hangar")},
      {"G", QObject::tr("Gate")},
      {"GS", QObject::tr("Gate Small")},
      {"GM", QObject::tr("Gate Medium")},
      {"GH", QObject::tr("Gate Heavy")},
      {"DGA", QObject::tr("Dock GA")},
      {"FUEL", QObject::tr("Fuel")},
      {"V", QObject::tr("Vehicles")}
    });

  /* Full name for all parking including type */
  parkingTypeMap = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"UNKNOWN", QObject::tr("Unknown")},
      {"RGA", QObject::tr("Ramp GA")},
      {"RGAS", QObject::tr("Ramp GA Small")},
      {"RGAM", QObject::tr("Ramp GA Medium")},
      {"RGAL", QObject::tr("Ramp GA Large")},
      {"RC", QObject::tr("Ramp Cargo")},
      {"RM", QObject::tr("Ramp Mil")},
      {"RMC", QObject::tr("Ramp Mil Cargo")},
      {"RMCB", QObject::tr("Ramp Mil Combat")},
      {"T", QObject::tr("Tie down")},
      {"H", QObject::tr("Hangar")},
      {"G", QObject::tr("Gate")},
      {"GS", QObject::tr("Gate Small")},
      {"GM", QObject::tr("Gate Medium")},
      {"GH", QObject::tr("Gate Heavy")},
      {"DGA", QObject::tr("Dock GA")},
      {"FUEL", QObject::tr("Fuel")},
      {"V", QObject::tr("Vehicles")}
    });

  parkingNameMap = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"UNKNOWN", QObject::tr("Unknown")},
      {"NONE", QObject::tr("No Parking")},
      {"P", QObject::tr("Parking")},
      {"NP", QObject::tr("N Parking")},
      {"NEP", QObject::tr("NE Parking")},
      {"EP", QObject::tr("E Parking")},
      {"SEP", QObject::tr("SE Parking")},
      {"SP", QObject::tr("S Parking")},
      {"SWP", QObject::tr("SW Parking")},
      {"WP", QObject::tr("W Parking")},
      {"NWP", QObject::tr("NW Parking")},
      {"G", QObject::tr("Gate")},
      {"D", QObject::tr("Dock")},
      {"GA", QObject::tr("Gate A")},
      {"GB", QObject::tr("Gate B")},
      {"GC", QObject::tr("Gate C")},
      {"GD", QObject::tr("Gate D")},
      {"GE", QObject::tr("Gate E")},
      {"GF", QObject::tr("Gate F")},
      {"GG", QObject::tr("Gate G")},
      {"GH", QObject::tr("Gate H")},
      {"GI", QObject::tr("Gate I")},
      {"GJ", QObject::tr("Gate J")},
      {"GK", QObject::tr("Gate K")},
      {"GL", QObject::tr("Gate L")},
      {"GM", QObject::tr("Gate M")},
      {"GN", QObject::tr("Gate N")},
      {"GO", QObject::tr("Gate O")},
      {"GP", QObject::tr("Gate P")},
      {"GQ", QObject::tr("Gate Q")},
      {"GR", QObject::tr("Gate R")},
      {"GS", QObject::tr("Gate S")},
      {"GT", QObject::tr("Gate T")},
      {"GU", QObject::tr("Gate U")},
      {"GV", QObject::tr("Gate V")},
      {"GW", QObject::tr("Gate W")},
      {"GX", QObject::tr("Gate X")},
      {"GY", QObject::tr("Gate Y")},
      {"GZ", QObject::tr("Gate Z")}
    });

  parkingDatabaseNameMap = QHash<QString, QString>(
    {
      {"NO_PARKING", QObject::tr("NONE")},
      {"PARKING", QObject::tr("P")},
      {"N_PARKING", QObject::tr("NP")},
      {"NE_PARKING", QObject::tr("NEP")},
      {"E_PARKING", QObject::tr("EP")},
      {"SE_PARKING", QObject::tr("SEP")},
      {"S_PARKING", QObject::tr("SP")},
      {"SW_PARKING", QObject::tr("SWP")},
      {"W_PARKING", QObject::tr("WP")},
      {"NW_PARKING", QObject::tr("NWP")},
      {"GATE", QObject::tr("G")},
      {"DOCK", QObject::tr("D")},
      {"GATE_A", QObject::tr("GA")},
      {"GATE_B", QObject::tr("GB")},
      {"GATE_C", QObject::tr("GC")},
      {"GATE_D", QObject::tr("GD")},
      {"GATE_E", QObject::tr("GE")},
      {"GATE_F", QObject::tr("GF")},
      {"GATE_G", QObject::tr("GG")},
      {"GATE_H", QObject::tr("GH")},
      {"GATE_I", QObject::tr("GI")},
      {"GATE_J", QObject::tr("GJ")},
      {"GATE_K", QObject::tr("GK")},
      {"GATE_L", QObject::tr("GL")},
      {"GATE_M", QObject::tr("GM")},
      {"GATE_N", QObject::tr("GN")},
      {"GATE_O", QObject::tr("GO")},
      {"GATE_P", QObject::tr("GP")},
      {"GATE_Q", QObject::tr("GQ")},
      {"GATE_R", QObject::tr("GR")},
      {"GATE_S", QObject::tr("GS")},
      {"GATE_T", QObject::tr("GT")},
      {"GATE_U", QObject::tr("GU")},
      {"GATE_V", QObject::tr("GV")},
      {"GATE_W", QObject::tr("GW")},
      {"GATE_X", QObject::tr("GX")},
      {"GATE_Y", QObject::tr("GY")},
      {"GATE_Z", QObject::tr("GZ")},
    });

  navTypeNamesVor = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"H", QObject::tr("H")},
      {"L", QObject::tr("L")},
      {"T", QObject::tr("T")},
      {"VH", QObject::tr("H")},
      {"VL", QObject::tr("L")},
      {"VT", QObject::tr("T")},
    });

  navTypeNamesVorLong = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"H", QObject::tr("High")},
      {"L", QObject::tr("Low")},
      {"T", QObject::tr("Terminal")},
      {"VTH", QObject::tr("High")},
      {"VTL", QObject::tr("Low")},
      {"VTT", QObject::tr("Terminal")}
    });

  navTypeNamesNdb = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"HH", QObject::tr("HH")},
      {"H", QObject::tr("H")},
      {"MH", QObject::tr("MH")},
      {"CP", QObject::tr("Compass Locator")},
      {"NHH", QObject::tr("HH")},
      {"NH", QObject::tr("H")},
      {"NMH", QObject::tr("MH")},
      {"NCP", QObject::tr("Compass Locator")},
    });

  navTypeNamesWaypoint = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"WN", QObject::tr("Named")},
      {"WT", QObject::tr("Track")},
      {"WU", QObject::tr("Unnamed")},
      {"V", QObject::tr("VOR")},
      {"N", QObject::tr("NDB")}
    });

  navTypeNames = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"VD", QObject::tr("VORDME")},
      {"VT", QObject::tr("VORTAC")},
      {"VTD", QObject::tr("DME only VORTAC")},
      {"V", QObject::tr("VOR")},
      {"D", QObject::tr("DME")},
      {"TC", QObject::tr("TACAN")},
      {"TCD", QObject::tr("DME only TACAN")},
      {"N", QObject::tr("NDB")},
      {"W", QObject::tr("Waypoint")}
    });

  comTypeNames = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"NONE", QObject::tr("None")},

      // FSX/P3D types
      // {"ATIS", QObject::tr("ATIS")},
      // {"MC", QObject::tr("Multicom")},
      // {"UC", QObject::tr("Unicom")},
      {"CTAF", QObject::tr("CTAF")},
      // {"G", QObject::tr("Ground")},
      // {"T", QObject::tr("Tower")},
      // {"C", QObject::tr("Clearance")},
      // {"A", QObject::tr("Approach")},
      // {"D", QObject::tr("Departure")},
      // {"CTR", QObject::tr("Center")},
      // {"FSS", QObject::tr("FSS")},
      // {"AWOS", QObject::tr("AWOS")},
      // {"ASOS", QObject::tr("ASOS")},
      // {"CPT", QObject::tr("Clearance pre Taxi")},
      {"RCD", QObject::tr("Remote Clearance Delivery")},

      // All new AIRAC types
      {"CTR", QObject::tr("Area Control Center")},
      {"ACP", QObject::tr("Airlift Command Post")},
      {"AIR", QObject::tr("Air to Air")},
      {"A", QObject::tr("Approach Control")},
      {"ARR", QObject::tr("Arrival Control")},
      {"ASOS", QObject::tr("ASOS")},
      {"ATIS", QObject::tr("ATIS")},
      {"AWI", QObject::tr("AWIB")},
      {"AWOS", QObject::tr("AWOS")},
      {"AWS", QObject::tr("AWIS")},
      {"C", QObject::tr("Clearance Delivery")},
      {"CPT", QObject::tr("Clearance Pre-Taxi")},
      {"CTA", QObject::tr("Terminal Control Area")},
      {"CTL", QObject::tr("Control")},
      {"D", QObject::tr("Departure Control")},
      {"DIR", QObject::tr("Director (Approach Control Radar)")},
      {"EFS", QObject::tr("Enroute Flight Advisory Service (EFAS)")},
      {"EMR", QObject::tr("Emergency")},
      {"FSS", QObject::tr("Flight Service Station")},
      {"GCO", QObject::tr("Ground Comm Outlet")},
      {"GET", QObject::tr("Gate Control")},
      {"G", QObject::tr("Ground Control")},
      {"HEL", QObject::tr("Helicopter Frequency")},
      {"INF", QObject::tr("Information")},
      {"MIL", QObject::tr("Military Frequency")},
      {"MC", QObject::tr("Multicom")},
      {"OPS", QObject::tr("Operations")},
      {"PAL", QObject::tr("Pilot Activated Lighting")},
      {"RDO", QObject::tr("Radio")},
      {"RDR", QObject::tr("Radar")},
      {"RFS", QObject::tr("Remote Flight Service Station (RFSS)")},
      {"RMP", QObject::tr("Ramp or Taxi Control")},
      {"RSA", QObject::tr("Airport Radar Service Area (ARSA)")},
      {"TCA", QObject::tr("Terminal Control Area (TCA)")},
      {"TMA", QObject::tr("Terminal Control Area (TMA)")},
      {"TML", QObject::tr("Terminal")},
      {"TRS", QObject::tr("Terminal Radar Service Area (TRSA)")},
      {"TWE", QObject::tr("Transcriber Weather Broadcast (TWEB)")},
      {"T", QObject::tr("Tower, Air Traffic Control")},
      {"UAC", QObject::tr("Upper Area Control")},
      {"UC", QObject::tr("Unicom")},
      {"VOL", QObject::tr("Volmet")}
    });

  airspaceTypeNameMap = QHash<map::MapAirspaceTypes, QString>(
    {
      {map::AIRSPACE_NONE, QObject::tr("No Airspace")},
      {map::CENTER, QObject::tr("Center")},
      {map::CLASS_A, QObject::tr("Class A")},
      {map::CLASS_B, QObject::tr("Class B")},
      {map::CLASS_C, QObject::tr("Class C")},
      {map::CLASS_D, QObject::tr("Class D")},
      {map::CLASS_E, QObject::tr("Class E")},
      {map::CLASS_F, QObject::tr("Class F")},
      {map::CLASS_G, QObject::tr("Class G")},
      {map::TOWER, QObject::tr("Tower")},
      {map::CLEARANCE, QObject::tr("Clearance")},
      {map::GROUND, QObject::tr("Ground")},
      {map::DEPARTURE, QObject::tr("Departure")},
      {map::APPROACH, QObject::tr("Approach")},
      {map::MOA, QObject::tr("MOA")},
      {map::RESTRICTED, QObject::tr("Restricted")},
      {map::PROHIBITED, QObject::tr("Prohibited")},
      {map::WARNING, QObject::tr("Warning")},
      {map::CAUTION, QObject::tr("Caution")},
      {map::ALERT, QObject::tr("Alert")},
      {map::DANGER, QObject::tr("Danger")},
      {map::NATIONAL_PARK, QObject::tr("National Park")},
      {map::MODEC, QObject::tr("Mode-C")},
      {map::RADAR, QObject::tr("Radar")},
      {map::TRAINING, QObject::tr("Training")},
      {map::GLIDERPROHIBITED, QObject::tr("Glider Prohibited")},
      {map::WAVEWINDOW, QObject::tr("Wave Window")},
      {map::ONLINE_OBSERVER, QObject::tr("Online Observer")}
    });

  airspaceFlagNameMap = QHash<map::MapAirspaceFlags, QString>(
    {
      // Values below only for actions
      {map::AIRSPACE_AT_FLIGHTPLAN, QObject::tr("At flight plan cruise altitude")},
      {map::AIRSPACE_BELOW_10000, QObject::tr("Below 10,000 ft only")},
      {map::AIRSPACE_BELOW_18000, QObject::tr("Below 18,000 ft only")},
      {map::AIRSPACE_ABOVE_10000, QObject::tr("Above 10,000 ft only")},
      {map::AIRSPACE_ABOVE_18000, QObject::tr("Above 18,000 ft only")},
      {map::AIRSPACE_ALL_ALTITUDE, QObject::tr("All Altitudes")}
    });

  airspaceRemarkMap = QHash<map::MapAirspaceTypes, QString>(
    {
      {map::AIRSPACE_NONE, QObject::tr("No Airspace")},
      {map::CENTER, QString()},
      {map::CLASS_A, QObject::tr("Controlled, above 18,000 ft MSL, IFR, no VFR, ATC clearance required.")},
      {map::CLASS_B, QObject::tr("Controlled, IFR and VFR, ATC clearance required.")},
      {map::CLASS_C, QObject::tr("Controlled, IFR and VFR, ATC clearance required, transponder required.")},
      {map::CLASS_D, QObject::tr("Controlled, IFR and VFR, ATC clearance required.")},
      {map::CLASS_E, QObject::tr("Controlled, IFR and VFR, ATC clearance required for IFR only.")},
      {map::CLASS_F, QObject::tr("Uncontrolled, IFR and VFR, ATC clearance not required.")},
      {map::CLASS_G, QObject::tr("Uncontrolled, IFR and VFR, ATC clearance not required.")},
      {map::TOWER, QString()},
      {map::CLEARANCE, QString()},
      {map::GROUND, QString()},
      {map::DEPARTURE, QString()},
      {map::APPROACH, QString()},
      {map::MOA,
       QObject::tr("Military operations area. Needs clearance for IFR if active. Check for traffic advisories.")},
      {map::RESTRICTED, QObject::tr("Needs authorization.")},
      {map::PROHIBITED, QObject::tr("No flight allowed.")},
      {map::WARNING, QObject::tr("Contains activity that may be hazardous to aircraft.")},
      {map::CAUTION, QString()},
      {map::ALERT, QObject::tr("High volume of pilot training or an unusual type of aerial activity.")},
      {map::DANGER, QObject::tr("Avoid or proceed with caution.")},
      {map::NATIONAL_PARK, QString()},
      {map::MODEC, QObject::tr("Needs altitude aware transponder.")},
      {map::RADAR, QObject::tr("Terminal radar area. Not controlled.")},
      {map::TRAINING, QString()},
      {map::GLIDERPROHIBITED, QString()},
      {map::WAVEWINDOW, QObject::tr("Sailplane Area.")},
      {map::ONLINE_OBSERVER, QObject::tr("Online network observer")}
    });

}

static QHash<QString, QString> parkingNameMapUntranslated(
  {
    {"INVALID", QLatin1Literal("Invalid")},
    {"UNKNOWN", QLatin1Literal("Unknown")},
    {"NONE", QLatin1Literal("No Parking")},
    {"P", QLatin1Literal("Parking")},
    {"NP", QLatin1Literal("N Parking")},
    {"NEP", QLatin1Literal("NE Parking")},
    {"EP", QLatin1Literal("E Parking")},
    {"SEP", QLatin1Literal("SE Parking")},
    {"SP", QLatin1Literal("S Parking")},
    {"SWP", QLatin1Literal("SW Parking")},
    {"WP", QLatin1Literal("W Parking")},
    {"NWP", QLatin1Literal("NW Parking")},
    {"G", QLatin1Literal("Gate")},
    {"D", QLatin1Literal("Dock")},
    {"GA", QLatin1Literal("Gate A")},
    {"GB", QLatin1Literal("Gate B")},
    {"GC", QLatin1Literal("Gate C")},
    {"GD", QLatin1Literal("Gate D")},
    {"GE", QLatin1Literal("Gate E")},
    {"GF", QLatin1Literal("Gate F")},
    {"GG", QLatin1Literal("Gate G")},
    {"GH", QLatin1Literal("Gate H")},
    {"GI", QLatin1Literal("Gate I")},
    {"GJ", QLatin1Literal("Gate J")},
    {"GK", QLatin1Literal("Gate K")},
    {"GL", QLatin1Literal("Gate L")},
    {"GM", QLatin1Literal("Gate M")},
    {"GN", QLatin1Literal("Gate N")},
    {"GO", QLatin1Literal("Gate O")},
    {"GP", QLatin1Literal("Gate P")},
    {"GQ", QLatin1Literal("Gate Q")},
    {"GR", QLatin1Literal("Gate R")},
    {"GS", QLatin1Literal("Gate S")},
    {"GT", QLatin1Literal("Gate T")},
    {"GU", QLatin1Literal("Gate U")},
    {"GV", QLatin1Literal("Gate V")},
    {"GW", QLatin1Literal("Gate W")},
    {"GX", QLatin1Literal("Gate X")},
    {"GY", QLatin1Literal("Gate Y")},
    {"GZ", QLatin1Literal("Gate Z")}
  });

/* The higher the better */
static QHash<QString, int> surfaceQualityMap(
  {
    {"C", 20},
    {"A", 20},
    {"B", 20},
    {"T", 20},
    {"M", 15},
    {"CE", 15},
    {"OT", 15},
    {"BR", 10},
    {"SM", 10},
    {"PL", 10},
    {"GR", 5},
    {"CR", 5},
    {"D", 5},
    {"SH", 5},
    {"CL", 5},
    {"S", 5},
    {"G", 5},
    {"SN", 5},
    {"I", 5},
    {"W", 1},
    {"TR", 1},
    {"UNKNOWN", 0},
    {"INVALID", 0}
  });

const static QHash<QString, map::MapAirspaceTypes> airspaceTypeFromDatabaseMap(
  {
    {"NONE", map::AIRSPACE_NONE},
    {"C", map::CENTER},
    {"CA", map::CLASS_A},
    {"CB", map::CLASS_B},
    {"CC", map::CLASS_C},
    {"CD", map::CLASS_D},
    {"CE", map::CLASS_E},
    {"CF", map::CLASS_F},
    {"CG", map::CLASS_G},
    {"T", map::TOWER},
    {"CL", map::CLEARANCE},
    {"G", map::GROUND},
    {"D", map::DEPARTURE},
    {"A", map::APPROACH},
    {"M", map::MOA},
    {"R", map::RESTRICTED},
    {"P", map::PROHIBITED},
    {"CN", map::CAUTION},
    {"W", map::WARNING},
    {"AL", map::ALERT},
    {"DA", map::DANGER},
    {"NP", map::NATIONAL_PARK},
    {"MD", map::MODEC},
    {"RD", map::RADAR},
    {"TR", map::TRAINING},
    {"GP", map::GLIDERPROHIBITED},
    {"WW", map::WAVEWINDOW},
    {"OBS", map::ONLINE_OBSERVER}, /* No database type */
  });

static QHash<map::MapAirspaceTypes, QString> airspaceTypeToDatabaseMap(
  {
    {map::AIRSPACE_NONE, "NONE"},
    {map::CENTER, "C"},
    {map::CLASS_A, "CA"},
    {map::CLASS_B, "CB"},
    {map::CLASS_C, "CC"},
    {map::CLASS_D, "CD"},
    {map::CLASS_E, "CE"},
    {map::CLASS_F, "CF"},
    {map::CLASS_G, "CG"},
    {map::TOWER, "T"},
    {map::CLEARANCE, "CL"},
    {map::GROUND, "G"},
    {map::DEPARTURE, "D"},
    {map::APPROACH, "A"},
    {map::MOA, "M"},
    {map::RESTRICTED, "R"},
    {map::PROHIBITED, "P"},
    {map::WARNING, "W"},
    {map::CAUTION, "CN"},
    {map::ALERT, "AL"},
    {map::DANGER, "DA"},
    {map::NATIONAL_PARK, "NP"},
    {map::MODEC, "MD"},
    {map::RADAR, "RD"},
    {map::TRAINING, "TR"},
    {map::GLIDERPROHIBITED, "GP"},
    {map::WAVEWINDOW, "WW"},

    {map::ONLINE_OBSERVER, "OBS"} /* Not a database type */
  });

/* Defines drawing sort order - lower values are drawn first - higher values are drawn on top */
const static QHash<map::MapAirspaceTypes, int> airspacePriorityMap(
  {
    {map::AIRSPACE_NONE, 1},

    {map::ONLINE_OBSERVER, 2},
    {map::CENTER, 3},

    {map::CLASS_A, 10},

    {map::CLASS_B, 11},
    {map::CLASS_C, 12},
    {map::CLASS_D, 13},

    {map::CLASS_E, 14},

    {map::CLASS_F, 20},
    {map::CLASS_G, 21},

    {map::TOWER, 51},
    {map::CLEARANCE, 52},
    {map::GROUND, 50},
    {map::DEPARTURE, 53},
    {map::APPROACH, 54},

    {map::MOA, 1},
    {map::WAVEWINDOW, 3},

    {map::GLIDERPROHIBITED, 99},
    {map::RESTRICTED, 100},
    {map::PROHIBITED, 102},

    {map::WARNING, 60},
    {map::CAUTION, 60},
    {map::ALERT, 61},
    {map::DANGER, 62},

    {map::NATIONAL_PARK, 2},
    {map::MODEC, 5},
    {map::RADAR, 6},
    {map::TRAINING, 59},
  });

void updateUnits()
{
  Q_ASSERT(!airspaceFlagNameMap.isEmpty());
  airspaceFlagNameMap[map::AIRSPACE_BELOW_10000] = QObject::tr("Below %1 only").arg(Unit::altFeet(10000.f));
  airspaceFlagNameMap[map::AIRSPACE_BELOW_18000] = QObject::tr("Below %1 only").arg(Unit::altFeet(18000.f));
  airspaceFlagNameMap[map::AIRSPACE_ABOVE_10000] = QObject::tr("Above %1 only").arg(Unit::altFeet(10000.f));
  airspaceFlagNameMap[map::AIRSPACE_ABOVE_18000] = QObject::tr("Above %1 only").arg(Unit::altFeet(18000.f));
}

QString navTypeName(const QString& type)
{
  QString retval = navTypeNameVor(type);
  if(retval.isEmpty())
    retval = navTypeNameNdb(type);
  if(retval.isEmpty())
    retval = navTypeNameVor(type);
  if(retval.isEmpty())
    retval = navTypeNameWaypoint(type);
  return retval;
}

const QString& navTypeNameVor(const QString& type)
{
  Q_ASSERT(!navTypeNamesVor.isEmpty());
  return navTypeNamesVor[type];
}

const QString& navTypeNameVorLong(const QString& type)
{
  Q_ASSERT(!navTypeNamesVorLong.isEmpty());
  return navTypeNamesVorLong[type];
}

const QString& navTypeNameNdb(const QString& type)
{
  Q_ASSERT(!navTypeNamesNdb.isEmpty());
  return navTypeNamesNdb[type];
}

const QString& navTypeNameWaypoint(const QString& type)
{
  Q_ASSERT(!navTypeNamesWaypoint.isEmpty());
  return navTypeNamesWaypoint[type];
}

const QString& navName(const QString& type)
{
  Q_ASSERT(!navTypeNames.isEmpty());
  return navTypeNames[type];
}

const QString& surfaceName(const QString& surface)
{
  Q_ASSERT(!surfaceMap.isEmpty());
  return surfaceMap[surface];
}

int surfaceQuality(const QString& surface)
{
  Q_ASSERT(!surfaceQualityMap.isEmpty());
  return surfaceQualityMap.value(surface, 0);
}

const QString& parkingGateName(const QString& gate)
{
  Q_ASSERT(!parkingMapGate.isEmpty());
  return parkingMapGate[gate];
}

const QString& parkingRampName(const QString& ramp)
{
  Q_ASSERT(!parkingMapRamp.isEmpty());
  return parkingMapRamp[ramp];
}

const QString& parkingTypeName(const QString& type)
{
  Q_ASSERT(!parkingTypeMap.isEmpty());
  return parkingTypeMap[type];
}

const QString& parkingName(const QString& name)
{
  Q_ASSERT(!parkingNameMap.isEmpty());

  if(parkingNameMap.contains(name))
    return parkingNameMap[name];
  else
    return name;
}

const QString& parkingDatabaseName(const QString& name)
{
  Q_ASSERT(!parkingDatabaseNameMap.isEmpty());

  return parkingDatabaseNameMap[name];
}

QString parkingNameNumberType(const map::MapParking& parking)
{
  QStringList name;

  if(parking.number != -1)
    name.append(map::parkingName(parking.name) + " " + QLocale().toString(parking.number));
  else
    name.append(map::parkingName(parking.name));

  const QString typeName = map::parkingTypeName(parking.type);

  if(!typeName.isEmpty())
    name.append(typeName);

  return name.join(", ");
}

QString startType(const map::MapStart& start)
{
  if(start.type == "R")
    return QObject::tr("Runway");
  else if(start.type == "W")
    return QObject::tr("Water");
  else if(start.type == "H")
    return QObject::tr("Helipad");
  else
    return QString();
}

QString parkingNameForFlightplan(const map::MapParking& parking)
{
  if(parking.number == -1)
    // Free name
    return parking.name;
  else
    // FSX/P3D type
    return parkingNameMapUntranslated.value(parking.name).toUpper() + " " + QString::number(parking.number);
}

bool MapAirport::closed() const
{
  return flags.testFlag(AP_CLOSED);
}

bool MapAirport::hard() const
{
  return flags.testFlag(AP_HARD);
}

bool MapAirport::tower() const
{
  return flags.testFlag(AP_TOWER);
}

bool MapAirport::towerObject() const
{
  return flags.testFlag(AP_TOWER_OBJ);
}

bool MapAirport::apron() const
{
  return flags.testFlag(AP_APRON);
}

bool MapAirport::taxiway() const
{
  return flags.testFlag(AP_TAXIWAY);
}

bool MapAirport::parking() const
{
  return flags.testFlag(AP_PARKING);
}

bool MapAirport::als() const
{
  return flags.testFlag(AP_ALS);
}

bool MapAirport::vasi() const
{
  return flags.testFlag(AP_VASI);
}

bool MapAirport::fence() const
{
  return flags.testFlag(AP_FENCE);
}

bool MapAirport::closedRunways() const
{
  return flags.testFlag(AP_RW_CLOSED);
}

bool MapAirport::emptyDraw() const
{
  if(NavApp::isNavdataAll())
    return false;

  return emptyDraw(OptionData::instance());
}

bool MapAirport::emptyDraw(const OptionData& od) const
{
  if(NavApp::isNavdataAll())
    return false;

  if(od.getFlags() & opts::MAP_EMPTY_AIRPORTS)
  {
    if(od.getFlags2() & opts2::MAP_EMPTY_AIRPORTS_3D && xplane)
      return !is3d() && !addon() && !waterOnly();
    else
      return empty() && !waterOnly();
  }
  else
    return false;
}

bool MapAirport::empty() const
{
  if(rating == -1)
    // Not calculated
    return !parking() && !taxiway() && !apron() && !addon() && !helipad();
  else
    return rating == 0;
}

bool MapAirport::addon() const
{
  return flags.testFlag(AP_ADDON);
}

bool MapAirport::is3d() const
{
  return flags.testFlag(AP_3D);
}

bool MapAirport::procedure() const
{
  return flags.testFlag(AP_PROCEDURE);
}

bool MapAirport::anyFuel() const
{
  return flags.testFlag(AP_AVGAS) || flags.testFlag(AP_JETFUEL);
}

bool MapAirport::complete() const
{
  return flags.testFlag(AP_COMPLETE);
}

bool MapAirport::soft() const
{
  return flags.testFlag(AP_SOFT);
}

bool MapAirport::softOnly() const
{
  return !flags.testFlag(AP_HARD) && flags.testFlag(AP_SOFT);
}

bool MapAirport::water() const
{
  return flags.testFlag(AP_WATER);
}

bool MapAirport::lighted() const
{
  return flags.testFlag(AP_LIGHT);
}

bool MapAirport::helipad() const
{
  return flags.testFlag(AP_HELIPAD);
}

bool MapAirport::waterOnly() const
{
  return !flags.testFlag(AP_HARD) && !flags.testFlag(AP_SOFT) && flags.testFlag(AP_WATER);
}

bool MapAirport::helipadOnly() const
{
  return !flags.testFlag(AP_HARD) && !flags.testFlag(AP_SOFT) &&
         !flags.testFlag(AP_WATER) && flags.testFlag(AP_HELIPAD);
}

bool MapAirport::noRunways() const
{
  return !flags.testFlag(AP_HARD) && !flags.testFlag(AP_SOFT) && !flags.testFlag(AP_WATER);
}

bool MapAirport::isVisible(map::MapObjectTypes objectTypes) const
{
  if(addon() && objectTypes.testFlag(map::AIRPORT_ADDON))
    return true;

  if(emptyDraw() && !objectTypes.testFlag(map::AIRPORT_EMPTY))
    return false;

  if(hard() && !objectTypes.testFlag(map::AIRPORT_HARD))
    return false;

  if((softOnly() || waterOnly() || noRunways()) && !objectTypes.testFlag(map::AIRPORT_SOFT))
    return false;

  return true;
}

/* Convert nav_search type */
map::MapObjectTypes navTypeToMapObjectType(const QString& navType)
{
  map::MapObjectTypes type = NONE;
  if(navType.startsWith("V") || navType == "D" || navType.startsWith("TC"))
    type = map::VOR;
  else if(navType == "N")
    type = map::NDB;
  else if(navType == "W")
    type = map::WAYPOINT;
  return type;
}

bool navTypeTacan(const QString& navType)
{
  return navType == "TC" || navType == "TCD";
}

bool navTypeVortac(const QString& navType)
{
  return navType == "VT" || navType == "VTD";
}

QString airwayTrackTypeToShortString(MapAirwayTrackType type)
{
  switch(type)
  {
    case map::NO_AIRWAY:
      break;

    case map::TRACK_NAT:
      return QObject::tr("N");

    case map::TRACK_PACOTS:
      return QObject::tr("P");

    case map::TRACK_AUSOTS:
      return QObject::tr("A");

    case map::AIRWAY_VICTOR:
      return QObject::tr("V");

    case map::AIRWAY_JET:
      return QObject::tr("J");

    case map::AIRWAY_BOTH:
      return QObject::tr("B");

  }
  return QString();
}

QString airwayTrackTypeToString(MapAirwayTrackType type)
{
  switch(type)
  {
    case map::NO_AIRWAY:
      break;

    case map::TRACK_NAT:
      return QObject::tr("NAT");

    case map::TRACK_PACOTS:
      return QObject::tr("PACOTS");

    case map::TRACK_AUSOTS:
      return QObject::tr("AUSOTS");

    case map::AIRWAY_VICTOR:
      return QObject::tr("Victor");

    case map::AIRWAY_JET:
      return QObject::tr("Jet");

    case map::AIRWAY_BOTH:
      return QObject::tr("Both");

  }
  return QString();
}

MapAirwayTrackType airwayTrackTypeFromString(const QString& typeStr)
{
  if(typeStr.startsWith("V"))
    return map::AIRWAY_VICTOR;
  else if(typeStr.startsWith("J"))
    return map::AIRWAY_JET;
  else if(typeStr.startsWith("B"))
    return map::AIRWAY_BOTH;
  else if(typeStr.startsWith("N"))
    return map::TRACK_NAT;
  else if(typeStr.startsWith("P"))
    return map::TRACK_PACOTS;
  else if(typeStr.startsWith("A"))
    return map::TRACK_AUSOTS;
  else
    return map::NO_AIRWAY;
}

QString airwayRouteTypeToString(map::MapAirwayRouteType type)
{
  switch(type)
  {
    case map::RT_NONE:
      break;

    case map::RT_AIRLINE:
      return QObject::tr("Airline");

    case map::RT_CONTROL:
      return QObject::tr("Control");

    case map::RT_DIRECT:
      return QObject::tr("Direct");

    case map::RT_HELICOPTER:
      return QObject::tr("Helicopter");

    case map::RT_OFFICIAL:
      return QObject::tr("Official");

    case map::RT_RNAV:
      return QObject::tr("RNAV");

    case map::RT_UNDESIGNATED:
      return QObject::tr("Undesignated");

    case map::RT_TRACK:
      return QObject::tr("Track");
  }
  return QString();
}

QString airwayRouteTypeToStringShort(map::MapAirwayRouteType type)
{
  switch(type)
  {
    case map::RT_NONE:
      break;

    case map::RT_AIRLINE:
      return QObject::tr("A");

    case map::RT_CONTROL:
      return QObject::tr("C");

    case map::RT_DIRECT:
      return QObject::tr("D");

    case map::RT_HELICOPTER:
      return QObject::tr("H");

    case map::RT_OFFICIAL:
      return QObject::tr("O");

    case map::RT_RNAV:
      return QObject::tr("R");

    case map::RT_UNDESIGNATED:
      return QObject::tr("S");

    case map::RT_TRACK:
      return QObject::tr("T");
  }
  return QString();
}

MapAirwayRouteType airwayRouteTypeFromString(const QString& typeStr)
{
  if(typeStr == "A")
    return RT_AIRLINE; /* A Airline Airway (Tailored Data) */
  else if(typeStr == "C")
    return RT_CONTROL; /* C Control */
  else if(typeStr == "D")
    return RT_DIRECT; /* D Direct Route */
  else if(typeStr == "H")
    return RT_HELICOPTER; /* H Helicopter Airways */
  else if(typeStr == "O")
    return RT_OFFICIAL; /* O Officially Designated Airways, except RNAV, Helicopter Airways */
  else if(typeStr == "R")
    return RT_RNAV; /* R RNAV Airways */
  else if(typeStr == "S")
    return RT_UNDESIGNATED; /* S Undesignated ATS Route */
  else
    return RT_NONE;
}

QDataStream& operator>>(QDataStream& dataStream, map::RangeMarker& obj)
{
  qint32 types;
  dataStream >> obj.text >> obj.ranges >> obj.center >> types;
  obj.type = static_cast<map::MapObjectTypes>(types);
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj)
{
  dataStream << obj.text << obj.ranges << obj.center << static_cast<qint32>(obj.type);
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj)
{
  bool dummy = true; // Value was removed
  dataStream >> obj.text >> obj.color >> obj.from >> obj.to >> obj.magvar >> dummy >> dummy;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj)
{
  bool dummy = false; // Value was removed
  dataStream << obj.text << obj.color << obj.from << obj.to << obj.magvar << dummy << dummy;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, TrafficPattern& obj)
{
  dataStream >> obj.airportIcao
  >> obj.runwayName
  >> obj.color
  >> obj.turnRight
  >> obj.base45Degree
  >> obj.showEntryExit
  >> obj.runwayLength
  >> obj.downwindDistance
  >> obj.baseDistance
  >> obj.courseTrue
  >> obj.magvar
  >> obj.position;

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const TrafficPattern& obj)
{
  dataStream << obj.airportIcao
             << obj.runwayName
             << obj.color
             << obj.turnRight
             << obj.base45Degree
             << obj.showEntryExit
             << obj.runwayLength
             << obj.downwindDistance
             << obj.baseDistance
             << obj.courseTrue
             << obj.magvar
             << obj.position;

  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, Hold& obj)
{
  dataStream
  >> obj.navIdent
  >> obj.navType
  >> obj.vorDmeOnly
  >> obj.vorHasDme
  >> obj.vorTacan
  >> obj.vorVortac
  >> obj.color
  >> obj.turnLeft
  >> obj.minutes
  >> obj.speedKts
  >> obj.courseTrue
  >> obj.magvar
  >> obj.position;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const Hold& obj)
{
  dataStream
    << obj.navIdent
    << obj.navType
    << obj.vorDmeOnly
    << obj.vorHasDme
    << obj.vorTacan
    << obj.vorVortac
    << obj.color
    << obj.turnLeft
    << obj.minutes
    << obj.speedKts
    << obj.courseTrue
    << obj.magvar
    << obj.position;

  return dataStream;
}

QString vorType(const MapVor& vor)
{
  return vorType(vor.dmeOnly, vor.hasDme, vor.tacan, vor.vortac);
}

QString vorType(bool dmeOnly, bool hasDme, bool tacan, bool vortac)
{
  if(vortac)
  {
    if(dmeOnly)
      return QObject::tr("DME only VORTAC");
    else
      return QObject::tr("VORTAC");
  }
  else if(tacan)
  {
    if(dmeOnly)
      return QObject::tr("DME only TACAN");
    else
      return QObject::tr("TACAN");

  }
  else
  {
    if(dmeOnly)
      return QObject::tr("DME");
    else if(hasDme)
      return QObject::tr("VORDME");
    else
      return QObject::tr("VOR");
  }
}

QString vorText(const MapVor& vor)
{
  return QObject::tr("%1 %2 (%3)").arg(vorType(vor)).arg(atools::capString(vor.name)).arg(vor.ident);
}

QString vorTextShort(const MapVor& vor)
{
  return QObject::tr("%1 (%2)").arg(atools::capString(vor.name)).arg(vor.ident);
}

QString ndbText(const MapNdb& ndb)
{
  return QObject::tr("NDB %1 (%2)").arg(atools::capString(ndb.name)).arg(ndb.ident);
}

QString ndbTextShort(const MapNdb& ndb)
{
  return QObject::tr("%1 (%2)").arg(atools::capString(ndb.name)).arg(ndb.ident);
}

QString waypointText(const MapWaypoint& waypoint)
{
  return QObject::tr("Waypoint %1").arg(waypoint.ident);
}

QString userpointText(const MapUserpoint& userpoint)
{
  return QObject::tr("Userpoint %1").arg(userpoint.ident.isEmpty() ? userpoint.name : userpoint.ident);
}

QString logEntryText(const MapLogbookEntry& logEntry)
{
  return QObject::tr("Logbook Entry %1 to %2").arg(logEntry.departureIdent).arg(logEntry.destinationIdent);
}

QString userpointRouteText(const MapUserpointRoute& userpoint)
{
  return QObject::tr("Position %1").arg(userpoint.ident);
}

QString airwayText(const MapAirway& airway)
{
  return QObject::tr("Airway %1").arg(airway.name);
}

QString airwayAltText(const MapAirway& airway)
{
  QString altTxt;
  if(airway.minAltitude > 0)
  {
    if(airway.maxAltitude > 0 && airway.maxAltitude < 60000)
      altTxt = Unit::altFeet(airway.minAltitude);
    else
      altTxt = QObject::tr("Min ") + Unit::altFeet(airway.minAltitude);
  }

  if(airway.maxAltitude > 0 && airway.maxAltitude < 60000)
  {
    if(airway.minAltitude > 0)
      altTxt += QObject::tr(" to ") + Unit::altFeet(airway.maxAltitude);
    else
      altTxt += QObject::tr("Max ") + Unit::altFeet(airway.maxAltitude);
  }
  return altTxt;
}

QString airwayAltTextShort(const MapAirway& airway, bool addUnit, bool narrow)
{
  if(airway.maxAltitude > 0 && airway.maxAltitude < 60000)
    return QObject::tr("%1-%2").
           arg(Unit::altFeet(airway.minAltitude, false /*addUnit*/, narrow)).
           arg(Unit::altFeet(airway.maxAltitude, addUnit, narrow));
  else if(airway.minAltitude > 0)
    return Unit::altFeet(airway.minAltitude, addUnit, narrow);
  else
    return QString();
}

QString airportText(const MapAirport& airport, int elideName)
{
  if(!airport.isValid())
    return QObject::tr("Airport");
  else if(airport.name.isEmpty())
    return QObject::tr("Airport %1").arg(airport.ident);
  else
    return QObject::tr("Airport %1 (%2)").arg(atools::elideTextShort(airport.name, elideName)).arg(airport.ident);
}

QString airportTextShort(const MapAirport& airport)
{
  return QObject::tr("%1 (%2)").arg(airport.name).arg(airport.ident);
}

QString comTypeName(const QString& type)
{
  Q_ASSERT(!comTypeNames.isEmpty());
  return comTypeNames.value(type);
}

QString magvarText(float magvar, bool shortText)
{
  QString num = QLocale().toString(std::abs(magvar), 'f', 1);

  if(!num.isEmpty())
  {
    // The only way to remove trailing zeros
    QString pt = QLocale().decimalPoint();
    if(num.endsWith(pt))
      num.chop(1);
    if(num.endsWith(pt + "0"))
      num.chop(2);

    if(magvar < -0.04f)
      return QObject::tr("%1°%2").arg(num).arg(shortText ? QObject::tr("W") : QObject::tr(" West"));
    else if(magvar > 0.04f)
      // positive" (or "easterly") variation
      return QObject::tr("%1°%2").arg(num).arg(shortText ? QObject::tr("E") : QObject::tr(" East"));
    else
      return QObject::tr("0°");
  }
  return QString();
}

bool isHardSurface(const QString& surface)
{
  return surface == "C" || surface == "A" || surface == "B" || surface == "T";
}

bool isWaterSurface(const QString& surface)
{
  return surface == "W";
}

bool isSoftSurface(const QString& surface)
{
  return !isWaterSurface(surface) && !isHardSurface(surface);
}

QString parkingShortName(const QString& name)
{
  if(name == "P")
    return QObject::tr("P");
  else if(name == "NP")
    return QObject::tr("N");
  else if(name == "NEP")
    return QObject::tr("NE");
  else if(name == "EP")
    return QObject::tr("E");
  else if(name == "SEP")
    return QObject::tr("SE");
  else if(name == "SP")
    return QObject::tr("S");
  else if(name == "SWP")
    return QObject::tr("SW");
  else if(name == "WP")
    return QObject::tr("W");
  else if(name == "NWP")
    return QObject::tr("NW");
  else if(name == "G")
    return QString();
  else if(name == "D")
    return QObject::tr("D");
  else if(name.startsWith("G"))
    return name.right(1);
  else
    return QString();
}

void MapSearchResult::clear(const MapObjectTypes& types)
{
  if(types & map::AIRPORT)
  {
    airports.clear();
    airportIds.clear();
  }

  if(types & map::WAYPOINT)
  {
    waypoints.clear();
    waypointIds.clear();
  }

  if(types & map::VOR)
  {
    vors.clear();
    vorIds.clear();
  }

  if(types & map::NDB)
  {
    ndbs.clear();
    ndbIds.clear();
  }

  if(types & map::AIRWAY)
    airways.clear();

  if(types & map::AIRSPACE)
    airspaces.clear();

  if(types & map::RUNWAYEND)
    runwayEnds.clear();

  if(types & map::ILS)
    ils.clear();

  if(types & map::AIRSPACE)
    airspaces.clear();

  if(types & map::USERPOINTROUTE)
    userpointsRoute.clear();

  if(types & map::USERPOINT)
  {
    userpoints.clear();
    userpointIds.clear();
  }

  if(types & map::LOGBOOK)
    logbookEntries.clear();

  if(types & map::AIRCRAFT_AI)
    aiAircraft.clear();

  if(types & map::AIRCRAFT)
    userAircraft = atools::fs::sc::SimConnectUserAircraft();

  if(types & map::AIRCRAFT_ONLINE)
  {
    onlineAircraft.clear();
    onlineAircraftIds.clear();
  }
}

template<typename T>
void MapSearchResult::clearAllButFirst(QList<T>& list)
{
  while(list.size() > 1)
    list.removeLast();
}

void MapSearchResult::clearAllButFirst(const MapObjectTypes& types)
{
  if(types & map::AIRPORT)
  {
    clearAllButFirst(airports);
    airportIds.clear();
  }

  if(types & map::WAYPOINT)
  {
    clearAllButFirst(waypoints);
    waypointIds.clear();
  }

  if(types & map::VOR)
  {
    clearAllButFirst(vors);
    vorIds.clear();
  }

  if(types & map::NDB)
  {
    clearAllButFirst(ndbs);
    ndbIds.clear();
  }

  if(types & map::AIRWAY)
    clearAllButFirst(airways);

  if(types & map::RUNWAYEND)
    clearAllButFirst(runwayEnds);

  if(types & map::ILS)
    clearAllButFirst(ils);

  if(types & map::AIRSPACE)
    clearAllButFirst(airspaces);

  if(types & map::USERPOINTROUTE)
    clearAllButFirst(userpointsRoute);

  if(types & map::USERPOINT)
  {
    clearAllButFirst(userpoints);
    userpointIds.clear();
  }

  if(types & map::LOGBOOK)
    clearAllButFirst(logbookEntries);

  if(types & map::AIRCRAFT_AI)
    clearAllButFirst(aiAircraft);

  if(types & map::AIRCRAFT)
    userAircraft = atools::fs::sc::SimConnectUserAircraft();

  if(types & map::AIRCRAFT_ONLINE)
  {
    clearAllButFirst(onlineAircraft);
    onlineAircraftIds.clear();
  }
}

void MapSearchResult::moveOnlineAirspacesToFront()
{
  QList<MapAirspace> list;
  for(const MapAirspace& a: airspaces)
  {
    if(a.isOnline())
      list.append(a);
  }
  for(const MapAirspace& a: airspaces)
  {
    if(!a.isOnline())
      list.append(a);
  }
  airspaces = list;
}

MapSearchResult MapSearchResult::moveOnlineAirspacesToFront() const
{
  MapSearchResult retval(*this);
  retval.moveOnlineAirspacesToFront();
  return retval;
}

bool MapSearchResult::hasSimNavUserAirspaces() const
{
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      return true;
  }
  return false;
}

bool MapSearchResult::hasOnlineAirspaces() const
{
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(airspace.isOnline())
      return true;
  }
  return false;
}

map::MapAirspace *MapSearchResult::firstSimNavUserAirspace()
{
  QList<map::MapAirspace>::iterator it =
    std::find_if(airspaces.begin(), airspaces.end(), [](const map::MapAirspace& a) -> bool
    {
      return !a.isOnline();
    });

  if(it != airspaces.end())
    return &(*it);

  return nullptr;
}

map::MapAirspace *MapSearchResult::firstOnlineAirspace()
{
  QList<map::MapAirspace>::iterator it =
    std::find_if(airspaces.begin(), airspaces.end(), [](const map::MapAirspace& a) -> bool
    {
      return a.isOnline();
    });

  if(it != airspaces.end())
    return &(*it);

  return nullptr;
}

int MapSearchResult::numSimNavUserAirspaces() const
{
  int num = 0;
  for(const map::MapAirspace& airspace : airspaces)
    num += !airspace.isOnline();
  return num;
}

int MapSearchResult::numOnlineAirspaces() const
{
  int num = 0;
  for(const map::MapAirspace& airspace : airspaces)
    num += airspace.isOnline();
  return num;
}

QList<map::MapAirspace> MapSearchResult::getSimNavUserAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

QList<map::MapAirspace> MapSearchResult::getOnlineAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

void MapSearchResult::clearNavdataAirspaces()
{
  QList<map::MapAirspace>::iterator it = std::remove_if(airspaces.begin(), airspaces.end(),
                                                        [](const map::MapAirspace& airspace) -> bool
    {
      return !airspace.isOnline();
    });
  if(it != airspaces.end())
    airspaces.erase(it, airspaces.end());
}

void MapSearchResult::clearOnlineAirspaces()
{
  QList<map::MapAirspace>::iterator it = std::remove_if(airspaces.begin(), airspaces.end(),
                                                        [](const map::MapAirspace& airspace) -> bool
    {
      return airspace.isOnline();
    });
  if(it != airspaces.end())
    airspaces.erase(it, airspaces.end());
}

const atools::geo::Pos& MapSearchResult::getPosition(const std::initializer_list<MapObjectTypes>& types) const
{
  for(const MapObjectTypes& t : types)
  {
    if(!isEmpty(t))
    {
      if(t == map::AIRPORT)
        return airports.first().getPosition();
      else if(t == map::WAYPOINT)
        return waypoints.first().getPosition();
      else if(t == map::VOR)
        return vors.first().getPosition();
      else if(t == map::NDB)
        return ndbs.first().getPosition();
      else if(t == map::AIRWAY)
        return airways.first().getPosition();
      else if(t == map::RUNWAYEND)
        return runwayEnds.first().getPosition();
      else if(t == map::ILS)
        return ils.first().getPosition();
      else if(t == map::AIRSPACE)
        return airspaces.first().getPosition();
      else if(t == map::USERPOINTROUTE)
        return userpointsRoute.first().getPosition();
      else if(t == map::USERPOINT)
        return userpoints.first().getPosition();
      else if(t == map::LOGBOOK)
        return logbookEntries.first().getPosition();
      else if(t == map::AIRCRAFT_AI)
        return aiAircraft.first().getPosition();
      else if(t == map::AIRCRAFT_ONLINE)
        return onlineAircraft.first().getPosition();
    }
  }
  return atools::geo::EMPTY_POS;
}

QString MapSearchResult::getIdent(const std::initializer_list<MapObjectTypes>& types) const
{
  for(const MapObjectTypes& t : types)
  {
    if(!isEmpty(t))
    {
      if(t == map::AIRPORT)
        return airports.first().ident;
      else if(t == map::WAYPOINT)
        return waypoints.first().ident;
      else if(t == map::VOR)
        return vors.first().ident;
      else if(t == map::NDB)
        return ndbs.first().ident;
      else if(t == map::AIRWAY)
        return airways.first().name;
      else if(t == map::RUNWAYEND)
        return runwayEnds.first().name;
      else if(t == map::ILS)
        return ils.first().ident;
      else if(t == map::AIRSPACE)
        return airspaces.first().name;
      else if(t == map::USERPOINTROUTE)
        return userpointsRoute.first().ident;
      else if(t == map::USERPOINT)
        return userpoints.first().ident;
      else if(t == map::LOGBOOK)
        return logbookEntries.first().departureIdent;
      else if(t == map::AIRCRAFT_AI)
        return aiAircraft.first().getAirplaneRegistration();
      else if(t == map::AIRCRAFT_ONLINE)
        return onlineAircraft.first().getAirplaneRegistration();
    }
  }
  return QString();
}

bool MapSearchResult::getIdAndType(int& id, MapObjectTypes& type,
                                   const std::initializer_list<MapObjectTypes>& types) const
{
  id = -1;
  type = NONE;

  for(const MapObjectTypes& t : types)
  {
    if(!isEmpty(t))
    {
      if(t == map::AIRPORT)
      {
        id = airports.first().getId();
        type = t;
        break;
      }
      else if(t == map::WAYPOINT)
      {
        id = waypoints.first().getId();
        type = t;
        break;
      }
      else if(t == map::VOR)
      {
        id = vors.first().getId();
        type = t;
        break;
      }
      else if(t == map::NDB)
      {
        id = ndbs.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRWAY)
      {
        id = airways.first().getId();
        type = t;
        break;
      }
      else if(t == map::RUNWAYEND)
      {
        id = runwayEnds.first().getId();
        type = t;
        break;
      }
      else if(t == map::ILS)
      {
        id = ils.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRSPACE)
      {
        id = airspaces.first().getId();
        type = t;
        break;
      }
      else if(t == map::USERPOINTROUTE)
      {
        id = userpointsRoute.first().getId();
        type = t;
        break;
      }
      else if(t == map::USERPOINT)
      {
        id = userpoints.first().getId();
        type = t;
        break;
      }
      else if(t == map::LOGBOOK)
      {
        id = logbookEntries.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRCRAFT_AI)
      {
        id = aiAircraft.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRCRAFT_ONLINE)
      {
        id = onlineAircraft.first().getId();
        type = t;
        break;
      }
    }
  }
  return id != -1;
}

int MapSearchResult::size(const MapObjectTypes& types) const
{
  int totalSize = 0;
  totalSize += types & map::AIRPORT ? airports.size() : 0;
  totalSize += types & map::WAYPOINT ? waypoints.size() : 0;
  totalSize += types & map::VOR ? vors.size() : 0;
  totalSize += types & map::NDB ? ndbs.size() : 0;
  totalSize += types & map::AIRWAY ? airways.size() : 0;
  totalSize += types & map::RUNWAYEND ? runwayEnds.size() : 0;
  totalSize += types & map::ILS ? ils.size() : 0;
  totalSize += types & map::AIRSPACE ? airspaces.size() : 0;
  totalSize += types & map::USERPOINTROUTE ? userpointsRoute.size() : 0;
  totalSize += types & map::USERPOINT ? userpoints.size() : 0;
  totalSize += types & map::LOGBOOK ? logbookEntries.size() : 0;
  totalSize += types & map::AIRCRAFT_AI ? aiAircraft.size() : 0;
  totalSize += types & map::AIRCRAFT_ONLINE ? onlineAircraft.size() : 0;
  return totalSize;
}

QString edgeLights(const QString& type)
{
  if(type == "L")
    return QObject::tr("Low");
  else if(type == "M")
    return QObject::tr("Medium");
  else if(type == "H")
    return QObject::tr("High");
  else
    return QString();
}

QString patternDirection(const QString& type)
{
  if(type == "L")
    return QObject::tr("Left");
  else if(type == "R")
    return QObject::tr("Right");
  else
    return QString();
}

QString vorFullShortText(const MapVor& vor)
{
  QString type = vor.type.startsWith("VT") ? vor.type.at(vor.type.size() - 1) : vor.type.at(0);

  if(vor.tacan)
    return QObject::tr("TACAN").arg(type);
  else if(vor.vortac)
    return QObject::tr("VORTAC (%1)").arg(type);
  else if(vor.dmeOnly)
    return QObject::tr("DME (%1)").arg(type);
  else if(vor.hasDme)
    return QObject::tr("VORDME (%1)").arg(type);
  else
    return QObject::tr("VOR (%1)").arg(type);
}

QString ndbFullShortText(const MapNdb& ndb)
{
  QString type = ndb.type == "CP" ? QObject::tr("CL") : ndb.type;
  return type.isEmpty() ? QObject::tr("NDB") : QObject::tr("NDB (%1)").arg(type);
}

const QString& airspaceTypeToString(map::MapAirspaceTypes type)
{
  Q_ASSERT(!airspaceTypeNameMap.isEmpty());
  return airspaceTypeNameMap[type];
}

const QString& airspaceFlagToString(map::MapAirspaceFlags type)
{
  Q_ASSERT(!airspaceFlagNameMap.isEmpty());
  return airspaceFlagNameMap[type];
}

QString mapObjectTypeToString(MapObjectTypes type)
{
  if(type == NONE)
    return QObject::tr("None");
  else
  {
    QStringList str;

    if(type & AIRPORT)
      str += QObject::tr("Airport");
    if(type & AIRPORT_HARD)
      str += QObject::tr("AirportHard");
    if(type & AIRPORT_SOFT)
      str += QObject::tr("AirportSoft");
    if(type & AIRPORT_EMPTY)
      str += QObject::tr("AirportEmpty");
    if(type & AIRPORT_ADDON)
      str += QObject::tr("AirportAddon");
    if(type & VOR)
      str += QObject::tr("VOR");
    if(type & NDB)
      str += QObject::tr("NDB");
    if(type & ILS)
      str += QObject::tr("ILS");
    if(type & MARKER)
      str += QObject::tr("Marker");
    if(type & WAYPOINT)
      str += QObject::tr("Waypoint");
    if(type & AIRWAY)
      str += QObject::tr("Airway");
    if(type & AIRWAYV)
      str += QObject::tr("Airwayv");
    if(type & AIRWAYJ)
      str += QObject::tr("Airwayj");
    if(type & TRACK)
      str += QObject::tr("Track");
    if(type & FLIGHTPLAN)
      str += QObject::tr("Flightplan");
    if(type & AIRCRAFT)
      str += QObject::tr("Aircraft");
    if(type & AIRCRAFT_AI)
      str += QObject::tr("AircraftAi");
    if(type & AIRCRAFT_AI_SHIP)
      str += QObject::tr("AircraftAiShip");
    if(type & AIRCRAFT_TRACK)
      str += QObject::tr("AircraftTrack");
    if(type & USERPOINTROUTE)
      str += QObject::tr("Userpointroute");
    if(type & PARKING)
      str += QObject::tr("Parking");
    if(type & RUNWAYEND)
      str += QObject::tr("Runwayend");
    if(type & INVALID)
      str += QObject::tr("Invalid");
    if(type & MISSED_APPROACH)
      str += QObject::tr("Missed_approach");
    if(type & PROCEDURE)
      str += QObject::tr("Procedure");
    if(type & AIRSPACE)
      str += QObject::tr("Airspace");
    if(type & HELIPAD)
      str += QObject::tr("Helipad");
    if(type & COMPASS_ROSE)
      str += QObject::tr("CompassRose");
    if(type & USERPOINT)
      str += QObject::tr("Userpoint");
    if(type & AIRCRAFT_ONLINE)
      str += QObject::tr("AircraftOnline");
    if(type & LOGBOOK)
      str += QObject::tr("Logbook");

    return str.join(QObject::tr(", "));
  }
}

const QString& airspaceRemark(map::MapAirspaceTypes type)
{
  Q_ASSERT(!airspaceRemarkMap.isEmpty());
  return airspaceRemarkMap[type];
}

int airspaceDrawingOrder(map::MapAirspaceTypes type)
{
  Q_ASSERT(!airspacePriorityMap.isEmpty());
  return airspacePriorityMap.value(type);
}

map::MapAirspaceTypes airspaceTypeFromDatabase(const QString& type)
{
  Q_ASSERT(!airspaceTypeFromDatabaseMap.isEmpty());
  return airspaceTypeFromDatabaseMap.value(type);
}

const QString& airspaceTypeToDatabase(map::MapAirspaceTypes type)
{
  Q_ASSERT(!airspaceTypeToDatabaseMap.isEmpty());
  return airspaceTypeToDatabaseMap[type];
}

QString airspaceSourceText(MapAirspaceSources src)
{
  QStringList retval;
  if(src == AIRSPACE_SRC_NONE)
    retval.append(QObject::tr("None"));
  else if(src == AIRSPACE_SRC_ALL)
    retval.append(QObject::tr("All"));
  else
  {
    if(src & AIRSPACE_SRC_SIM)
      retval.append(QObject::tr("Simulator"));

    if(src & AIRSPACE_SRC_NAV)
      retval.append(QObject::tr("Navigraph"));

    if(src & AIRSPACE_SRC_ONLINE)
      retval.append(QObject::tr("Online"));

    if(src & AIRSPACE_SRC_USER)
      retval.append(QObject::tr("User"));
  }

  return retval.join(QObject::tr(", "));
}

bool runwayAlmostEqual(const QString& name1, const QString& name2)
{
  QString rwdesignator1, rwdesignator2;
  int rwnum1, rwnum2;
  map::runwayNameSplit(name1, &rwnum1, &rwdesignator1);
  map::runwayNameSplit(name2, &rwnum2, &rwdesignator2);

  return (rwnum2 == rwnum1 || rwnum2 == (rwnum1 < 36 ? rwnum1 + 1 : 1) || rwnum2 == (rwnum1 > 1 ? rwnum1 - 1 : 36)) &&
         rwdesignator1 == rwdesignator2;
}

bool runwayEqual(QString name1, QString name2)
{
  if(name1.startsWith('0'))
    name1 = name1.mid(1);
  else if(name1.startsWith("RW0"))
    name1 = name1.mid(3);

  if(name2.startsWith('0'))
    name2 = name2.mid(1);
  else if(name2.startsWith("RW0"))
    name2 = name2.mid(3);
  return name1.compare(name2, Qt::CaseInsensitive) == 0;
}

QString runwayNameJoin(int number, const QString& designator)
{
  return QString("%1%2").arg(number, 2, 10, QChar('0')).arg(designator);
}

/* Gives all variants of the runway (+1 and -1) plus the original one as the first in the list */
QStringList runwayNameVariants(QString name)
{
  QString prefix;
  if(name.startsWith("RW"))
  {
    prefix = "RW";
    name = name.mid(2);
  }

  QStringList retval({name});
  QString designator;
  int number;
  map::runwayNameSplit(name, &number, &designator);

  // Try next higher runway number
  retval.append(prefix + map::runwayNameJoin(number < 36 ? number + 1 : 1, designator));

  // Try next lower runway number
  retval.append(prefix + map::runwayNameJoin(number > 1 ? number - 1 : 36, designator));

  return retval;
}

QStringList runwayNameZeroPrefixVariants(QString name)
{
  QStringList retval({name});

  if(name.startsWith('0'))
    retval.append(name.mid(1));
  else if(name.startsWith("RW0"))
    retval.append("RW" + name.mid(3));

  return retval;
}

/* Gives all variants of the runway (+1 and -1) plus the original one as the first in the list for an
 * ARINC name like N32 or I19-Y */
QStringList arincNameNameVariants(const QString& name)
{
  QStringList retval({name});
  QString prefix, suffix, rw;
  if(name.size() >= 3 && name.at(1).isDigit() && name.at(2).isDigit())
  {
    prefix = name.left(1);
    rw = name.mid(1, 2);
    suffix = name.mid(3);

    QString designator;
    int number;
    map::runwayNameSplit(rw, &number, &designator);

    // Try next higher runway number
    retval.append(prefix + map::runwayNameJoin(number < 36 ? number + 1 : 1, designator) + suffix);

    // Try next lower runway number
    retval.append(prefix + map::runwayNameJoin(number > 1 ? number - 1 : 36, designator) + suffix);
  }
  return retval;
}

QString runwayBestFit(const QString& procRunwayName, const QStringList& airportRunwayNames)
{
  // Get a extended list that also contains all variants like 09C vs 9C
  QStringList names;
  for(const QString& r : airportRunwayNames)
    names.append(map::runwayNameZeroPrefixVariants(r));

  QString prefix;
  QString rwname(procRunwayName);
  if(rwname.startsWith("RW"))
  {
    prefix = "RW";
    rwname = rwname.mid(2);
  }

  if(rwname.isEmpty())
    return QString();

  // First check for exact match
  if(names.contains(rwname))
    return procRunwayName;

  if(rwname.startsWith('0') && names.contains(rwname.mid(1)))
    return procRunwayName;

  QStringList variants = runwayNameVariants(rwname);
  for(const QString& runway : names)
  {
    if(variants.contains(runway))
      return prefix + runway;
  }

  if(rwname.startsWith('0'))
  {
    variants = runwayNameVariants(rwname.mid(1));
    for(const QString& runway : names)
    {
      if(variants.contains(runway))
        return prefix + runway;
    }
  }
  return QString();
}

bool runwayNameSplit(const QString& name, int *number, QString *designator)
{
  // Extract runway number and designator
  static QRegularExpression NUM_DESIGNATOR("^([0-9]{1,2})([LRCWAB]?)$");

  QString rwname(name);
  if(rwname.startsWith("RW"))
    rwname = rwname.mid(2);

  QRegularExpressionMatch match = NUM_DESIGNATOR.match(rwname);
  if(match.hasMatch())
  {
    if(number != nullptr)
      *number = match.captured(1).toInt();
    if(designator != nullptr)
      *designator = match.captured(2);
    return true;
  }
  return false;
}

bool runwayNameSplit(const QString& name, QString *number, QString *designator)
{
  int num = 0;
  bool retval = runwayNameSplit(name, &num, designator);

  if(retval && number != nullptr)
    // If it is a number with designator make sure to add a 0 prefix
    *number = QString("%1").arg(num, 2, 10, QChar('0'));
  return retval;
}

QDebug operator<<(QDebug out, const map::MapSearchResult& record)
{
  QDebugStateSaver saver(out);

  if(!record.airports.isEmpty())
  {
    out << "Airport";
    for(const map::MapAirport& obj :  record.airports)
      out << obj.id << obj.ident << ",";
  }
  if(!record.runwayEnds.isEmpty())
  {
    out << "RunwayEnd";
    for(const map::MapRunwayEnd& obj :  record.runwayEnds)
      out << obj.name << ",";
  }
  if(!record.parkings.isEmpty())
  {
    out << "Parking";
    for(const map::MapParking& obj :  record.parkings)
      out << obj.id << obj.name << obj.number << obj.type << ",";
  }
  if(!record.waypoints.isEmpty())
  {
    out << "Waypoint";
    for(const map::MapWaypoint& obj :  record.waypoints)
      out << obj.id << obj.ident << obj.region << ",";
  }
  if(!record.vors.isEmpty())
  {
    out << "VOR";
    for(const map::MapVor& obj :  record.vors)
      out << obj.id << obj.ident << obj.region << ",";
  }
  if(!record.ndbs.isEmpty())
  {
    out << "NDB";
    for(const map::MapNdb& obj :  record.ndbs)
      out << obj.id << obj.ident << obj.region << ",";
  }
  if(!record.ils.isEmpty())
  {
    out << "ILS";
    for(const map::MapIls& obj :  record.ils)
      out << obj.id << obj.ident << ",";
  }
  if(!record.airways.isEmpty())
  {
    out << "Airway";
    for(const map::MapAirway& obj :  record.airways)
      out << obj.id << obj.name << ",";
  }
  if(!record.airspaces.isEmpty())
  {
    out << "Airspace";
    for(const map::MapAirspace& obj :  record.airspaces)
      out << obj.id << obj.name << ",";
  }
  if(!record.userpointsRoute.isEmpty())
  {
    out << "UserpointRoute";
    for(const map::MapUserpointRoute& obj :  record.userpointsRoute)
      out << obj.id << obj.ident << ",";
  }
  if(!record.userpoints.isEmpty())
  {
    out << "Userpoint";
    for(const map::MapUserpoint& obj :  record.userpoints)
      out << obj.id << obj.name << ",";
  }
  return out;
}

QDebug operator<<(QDebug out, const MapBase& obj)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapBase["
                          << "id " << obj.id
                          << ", type " << mapObjectTypeToString(obj.objType)
                          << ", " << obj.position
                          << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapObjectRef& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapObjectRef["
                          << "id " << ref.id
                          << ", type " << mapObjectTypeToString(ref.objType)
                          << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapObjectRefExt& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapObjectRefExt["
                          << "id " << ref.id
                          << ", type " << mapObjectTypeToString(ref.objType);

  if(!ref.name.isEmpty())
    out.noquote().nospace() << ", name " << ref.name;

  if(ref.position.isValid())
    out.noquote().nospace() << ", " << ref.position;

  out.noquote().nospace() << "]";
  return out;
}

QDebug operator<<(QDebug out, const WeatherContext& record)
{
  QDebugStateSaver saver(out);

  out << "WeatherContext["
      << "Sim METAR" << record.fsMetar
      << "IVAO METAR" << record.ivaoMetar
      << "NOAA METAR" << record.noaaMetar
      << "VATSIM METAR" << record.vatsimMetar
      << "AS departure" << record.isAsDeparture
      << "AS destination" << record.isAsDestination
      << "AS METAR" << record.asMetar
      << "AS type" << record.asType
      << "ident" << record.ident << "]";
  return out;
}

QString ilsText(const MapIls& ils)
{
  QString text = ils.ident + " / " +
                 QString::number(ils.frequency / 1000., 'f', 2) + " / " +
                 QString::number(atools::geo::normalizeCourse(ils.heading - ils.magvar), 'f', 0) + QObject::tr("°M");

  if(ils.slope > 0.1f)
    text += QObject::tr(" / GS ") + QString::number(ils.slope, 'f', 1) + QObject::tr("°");
  if(ils.hasDme)
    text += QObject::tr(" / DME");

  return text;
}

QString ilsTextShort(const map::MapIls& ils)
{
  return ilsTextShort(ils.ident, ils.name, ils.slope > 0.f, ils.hasDme);
}

QString ilsType(const map::MapIls& ils)
{
  QString txt = ils.slope > 0.f ? QObject::tr("ILS, GS") : QObject::tr("LOC");
  if(ils.hasDme)
    txt.append(QObject::tr(", DME"));
  return txt;
}

QString ilsTextShort(QString ident, QString name, bool gs, bool dme)
{
  QString text;

  if(gs)
  {
    // Is real ILS with glideslooe
    if(!name.isEmpty() && !name.startsWith(QObject::tr("ILS")))
      name.prepend(QObject::tr("ILS "));
  }
  else if(!name.startsWith(QObject::tr("LOC")))
  {
    // Is localizer only
    if(!name.isEmpty())
      name.prepend(QObject::tr("LOC "));
  }

  if(name.isEmpty())
    text.append(ident);
  else
    text.append(QObject::tr("%1 (%2)").arg(name).arg(ident));

  if(gs)
    text += QObject::tr(", GS ");
  if(dme)
    text += QObject::tr(", DME");

  return text;
}

float Hold::magCourse() const
{
  return atools::geo::normalizeCourse(courseTrue - magvar);
}

float TrafficPattern::magCourse() const
{
  return atools::geo::normalizeCourse(courseTrue - magvar);
}

atools::geo::Line MapIls::centerLine() const
{
  return atools::geo::Line(position, posmid);
}

atools::geo::LineString MapIls::boundary() const
{
  if(slope > 0.f)
    return atools::geo::LineString({position, pos1, pos2, position});
  else
    return atools::geo::LineString({position, pos1, posmid, pos2, position});
}

void MapSearchResultIndex::addFromResult(const MapSearchResult& resultParm, const MapObjectTypes& types)
{
  if(types & AIRPORT)
  {
    result.airports.append(resultParm.airports);
    addAll(result.airports);
  }
  if(types & RUNWAYEND)
  {
    result.runwayEnds.append(resultParm.runwayEnds);
    addAll(result.runwayEnds);
  }
  if(types & TOWER)
  {
    result.towers.append(resultParm.towers);
    addAll(result.towers);
  }
  if(types & PARKING)
  {
    result.parkings.append(resultParm.parkings);
    addAll(result.parkings);
  }
  if(types & HELIPAD)
  {
    result.helipads.append(resultParm.helipads);
    addAll(result.helipads);
  }
  if(types & WAYPOINT)
  {
    result.waypoints.append(resultParm.waypoints);
    addAll(result.waypoints);
  }
  if(types & VOR)
  {
    result.vors.append(resultParm.vors);
    addAll(result.vors);
  }
  if(types & NDB)
  {
    result.ndbs.append(resultParm.ndbs);
    addAll(result.ndbs);
  }
  if(types & MARKER)
  {
    result.markers.append(resultParm.markers);
    addAll(result.markers);
  }
  if(types & ILS)
  {
    result.ils.append(resultParm.ils);
    addAll(result.ils);
  }
  if(types & AIRWAY)
  {
    result.airways.append(resultParm.airways);
    addAll(result.airways);
  }
  if(types & AIRSPACE)
  {
    result.airspaces.append(resultParm.airspaces);
    addAll(result.airspaces);
  }
  if(types & USERPOINTROUTE)
  {
    result.userpointsRoute.append(resultParm.userpointsRoute);
    addAll(result.userpointsRoute);
  }
  if(types & USERPOINT)
  {
    result.userpoints.append(resultParm.userpoints);
    addAll(result.userpoints);
  }
  if(types & LOGBOOK)
  {
    result.logbookEntries.append(resultParm.logbookEntries);
    addAll(result.logbookEntries);
  }
}

void MapSearchResultIndex::sortByDistance(const atools::geo::Pos& pos, bool sortNearToFar)
{
  if(isEmpty() || !pos.isValid())
    return;

  std::sort(begin(), end(),
            [ = ](const MapBase *obj1, const MapBase *obj2) -> bool
    {
      bool res = obj1->getPosition().distanceMeterTo(pos) < obj2->getPosition().distanceMeterTo(pos);
      return sortNearToFar ? res : !res;
    });
}

void MapSearchResultIndex::removeByDistance(const atools::geo::Pos& pos, float maxDistanceNm)
{
  if(isEmpty() || !pos.isValid())
    return;

  float maxMeter = atools::geo::nmToMeter(maxDistanceNm);

  auto it = std::remove_if(begin(), end(), [ = ](const MapBase *obj) -> bool
    {
      return obj->position.distanceMeterTo(pos) > maxMeter;
    });

  if(it != end())
    erase(it, end());
}

} // namespace types
