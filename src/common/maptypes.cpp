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

#include "common/maptypes.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/formatter.h"
#include "common/mapcolors.h"
#include "common/proctypes.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "common/vehicleicons.h"
#include "fs/util/fsutil.h"
#include "geo/calculations.h"
#include "mapgui/maplayer.h"
#include "userdata/userdataicons.h"

#include <QDataStream>
#include <QIcon>
#include <QRegularExpression>
#include <QRegularExpression>
#include <QStringBuilder>

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
static QVector<std::pair<QRegularExpression, QString> > parkingDatabaseKeywords;
static QHash<QString, QString> navTypeNamesVor;
static QHash<QString, QString> navTypeNamesVorLong;
static QHash<QString, QString> navTypeNamesNdb;
static QHash<QString, QString> navTypeNamesWaypoint;
static QHash<QString, QString> navTypeNames;
static QHash<QString, QString> comTypeNames;
static QHash<map::MapAirspaceTypes, QString> airspaceTypeNameMap;
static QHash<map::MapAirspaceTypes, QString> airspaceTypeShortNameMap;
static QHash<map::MapAirspaceFlags, QString> airspaceFlagNameMap;
static QHash<map::MapAirspaceFlags, QString> airspaceFlagNameMapLong;
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
      {"RE", QObject::tr("Ramp Extra")},
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
      {"GE", QObject::tr("Extra")},
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
      {"RE", QObject::tr("Ramp Extra")},
      {"RM", QObject::tr("Ramp Mil")},
      {"RMC", QObject::tr("Ramp Mil Cargo")},
      {"RMCB", QObject::tr("Ramp Mil Combat")},
      {"T", QObject::tr("Tie down")},
      {"H", QObject::tr("Hangar")},
      {"G", QObject::tr("Gate")},
      {"GS", QObject::tr("Gate Small")},
      {"GM", QObject::tr("Gate Medium")},
      {"GH", QObject::tr("Gate Heavy")},
      {"GE", QObject::tr("Gate Extra")},
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
      {"RE", QObject::tr("Ramp GA Extra")},
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
      {"GE", QObject::tr("Gate Extra")},
      {"DGA", QObject::tr("Dock GA")},
      {"FUEL", QObject::tr("Fuel")},
      {"V", QObject::tr("Vehicles")}
    });

  // Map for user interface names
  parkingNameMap = QHash<QString, QString>(
    {
      {"INVALID", QObject::tr("Invalid")},
      {"UNKNOWN", QObject::tr("Unknown")},
      {"NONE", QObject::tr("Parking")},
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

  /* *INDENT-OFF* */
  // Order is important
  parkingDatabaseKeywords = QVector<std::pair<QRegularExpression, QString> >({
      {QRegularExpression("\\b" % QObject::tr("Apron", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "A"},
      {QRegularExpression("\\b" % QObject::tr("Cargo", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "C"},
      {QRegularExpression("\\b" % QObject::tr("Combat", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "C"},
      {QRegularExpression("\\b" % QObject::tr("Commuter") % "\\b", QRegularExpression::CaseInsensitiveOption), "C"},
      {QRegularExpression("\\b" % QObject::tr("Club") % "\\b", QRegularExpression::CaseInsensitiveOption), "C"},
      {QRegularExpression("\\b" % QObject::tr("Concrete") % "\\b", QRegularExpression::CaseInsensitiveOption), "C"},
      {QRegularExpression("\\b" % QObject::tr("Center") % "\\b", QRegularExpression::CaseInsensitiveOption), "C"},
      {QRegularExpression("\\b" % QObject::tr("Domestic") % "\\b", QRegularExpression::CaseInsensitiveOption), "D"},
      {QRegularExpression("\\b" % QObject::tr("Docking") % "\\b", QRegularExpression::CaseInsensitiveOption), "D"},
      {QRegularExpression("\\b" % QObject::tr("Dock", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "D"},
      {QRegularExpression("\\b" % QObject::tr("Eastern") % "\\b", QRegularExpression::CaseInsensitiveOption), "E"},
      {QRegularExpression("\\b" % QObject::tr("East", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "E"},
      {QRegularExpression("\\b" % QObject::tr("Extra", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "E"},
      {QRegularExpression("\\b" % QObject::tr("Executive") % "\\b", QRegularExpression::CaseInsensitiveOption), "E"},
      {QRegularExpression("\\b" % QObject::tr("Fuel-Start") % "\\b", QRegularExpression::CaseInsensitiveOption), "F"},
      {QRegularExpression("\\b" % QObject::tr("Fueling Station") % "\\b", QRegularExpression::CaseInsensitiveOption), "F"},
      {QRegularExpression("\\b" % QObject::tr("Fuel", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "F"},
      {QRegularExpression("\\b" % QObject::tr("Gate", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "G"},
      {QRegularExpression("\\b" % QObject::tr("General Aviation") % "\\b", QRegularExpression::CaseInsensitiveOption), "GA"},
      {QRegularExpression("\\b" % QObject::tr("Hangars") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Hangar") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Hangers") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Hanger") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Heavy", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Hold Short") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Hold") % "\\b", QRegularExpression::CaseInsensitiveOption), "H"},
      {QRegularExpression("\\b" % QObject::tr("Maintenance") % "\\b", QRegularExpression::CaseInsensitiveOption), "M"},
      {QRegularExpression("\\b" % QObject::tr("Medium", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "M"},
      {QRegularExpression("\\b" % QObject::tr("Military", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "M"},
      {QRegularExpression("\\b" % QObject::tr("Mil", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "M"},
      {QRegularExpression("\\b" % QObject::tr("New") % "\\b", QRegularExpression::CaseInsensitiveOption), ""},
      {QRegularExpression("\\b" % QObject::tr("Northern") % "\\b", QRegularExpression::CaseInsensitiveOption), "N"},
      {QRegularExpression("\\b" % QObject::tr("North", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "N"},
      {QRegularExpression("\\b" % QObject::tr("Parking", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "P"},
      {QRegularExpression("\\b" % QObject::tr("Park") % "\\b", QRegularExpression::CaseInsensitiveOption), "P"},
      {QRegularExpression("\\b" % QObject::tr("Position") % "\\b", QRegularExpression::CaseInsensitiveOption), "P"},
      {QRegularExpression("\\b" % QObject::tr("Pos") % "\\b", QRegularExpression::CaseInsensitiveOption), "P"},
      {QRegularExpression("\\b" % QObject::tr("Private") % "\\b", QRegularExpression::CaseInsensitiveOption), "P"},
      {QRegularExpression("\\b" % QObject::tr("Ramp", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "R"},
      {QRegularExpression("\\b" % QObject::tr("Run up area") % "\\b", QRegularExpression::CaseInsensitiveOption), "R"},
      {QRegularExpression("\\b" % QObject::tr("Run up") % "\\b", QRegularExpression::CaseInsensitiveOption), "R"},
      {QRegularExpression("\\b" % QObject::tr("Sky Dive") % "\\b", QRegularExpression::CaseInsensitiveOption), "SD"},
      {QRegularExpression("\\b" % QObject::tr("Small") % "\\b", QRegularExpression::CaseInsensitiveOption), "S"},
      {QRegularExpression("\\b" % QObject::tr("Southern") % "\\b", QRegularExpression::CaseInsensitiveOption), "S"},
      {QRegularExpression("\\b" % QObject::tr("South", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "S"},
      {QRegularExpression("\\b" % QObject::tr("Stand") % "\\b", QRegularExpression::CaseInsensitiveOption), "S"},
      {QRegularExpression("\\b" % QObject::tr("Start") % "\\b", QRegularExpression::CaseInsensitiveOption), "S"},
      {QRegularExpression("\\b" % QObject::tr("Takeoff") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Terminal") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Tie down") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Tie-down") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Tiedowns") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Tiedown") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Transient") % "\\b", QRegularExpression::CaseInsensitiveOption), "T"},
      {QRegularExpression("\\b" % QObject::tr("Western") % "\\b", QRegularExpression::CaseInsensitiveOption), "W"},
      {QRegularExpression("\\b" % QObject::tr("West", "Has to match other parking keyword translations") % "\\b", QRegularExpression::CaseInsensitiveOption), "W"}
    });
  /* *INDENT-ON* */

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
      {"N", QObject::tr("NDB")},
      {"VFR", QObject::tr("VFR")},
      {"RNAV", QObject::tr("RNAV")},
      {"OA", QObject::tr("Off Airway")},
      {"IAF", QObject::tr("IAF")},
      {"FAF", QObject::tr("FAF")}
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
      {"EFS", QObject::tr("En-route Flight Advisory Service (EFAS)")},
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
      {"UC", QObject::tr("UNICOM")},
      {"VOL", QObject::tr("VOLMET")}
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
      {map::FIR, QObject::tr("FIR")},
      {map::UIR, QObject::tr("UIR")},
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
      {map::GCA, QObject::tr("General Control Area")},
      {map::MCTR, QObject::tr("Military Control Zone")},
      {map::TRSA, QObject::tr("Terminal Radar Service Area")},
      {map::TRAINING, QObject::tr("Training")},
      {map::GLIDERPROHIBITED, QObject::tr("Glider Prohibited")},
      {map::WAVEWINDOW, QObject::tr("Wave Window")},
      {map::ONLINE_OBSERVER, QObject::tr("Online Observer")}
    });

  airspaceTypeShortNameMap = QHash<map::MapAirspaceTypes, QString>(
    {
      {map::AIRSPACE_NONE, QObject::tr("No Airspace")},
      {map::CENTER, QObject::tr("CTR")},
      {map::CLASS_A, QObject::tr("A")},
      {map::CLASS_B, QObject::tr("B")},
      {map::CLASS_C, QObject::tr("C")},
      {map::CLASS_D, QObject::tr("D")},
      {map::CLASS_E, QObject::tr("E")},
      {map::CLASS_F, QObject::tr("F")},
      {map::CLASS_G, QObject::tr("G")},
      {map::FIR, QObject::tr("FIR")},
      {map::UIR, QObject::tr("UIR")},
      {map::TOWER, QObject::tr("TWR")},
      {map::CLEARANCE, QObject::tr("CLR")},
      {map::GROUND, QObject::tr("GND")},
      {map::DEPARTURE, QObject::tr("DEP")},
      {map::APPROACH, QObject::tr("APP")},
      {map::MOA, QObject::tr("MOA")},
      {map::RESTRICTED, QObject::tr("R")},
      {map::PROHIBITED, QObject::tr("P")},
      {map::WARNING, QObject::tr("W")},
      {map::CAUTION, QObject::tr("CN")},
      {map::ALERT, QObject::tr("A")},
      {map::DANGER, QObject::tr("D")},
      {map::NATIONAL_PARK, QObject::tr("National Park")},
      {map::MODEC, QObject::tr("Mode-C")},
      {map::RADAR, QObject::tr("Radar")},
      {map::GCA, QObject::tr("GCA")},
      {map::MCTR, QObject::tr("MCZ")},
      {map::TRSA, QObject::tr("TRSA")},
      {map::TRAINING, QObject::tr("T")},
      {map::GLIDERPROHIBITED, QObject::tr("Glider Prohibited")},
      {map::WAVEWINDOW, QObject::tr("Wave Window")},
      {map::ONLINE_OBSERVER, QObject::tr("Online Observer")}
    });

  airspaceFlagNameMap = QHash<map::MapAirspaceFlags, QString>(
    {
      // Values below only for actions
      {map::AIRSPACE_ALTITUDE_ALL, QObject::tr("&All altitudes")},
      {map::AIRSPACE_ALTITUDE_FLIGHTPLAN, QObject::tr("At &flight plan cruise altitude")},
      {map::AIRSPACE_ALTITUDE_SET, QObject::tr("For &minimum and maximum altitude")}
    });

  airspaceFlagNameMapLong = QHash<map::MapAirspaceFlags, QString>(
    {
      // Values below only for actions
      {map::AIRSPACE_ALTITUDE_ALL, QObject::tr("Show airspaces for all altitudes")},
      {map::AIRSPACE_ALTITUDE_FLIGHTPLAN, QObject::tr("Show airspaces touching flight plan cruise altitude")},
      {map::AIRSPACE_ALTITUDE_SET, QObject::tr("Show airspaces overlapping selected minimum and maximum altitude")}
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
      {map::FIR, QObject::tr("Uncontrolled, IFR and VFR, ATC clearance not required.")},
      {map::UIR, QObject::tr("Uncontrolled, IFR and VFR, ATC clearance not required.")},
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
      {map::GCA, QString()},
      {map::MCTR, QString()},
      {map::TRSA, QString()},
      {map::TRAINING, QString()},
      {map::GLIDERPROHIBITED, QString()},
      {map::WAVEWINDOW, QObject::tr("Sailplane Area.")},
      {map::ONLINE_OBSERVER, QObject::tr("Online network observer")}
    });

}

// Map for flight plan values
static QHash<QString, QString> parkingNameMapUntranslated(
  {
    {"INVALID", QLatin1String("Invalid")},
    {"UNKNOWN", QLatin1String("Unknown")},
    {"NONE", QLatin1String("Parking None")},
    {"P", QLatin1String("Parking")},
    {"NP", QLatin1String("N Parking")},
    {"NEP", QLatin1String("NE Parking")},
    {"EP", QLatin1String("E Parking")},
    {"SEP", QLatin1String("SE Parking")},
    {"SP", QLatin1String("S Parking")},
    {"SWP", QLatin1String("SW Parking")},
    {"WP", QLatin1String("W Parking")},
    {"NWP", QLatin1String("NW Parking")},
    {"G", QLatin1String("Gate")},
    {"D", QLatin1String("Dock")},
    {"GA", QLatin1String("Gate A")},
    {"GB", QLatin1String("Gate B")},
    {"GC", QLatin1String("Gate C")},
    {"GD", QLatin1String("Gate D")},
    {"GE", QLatin1String("Gate E")},
    {"GF", QLatin1String("Gate F")},
    {"GG", QLatin1String("Gate G")},
    {"GH", QLatin1String("Gate H")},
    {"GI", QLatin1String("Gate I")},
    {"GJ", QLatin1String("Gate J")},
    {"GK", QLatin1String("Gate K")},
    {"GL", QLatin1String("Gate L")},
    {"GM", QLatin1String("Gate M")},
    {"GN", QLatin1String("Gate N")},
    {"GO", QLatin1String("Gate O")},
    {"GP", QLatin1String("Gate P")},
    {"GQ", QLatin1String("Gate Q")},
    {"GR", QLatin1String("Gate R")},
    {"GS", QLatin1String("Gate S")},
    {"GT", QLatin1String("Gate T")},
    {"GU", QLatin1String("Gate U")},
    {"GV", QLatin1String("Gate V")},
    {"GW", QLatin1String("Gate W")},
    {"GX", QLatin1String("Gate X")},
    {"GY", QLatin1String("Gate Y")},
    {"GZ", QLatin1String("Gate Z")}
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
    {"FIR", map::FIR}, // New FIR region that replaces certain centers
    {"UIR", map::UIR}, // As above for UIR
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
    {"GCA", map::GCA}, // Control area - new type
    {"MCTR", map::MCTR},
    {"TRSA", map::TRSA},
    {"TR", map::TRAINING},
    {"GP", map::GLIDERPROHIBITED},
    {"WW", map::WAVEWINDOW},
    {"OBS", map::ONLINE_OBSERVER} /* No database type */
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
    {map::FIR, "FIR"},
    {map::UIR, "UIR"},
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
    {map::GCA, "GCA"},
    {map::MCTR, "MCTR"},
    {map::TRSA, "TRSA"},
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

    {map::FIR, 4},
    {map::UIR, 5},

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
    {map::MODEC, 6},
    {map::RADAR, 7},

    {map::GCA, 15},
    {map::MCTR, 16},
    {map::TRSA, 17},

    {map::TRAINING, 59},
  });

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

QString navTypeNameVor(const QString& type)
{
  return navTypeNamesVor.value(type);
}

QString navTypeNameVorLong(const QString& type)
{
  return navTypeNamesVorLong.value(type);
}

QString navTypeNameNdb(const QString& type)
{
  return navTypeNamesNdb.value(type);
}

QString navTypeNameWaypoint(const QString& type)
{
  return navTypeNamesWaypoint.value(type);
}

QString navTypeArincNamesWaypoint(const QString& type)
{
  QStringList types;

  QChar c0 = atools::charAt(type, 0).toUpper();
  if(c0 == 'A')
    types.append(QObject::tr("ARC center fix waypoint"));
  else if(c0 == 'C')
    types.append(QObject::tr("Combined named intersection and RNAV waypoint"));
  else if(c0 == 'I')
    types.append(QObject::tr("Unnamed, charted intersection"));
  else if(c0 == 'M')
    types.append(QObject::tr("Middle marker as waypoint"));
  else if(c0 == 'N')
    types.append(QObject::tr("Terminal NDB navaid as waypoint"));
  else if(c0 == 'O')
    types.append(QObject::tr("Outer marker as waypoint"));
  else if(c0 == 'R')
    types.append(QObject::tr("Named intersection"));
  else if(c0 == 'V')
    types.append(QObject::tr("VFR waypoint"));
  else if(c0 == 'W')
    types.append(QObject::tr("RNAV waypoint"));

  QChar c1 = atools::charAt(type, 1).toUpper();
  if(c1 == 'A')
    types.append(QObject::tr("Final approach fix"));
  else if(c1 == 'B')
    types.append(QObject::tr("Initial approach fix and final approach fix"));
  else if(c1 == 'C')
    types.append(QObject::tr("final approach course fix"));
  else if(c1 == 'D')
    types.append(QObject::tr("Intermediate approach fix"));
  else if(c1 == 'I')
    types.append(QObject::tr("Initial approach fix"));
  else if(c1 == 'K')
    types.append(QObject::tr("Final approach course fix at initial approach fix"));
  else if(c1 == 'L')
    types.append(QObject::tr("Final approach course fix at intermediate approach fix"));
  else if(c1 == 'M')
    types.append(QObject::tr("Missed approach fix"));
  else if(c1 == 'N')
    types.append(QObject::tr("Initial approach fix and missed approach fix"));
  else if(c1 == 'P')
    types.append(QObject::tr("Unnamed stepdown fix"));
  else if(c1 == 'S')
    types.append(QObject::tr("Named stepdown fix"));
  else if(c1 == 'U')
    types.append(QObject::tr("FIR/UIR or controlled airspace intersection"));

  QChar c2 = atools::charAt(type, 2).toUpper();
  if(c2 == 'D')
    types.append(QObject::tr("SID"));
  else if(c2 == 'E')
    types.append(QObject::tr("STAR"));
  else if(c2 == 'F')
    types.append(QObject::tr("Approach"));
  else if(c2 == 'Z')
    types.append(QObject::tr("Multiple"));

#ifdef DEBUG_INFORMATION
  types.append(QString("[%1]").arg(type));
#endif

  return types.join(QObject::tr(", "));
}

QString navName(const QString& type)
{
  return navTypeNames.value(type);
}

const QString& surfaceName(const QString& surface)
{
  Q_ASSERT(!surfaceMap.isEmpty());
  return surfaceMap[surface];
}

QString smoothnessName(QVariant smoothnessVar)
{
  QString smoothnessStr;
  if(!smoothnessVar.isNull())
  {
    float smooth = smoothnessVar.toFloat();
    if(smooth >= 0.f)
    {
      if(smooth <= 0.2f)
        smoothnessStr = QObject::tr("Very smooth");
      else if(smooth <= 0.4f)
        smoothnessStr = QObject::tr("Smooth");
      else if(smooth <= 0.6f)
        smoothnessStr = QObject::tr("Normal");
      else if(smooth <= 0.8f)
        smoothnessStr = QObject::tr("Rough");
      else
        smoothnessStr = QObject::tr("Very rough");
    }
  }
  return smoothnessStr;
}

int surfaceQuality(const QString& surface)
{
  Q_ASSERT(!surfaceQualityMap.isEmpty());
  return surfaceQualityMap.value(surface, 0);
}

const QVector<std::pair<QRegularExpression, QString> >& parkingKeywords()
{
  Q_ASSERT(!parkingDatabaseKeywords.isEmpty());
  return parkingDatabaseKeywords;
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

QString parkingText(const MapParking& parking)
{
  QStringList retval;

  if(parking.type.isEmpty())
    retval.append(QObject::tr("Parking"));

  retval.append(map::parkingName(parking.name));

  retval.append(parking.number != -1 ? " " % QLocale().toString(parking.number) % parking.suffix : QString());
  return atools::strJoin(retval, QObject::tr(" "));
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

QString parkingNameNumberAndType(const map::MapParking& parking)
{
  return atools::strJoin({parkingNameOrNumber(parking), parkingTypeName(parking.type)}, QObject::tr(", "));
}

QString parkingNameOrNumber(const MapParking& parking)
{
  if(parking.number != -1)
    return map::parkingName(parking.name) % " " % QLocale().toString(parking.number) % parking.suffix;
  else
    return map::parkingName(parking.name);
}

QString startType(const map::MapStart& start)
{
  if(start.isRunway())
    return QObject::tr("Runway");
  else if(start.isWater())
    return QObject::tr("Water");
  else if(start.isHelipad())
    return QObject::tr("Helipad");
  else
    return QString();
}

QString parkingNameForFlightplan(const map::MapParking& parking)
{
  if(parking.number == -1)
    // Free name - X-Plane
    return parking.name;
  else
    // FSX/P3D type
    return parkingNameMapUntranslated.value(parking.name).toUpper() % " " % QString::number(parking.number) % parking.suffix;
}

const QString& MapAirport::displayIdent(bool useIata) const
{
  if(xplane)
  {
    // ICAO is mostly identical to ident except for small fields
    if(!icao.isEmpty())
      return icao;

    if(!faa.isEmpty())
      return faa;

    // Use IATA only if present
    if(useIata && !iata.isEmpty())
      return iata;

    if(!local.isEmpty())
      return local;
  }

  // Otherwise internal id
  return ident;
}

bool MapAirport::closed() const
{
  return flags.testFlag(AP_CLOSED);
}

bool MapAirport::military() const
{
  return flags.testFlag(AP_MIL);
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

bool MapAirport::closedRunways() const
{
  return flags.testFlag(AP_RW_CLOSED);
}

bool MapAirport::emptyDraw() const
{
  if(NavApp::isNavdataAll())
    return false;

  const OptionData& od = OptionData::instance();
  return emptyDraw(od.getFlags().testFlag(opts::MAP_EMPTY_AIRPORTS), od.getFlags2().testFlag(opts2::MAP_EMPTY_AIRPORTS_3D));
}

bool MapAirport::emptyDraw(bool emptyOptsFlag, bool emptyOpts3dFlag) const
{
  if(NavApp::isNavdataAll())
    return false;

  if(emptyOptsFlag)
  {
    if(emptyOpts3dFlag && xplane)
      return !is3d() && !addon() && !waterOnly();
    else
      return rating == 0 && !waterOnly();
  }
  else
    return false;
}

int MapAirport::paintPriority(bool forceAddonFlag, bool emptyOptsFlag, bool empty3dOptsFlag) const
{
  if(forceAddonFlag && addon())
    // Force add on to top of all if configured. Has to be > longest runway length
    return 40000;

  if(emptyDraw(emptyOptsFlag, empty3dOptsFlag))
    // Put featureless airports on bottom if configured
    return 0;

  if(waterOnly())
    return 1;

  if(helipadOnly())
    return 2;

  if(softOnly())
    return 3;

  // Define higher airports with hard runways by runway length
  return longestRunwayLength;
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

bool MapAirport::isMinor() const
{
  return softOnly() || helipadOnly() || waterOnly() || closed();
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
  return !flags.testFlag(AP_HARD) && !flags.testFlag(AP_SOFT) && !flags.testFlag(AP_WATER) && flags.testFlag(AP_HELIPAD);
}

bool MapAirport::noRunways() const
{
  return !flags.testFlag(AP_HARD) && !flags.testFlag(AP_SOFT) && !flags.testFlag(AP_WATER);
}

bool MapAirport::isVisible(map::MapTypes types, int minRunwayFt, const MapLayer *layer) const
{
  // Show addon independent of layer if flag is set
  bool overrideAddon = addon() && types.testFlag(map::AIRPORT_ADDON);

  if(!overrideAddon)
    // Use max of layer and GUI min runway length if not overrideing addon
    // otherwise use only GUI limit to force addons
    minRunwayFt = std::max(minRunwayFt, layer->getMinRunwayLength());

  if(minRunwayFt > 0 && longestRunwayLength < minRunwayFt)
    return false;

  if(emptyDraw() && !types.testFlag(map::AIRPORT_EMPTY))
    return false;

  if(hard() && !types.testFlag(map::AIRPORT_HARD))
    return false;

  // Check user settings in types
  if(softOnly() && !types.testFlag(map::AIRPORT_SOFT))
    return false;

  // Check layer minor/major airport flag but not if overriding addon
  if(!overrideAddon && !layer->isAirportMinor() && isMinor())
    return false;

  if(waterOnly() && !types.testFlag(map::AIRPORT_WATER))
    return false;

  if(helipadOnly() && !types.testFlag(map::AIRPORT_HELIPAD))
    return false;

  if(!lighted() && !types.testFlag(map::AIRPORT_UNLIGHTED))
    return false;

  if(!procedure() && !types.testFlag(map::AIRPORT_NO_PROCS))
    return false;

  if(closed() && !types.testFlag(map::AIRPORT_CLOSED))
    return false;

  if(military() && !types.testFlag(map::AIRPORT_MILITARY))
    return false;

  return true;
}

/* Convert nav_search type */
map::MapTypes navTypeToMapType(const QString& navType)
{
  map::MapTypes type = NONE;
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
  dataStream >> obj.text >> obj.ranges >> obj.position >> obj.color;
  obj.objType = map::MARK_RANGE;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj)
{
  dataStream << obj.text << obj.ranges << obj.position << obj.color;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj)
{
  dataStream >> obj.text >> obj.color >> obj.from >> obj.to >> obj.magvar >> obj.flags;
  obj.objType = map::MARK_DISTANCE;
  obj.position = obj.to;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj)
{
  dataStream << obj.text << obj.color << obj.from << obj.to << obj.magvar << obj.flags;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, PatternMarker& obj)
{
  dataStream >> obj.airportIcao >> obj.runwayName >> obj.color >> obj.turnRight >> obj.base45Degree >> obj.showEntryExit
  >> obj.runwayLength >> obj.downwindParallelDistance >> obj.finalDistance >> obj.departureDistance
  >> obj.courseTrue >> obj.magvar >> obj.position;
  obj.objType = map::MARK_PATTERNS;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const PatternMarker& obj)
{
  dataStream << obj.airportIcao << obj.runwayName << obj.color << obj.turnRight << obj.base45Degree << obj.showEntryExit
             << obj.runwayLength << obj.downwindParallelDistance << obj.finalDistance << obj.departureDistance
             << obj.courseTrue << obj.magvar << obj.position;

  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, HoldingMarker& obj)
{
  dataStream >> obj.holding.navIdent >> obj.holding.navType >> obj.holding.vorDmeOnly >> obj.holding.vorHasDme
  >> obj.holding.vorTacan >> obj.holding.vorVortac >> obj.holding.color >> obj.holding.turnLeft >> obj.holding.time
  >> obj.holding.speedKts >> obj.holding.courseTrue >> obj.holding.magvar >> obj.holding.position;

  obj.holding.length = obj.holding.speedLimit = obj.holding.minAltititude = obj.holding.maxAltititude = 0.f;
  obj.holding.airportIdent.clear();

  obj.position = obj.holding.position;
  obj.objType = map::MARK_HOLDING;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const HoldingMarker& obj)
{
  dataStream << obj.holding.navIdent << obj.holding.navType << obj.holding.vorDmeOnly << obj.holding.vorHasDme
             << obj.holding.vorTacan << obj.holding.vorVortac << obj.holding.color << obj.holding.turnLeft
             << obj.holding.time << obj.holding.speedKts << obj.holding.courseTrue << obj.holding.magvar << obj.holding.position;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::MsaMarker& obj)
{
  dataStream >> obj.msa.airportIdent >> obj.msa.navIdent >> obj.msa.region >> obj.msa.multipleCode >> obj.msa.vorType;
  dataStream >> obj.msa.navType >> obj.msa.vorDmeOnly >> obj.msa.vorHasDme >> obj.msa.vorTacan >> obj.msa.vorVortac;
  dataStream >> obj.msa.radius >> obj.msa.magvar;
  dataStream >> obj.msa.bearings >> obj.msa.altitudes >> obj.msa.trueBearing
  >> obj.msa.geometry >> obj.msa.labelPositions >> obj.msa.bearingEndPositions >> obj.msa.bounding >> obj.msa.position;

  obj.position = obj.msa.position;
  obj.objType = map::MARK_MSA;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::MsaMarker& obj)
{
  dataStream << obj.msa.airportIdent << obj.msa.navIdent << obj.msa.region << obj.msa.multipleCode << obj.msa.vorType;
  dataStream << obj.msa.navType << obj.msa.vorDmeOnly << obj.msa.vorHasDme << obj.msa.vorTacan << obj.msa.vorVortac;
  dataStream << obj.msa.radius << obj.msa.magvar;
  dataStream << obj.msa.bearings << obj.msa.altitudes << obj.msa.trueBearing
             << obj.msa.geometry << obj.msa.labelPositions << obj.msa.bearingEndPositions << obj.msa.bounding << obj.msa.position;
  return dataStream;
}

QString vorType(const MapVor& vor)
{
  if(vor.isValid())
    return vorType(vor.dmeOnly, vor.hasDme, vor.tacan, vor.vortac);
  else
    return QString();
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

QString vorFullShortText(const MapVor& vor)
{
  return vorFullShortText(vor.type, vor.dmeOnly, vor.hasDme, vor.tacan, vor.vortac);
}

QString vorFullShortText(const QString& vorType, bool dmeOnly, bool hasDme, bool tacan, bool vortac)
{
  if(tacan)
    return QObject::tr("TACAN");
  else if(vorType.isEmpty())
  {
    if(vortac)
      return QObject::tr("VORTAC");
    else if(dmeOnly)
      return QObject::tr("DME");
    else if(hasDme)
      return QObject::tr("VORDME");
    else
      return QObject::tr("VOR");
  }
  else
  {
    QString type = vorType.startsWith("VT") ? vorType.at(vorType.size() - 1) : vorType.at(0);

    if(vortac)
      return QObject::tr("VORTAC (%1)").arg(type);
    else if(dmeOnly)
      return QObject::tr("DME (%1)").arg(type);
    else if(hasDme)
      return QObject::tr("VORDME (%1)").arg(type);
    else
      return QObject::tr("VOR (%1)").arg(type);
  }
}

QString vorText(const MapVor& vor, int elideName)
{
  return QObject::tr("%1 %2 (%3)").
         arg(vorType(vor)).arg(atools::elideTextShort(atools::capString(vor.name), elideName)).arg(vor.ident);
}

QString vorTextShort(const MapVor& vor)
{
  return QObject::tr("%1 (%2)").arg(atools::capString(vor.name)).arg(vor.ident);
}

QString ndbText(const MapNdb& ndb, int elideName)
{
  return QObject::tr("NDB %1 (%2)").arg(atools::elideTextShort(atools::capString(ndb.name), elideName)).arg(ndb.ident);
}

QString ndbTextShort(const MapNdb& ndb)
{
  return QObject::tr("%1 (%2)").arg(atools::capString(ndb.name)).arg(ndb.ident);
}

QString waypointText(const MapWaypoint& waypoint)
{
  return QObject::tr("Waypoint %1").arg(waypoint.ident);
}

QString userpointText(const MapUserpoint& userpoint, int elideName)
{
  if(userpoint.ident.isEmpty() && userpoint.name.isEmpty())
    return QObject::tr("Userpoint %1").arg(atools::elideTextShort(userpoint.type, elideName));
  else
    return QObject::tr("Userpoint %1").arg(atools::elideTextShort(userpoint.ident.isEmpty() ? userpoint.name : userpoint.ident, elideName));
}

QString userpointShortText(const MapUserpoint& userpoint, int elideName)
{
  if(userpoint.ident.isEmpty() && userpoint.name.isEmpty())
    return atools::elideTextShort(userpoint.type, elideName);
  else
    return atools::elideTextShort(userpoint.ident.isEmpty() ? userpoint.name : userpoint.ident, elideName);
}

QString logEntryText(const MapLogbookEntry& logEntry)
{
  return QObject::tr("Logbook Entry %1 to %2").arg(logEntry.departureIdent).arg(logEntry.destinationIdent);
}

QString rangeMarkText(const RangeMarker& obj)
{
  if(obj.text.isEmpty())
    return QObject::tr("Range Rings");
  else
    return QObject::tr("Range Rings %1").arg(obj.text);
}

QString distanceMarkText(const DistanceMarker& obj)
{
  float distanceMeter = obj.getDistanceMeter();
  QString distStr(distanceMeter < map::INVALID_DISTANCE_VALUE ? Unit::distMeter(distanceMeter) : QString());

  if(obj.text.isEmpty())
    return QObject::tr("Measurement %1").arg(distStr);
  else
    return QObject::tr("Measurement %1 %2").arg(obj.text).arg(distStr);
}

QString holdingMarkText(const HoldingMarker& obj)
{
  if(obj.holding.navIdent.isEmpty())
    return QObject::tr("User Holding %1 %2").
           arg(obj.holding.turnLeft ? QObject::tr("L") : QObject::tr("R")).
           arg(Unit::altFeet(obj.holding.position.getAltitude()));
  else
    return QObject::tr("User Holding %1 %2 %3").
           arg(obj.holding.navIdent).
           arg(obj.holding.turnLeft ? QObject::tr("L") : QObject::tr("R")).
           arg(Unit::altFeet(obj.holding.position.getAltitude()));
}

QString patternMarkText(const PatternMarker& obj)
{
  if(obj.airportIcao.isEmpty())
    return QObject::tr("Traffic Pattern %1 RW %2").arg(obj.turnRight ? QObject::tr("R") : QObject::tr("L")).arg(obj.runwayName);
  else
    return QObject::tr("Traffic Pattern %1 %2 RW %3").
           arg(obj.airportIcao).arg(obj.turnRight ? QObject::tr("R") : QObject::tr("L")).arg(obj.runwayName);
}

QString msaMarkText(const MsaMarker& obj)
{
  return airportMsaText(obj.msa, true);
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
    if(airway.maxAltitude > 0 && airway.maxAltitude < map::MapAirway::MAX_ALTITUDE_LIMIT_FT)
      altTxt = Unit::altFeet(airway.minAltitude);
    else
      altTxt = QObject::tr("Min ") % Unit::altFeet(airway.minAltitude);
  }

  if(airway.maxAltitude > 0 && airway.maxAltitude < map::MapAirway::MAX_ALTITUDE_LIMIT_FT)
  {
    if(airway.minAltitude > 0)
      altTxt += QObject::tr(" to ") % Unit::altFeet(airway.maxAltitude);
    else
      altTxt += QObject::tr("Max ") % Unit::altFeet(airway.maxAltitude);
  }
  return altTxt;
}

QString airwayAltTextShort(const MapAirway& airway, bool addUnit, bool narrow)
{
  if(airway.maxAltitude > 0 && airway.maxAltitude < map::MapAirway::MAX_ALTITUDE_LIMIT_FT)
    return QObject::tr("%1-%2").
           arg(Unit::altFeet(airway.minAltitude, false /*addUnit*/, narrow)).
           arg(Unit::altFeet(airway.maxAltitude, addUnit, narrow));
  else if(airway.minAltitude > 0)
    return Unit::altFeet(airway.minAltitude, addUnit, narrow);
  else
    return QString();
}

QString airportText(const MapAirport& airport, int elideName, bool includeIdent)
{
  if(!airport.isValid())
    return QObject::tr("Airport");
  else
    return QObject::tr("Airport %1").arg(airportTextShort(airport, elideName), includeIdent);
}

QString airportTextShort(const MapAirport& airport, int elideName, bool includeIdent)
{
  QString displayIdent = airport.displayIdent();

  if(includeIdent && airport.ident != displayIdent)
    // Show internal ident first if it differs from display ident (airport search context menu)
    displayIdent = QObject::tr("%1, %2").arg(airport.ident).arg(displayIdent);

  if(!airport.isValid())
    return QObject::tr("Airport");
  else if(airport.name.isEmpty())
    return displayIdent;
  else
    return QObject::tr("%1 (%2)").arg(atools::elideTextShort(airport.name, elideName)).arg(displayIdent);
}

QString airportMsaTextShort(const MapAirportMsa& airportMsa)
{
  if(!airportMsa.isValid())
    return QObject::tr("MSA");
  else if(airportMsa.airportIdent == airportMsa.navIdent)
    return airportMsa.airportIdent;
  else
    return QObject::tr("%1 (%2)").arg(airportMsa.airportIdent).arg(airportMsa.navIdent);
}

QString airportMsaText(const MapAirportMsa& airportMsa, bool user)
{
  QString type = user ? QObject::tr("User MSA") : QObject::tr("MSA");
  if(!airportMsa.isValid())
    return type;
  else if(airportMsa.airportIdent == airportMsa.navIdent)
    return QObject::tr("%1 at %2").arg(type).arg(airportMsa.airportIdent);
  else
    return QObject::tr("%1 at %2 (%3)").arg(type).arg(airportMsa.airportIdent).arg(airportMsa.navIdent);
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
    if(num.endsWith(pt % "0"))
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

QString ndbFullShortText(const MapNdb& ndb)
{
  // Compass point vs. compass locator
  QString type = ndb.type == "CP" ? QObject::tr("CL") : ndb.type;
  return type.isEmpty() ? QObject::tr("NDB") : QObject::tr("NDB (%1)").arg(type);
}

const QString& airspaceTypeToString(map::MapAirspaceTypes type)
{
  return airspaceTypeNameMap[type];
}

const QString& airspaceTypeShortToString(map::MapAirspaceTypes type)
{
  return airspaceTypeShortNameMap[type];
}

const QString& airspaceFlagToString(map::MapAirspaceFlags type)
{
  return airspaceFlagNameMap[type];
}

const QString& airspaceFlagToStringLong(map::MapAirspaceFlags type)
{
  return airspaceFlagNameMapLong[type];
}

QString mapTypeToString(MapTypes type)
{
  if(type == NONE)
    return QObject::tr("None");
  else
  {
    QStringList str;

    if(type.testFlag(map::AIRCRAFT))
      str.append("AIRCRAFT");
    if(type.testFlag(map::AIRCRAFT_AI))
      str.append("AIRCRAFT_AI");
    if(type.testFlag(map::AIRCRAFT_AI_SHIP))
      str.append("AIRCRAFT_AI_SHIP");
    if(type.testFlag(map::AIRCRAFT_ONLINE))
      str.append("AIRCRAFT_ONLINE");
    if(type.testFlag(map::AIRPORT))
      str.append("AIRPORT");
    if(type.testFlag(map::AIRPORT_ADDON))
      str.append("AIRPORT_ADDON");
    if(type.testFlag(map::AIRPORT_EMPTY))
      str.append("AIRPORT_EMPTY");
    if(type.testFlag(map::AIRPORT_HARD))
      str.append("AIRPORT_HARD");
    if(type.testFlag(map::AIRPORT_HELIPAD))
      str.append("AIRPORT_HELIPAD");
    if(type.testFlag(map::AIRPORT_MSA))
      str.append("AIRPORT_MSA");
    if(type.testFlag(map::AIRPORT_NO_PROCS))
      str.append("AIRPORT_NO_PROCS");
    if(type.testFlag(map::AIRPORT_SOFT))
      str.append("AIRPORT_SOFT");
    if(type.testFlag(map::AIRPORT_UNLIGHTED))
      str.append("AIRPORT_UNLIGHTED");
    if(type.testFlag(map::AIRPORT_WATER))
      str.append("AIRPORT_WATER");
    if(type.testFlag(map::AIRSPACE))
      str.append("AIRSPACE");
    if(type.testFlag(map::AIRWAY))
      str.append("AIRWAY");
    if(type.testFlag(map::AIRWAYJ))
      str.append("AIRWAYJ");
    if(type.testFlag(map::AIRWAYV))
      str.append("AIRWAYV");
    if(type.testFlag(map::HELIPAD))
      str.append("HELIPAD");
    if(type.testFlag(map::HOLDING))
      str.append("HOLDING");
    if(type.testFlag(map::ILS))
      str.append("ILS");
    if(type.testFlag(map::INVALID))
      str.append("INVALID");
    if(type.testFlag(map::LOGBOOK))
      str.append("LOGBOOK");
    if(type.testFlag(map::MARKER))
      str.append("MARKER");
    if(type.testFlag(map::MARK_DISTANCE))
      str.append("MARK_DISTANCE");
    if(type.testFlag(map::MARK_HOLDING))
      str.append("MARK_HOLDING");
    if(type.testFlag(map::MARK_MSA))
      str.append("MARK_MSA");
    if(type.testFlag(map::MARK_PATTERNS))
      str.append("MARK_PATTERNS");
    if(type.testFlag(map::MARK_RANGE))
      str.append("MARK_RANGE");
    if(type.testFlag(map::MISSED_APPROACH))
      str.append("MISSED_APPROACH");
    if(type.testFlag(map::NDB))
      str.append("NDB");
    if(type.testFlag(map::PARKING))
      str.append("PARKING");
    if(type.testFlag(map::PROCEDURE))
      str.append("PROCEDURE");
    if(type.testFlag(map::PROCEDURE_POINT))
      str.append("PROCEDURE_POINT");
    if(type.testFlag(map::RUNWAYEND))
      str.append("RUNWAYEND");
    if(type.testFlag(map::RUNWAY))
      str.append("RUNWAY");
    if(type.testFlag(map::TRACK))
      str.append("TRACK");
    if(type.testFlag(map::USERPOINT))
      str.append("USERPOINT");
    if(type.testFlag(map::USERPOINTROUTE))
      str.append("USERPOINTROUTE");
    if(type.testFlag(map::USER_FEATURE))
      str.append("USER_FEATURE");
    if(type.testFlag(map::VOR))
      str.append("VOR");
    if(type.testFlag(map::WAYPOINT))
      str.append("WAYPOINT");

    return str.join(",");
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
    if(src.testFlag(AIRSPACE_SRC_SIM))
      retval.append(QObject::tr("Simulator"));

    if(src.testFlag(AIRSPACE_SRC_NAV))
      retval.append(QObject::tr("Navigraph"));

    if(src.testFlag(AIRSPACE_SRC_ONLINE))
      retval.append(QObject::tr("Online"));

    if(src.testFlag(AIRSPACE_SRC_USER))
      retval.append(QObject::tr("User"));
  }

  return retval.join(QObject::tr(", "));
}

QDebug operator<<(QDebug out, const MapBase& obj)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapBase["
                          << "id " << obj.id
                          << ", type " << mapTypeToString(obj.objType)
                          << ", " << obj.position
                          << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapRef& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapObjectRef["
                          << "id " << ref.id
                          << ", type " << mapTypeToString(ref.objType)
                          << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapRefExt& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapObjectRefExt["
                          << "id " << ref.id
                          << ", type " << mapTypeToString(ref.objType);

  if(!ref.name.isEmpty())
    out.noquote().nospace() << ", name " << ref.name;

  if(ref.position.isValid())
    out.noquote().nospace() << ", " << ref.position;

  out.noquote().nospace() << "]";
  return out;
}

QString ilsText(const MapIls& ils)
{
  QStringList texts;
  texts.append(ilsType(ils, false /* gs */, false /* dme */, QObject::tr(" / ")));
  texts.append(ils.ident);

  if(!ils.isAnyGlsRnp()) // Channel is not relevant on map display
    texts.append(ils.freqMHz());

  texts.append(QObject::tr("%1°M").arg(QString::number(atools::geo::normalizeCourse(ils.heading - ils.magvar), 'f', 0)));

  if(ils.hasGlideslope())
    texts.append((ils.isAnyGlsRnp() ? QObject::tr("GP %1°") : QObject::tr("GS %1°")).arg(QString::number(ils.slope, 'f', 1)));

  if(ils.hasDme)
    texts.append(QObject::tr("DME"));

  return texts.join(QObject::tr(" / "));
}

QString ilsTypeShort(const map::MapIls& ils)
{
  QString text;

  if(ils.isGls())
    text = QObject::tr("GLS");
  else if(ils.isRnp())
    text = QObject::tr("RNP");
  else if(ils.isIls())
    text = QObject::tr("ILS");
  else if(ils.isLoc())
    text = QObject::tr("LOC");
  else if(ils.isIgs())
    text = QObject::tr("IGS");
  else if(ils.isLda())
    text = QObject::tr("LDA");
  else if(ils.isSdf())
    text = QObject::tr("SDF");
  else
    text = ils.name;
  return text;
}

QString ilsType(const map::MapIls& ils, bool gs, bool dme, const QString& separator)
{
  QString text = ilsTypeShort(ils);

  if(!ils.isAnyGlsRnp())
  {
    if(ils.type == '1')
      text += QObject::tr(" CAT I");
    else if(ils.type == '2')
      text += QObject::tr(" CAT II");
    else if(ils.type == '3')
      text += QObject::tr(" CAT III");

    if(gs && ils.hasGlideslope())
      text += separator % QObject::tr("GS");
    if(dme && ils.hasDme)
      text += separator % QObject::tr("DME");
  }
  else
  {
    if(!ils.perfIndicator.isEmpty())
      text += separator % ils.perfIndicator;

    // Ignore EGNOS, WAAS display
    // if(!ils.provider.isEmpty())
    // text += separator % ils.provider;
  }

  return text;
}

QString ilsTextShort(const map::MapIls& ils)
{
  return QObject::tr("%1 %2").arg(ilsType(ils, true /* gs */, true /* dme */, QObject::tr(", "))).arg(ils.ident);
}

QString holdingTextShort(const map::MapHolding& holding, bool user)
{
  QString text;
  if(user)
    text.append(QObject::tr("User holding at %1").
                arg(holding.navIdent.isEmpty() ? Unit::coords(holding.position) : holding.navIdent));
  else
    text.append(QObject::tr("Holding at %1").
                arg(holding.navIdent.isEmpty() ? Unit::coords(holding.position) : holding.navIdent));

  if(holding.time > 0.f)
    text.append(QObject::tr(", %1 min").arg(QLocale().toString(holding.time)));

  if(holding.length > 0.f)
    text.append(QObject::tr(", %1:").arg(Unit::distNm(holding.length)));

  if(holding.speedKts > 0.f)
    text.append(QObject::tr(", %1:").arg(Unit::speedKts(holding.speedKts)));

  if(holding.speedLimit > 0.f)
    text.append(QObject::tr(", max %1").arg(Unit::speedKts(holding.speedLimit)));

  if(holding.minAltititude > 0.f)
    text.append(QObject::tr(", A%1").arg(Unit::altFeet(holding.minAltititude)));

  return text;
}

float MapHolding::magCourse() const
{
  return atools::geo::normalizeCourse(courseTrue - magvar);
}

float MapHolding::distance(bool *estimated) const
{
  bool est = true;
  float dist = 5.f;

  if(length > 0.f)
  {
    est = false;
    dist = length;
  }
  else if(time > 0.f)
  {
    if(speedLimit > 0.f)
    {
      est = true;
      dist = speedLimit * time / 60.f;
    }
    else if(speedKts > 0.f)
    {
      est = false;
      dist = speedKts * time / 60.f;
    }
    else
    {
      est = true;
      dist = 200.f * time / 60.f;
    }
  }

  if(estimated != nullptr)
    *estimated = est;

  return dist;
}

float PatternMarker::magCourse() const
{
  return atools::geo::normalizeCourse(courseTrue - magvar);
}

atools::geo::Line MapIls::centerLine() const
{
  return atools::geo::Line(position, posmid);
}

QString MapIls::freqMHzOrChannel() const
{
  if(type == GLS_GROUND_STATION || type == SBAS_GBAS_THRESHOLD)
    return QObject::tr("%1").arg(frequency);
  else
    return QObject::tr("%1").arg(static_cast<float>(frequency / 1000.f), 0, 'f', 2);
}

QString MapIls::freqMHzOrChannelLocale() const
{
  if(type == GLS_GROUND_STATION || type == SBAS_GBAS_THRESHOLD)
    return QObject::tr("%L1").arg(frequency);
  else
    return QObject::tr("%L1").arg(static_cast<float>(frequency / 1000.f), 0, 'f', 2);
}

QString MapIls::freqMHz() const
{
  if(type == GLS_GROUND_STATION || type == SBAS_GBAS_THRESHOLD)
    return QString();
  else
    return QObject::tr("%1").arg(static_cast<float>(frequency / 1000.f), 0, 'f', 2);
}

QString MapIls::freqMHzLocale() const
{
  if(type == GLS_GROUND_STATION || type == SBAS_GBAS_THRESHOLD)
    return QString();
  else
    return QObject::tr("%L1").arg(static_cast<float>(frequency / 1000.f), 0, 'f', 2);
}

atools::geo::LineString MapIls::boundary() const
{
  if(hasGeometry)
  {
    if(hasGlideslope())
      return atools::geo::LineString({position, pos1, pos2, position});
    else
      return atools::geo::LineString({position, pos1, posmid, pos2, position});
  }
  else
    return atools::geo::EMPTY_LINESTRING;
}

QString airspaceName(const MapAirspace& airspace)
{
  return airspace.isOnline() ? airspace.name : formatter::capNavString(airspace.name);
}

QString airspaceRestrictiveNameMap(const MapAirspace& airspace)
{
  QString restrictedName;
  if(!airspace.restrictiveDesignation.isEmpty())
  {
    QString name = formatter::capNavString(airspace.restrictiveDesignation).trimmed();

    if(name.endsWith('*'))
      name = name.remove('*').trimmed();

    restrictedName = airspace.restrictiveType % QObject::tr("-") % name;
  }
  return restrictedName;
}

QStringList airspaceNameMap(const MapAirspace& airspace, int maxTextLength, bool name, bool restrictiveName, bool type, bool altitude,
                            bool com)
{
  QStringList texts;

  QString restrNameStr = airspaceRestrictiveNameMap(airspace);
  if(type)
  {
    // Type only if it not a part of the restrictive name
    const QString& typeStr = airspaceTypeShortToString(airspace.type);
    if(!((name && airspace.name.startsWith(typeStr % '-')) || (restrictiveName && restrNameStr.startsWith(typeStr % '-'))))
      texts.append(typeStr);
  }

  // Name if requested but always for FIR and UIR spaces
  if(name || airspace.type == map::FIR || airspace.type == map::UIR)
    texts.append(atools::elideTextShort(airspace.isOnline() ? airspace.name : formatter::capNavString(airspace.name), maxTextLength));

  if(restrictiveName)
    texts.append(restrNameStr);

  if(altitude && !airspace.isOnline())
  {
    QString altTextLower, altTextUpper;

    if(!airspace.maxAltitudeType.isEmpty() && airspace.maxAltitudeType != "UL" && airspace.maxAltitude < 60000)
      altTextUpper = Unit::altFeet(airspace.maxAltitude, false /* addUnit */, true /* narrow */) %
                     QObject::tr(" ") % airspace.maxAltitudeType;

    if(!airspace.minAltitudeType.isEmpty() && airspace.minAltitude > 0)
      altTextLower = Unit::altFeet(airspace.minAltitude, false /* addUnit */, true /* narrow */);

    if(airspace.maxAltitudeType != airspace.minAltitudeType && !altTextLower.isEmpty() && !altTextUpper.isEmpty())
      altTextLower = QObject::tr(" ") % airspace.minAltitudeType;

    if(!airspace.minAltitudeType.isEmpty() && airspace.minAltitude == 0 && !altTextUpper.isEmpty())
      altTextLower = QObject::tr("0");

    if(!altTextLower.isEmpty() || !altTextUpper.isEmpty())
      texts.append(altTextLower % QObject::tr("-") % altTextUpper);
  }

  if(com)
  {
    QStringList freqTxt;
    for(int freq : airspace.comFrequencies)
    {
      if(airspace.isOnline())
        // Use online freqencies as is - convert kHz to MHz
        freqTxt.append(QString::number(freq / 1000.f, 'f', 3));
      else
        freqTxt.append(QString::number(atools::fs::util::roundComFrequency(freq), 'f', 3));
    }
    texts.append(atools::strJoin(freqTxt, QObject::tr(" / ")));
  }

  texts.removeDuplicates();
  return texts;
}

QString airspaceText(const MapAirspace& airspace)
{
  return QObject::tr("Airspace %1 (%2)").arg(airspaceName(airspace)).arg(airspaceTypeToString(airspace.type));
}

QString aircraftType(const atools::fs::sc::SimConnectAircraft& aircraft)
{
  if(!aircraft.getAirplaneType().isEmpty())
    return aircraft.getAirplaneType();
  else
    // Convert model ICAO code to a name
    return atools::fs::util::aircraftTypeForCode(aircraft.getAirplaneModel());
}

QString aircraftTypeString(const atools::fs::sc::SimConnectAircraft& aircraft)
{
  QString type(QObject::tr(" Vehicle"));
  switch(aircraft.getCategory())
  {
    case atools::fs::sc::BOAT:
      type = QObject::tr(" Ship");
      break;
    case atools::fs::sc::CARRIER:
      type = QObject::tr(" Carrier");
      break;
    case atools::fs::sc::FRIGATE:
      type = QObject::tr(" Frigate");
      break;
    case atools::fs::sc::AIRPLANE:
      type = QObject::tr(" Aircraft");
      break;
    case atools::fs::sc::HELICOPTER:
      type = QObject::tr(" Helicopter");
      break;
    case atools::fs::sc::UNKNOWN:
    case atools::fs::sc::GROUNDVEHICLE:
    case atools::fs::sc::CONTROLTOWER:
    case atools::fs::sc::SIMPLEOBJECT:
    case atools::fs::sc::VIEWER:
      break;
  }
  return type;
}

QString aircraftTextShort(const atools::fs::sc::SimConnectAircraft& aircraft)
{
  QStringList text;
  QString typeName = aircraftTypeString(aircraft);

  if(aircraft.isUser())
    text.append(QObject::tr("User %1").arg(typeName));
  else if(aircraft.isOnline())
    text.append(QObject::tr("Online Client"));
  else
    text.append(QObject::tr("AI / Multiplayer %1").arg(typeName));

  text.append(aircraft.getAirplaneRegistration());
  text.append(aircraft.getAirplaneModel());

  return atools::strJoin(text, QObject::tr(", "));
}

QString helipadText(const MapHelipad& helipad)
{
  return QObject::tr("Helipad %1").arg(helipad.runwayName);
}

int routeIndex(const map::MapBase *base)
{
  if(base != nullptr)
  {
    map::MapTypes type = base->getType();
    if(type == map::AIRPORT)
      return base->asPtr<map::MapAirport>()->routeIndex;
    else if(type == map::VOR)
      return base->asPtr<map::MapVor>()->routeIndex;
    else if(type == map::NDB)
      return base->asPtr<map::MapNdb>()->routeIndex;
    else if(type == map::WAYPOINT)
      return base->asPtr<map::MapWaypoint>()->routeIndex;
    else if(type == map::USERPOINTROUTE)
      return base->asPtr<map::MapUserpointRoute>()->routeIndex;
    else if(type == map::PROCEDURE_POINT)
      return base->asPtr<map::MapProcedurePoint>()->routeIndex;
  }
  return -1;
}

bool isAircraftShadow(const map::MapBase *base)
{
  if(base != nullptr)
  {
    map::MapTypes type = base->getType();
    if(type == map::AIRCRAFT)
      return base->asPtr<map::MapUserAircraft>()->getAircraft().isOnlineShadow();
    else if(type == map::AIRCRAFT_AI)
      return base->asPtr<map::MapAiAircraft>()->getAircraft().isOnlineShadow();
    else if(type == map::AIRCRAFT_ONLINE)
      return base->asPtr<map::MapOnlineAircraft>()->getAircraft().isOnlineShadow();
  }
  return false;
}

map::MapAirspaceSources airspaceSource(const map::MapBase *base)
{
  if(base != nullptr)
  {
    map::MapTypes type = base->getType();
    if(type == map::AIRSPACE)
      return base->asPtr<map::MapAirspace>()->src;
  }
  return map::MapAirspaceSource::AIRSPACE_SRC_NONE;
}

int getNextUserFeatureId()
{
  static int currentUserFeatureId = 1;
  return currentUserFeatureId++;

}

QString mapBaseText(const map::MapBase *base, int elideAirportName)
{
  if(base != nullptr)
  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    switch(base->getTypeEnum())
#pragma GCC diagnostic pop
    {
      case map::AIRPORT:
        return map::airportText(*base->asPtr<map::MapAirport>(), elideAirportName);

      case map::AIRPORT_MSA:
        return map::airportMsaText(*base->asPtr<map::MapAirportMsa>(), false);

      case map::VOR:
        return map::vorText(*base->asPtr<map::MapVor>());

      case map::NDB:
        return map::ndbText(*base->asPtr<map::MapNdb>());

      case map::ILS:
        return map::ilsTextShort(*base->asPtr<map::MapIls>());

      case map::HOLDING:
        return map::holdingTextShort(*base->asPtr<map::MapHolding>(), false);

      case map::WAYPOINT:
        return map::waypointText(*base->asPtr<map::MapWaypoint>());

      case map::AIRWAY:
        return map::airwayText(*base->asPtr<map::MapAirway>());

      case map::AIRCRAFT:
        return map::aircraftTextShort(base->asPtr<map::MapUserAircraft>()->getAircraft());

      case map::AIRCRAFT_AI:
        return map::aircraftTextShort(base->asPtr<map::MapAiAircraft>()->getAircraft());

      case map::AIRCRAFT_ONLINE:
        return map::aircraftTextShort(base->asPtr<map::MapOnlineAircraft>()->getAircraft());

      case map::USERPOINTROUTE:
        return base->asPtr<map::MapUserpointRoute>()->ident;

      case map::AIRSPACE:
        return map::airspaceText(*base->asPtr<map::MapAirspace>());

      case map::PARKING:
        return map::parkingText(*base->asPtr<map::MapParking>());

      case map::HELIPAD:
        return map::helipadText(*base->asPtr<map::MapHelipad>());

      case map::USERPOINT:
        return map::userpointText(*base->asPtr<map::MapUserpoint>());

      case map::LOGBOOK:
        return map::logEntryText(*base->asPtr<map::MapLogbookEntry>());

      case map::PROCEDURE_POINT:
        return map::procedurePointTextShort(*base->asPtr<map::MapProcedurePoint>());

      case map::MARK_RANGE:
        return map::rangeMarkText(*base->asPtr<map::RangeMarker>());

      case map::MARK_DISTANCE:
        return map::distanceMarkText(*base->asPtr<map::DistanceMarker>());

      case map::MARK_HOLDING:
        return map::holdingMarkText(*base->asPtr<map::HoldingMarker>());

      case map::MARK_PATTERNS:
        return map::patternMarkText(*base->asPtr<map::PatternMarker>());

      case map::MARK_MSA:
        return map::msaMarkText(*base->asPtr<map::MsaMarker>());
    }
  }
  return QString();
}

QIcon mapBaseIcon(const map::MapBase *base, int size)
{
  if(base != nullptr)
  {
    // Get size for icons
    VehicleIcons *vehicleIcons = NavApp::getVehicleIcons();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    switch(base->getTypeEnum())
#pragma GCC diagnostic pop
    {
      case map::AIRPORT:
        return SymbolPainter::createAirportIcon(*base->asPtr<map::MapAirport>(), size);

      case map::AIRPORT_MSA:
        return QIcon(":/littlenavmap/resources/icons/msa.svg");

      case map::VOR:
        return SymbolPainter::createVorIcon(*base->asPtr<map::MapVor>(), size);

      case map::NDB:
        return SymbolPainter::createNdbIcon(size);

      case map::ILS:
        return map::ilsIcon(base->asObj<map::MapIls>());

      case map::HOLDING:
        return QIcon(":/littlenavmap/resources/icons/enroutehold.svg");

      case map::WAYPOINT:
        return SymbolPainter::createWaypointIcon(size);

      case map::AIRWAY:
        return SymbolPainter::createAirwayIcon(*base->asPtr<map::MapAirway>(), size);

      case map::AIRCRAFT:
        return vehicleIcons->iconFromCache(base->asPtr<map::MapUserAircraft>()->getAircraft(), size, 45 /* rotate */);

      case map::AIRCRAFT_AI:
        return vehicleIcons->iconFromCache(base->asPtr<map::MapAiAircraft>()->getAircraft(), size, 45 /* rotate */);

      case map::AIRCRAFT_ONLINE:
        return vehicleIcons->iconFromCache(base->asPtr<map::MapOnlineAircraft>()->getAircraft(), size, 45 /* rotate */);

      case map::USERPOINTROUTE:
        return SymbolPainter::createUserpointIcon(size);

      case map::AIRSPACE:
        {
          const OptionData& optionData = OptionData::instance();
          return SymbolPainter::createAirspaceIcon(*base->asPtr<map::MapAirspace>(), size,
                                                   optionData.getDisplayThicknessAirspace(),
                                                   optionData.getDisplayTransparencyAirspace());
        }

      case map::PARKING:
        return mapcolors::iconForParkingType(base->asPtr<map::MapParking>()->type);

      case map::HELIPAD:
        return SymbolPainter::createHelipadIcon(*base->asPtr<map::MapHelipad>(), size);

      case map::USERPOINT:
        return QIcon(NavApp::getUserdataIcons()->getIconPath(base->asPtr<map::MapUserpoint>()->type));

      case map::LOGBOOK:
        return QIcon(":/littlenavmap/resources/icons/logbook.svg");

      case map::PROCEDURE_POINT:
        return QIcon(":/littlenavmap/resources/icons/approach.svg");

      case map::MARK_RANGE:
        return QIcon(":/littlenavmap/resources/icons/rangerings.svg");

      case map::MARK_DISTANCE:
        return QIcon(":/littlenavmap/resources/icons/distancemeasure.svg");

      case map::MARK_HOLDING:
        return QIcon(":/littlenavmap/resources/icons/hold.svg");

      case map::MARK_PATTERNS:
        return QIcon(":/littlenavmap/resources/icons/trafficpattern.svg");

      case map::MARK_MSA:
        return QIcon(":/littlenavmap/resources/icons/msa.svg");
    }
  }
  return QIcon();
}

QStringList aircraftIcing(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool narrow)
{
  QStringList text;
  if(aircraft.getPitotIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Pitot %L1") : QObject::tr("Pitot&nbsp;%L1")).arg(aircraft.getPitotIcePercent(), 0, 'f', 0));

  if(aircraft.getStructuralIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Struct %L1") : QObject::tr("Structure&nbsp;%L1")).
                arg(aircraft.getStructuralIcePercent(), 0, 'f', 0));

  if(aircraft.getAoaIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("AOA %L1") : QObject::tr("AOA&nbsp;%L1")).arg(aircraft.getAoaIcePercent(), 0, 'f', 0));

  if(aircraft.getInletIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Inlt %L1") : QObject::tr("Inlet&nbsp;%L1")).arg(aircraft.getInletIcePercent(), 0, 'f', 0));

  if(aircraft.getPropIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Prop %L1") : QObject::tr("Prop&nbsp;%L1")).arg(aircraft.getPropIcePercent(), 0, 'f', 0));

  if(aircraft.getStatIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Static %L1") : QObject::tr("Static&nbsp;%L1")).arg(aircraft.getStatIcePercent(), 0, 'f', 0));

  if(aircraft.getWindowIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Wind %L1") : QObject::tr("Window&nbsp;%L1")).arg(aircraft.getWindowIcePercent(), 0, 'f', 0));

  if(aircraft.getCarbIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Carb %L1") : QObject::tr("Carb.&nbsp;%L1")).arg(aircraft.getCarbIcePercent(), 0, 'f', 0));

  if(narrow)
  {
    // Combine two entries into one line
    QStringList text2;
    for(int i = 0; i < text.size(); i++)
    {
      if((i % 2) == 0)
        text2.append(text.at(i));
      else
        text2.last().append(QObject::tr(", ") % text.at(i));
    }
    return text2;
  }
  else
    return text;
}

bool aircraftHasIcing(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  return aircraft.getPitotIcePercent() >= 1.f ||
         aircraft.getStructuralIcePercent() >= 1.f ||
         aircraft.getAoaIcePercent() >= 1.f ||
         aircraft.getInletIcePercent() >= 1.f ||
         aircraft.getPropIcePercent() >= 1.f ||
         aircraft.getStatIcePercent() >= 1.f ||
         aircraft.getWindowIcePercent() >= 1.f ||
         aircraft.getCarbIcePercent() >= 1.f;
}

QStringList MapRunwayEnd::uniqueVasiTypeStr() const
{
  QStringList vasi({leftVasiTypeStr(), rightVasiTypeStr()});
  vasi.removeAll(QString());
  vasi.removeDuplicates();
  return vasi;
}

MapProcedurePoint::MapProcedurePoint()
  : MapBase(staticType())
{
  legs = new proc::MapProcedureLegs();
}

MapProcedurePoint::MapProcedurePoint(const proc::MapProcedureLegs& legsParam, int legIndexParam, int routeIndexParam, bool previewParam,
                                     bool previewAllParam)
  : map::MapBase(staticType())
{
  legs = new proc::MapProcedureLegs();
  *legs = legsParam;

  legIndex = legIndexParam;
  position = getLeg().line.getPos1();
  id = getLeg().legId;

  preview = previewParam;
  previewAll = previewAllParam;
  routeIndex = routeIndexParam;
}

MapProcedurePoint::~MapProcedurePoint()
{
  delete legs;
}

MapProcedurePoint::MapProcedurePoint(const MapProcedurePoint& other)
  : map::MapBase(other.objType)
{
  legs = new proc::MapProcedureLegs();
  this->operator=(other);
}

MapProcedurePoint& MapProcedurePoint::operator=(const MapProcedurePoint& other)
{
  MapBase::operator=(other);
  *legs = *other.legs;
  legIndex = other.legIndex;
  routeIndex = other.routeIndex;
  preview = other.preview;
  previewAll = other.previewAll;
  return *this;
}

std::tuple<int, int, int> MapProcedurePoint::compoundId() const
{
  return std::make_tuple(legs->ref.airportId, legs->ref.procedureId, getLeg().isAnyTransition() ? legs->ref.transitionId : -1);
}

const proc::MapProcedureLeg& MapProcedurePoint::getLeg() const
{
  return legs->at(legIndex);
}

const QString& MapProcedurePoint::getIdent() const
{
  return legs->procedureFixIdent;
}

QString procedurePointText(const MapProcedurePoint& procPoint)
{
  return proc::procedureLegsText(*procPoint.legs, procPoint.getLeg().mapType,
                                 false /* narrow */, true /* includeRunway*/, false /* missedAsApproach*/,
                                 false /* transitionAsProcedure */);
}

QString procedurePointTextShort(const MapProcedurePoint& procPoint)
{
  return proc::procedureLegsText(*procPoint.legs, procPoint.getLeg().mapType,
                                 true /* narrow */, true /* includeRunway*/, false /* missedAsApproach*/,
                                 false /* transitionAsProcedure */);
}

const QIcon& ilsIcon(const MapIls& ils)
{
  const static QIcon GLS(":/littlenavmap/resources/icons/gls.svg");
  const static QIcon ILS(":/littlenavmap/resources/icons/ils.svg");
  const static QIcon LOC(":/littlenavmap/resources/icons/loc.svg");

  if(ils.isAnyGlsRnp())
    return GLS;
  else if(ils.hasGlideslope())
    return ILS;
  else
    return LOC;
}

float DistanceMarker::getDistanceNm() const
{
  return atools::geo::meterToNm(getDistanceMeter());
}

QString airspaceRestrictiveName(const MapAirspace& airspace)
{
  if(!airspace.restrictiveDesignation.isEmpty())
  {
    QString name = formatter::capNavString(airspace.restrictiveDesignation).trimmed();

    if(name.endsWith('*'))
      name = name.remove('*').trimmed();

    QString restrictedName = airspace.restrictiveType % QObject::tr("-") % name;
    if(restrictedName != airspace.name)
      return restrictedName;
  }
  return QString();
}

} // namespace types
