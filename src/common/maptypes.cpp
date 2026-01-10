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
static QList<std::pair<QRegularExpression, QString> > parkingDatabaseKeywords;
static QHash<QString, QString> navTypeNamesVor;
static QHash<QString, QString> navTypeNamesVorLong;
static QHash<QString, QString> navTypeNamesNdb;
static QHash<QString, QString> navTypeNamesWaypoint;
static QHash<QString, QString> navTypeNames;
static QHash<QString, QString> comTypeNames;
static QHash<map::MapAirspaceType, QString> airspaceTypeNameMap;
static QHash<map::MapAirspaceType, QString> airspaceTypeShortNameMap;
static QHash<map::MapAirspaceFlag, QString> airspaceFlagNameMap;
static QHash<map::MapAirspaceFlag, QString> airspaceFlagNameMapLong;
static QHash<map::MapAirspaceType, QString> airspaceRemarkMap;

void initTranslateableTexts()
{
  surfaceMap = QHash<QString, QString>(
    {
      {QStringLiteral("C"), QObject::tr("Concrete")},
      {QStringLiteral("G"), QObject::tr("Grass")},
      {QStringLiteral("W"), QObject::tr("Water")},
      {QStringLiteral("A"), QObject::tr("Asphalt")},
      {QStringLiteral("CE"), QObject::tr("Cement")},
      {QStringLiteral("CL"), QObject::tr("Clay")},
      {QStringLiteral("SN"), QObject::tr("Snow")},
      {QStringLiteral("I"), QObject::tr("Ice")},
      {QStringLiteral("D"), QObject::tr("Dirt")},
      {QStringLiteral("CR"), QObject::tr("Coral")},
      {QStringLiteral("GR"), QObject::tr("Gravel")},
      {QStringLiteral("OT"), QObject::tr("Oil treated")},
      {QStringLiteral("SM"), QObject::tr("Steel Mats")},
      {QStringLiteral("B"), QObject::tr("Bituminous")},
      {QStringLiteral("BR"), QObject::tr("Brick")},
      {QStringLiteral("M"), QObject::tr("Macadam")},
      {QStringLiteral("PL"), QObject::tr("Planks")},
      {QStringLiteral("S"), QObject::tr("Sand")},
      {QStringLiteral("SH"), QObject::tr("Shale")},
      {QStringLiteral("T"), QObject::tr("Tarmac")},
      {QStringLiteral("TR"), QObject::tr("Transparent")},
      {QStringLiteral("UNKNOWN"), QObject::tr("Unknown")},
      {QStringLiteral("INVALID"), QObject::tr("Invalid")},
      {QString(), QObject::tr("Unknown")}
    });

  /* Short size name for gate and full name for others */
  parkingMapGate = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid")},
      {QStringLiteral("UNKNOWN"), QObject::tr("Unknown")},
      {QStringLiteral("RGA"), QObject::tr("Ramp GA")},
      {QStringLiteral("RGAS"), QObject::tr("Ramp GA Small")},
      {QStringLiteral("RGAM"), QObject::tr("Ramp GA Medium")},
      {QStringLiteral("RGAL"), QObject::tr("Ramp GA Large")},
      {QStringLiteral("RE"), QObject::tr("Ramp Extra")},
      {QStringLiteral("RC"), QObject::tr("Ramp Cargo")},
      {QStringLiteral("RM"), QObject::tr("Ramp Mil")},
      {QStringLiteral("RMC"), QObject::tr("Ramp Mil Cargo")},
      {QStringLiteral("RMCB"), QObject::tr("Ramp Mil Combat")},
      {QStringLiteral("T"), QObject::tr("Tie down")},
      {QStringLiteral("H"), QObject::tr("Hangar")},
      {QStringLiteral("G"), QString()},
      {QStringLiteral("GS"), QObject::tr("Small")},
      {QStringLiteral("GM"), QObject::tr("Medium")},
      {QStringLiteral("GH"), QObject::tr("Heavy")},
      {QStringLiteral("GE"), QObject::tr("Extra")},
      {QStringLiteral("DGA"), QObject::tr("Dock GA")},
      {QStringLiteral("FUEL"), QObject::tr("Fuel")},
      {QStringLiteral("V"), QObject::tr("Vehicles")}
    });

  /* Short size name for parking and full name for others */
  parkingMapRamp = QHash<QString, QString>(
    {
      {QStringLiteral("UNKNOWN"), QObject::tr("Unknown")},
      {QStringLiteral("RGA"), QObject::tr("Ramp GA")},
      {QStringLiteral("RGAS"), QObject::tr("Small")},
      {QStringLiteral("RGAM"), QObject::tr("Medium")},
      {QStringLiteral("RGAL"), QObject::tr("Large")},
      {QStringLiteral("RC"), QObject::tr("Ramp Cargo")},
      {QStringLiteral("RE"), QObject::tr("Ramp Extra")},
      {QStringLiteral("RM"), QObject::tr("Ramp Mil")},
      {QStringLiteral("RMC"), QObject::tr("Ramp Mil Cargo")},
      {QStringLiteral("RMCB"), QObject::tr("Ramp Mil Combat")},
      {QStringLiteral("T"), QObject::tr("Tie down")},
      {QStringLiteral("H"), QObject::tr("Hangar")},
      {QStringLiteral("G"), QObject::tr("Gate")},
      {QStringLiteral("GS"), QObject::tr("Gate Small")},
      {QStringLiteral("GM"), QObject::tr("Gate Medium")},
      {QStringLiteral("GH"), QObject::tr("Gate Heavy")},
      {QStringLiteral("GE"), QObject::tr("Gate Extra")},
      {QStringLiteral("DGA"), QObject::tr("Dock GA")},
      {QStringLiteral("FUEL"), QObject::tr("Fuel")},
      {QStringLiteral("V"), QObject::tr("Vehicles")}
    });

  /* Full name for all parking including type */
  parkingTypeMap = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid")},
      {QStringLiteral("UNKNOWN"), QObject::tr("Unknown")},
      {QStringLiteral("RGA"), QObject::tr("Ramp GA")},
      {QStringLiteral("RGAS"), QObject::tr("Ramp GA Small")},
      {QStringLiteral("RGAM"), QObject::tr("Ramp GA Medium")},
      {QStringLiteral("RGAL"), QObject::tr("Ramp GA Large")},
      {QStringLiteral("RE"), QObject::tr("Ramp GA Extra")},
      {QStringLiteral("RC"), QObject::tr("Ramp Cargo")},
      {QStringLiteral("RM"), QObject::tr("Ramp Mil")},
      {QStringLiteral("RMC"), QObject::tr("Ramp Mil Cargo")},
      {QStringLiteral("RMCB"), QObject::tr("Ramp Mil Combat")},
      {QStringLiteral("T"), QObject::tr("Tie down")},
      {QStringLiteral("H"), QObject::tr("Hangar")},
      {QStringLiteral("G"), QObject::tr("Gate")},
      {QStringLiteral("GS"), QObject::tr("Gate Small")},
      {QStringLiteral("GM"), QObject::tr("Gate Medium")},
      {QStringLiteral("GH"), QObject::tr("Gate Heavy")},
      {QStringLiteral("GE"), QObject::tr("Gate Extra")},
      {QStringLiteral("DGA"), QObject::tr("Dock GA")},
      {QStringLiteral("FUEL"), QObject::tr("Fuel")},
      {QStringLiteral("V"), QObject::tr("Vehicles")}
    });

  // Map for user interface names
  parkingNameMap = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid")},
      {QStringLiteral("UNKNOWN"), QObject::tr("Unknown")},
      {QStringLiteral("NONE"), QObject::tr("Parking")},
      {QStringLiteral("P"), QObject::tr("Parking")},
      {QStringLiteral("NP"), QObject::tr("N Parking")},
      {QStringLiteral("NEP"), QObject::tr("NE Parking")},
      {QStringLiteral("EP"), QObject::tr("E Parking")},
      {QStringLiteral("SEP"), QObject::tr("SE Parking")},
      {QStringLiteral("SP"), QObject::tr("S Parking")},
      {QStringLiteral("SWP"), QObject::tr("SW Parking")},
      {QStringLiteral("WP"), QObject::tr("W Parking")},
      {QStringLiteral("NWP"), QObject::tr("NW Parking")},
      {QStringLiteral("G"), QObject::tr("Gate")},
      {QStringLiteral("D"), QObject::tr("Dock")},
      {QStringLiteral("GA"), QObject::tr("Gate A")},
      {QStringLiteral("GB"), QObject::tr("Gate B")},
      {QStringLiteral("GC"), QObject::tr("Gate C")},
      {QStringLiteral("GD"), QObject::tr("Gate D")},
      {QStringLiteral("GE"), QObject::tr("Gate E")},
      {QStringLiteral("GF"), QObject::tr("Gate F")},
      {QStringLiteral("GG"), QObject::tr("Gate G")},
      {QStringLiteral("GH"), QObject::tr("Gate H")},
      {QStringLiteral("GI"), QObject::tr("Gate I")},
      {QStringLiteral("GJ"), QObject::tr("Gate J")},
      {QStringLiteral("GK"), QObject::tr("Gate K")},
      {QStringLiteral("GL"), QObject::tr("Gate L")},
      {QStringLiteral("GM"), QObject::tr("Gate M")},
      {QStringLiteral("GN"), QObject::tr("Gate N")},
      {QStringLiteral("GO"), QObject::tr("Gate O")},
      {QStringLiteral("GP"), QObject::tr("Gate P")},
      {QStringLiteral("GQ"), QObject::tr("Gate Q")},
      {QStringLiteral("GR"), QObject::tr("Gate R")},
      {QStringLiteral("GS"), QObject::tr("Gate S")},
      {QStringLiteral("GT"), QObject::tr("Gate T")},
      {QStringLiteral("GU"), QObject::tr("Gate U")},
      {QStringLiteral("GV"), QObject::tr("Gate V")},
      {QStringLiteral("GW"), QObject::tr("Gate W")},
      {QStringLiteral("GX"), QObject::tr("Gate X")},
      {QStringLiteral("GY"), QObject::tr("Gate Y")},
      {QStringLiteral("GZ"), QObject::tr("Gate Z")}
    });

  parkingDatabaseNameMap = QHash<QString, QString>(
    {
      {QStringLiteral("NO_PARKING"), QStringLiteral("NONE")},
      {QStringLiteral("PARKING"), QStringLiteral("P")},
      {QStringLiteral("N_PARKING"), QStringLiteral("NP")},
      {QStringLiteral("NE_PARKING"), QStringLiteral("NEP")},
      {QStringLiteral("E_PARKING"), QStringLiteral("EP")},
      {QStringLiteral("SE_PARKING"), QStringLiteral("SEP")},
      {QStringLiteral("S_PARKING"), QStringLiteral("SP")},
      {QStringLiteral("SW_PARKING"), QStringLiteral("SWP")},
      {QStringLiteral("W_PARKING"), QStringLiteral("WP")},
      {QStringLiteral("NW_PARKING"), QStringLiteral("NWP")},
      {QStringLiteral("GATE"), QStringLiteral("G")},
      {QStringLiteral("DOCK"), QStringLiteral("D")},
      {QStringLiteral("GATE_A"), QStringLiteral("GA")},
      {QStringLiteral("GATE_B"), QStringLiteral("GB")},
      {QStringLiteral("GATE_C"), QStringLiteral("GC")},
      {QStringLiteral("GATE_D"), QStringLiteral("GD")},
      {QStringLiteral("GATE_E"), QStringLiteral("GE")},
      {QStringLiteral("GATE_F"), QStringLiteral("GF")},
      {QStringLiteral("GATE_G"), QStringLiteral("GG")},
      {QStringLiteral("GATE_H"), QStringLiteral("GH")},
      {QStringLiteral("GATE_I"), QStringLiteral("GI")},
      {QStringLiteral("GATE_J"), QStringLiteral("GJ")},
      {QStringLiteral("GATE_K"), QStringLiteral("GK")},
      {QStringLiteral("GATE_L"), QStringLiteral("GL")},
      {QStringLiteral("GATE_M"), QStringLiteral("GM")},
      {QStringLiteral("GATE_N"), QStringLiteral("GN")},
      {QStringLiteral("GATE_O"), QStringLiteral("GO")},
      {QStringLiteral("GATE_P"), QStringLiteral("GP")},
      {QStringLiteral("GATE_Q"), QStringLiteral("GQ")},
      {QStringLiteral("GATE_R"), QStringLiteral("GR")},
      {QStringLiteral("GATE_S"), QStringLiteral("GS")},
      {QStringLiteral("GATE_T"), QStringLiteral("GT")},
      {QStringLiteral("GATE_U"), QStringLiteral("GU")},
      {QStringLiteral("GATE_V"), QStringLiteral("GV")},
      {QStringLiteral("GATE_W"), QStringLiteral("GW")},
      {QStringLiteral("GATE_X"), QStringLiteral("GX")},
      {QStringLiteral("GATE_Y"), QStringLiteral("GY")},
      {QStringLiteral("GATE_Z"), QStringLiteral("GZ")},
    });

  /* *INDENT-OFF* */
  // Order is important
  parkingDatabaseKeywords = QList<std::pair<QRegularExpression, QString> >({
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
      {QStringLiteral("INVALID"), QObject::tr("Invalid", "VOR type name")},
      {QStringLiteral("H"), QObject::tr("H", "VOR type name")},
      {QStringLiteral("L"), QObject::tr("L", "VOR type name")},
      {QStringLiteral("T"), QObject::tr("T", "VOR type name")},
      {QStringLiteral("VH"), QObject::tr("H", "VOR type name")},
      {QStringLiteral("VL"), QObject::tr("L", "VOR type name")},
      {QStringLiteral("VT"), QObject::tr("T", "VOR type name")},
    });

  navTypeNamesVorLong = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid", "Long VOR type name")},
      {QStringLiteral("H"), QObject::tr("High", "Long VOR type name")},
      {QStringLiteral("L"), QObject::tr("Low", "Long VOR type name")},
      {QStringLiteral("T"), QObject::tr("Terminal", "Long VOR type name")},
      {QStringLiteral("VTH"), QObject::tr("High", "Long VOR type name")},
      {QStringLiteral("VTL"), QObject::tr("Low", "Long VOR type name")},
      {QStringLiteral("VTT"), QObject::tr("Terminal", "Long VOR type name")}
    });

  navTypeNamesNdb = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid", "NDB type name")},
      {QStringLiteral("HH"), QObject::tr("HH", "NDB type name")},
      {QStringLiteral("H"), QObject::tr("H", "NDB type name")},
      {QStringLiteral("MH"), QObject::tr("MH", "NDB type name")},
      {QStringLiteral("CP"), QObject::tr("Compass Locator", "NDB type name")},
      {QStringLiteral("NHH"), QObject::tr("HH", "NDB type name")},
      {QStringLiteral("NH"), QObject::tr("H", "NDB type name")},
      {QStringLiteral("NMH"), QObject::tr("MH", "NDB type name")},
      {QStringLiteral("NCP"), QObject::tr("Compass Locator", "NDB type name")},
    });

  navTypeNamesWaypoint = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid", "Waypoint type name")},
      {QStringLiteral("WN"), QObject::tr("Named", "Waypoint type name")},
      {QStringLiteral("WT"), QObject::tr("Track", "Waypoint type name")},
      {QStringLiteral("WU"), QObject::tr("Unnamed", "Waypoint type name")},
      {QStringLiteral("V"), QObject::tr("VOR", "Waypoint type name")},
      {QStringLiteral("N"), QObject::tr("NDB", "Waypoint type name")},
      {QStringLiteral("VFR"), QObject::tr("VFR", "Waypoint type name")},
      {QStringLiteral("RNAV"), QObject::tr("RNAV", "Waypoint type name")},
      {QStringLiteral("OA"), QObject::tr("Off Airway", "Waypoint type name")},
      {QStringLiteral("IAF"), QObject::tr("IAF", "Waypoint type name")},
      {QStringLiteral("FAF"), QObject::tr("FAF", "Waypoint type name")}
    });

  navTypeNames = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid", "All navaids type name")},
      {QStringLiteral("VD"), QObject::tr("VORDME", "All navaids type name")},
      {QStringLiteral("VT"), QObject::tr("VORTAC", "All navaids type name")},
      {QStringLiteral("VTD"), QObject::tr("DME only VORTAC", "All navaids type name")},
      {QStringLiteral("V"), QObject::tr("VOR", "All navaids type name")},
      {QStringLiteral("D"), QObject::tr("DME", "All navaids type name")},
      {QStringLiteral("TC"), QObject::tr("TACAN", "All navaids type name")},
      {QStringLiteral("TCD"), QObject::tr("DME only TACAN", "All navaids type name")},
      {QStringLiteral("N"), QObject::tr("NDB", "All navaids type name")},
      {QStringLiteral("W"), QObject::tr("Waypoint", "All navaids type name")}
    });

  comTypeNames = QHash<QString, QString>(
    {
      {QStringLiteral("INVALID"), QObject::tr("Invalid", "COM type name")},
      {QStringLiteral("NONE"), QObject::tr("None", "COM type name")},

      // FSX/P3D types
      // {"ATIS", QObject::tr("ATIS")},
      // {"MC", QObject::tr("Multicom")},
      // {"UC", QObject::tr("Unicom")},
      {QStringLiteral("CTAF"), QObject::tr("CTAF", "COM type name")},
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
      {QStringLiteral("RCD"), QObject::tr("Remote Clearance Delivery", "COM type name")},

      // All new AIRAC types
      {QStringLiteral("CTR"), QObject::tr("Area Control Center", "COM type name")},
      {QStringLiteral("ACP"), QObject::tr("Airlift Command Post", "COM type name")},
      {QStringLiteral("AIR"), QObject::tr("Air to Air", "COM type name")},
      {QStringLiteral("A"), QObject::tr("Approach Control", "COM type name")},
      {QStringLiteral("ARR"), QObject::tr("Arrival Control", "COM type name")},
      {QStringLiteral("ASOS"), QObject::tr("ASOS", "COM type name")},
      {QStringLiteral("ATIS"), QObject::tr("ATIS", "COM type name")},
      {QStringLiteral("AWI"), QObject::tr("AWIB", "COM type name")},
      {QStringLiteral("AWOS"), QObject::tr("AWOS", "COM type name")},
      {QStringLiteral("AWS"), QObject::tr("AWIS", "COM type name")},
      {QStringLiteral("C"), QObject::tr("Clearance Delivery", "COM type name")},
      {QStringLiteral("CPT"), QObject::tr("Clearance Pre-Taxi", "COM type name")},
      {QStringLiteral("CTA"), QObject::tr("Terminal Control Area", "COM type name")},
      {QStringLiteral("CTL"), QObject::tr("Control", "COM type name")},
      {QStringLiteral("D"), QObject::tr("Departure Control", "COM type name")},
      {QStringLiteral("DIR"), QObject::tr("Director (Approach Control Radar)", "COM type name")},
      {QStringLiteral("EFS"), QObject::tr("En-route Flight Advisory Service (EFAS)", "COM type name")},
      {QStringLiteral("EMR"), QObject::tr("Emergency", "COM type name")},
      {QStringLiteral("FSS"), QObject::tr("Flight Service Station", "COM type name")},
      {QStringLiteral("GCO"), QObject::tr("Ground Comm Outlet", "COM type name")},
      {QStringLiteral("GET"), QObject::tr("Gate Control", "COM type name")},
      {QStringLiteral("G"), QObject::tr("Ground Control", "COM type name")},
      {QStringLiteral("HEL"), QObject::tr("Helicopter Frequency", "COM type name")},
      {QStringLiteral("INF"), QObject::tr("Information", "COM type name")},
      {QStringLiteral("MIL"), QObject::tr("Military Frequency", "COM type name")},
      {QStringLiteral("MC"), QObject::tr("Multicom", "COM type name")},
      {QStringLiteral("OPS"), QObject::tr("Operations", "COM type name")},
      {QStringLiteral("PAL"), QObject::tr("Pilot Activated Lighting", "COM type name")},
      {QStringLiteral("RDO"), QObject::tr("Radio", "COM type name")},
      {QStringLiteral("RDR"), QObject::tr("Radar", "COM type name")},
      {QStringLiteral("RFS"), QObject::tr("Remote Flight Service Station (RFSS)", "COM type name")},
      {QStringLiteral("RMP"), QObject::tr("Ramp or Taxi Control", "COM type name")},
      {QStringLiteral("RSA"), QObject::tr("Airport Radar Service Area (ARSA)", "COM type name")},
      {QStringLiteral("TCA"), QObject::tr("Terminal Control Area (TCA)", "COM type name")},
      {QStringLiteral("TMA"), QObject::tr("Terminal Control Area (TMA)", "COM type name")},
      {QStringLiteral("TML"), QObject::tr("Terminal", "COM type name")},
      {QStringLiteral("TRS"), QObject::tr("Terminal Radar Service Area (TRSA)", "COM type name")},
      {QStringLiteral("TWE"), QObject::tr("Transcriber Weather Broadcast (TWEB)", "COM type name")},
      {QStringLiteral("T"), QObject::tr("Tower, Air Traffic Control", "COM type name")},
      {QStringLiteral("UAC"), QObject::tr("Upper Area Control", "COM type name")},
      {QStringLiteral("UC"), QObject::tr("UNICOM", "COM type name")},
      {QStringLiteral("VOL"), QObject::tr("VOLMET", "COM type name")}
    });

  airspaceTypeNameMap = QHash<map::MapAirspaceType, QString>(
    {
      {map::AIRSPACE_NONE, QObject::tr("No Airspace", "Airspace type name")},
      {map::CENTER, QObject::tr("Center", "Airspace type name")},
      {map::CLASS_A, QObject::tr("Class A", "Airspace type name")},
      {map::CLASS_B, QObject::tr("Class B", "Airspace type name")},
      {map::CLASS_C, QObject::tr("Class C", "Airspace type name")},
      {map::CLASS_D, QObject::tr("Class D", "Airspace type name")},
      {map::CLASS_E, QObject::tr("Class E", "Airspace type name")},
      {map::CLASS_F, QObject::tr("Class F", "Airspace type name")},
      {map::CLASS_G, QObject::tr("Class G", "Airspace type name")},
      {map::FIR, QObject::tr("FIR", "Airspace type name")},
      {map::UIR, QObject::tr("UIR", "Airspace type name")},
      {map::TOWER, QObject::tr("Tower", "Airspace type name")},
      {map::CLEARANCE, QObject::tr("Clearance", "Airspace type name")},
      {map::GROUND, QObject::tr("Ground", "Airspace type name")},
      {map::DEPARTURE, QObject::tr("Departure", "Airspace type name")},
      {map::APPROACH, QObject::tr("Approach", "Airspace type name")},
      {map::MOA, QObject::tr("MOA", "Airspace type name")},
      {map::RESTRICTED, QObject::tr("Restricted", "Airspace type name")},
      {map::PROHIBITED, QObject::tr("Prohibited", "Airspace type name")},
      {map::WARNING, QObject::tr("Warning", "Airspace type name")},
      {map::CAUTION, QObject::tr("Caution", "Airspace type name")},
      {map::ALERT, QObject::tr("Alert", "Airspace type name")},
      {map::DANGER, QObject::tr("Danger", "Airspace type name")},
      {map::NATIONAL_PARK, QObject::tr("National Park", "Airspace type name")},
      {map::MODEC, QObject::tr("Mode-C", "Airspace type name")},
      {map::RADAR, QObject::tr("Radar", "Airspace type name")},
      {map::GCA, QObject::tr("General Control Area", "Airspace type name")},
      {map::MCTR, QObject::tr("Military Control Zone", "Airspace type name")},
      {map::TRSA, QObject::tr("Terminal Radar Service Area", "Airspace type name")},
      {map::TRAINING, QObject::tr("Training", "Airspace type name")},
      {map::GLIDERPROHIBITED, QObject::tr("Glider Prohibited", "Airspace type name")},
      {map::WAVEWINDOW, QObject::tr("Wave Window", "Airspace type name")},
      {map::ONLINE_OBSERVER, QObject::tr("Online Observer", "Airspace type name")}
    });

  airspaceTypeShortNameMap = QHash<map::MapAirspaceType, QString>(
    {
      {map::AIRSPACE_NONE, QObject::tr("No Airspace", "Airspace short type name")},
      {map::CENTER, QObject::tr("CTR", "Airspace short type name")},
      {map::CLASS_A, QObject::tr("A", "Airspace short type name")},
      {map::CLASS_B, QObject::tr("B", "Airspace short type name")},
      {map::CLASS_C, QObject::tr("C", "Airspace short type name")},
      {map::CLASS_D, QObject::tr("D", "Airspace short type name")},
      {map::CLASS_E, QObject::tr("E", "Airspace short type name")},
      {map::CLASS_F, QObject::tr("F", "Airspace short type name")},
      {map::CLASS_G, QObject::tr("G", "Airspace short type name")},
      {map::FIR, QObject::tr("FIR", "Airspace short type name")},
      {map::UIR, QObject::tr("UIR", "Airspace short type name")},
      {map::TOWER, QObject::tr("TWR", "Airspace short type name")},
      {map::CLEARANCE, QObject::tr("CLR", "Airspace short type name")},
      {map::GROUND, QObject::tr("GND", "Airspace short type name")},
      {map::DEPARTURE, QObject::tr("DEP", "Airspace short type name")},
      {map::APPROACH, QObject::tr("APP", "Airspace short type name")},
      {map::MOA, QObject::tr("MOA", "Airspace short type name")},
      {map::RESTRICTED, QObject::tr("R", "Airspace short type name")},
      {map::PROHIBITED, QObject::tr("P", "Airspace short type name")},
      {map::WARNING, QObject::tr("W", "Airspace short type name")},
      {map::CAUTION, QObject::tr("CN", "Airspace short type name")},
      {map::ALERT, QObject::tr("A", "Airspace short type name")},
      {map::DANGER, QObject::tr("D", "Airspace short type name")},
      {map::NATIONAL_PARK, QObject::tr("National Park", "Airspace short type name")},
      {map::MODEC, QObject::tr("Mode-C", "Airspace short type name")},
      {map::RADAR, QObject::tr("Radar", "Airspace short type name")},
      {map::GCA, QObject::tr("GCA", "Airspace short type name")},
      {map::MCTR, QObject::tr("MCZ", "Airspace short type name")},
      {map::TRSA, QObject::tr("TRSA", "Airspace short type name")},
      {map::TRAINING, QObject::tr("T", "Airspace short type name")},
      {map::GLIDERPROHIBITED, QObject::tr("Glider Prohibited", "Airspace short type name")},
      {map::WAVEWINDOW, QObject::tr("Wave Window", "Airspace short type name")},
      {map::ONLINE_OBSERVER, QObject::tr("Online Observer", "Airspace short type name")}
    });

  airspaceFlagNameMap = QHash<map::MapAirspaceFlag, QString>(
    {
      // Values below only for actions
      {map::AIRSPACE_ALTITUDE_ALL, QObject::tr("&All altitudes")},
      {map::AIRSPACE_ALTITUDE_FLIGHTPLAN, QObject::tr("At &flight plan cruise altitude")},
      {map::AIRSPACE_ALTITUDE_SET, QObject::tr("For &minimum and maximum altitude")}
    });

  airspaceFlagNameMapLong = QHash<map::MapAirspaceFlag, QString>(
    {
      // Values below only for actions
      {map::AIRSPACE_ALTITUDE_ALL, QObject::tr("Show airspaces for all altitudes")},
      {map::AIRSPACE_ALTITUDE_FLIGHTPLAN, QObject::tr("Show airspaces touching flight plan cruise altitude")},
      {map::AIRSPACE_ALTITUDE_SET, QObject::tr("Show airspaces overlapping selected minimum and maximum altitude")}
    });

  airspaceRemarkMap = QHash<map::MapAirspaceType, QString>(
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
    {QStringLiteral("INVALID"), QStringLiteral("Invalid")},
    {QStringLiteral("UNKNOWN"), QStringLiteral("Unknown")},
    {QStringLiteral("NONE"), QStringLiteral("Parking None")},
    {QStringLiteral("P"), QStringLiteral("Parking")},
    {QStringLiteral("NP"), QStringLiteral("N Parking")},
    {QStringLiteral("NEP"), QStringLiteral("NE Parking")},
    {QStringLiteral("EP"), QStringLiteral("E Parking")},
    {QStringLiteral("SEP"), QStringLiteral("SE Parking")},
    {QStringLiteral("SP"), QStringLiteral("S Parking")},
    {QStringLiteral("SWP"), QStringLiteral("SW Parking")},
    {QStringLiteral("WP"), QStringLiteral("W Parking")},
    {QStringLiteral("NWP"), QStringLiteral("NW Parking")},
    {QStringLiteral("G"), QStringLiteral("Gate")},
    {QStringLiteral("D"), QStringLiteral("Dock")},
    {QStringLiteral("GA"), QStringLiteral("Gate A")},
    {QStringLiteral("GB"), QStringLiteral("Gate B")},
    {QStringLiteral("GC"), QStringLiteral("Gate C")},
    {QStringLiteral("GD"), QStringLiteral("Gate D")},
    {QStringLiteral("GE"), QStringLiteral("Gate E")},
    {QStringLiteral("GF"), QStringLiteral("Gate F")},
    {QStringLiteral("GG"), QStringLiteral("Gate G")},
    {QStringLiteral("GH"), QStringLiteral("Gate H")},
    {QStringLiteral("GI"), QStringLiteral("Gate I")},
    {QStringLiteral("GJ"), QStringLiteral("Gate J")},
    {QStringLiteral("GK"), QStringLiteral("Gate K")},
    {QStringLiteral("GL"), QStringLiteral("Gate L")},
    {QStringLiteral("GM"), QStringLiteral("Gate M")},
    {QStringLiteral("GN"), QStringLiteral("Gate N")},
    {QStringLiteral("GO"), QStringLiteral("Gate O")},
    {QStringLiteral("GP"), QStringLiteral("Gate P")},
    {QStringLiteral("GQ"), QStringLiteral("Gate Q")},
    {QStringLiteral("GR"), QStringLiteral("Gate R")},
    {QStringLiteral("GS"), QStringLiteral("Gate S")},
    {QStringLiteral("GT"), QStringLiteral("Gate T")},
    {QStringLiteral("GU"), QStringLiteral("Gate U")},
    {QStringLiteral("GV"), QStringLiteral("Gate V")},
    {QStringLiteral("GW"), QStringLiteral("Gate W")},
    {QStringLiteral("GX"), QStringLiteral("Gate X")},
    {QStringLiteral("GY"), QStringLiteral("Gate Y")},
    {QStringLiteral("GZ"), QStringLiteral("Gate Z")}
  });

/* The higher the better */
static QHash<QString, int> surfaceQualityMap(
  {
    {QStringLiteral("C"), 20},
    {QStringLiteral("A"), 20},
    {QStringLiteral("B"), 20},
    {QStringLiteral("T"), 20},
    {QStringLiteral("M"), 15},
    {QStringLiteral("CE"), 15},
    {QStringLiteral("OT"), 15},
    {QStringLiteral("BR"), 10},
    {QStringLiteral("SM"), 10},
    {QStringLiteral("PL"), 10},
    {QStringLiteral("GR"), 5},
    {QStringLiteral("CR"), 5},
    {QStringLiteral("D"), 5},
    {QStringLiteral("SH"), 5},
    {QStringLiteral("CL"), 5},
    {QStringLiteral("S"), 5},
    {QStringLiteral("G"), 5},
    {QStringLiteral("SN"), 5},
    {QStringLiteral("I"), 5},
    {QStringLiteral("W"), 1},
    {QStringLiteral("TR"), 1},
    {QStringLiteral("UNKNOWN"), 0},
    {QStringLiteral("INVALID"), 0}
  });

const static QHash<QString, map::MapAirspaceType> airspaceTypeFromDatabaseMap(
  {
    {QStringLiteral("NONE"), map::AIRSPACE_NONE},
    {QStringLiteral("C"), map::CENTER},
    {QStringLiteral("CA"), map::CLASS_A},
    {QStringLiteral("CB"), map::CLASS_B},
    {QStringLiteral("CC"), map::CLASS_C},
    {QStringLiteral("CD"), map::CLASS_D},
    {QStringLiteral("CE"), map::CLASS_E},
    {QStringLiteral("CF"), map::CLASS_F},
    {QStringLiteral("CG"), map::CLASS_G},
    {QStringLiteral("FIR"), map::FIR}, // New FIR region that replaces certain centers
    {QStringLiteral("UIR"), map::UIR}, // As above for UIR
    {QStringLiteral("T"), map::TOWER},
    {QStringLiteral("CL"), map::CLEARANCE},
    {QStringLiteral("G"), map::GROUND},
    {QStringLiteral("D"), map::DEPARTURE},
    {QStringLiteral("A"), map::APPROACH},
    {QStringLiteral("M"), map::MOA},
    {QStringLiteral("R"), map::RESTRICTED},
    {QStringLiteral("P"), map::PROHIBITED},
    {QStringLiteral("CN"), map::CAUTION},
    {QStringLiteral("W"), map::WARNING},
    {QStringLiteral("AL"), map::ALERT},
    {QStringLiteral("DA"), map::DANGER},
    {QStringLiteral("NP"), map::NATIONAL_PARK},
    {QStringLiteral("MD"), map::MODEC},
    {QStringLiteral("RD"), map::RADAR},
    {QStringLiteral("GCA"), map::GCA}, // Control area - new type
    {QStringLiteral("MCTR"), map::MCTR},
    {QStringLiteral("TRSA"), map::TRSA},
    {QStringLiteral("TR"), map::TRAINING},
    {QStringLiteral("GP"), map::GLIDERPROHIBITED},
    {QStringLiteral("WW"), map::WAVEWINDOW},
    {QStringLiteral("OBS"), map::ONLINE_OBSERVER} /* No database type */
  });

static QHash<map::MapAirspaceType, QString> airspaceTypeToDatabaseMap(
  {
    {map::AIRSPACE_NONE, QStringLiteral("NONE")},
    {map::CENTER, QStringLiteral("C")},
    {map::CLASS_A, QStringLiteral("CA")},
    {map::CLASS_B, QStringLiteral("CB")},
    {map::CLASS_C, QStringLiteral("CC")},
    {map::CLASS_D, QStringLiteral("CD")},
    {map::CLASS_E, QStringLiteral("CE")},
    {map::CLASS_F, QStringLiteral("CF")},
    {map::CLASS_G, QStringLiteral("CG")},
    {map::FIR, QStringLiteral("FIR")},
    {map::UIR, QStringLiteral("UIR")},
    {map::TOWER, QStringLiteral("T")},
    {map::CLEARANCE, QStringLiteral("CL")},
    {map::GROUND, QStringLiteral("G")},
    {map::DEPARTURE, QStringLiteral("D")},
    {map::APPROACH, QStringLiteral("A")},
    {map::MOA, QStringLiteral("M")},
    {map::RESTRICTED, QStringLiteral("R")},
    {map::PROHIBITED, QStringLiteral("P")},
    {map::WARNING, QStringLiteral("W")},
    {map::CAUTION, QStringLiteral("CN")},
    {map::ALERT, QStringLiteral("AL")},
    {map::DANGER, QStringLiteral("DA")},
    {map::NATIONAL_PARK, QStringLiteral("NP")},
    {map::MODEC, QStringLiteral("MD")},
    {map::RADAR, QStringLiteral("RD")},
    {map::GCA, QStringLiteral("GCA")},
    {map::MCTR, QStringLiteral("MCTR")},
    {map::TRSA, QStringLiteral("TRSA")},
    {map::TRAINING, QStringLiteral("TR")},
    {map::GLIDERPROHIBITED, QStringLiteral("GP")},
    {map::WAVEWINDOW, QStringLiteral("WW")},

    {map::ONLINE_OBSERVER, QStringLiteral("OBS")} /* Not a database type */
  });

/* Defines drawing sort order - lower values are drawn first - higher values are drawn on top */
const static QHash<map::MapAirspaceType, int> airspacePriorityMap(
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

const QString& navTypeNameVor(const QString& type)
{
  return atools::hashValue(navTypeNamesVor, type);
}

const QString& navTypeNameVorLong(const QString& type)
{
  return atools::hashValue(navTypeNamesVorLong, type);
}

const QString& navTypeNameNdb(const QString& type)
{
  return atools::hashValue(navTypeNamesNdb, type);
}

const QString& navTypeNameWaypoint(const QString& type)
{
  return atools::hashValue(navTypeNamesWaypoint, type);
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
  types.append(QStringLiteral("[%1]").arg(type));
#endif

  return types.join(QObject::tr(", "));
}

const QString& navName(const QString& type)
{
  return atools::hashValue(navTypeNames, type);
}

bool surfaceValid(const QString& surface)
{
  return !surface.isEmpty() && surface != QStringLiteral("TR") && surface != QStringLiteral("UNKNOWN") &&
         surface != QStringLiteral("INVALID");
}

const QString& surfaceName(const QString& surface)
{
  return atools::hashValue(surfaceMap, surface);
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
  return surfaceQualityMap.value(surface, 0);
}

const QList<std::pair<QRegularExpression, QString> >& parkingKeywords()
{
  return parkingDatabaseKeywords;
}

const QString& parkingGateName(const QString& gate)
{
  return atools::hashValue(parkingMapGate, gate);
}

const QString& parkingRampName(const QString& ramp)
{
  return atools::hashValue(parkingMapRamp, ramp);
}

const QString& parkingTypeName(const QString& type)
{
  return atools::hashValue(parkingTypeMap, type);
}

QString parkingText(const MapParking& parking)
{
  QStringList retval;

  if(parking.type.isEmpty())
    retval.append(QObject::tr("Parking"));

  retval.append(map::parkingName(parking.name));

  retval.append(parking.number != -1 ? QStringLiteral(" ") % QLocale().toString(parking.number) % parking.suffix : QString());
  return atools::strJoin(retval, QObject::tr(" "));
}

QString parkingName(const QString& name)
{
  return parkingNameMap.value(name, name);
}

const QString& parkingDatabaseName(const QString& name)
{
  return atools::hashValue(parkingDatabaseNameMap, name);
}

QString parkingNameNumberAndType(const map::MapParking& parking)
{
  return atools::strJoin({parkingNameOrNumber(parking), parkingTypeName(parking.type)}, QObject::tr(", "));
}

QString parkingNameOrNumber(const MapParking& parking)
{
  if(parking.number != -1)
    return map::parkingName(parking.name) % QStringLiteral(" ") % QLocale().toString(parking.number) % parking.suffix;
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
    return parkingNameMapUntranslated.value(parking.name).toUpper() % QStringLiteral(" ") % QString::number(parking.number) %
           parking.suffix;
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
  return emptyDraw(od.getFlags().testFlag(opts::MAP_EMPTY_AIRPORTS),
                   od.getFlags2().testFlag(opts2::MAP_EMPTY_AIRPORTS_3D) &&
                   NavApp::getCurrentSimulatorDb() != atools::fs::FsPaths::XPLANE_12);
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
  if(addon() && types.testFlag(map::AIRPORT_ADDON_ZOOM_FILTER))
    return true;

  bool overrideAddon = addon() && types.testFlag(map::AIRPORT_ADDON_ZOOM);

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
  if(navType.startsWith(QStringLiteral("V")) || navType == QStringLiteral("D") || navType.startsWith(QStringLiteral("TC")))
    type = map::VOR;
  else if(navType == QStringLiteral("N"))
    type = map::NDB;
  else if(navType == QStringLiteral("W"))
    type = map::WAYPOINT;
  return type;
}

bool navTypeTacan(const QString& navType)
{
  return navType == QStringLiteral("TC") || navType == QStringLiteral("TCD");
}

bool navTypeVortac(const QString& navType)
{
  return navType == QStringLiteral("VT") || navType == QStringLiteral("VTD");
}

QString airwayTrackTypeToShortString(MapAirwayTrackType type)
{
  switch(type)
  {
    case map::NO_AIRWAY:
      break;

    case map::TRACK_NAT:
      return QObject::tr("N", "Oceanic track type");

    case map::TRACK_PACOTS:
      return QObject::tr("P", "Oceanic track type");

    case map::AIRWAY_VICTOR:
      return QObject::tr("L", "Oceanic track type");

    case map::AIRWAY_JET:
      return QObject::tr("H", "Oceanic track type");

    case map::AIRWAY_BOTH:
      return QObject::tr("B", "Oceanic track type");

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
      return QObject::tr("NAT", "Oceanic track system name");

    case map::TRACK_PACOTS:
      return QObject::tr("PACOTS", "Oceanic track system name");

    case map::AIRWAY_VICTOR:
      return QObject::tr("Low", "Oceanic track system name");

    case map::AIRWAY_JET:
      return QObject::tr("High", "Oceanic track system name");

    case map::AIRWAY_BOTH:
      return QObject::tr("Both", "Oceanic track system name");

  }
  return QString();
}

MapAirwayTrackType airwayTrackTypeFromString(const QString& typeStr)
{
  if(typeStr.startsWith('V') || typeStr.startsWith('L'))
    return map::AIRWAY_VICTOR;
  else if(typeStr.startsWith('J') || typeStr.startsWith('H'))
    return map::AIRWAY_JET;
  else if(typeStr.startsWith('B'))
    return map::AIRWAY_BOTH;
  else if(typeStr.startsWith('N'))
    return map::TRACK_NAT;
  else if(typeStr.startsWith('P'))
    return map::TRACK_PACOTS;
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
      return QStringLiteral("A");

    case map::RT_CONTROL:
      return QStringLiteral("C");

    case map::RT_DIRECT:
      return QStringLiteral("D");

    case map::RT_HELICOPTER:
      return QStringLiteral("H");

    case map::RT_OFFICIAL:
      return QStringLiteral("O");

    case map::RT_RNAV:
      return QStringLiteral("R");

    case map::RT_UNDESIGNATED:
      return QStringLiteral("S");

    case map::RT_TRACK:
      return QStringLiteral("T");
  }
  return QString();
}

MapAirwayRouteType airwayRouteTypeFromString(const QString& typeStr)
{
  if(typeStr == QStringLiteral("A"))
    return RT_AIRLINE; /* A Airline Airway (Tailored Data) */
  else if(typeStr == QStringLiteral("C"))
    return RT_CONTROL; /* C Control */
  else if(typeStr == QStringLiteral("D"))
    return RT_DIRECT; /* D Direct Route */
  else if(typeStr == QStringLiteral("H"))
    return RT_HELICOPTER; /* H Helicopter Airways */
  else if(typeStr == QStringLiteral("O"))
    return RT_OFFICIAL; /* O Officially Designated Airways, except RNAV, Helicopter Airways */
  else if(typeStr == QStringLiteral("R"))
    return RT_RNAV; /* R RNAV Airways */
  else if(typeStr == QStringLiteral("S"))
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
    QString type = vorType.startsWith(QStringLiteral("VT")) ? vorType.at(vorType.size() - 1) : vorType.at(0);

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
           arg(obj.holding.turnLeft ? QObject::tr("L", "Holding direction") : QObject::tr("R", "Holding direction")).
           arg(Unit::altFeet(obj.holding.position.getAltitude()));
  else
    return QObject::tr("User Holding %1 %2 %3").
           arg(obj.holding.navIdent).
           arg(obj.holding.turnLeft ? QObject::tr("L", "Holding direction") : QObject::tr("R", "Holding direction")).
           arg(Unit::altFeet(obj.holding.position.getAltitude()));
}

QString patternMarkText(const PatternMarker& obj)
{
  if(obj.airportIcao.isEmpty())
    return QObject::tr("Traffic Pattern %1 RW %2").arg(obj.turnRight ?
                                                       QObject::tr("R", "Pattern direction") :
                                                       QObject::tr("L", "Pattern direction")).arg(obj.runwayName);
  else
    return QObject::tr("Traffic Pattern %1 %2 RW %3").
           arg(obj.airportIcao).arg(obj.turnRight ?
                                    QObject::tr("R", "Pattern direction") :
                                    QObject::tr("L", "Pattern direction")).arg(obj.runwayName);
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
      altTxt = QObject::tr("Min ", "Airway altitude restriction") % Unit::altFeet(airway.minAltitude);
  }

  if(airway.maxAltitude > 0 && airway.maxAltitude < map::MapAirway::MAX_ALTITUDE_LIMIT_FT)
  {
    if(airway.minAltitude > 0)
      altTxt += QObject::tr(" to ", "Airway altitude restriction") % Unit::altFeet(airway.maxAltitude);
    else
      altTxt += QObject::tr("Max ", "Airway altitude restriction") % Unit::altFeet(airway.maxAltitude);
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

const QString& comTypeName(const QString& type)
{

  return atools::hashValue(comTypeNames, type);
}

QString magvarText(float magvar, bool shortText, bool degSign)
{
  QString num = QLocale().toString(std::abs(magvar), 'f', 1);

  if(!num.isEmpty())
  {
    // The only way to remove trailing zeros
    QString pt = QLocale().decimalPoint();
    if(num.endsWith(pt))
      num.chop(1);
    if(num.endsWith(pt % QStringLiteral("0")))
      num.chop(2);

    const QString txt = degSign ? QObject::tr("%1°%2") : QObject::tr("%1%2");

    if(magvar < -0.04f)
      return txt.arg(num).arg(shortText ? QObject::tr("W", "Compass direction west") : QObject::tr(" West"));
    else if(magvar > 0.04f)
      // positive" (or "easterly") variation
      return txt.arg(num).arg(shortText ? QObject::tr("E", "Compass direction east") : QObject::tr(" East"));
    else
      return degSign ? QObject::tr("0°") : QObject::tr("0");
  }
  return QString();
}

bool isHardSurface(const QString& surface)
{
  return surface == QStringLiteral("C") || surface == QStringLiteral("A") || surface == QStringLiteral("B") ||
         surface == QStringLiteral("T");
}

bool isWaterSurface(const QString& surface)
{
  return surface == QStringLiteral("W");
}

bool isSoftSurface(const QString& surface)
{
  return !isWaterSurface(surface) && !isHardSurface(surface);
}

QString edgeLights(const QString& type)
{
  if(type == QStringLiteral("L"))
    return QObject::tr("Low", "Taxi edge lights");
  else if(type == QStringLiteral("M"))
    return QObject::tr("Medium", "Taxi edge lights");
  else if(type == QStringLiteral("H"))
    return QObject::tr("High", "Taxi edge lights");
  else
    return QString();
}

QString patternDirection(const QString& type)
{
  if(type == QStringLiteral("L"))
    return QObject::tr("Left", "Traffic pattern direction");
  else if(type == QStringLiteral("R"))
    return QObject::tr("Right", "Traffic pattern direction");
  else
    return QString();
}

QString ndbFullShortText(const MapNdb& ndb)
{
  // Compass point vs. compass locator
  QString type = ndb.type == QStringLiteral("CP") ? QObject::tr("CL") : ndb.type;
  return type.isEmpty() ? QObject::tr("NDB") : QObject::tr("NDB (%1)").arg(type);
}

const QString& airspaceTypeToString(map::MapAirspaceType type)
{
  return atools::hashValue(airspaceTypeNameMap, type);
}

const QString& airspaceTypeShortToString(map::MapAirspaceType type)
{
  return atools::hashValue(airspaceTypeShortNameMap, type);
}

const QString& airspaceFlagToString(map::MapAirspaceFlag type)
{
  return atools::hashValue(airspaceFlagNameMap, type);
}

const QString& airspaceFlagToStringLong(map::MapAirspaceFlag type)
{
  return atools::hashValue(airspaceFlagNameMapLong, type);
}

const QString& airspaceRemark(map::MapAirspaceType type)
{
  return atools::hashValue(airspaceRemarkMap, type);
}

int airspaceDrawingOrder(map::MapAirspaceType type)
{
  return airspacePriorityMap.value(type, map::AIRSPACE_NONE);
}

map::MapAirspaceType airspaceTypeFromDatabase(const QString& type)
{
  return atools::hashValue(airspaceTypeFromDatabaseMap, type, map::AIRSPACE_NONE);
}

const QString& airspaceTypeToDatabase(map::MapAirspaceType type)
{
  return atools::hashValue(airspaceTypeToDatabaseMap, type);
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

QDebug operator<<(QDebug out, const map::MapAirport& obj)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapAirport[" << "id " << obj.id
                          << ", ident " << obj.ident
                          << ", navdata " << obj.navdata
                          << ", type " << obj.objType
                          << ", procedure " << obj.procedure()
                          << ", " << obj.position << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapBase& obj)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapBase[" << "id " << obj.id << ", type " << obj.objType << ", " << obj.position << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapRef& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapObjectRef[" << "id " << ref.id << ", type " << ref.objType << "]";
  return out;
}

QDebug operator<<(QDebug out, const MapRefExt& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapObjectRefExt[" << "id " << ref.id << ", type " << ref.objType;

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
      text += separator % QObject::tr("GS", "ILS type");
    if(dme && ils.hasDme)
      text += separator % QObject::tr("DME", "ILS type");
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

QString airspaceRestrictiveNameMap(const MapAirspace& airspace)
{
  QString restrictedName;
  if(!airspace.restrictiveDesignation.isEmpty())
  {
    QString name = airspace.restrictiveDesignation;

    if(name.endsWith('*'))
      name = name.remove('*').trimmed();

    restrictedName = airspace.restrictiveType % QObject::tr("-", "Airspace text separator") % name;
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
    texts.append(atools::elideTextShort(airspace.name, maxTextLength));
  if(restrictiveName)
    texts.append(restrNameStr);

  if(altitude && !airspace.isOnline())
  {
    QString altTextLower, altTextUpper;

    if(airspace.maxAltitude != 0 || airspace.minAltitude != 0)
    {
      if(airspace.maxAltitudeType != QStringLiteral("UL") && airspace.maxAltitude < 60000)
        // Upper altitude text if not unlimited
        altTextUpper = Unit::altFeet(airspace.maxAltitude, false /* addUnit */, true /* narrow */);

      if(!altTextUpper.isEmpty() && !airspace.maxAltitudeType.isEmpty())
        altTextUpper += QObject::tr(" ") % airspace.maxAltitudeType;

      if(!airspace.minAltitudeType.isEmpty() && airspace.minAltitude == 0 && !altTextUpper.isEmpty())
        // 0 is zero. No matter if AGL or MSL
        altTextLower = QObject::tr("0");
      else if(airspace.minAltitude > 0)
      {
        // Lower altitiude text
        altTextLower = Unit::altFeet(airspace.minAltitude, false /* addUnit */, true /* narrow */);

        if(!altTextLower.isEmpty() && !airspace.minAltitudeType.isEmpty() && airspace.maxAltitudeType != airspace.minAltitudeType)
          // Add MSL/AGL to lower text if given or different from max type
          altTextLower += QObject::tr(" ") % airspace.minAltitudeType;
      }

      if(!altTextLower.isEmpty() || !altTextUpper.isEmpty())
        texts.append(altTextLower % QObject::tr("-") % altTextUpper);
    }
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
  return QObject::tr("Airspace %1 (%2)").arg(airspace.name).arg(airspaceTypeToString(airspace.type));
}

const QString& aircraftType(const atools::fs::sc::SimConnectAircraft& aircraft)
{
  if(!aircraft.getAirplaneType().isEmpty())
    return aircraft.getAirplaneType();
  else
    // Convert model ICAO code to a name
    return atools::fs::util::aircraftTypeForCode(aircraft.getAirplaneModel());
}

QString aircraftTypeString(const atools::fs::sc::SimConnectAircraft& aircraft)
{
  switch(aircraft.getCategory())
  {
    case atools::fs::sc::BOAT:
      return QObject::tr("Ship", "Aircraft category");

    case atools::fs::sc::CARRIER:
      return QObject::tr("Carrier", "Aircraft category");

    case atools::fs::sc::FRIGATE:
      return QObject::tr("Frigate", "Aircraft category");

    case atools::fs::sc::AIRPLANE:
      return QObject::tr("Aircraft", "Aircraft category");

    case atools::fs::sc::HELICOPTER:
      return QObject::tr("Helicopter", "Aircraft category");

    case atools::fs::sc::UNKNOWN:
      return QObject::tr("Unknown", "Aircraft category");

    case atools::fs::sc::GROUNDVEHICLE:
      return QObject::tr("Ground Vehicle", "Aircraft category");

    case atools::fs::sc::CONTROLTOWER:
      return QObject::tr("Control Tower", "Aircraft category");

    case atools::fs::sc::SIMPLEOBJECT:
      return QObject::tr("Simple Object", "Aircraft category");

    case atools::fs::sc::VIEWER:
      return QObject::tr("Viewer", "Aircraft category");
  }
  return QObject::tr("Vehicle", "Aircraft category");
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

      case map::START:
        return QString();

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
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/msa.svg"));

      case map::VOR:
        return SymbolPainter::createVorIcon(*base->asPtr<map::MapVor>(), size, NavApp::isGuiStyleDark());

      case map::NDB:
        return SymbolPainter::createNdbIcon(size, NavApp::isGuiStyleDark());

      case map::ILS:
        return map::ilsIcon(base->asObj<map::MapIls>());

      case map::HOLDING:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/enroutehold.svg"));

      case map::WAYPOINT:
        return SymbolPainter::createWaypointIcon(size);

      case map::AIRWAY:
        return SymbolPainter::createAirwayIcon(*base->asPtr<map::MapAirway>(), size, NavApp::isGuiStyleDark());

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

      case map::START:
        return QIcon();

      case map::HELIPAD:
        return SymbolPainter::createHelipadIcon(*base->asPtr<map::MapHelipad>(), size);

      case map::USERPOINT:
        return QIcon(NavApp::getUserdataIcons()->getIconPath(base->asPtr<map::MapUserpoint>()->type));

      case map::LOGBOOK:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/logbook.svg"));

      case map::PROCEDURE_POINT:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/approach.svg"));

      case map::MARK_RANGE:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/rangerings.svg"));

      case map::MARK_DISTANCE:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/distancemeasure.svg"));

      case map::MARK_HOLDING:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/hold.svg"));

      case map::MARK_PATTERNS:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/trafficpattern.svg"));

      case map::MARK_MSA:
        return QIcon(QStringLiteral(":/littlenavmap/resources/icons/msa.svg"));
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

  if(aircraft.getPropIcePercent() >= 1.f && aircraft.getEngineType() != atools::fs::sc::JET)
    text.append((narrow ? QObject::tr("Prop %L1") : QObject::tr("Prop&nbsp;%L1")).arg(aircraft.getPropIcePercent(), 0, 'f', 0));

  if(aircraft.getStatIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Static %L1") : QObject::tr("Static&nbsp;%L1")).arg(aircraft.getStatIcePercent(), 0, 'f', 0));

  if(aircraft.getWindowIcePercent() >= 1.f)
    text.append((narrow ? QObject::tr("Wind %L1") : QObject::tr("Window&nbsp;%L1")).arg(aircraft.getWindowIcePercent(), 0, 'f', 0));

  if(aircraft.getCarbIcePercent() >= 1.f && aircraft.getEngineType() == atools::fs::sc::PISTON)
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

atools::geo::Pos MapRunway::getApproachPosition(bool secondary) const
{
  if(secondary)
  {
    if(secondaryOffset > 1.f)
      // Return position minus secondary offset
      return secondaryPosition.endpoint(atools::geo::feetToMeter(secondaryOffset), atools::geo::opposedCourseDeg(heading));
    else
      return secondaryPosition;
  }
  else
  {
    if(primaryOffset > 1.f)
      // Return position minus primary offset
      return primaryPosition.endpoint(atools::geo::feetToMeter(primaryOffset), heading);
    else
      return primaryPosition;
  }
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

std::tuple<int, int> MapProcedurePoint::compoundIdBase() const
{
  return std::make_tuple(legs->ref.airportId, legs->ref.procedureId);
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
  const static QIcon GLS(QStringLiteral(":/littlenavmap/resources/icons/gls.svg"));
  const static QIcon ILS(QStringLiteral(":/littlenavmap/resources/icons/ils.svg"));
  const static QIcon LOC(QStringLiteral(":/littlenavmap/resources/icons/loc.svg"));

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
    QString name = airspace.restrictiveDesignation;

    if(name.endsWith('*'))
      name = name.remove('*').trimmed();

    QString restrictedName = airspace.restrictiveType % QObject::tr("-") % name;
    if(restrictedName != airspace.name)
      return restrictedName;
  }
  return QString();
}

void registerMetaTypes()
{
  qRegisterMetaType<map::MapRef>();
  qRegisterMetaType<QList<map::MapRef> >();
  qRegisterMetaType<map::RangeMarker>();
  qRegisterMetaType<QList<map::RangeMarker> >();
  qRegisterMetaType<map::DistanceMarker>();
  qRegisterMetaType<QList<map::DistanceMarker> >();
  qRegisterMetaType<map::PatternMarker>();
  qRegisterMetaType<QList<map::PatternMarker> >();
  qRegisterMetaType<map::HoldingMarker>();
  qRegisterMetaType<QList<map::HoldingMarker> >();
  qRegisterMetaType<map::MsaMarker>();
  qRegisterMetaType<QList<map::MsaMarker> >();
}

} // namespace types
