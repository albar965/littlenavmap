/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include <QDataStream>
#include <QHash>
#include <QObject>

namespace map {

const static QHash<QString, QString> surfaceMap(
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
    {"UNKNOWN", QObject::tr("Unknown")},
    {"INVALID", QString()}
  });

/* The higher the better */
const static QHash<QString, int> surfaceQualityMap(
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
    {"UNKNOWN", 0},
    {"INVALID", 0}
  });

/* Short size name for gate and full name for others */
const static QHash<QString, QString> parkingMapGate(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"UNKNOWN", QObject::tr("Unknown")},
    {"RGA", QObject::tr("Ramp GA")},
    {"RGAS", QObject::tr("Ramp GA Small")},
    {"RGAM", QObject::tr("Ramp GA Medium")},
    {"RGAL", QObject::tr("Ramp GA Large")},
    {"RC", QObject::tr("Ramp Cargo")},
    {"RMC", QObject::tr("Ramp Mil Cargo")},
    {"RMCB", QObject::tr("Ramp Mil Combat")},
    {"GS", QObject::tr("Small")},
    {"GM", QObject::tr("Medium")},
    {"GH", QObject::tr("Heavy")},
    {"DGA", QObject::tr("Dock GA")},
    {"FUEL", QObject::tr("Fuel")},
    {"V", QObject::tr("Vehicles")}
  });

/* Short size name for parking and full name for others */
const static QHash<QString, QString> parkingMapRamp(
  {
    {"UNKNOWN", QObject::tr("Unknown")},
    {"RGA", QObject::tr("Ramp GA")},
    {"RGAS", QObject::tr("Small")},
    {"RGAM", QObject::tr("Medium")},
    {"RGAL", QObject::tr("Large")},
    {"RC", QObject::tr("Ramp Cargo")},
    {"RMC", QObject::tr("Ramp Mil Cargo")},
    {"RMCB", QObject::tr("Ramp Mil Combat")},
    {"GS", QObject::tr("Gate Small")},
    {"GM", QObject::tr("Gate Medium")},
    {"GH", QObject::tr("Gate Heavy")},
    {"DGA", QObject::tr("Dock GA")},
    {"FUEL", QObject::tr("Fuel")},
    {"V", QObject::tr("Vehicles")}
  });

/* Full name for all parking including type */
const static QHash<QString, QString> parkingTypeMap(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"UNKNOWN", QObject::tr("Unknown")},
    {"RGA", QObject::tr("Ramp GA")},
    {"RGAS", QObject::tr("Ramp GA Small")},
    {"RGAM", QObject::tr("Ramp GA Medium")},
    {"RGAL", QObject::tr("Ramp GA Large")},
    {"RC", QObject::tr("Ramp Cargo")},
    {"RMC", QObject::tr("Ramp Mil Cargo")},
    {"RMCB", QObject::tr("Ramp Mil Combat")},
    {"GS", QObject::tr("Gate Small")},
    {"GM", QObject::tr("Gate Medium")},
    {"GH", QObject::tr("Gate Heavy")},
    {"DGA", QObject::tr("Dock GA")},
    {"FUEL", QObject::tr("Fuel")},
    {"V", QObject::tr("Vehicles")}
  });

const static QHash<QString, QString> parkingNameMap(
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

const static QHash<QString, QString> parkingDatabaseNameMap(
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

const static QHash<QString, QString> navTypeNamesVor(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"H", QObject::tr("H")},
    {"L", QObject::tr("L")},
    {"T", QObject::tr("T")},
    {"VH", QObject::tr("H")},
    {"VL", QObject::tr("L")},
    {"VT", QObject::tr("T")},
  });

const static QHash<QString, QString> navTypeNamesVorLong(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"H", QObject::tr("High")},
    {"L", QObject::tr("Low")},
    {"T", QObject::tr("Terminal")},
    {"VH", QObject::tr("High")},
    {"VL", QObject::tr("Low")},
    {"VT", QObject::tr("Terminal")},
  });

const static QHash<QString, QString> navTypeNamesNdb(
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

const static QHash<QString, QString> navTypeNamesWaypoint(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"WN", QObject::tr("Named")},
    {"WU", QObject::tr("Unnamed")},
    {"V", QObject::tr("VOR")},
    {"N", QObject::tr("NDB")}
  });

const static QHash<QString, QString> navNames(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"VD", QObject::tr("VORDME")},
    {"V", QObject::tr("VOR")},
    {"D", QObject::tr("DME")},
    {"N", QObject::tr("NDB")},
    {"W", QObject::tr("Waypoint")}
  });

const static QHash<QString, QString> comTypeNames(
  {
    {"INVALID", QObject::tr("Invalid")},
    {"NONE", QObject::tr("None")},
    {"ATIS", QObject::tr("ATIS")},
    {"MC", QObject::tr("Multicom")},
    {"UC", QObject::tr("Unicom")},
    {"CTAF", QObject::tr("CTAF")},
    {"G", QObject::tr("Ground")},
    {"T", QObject::tr("Tower")},
    {"C", QObject::tr("Clearance")},
    {"A", QObject::tr("Approach")},
    {"D", QObject::tr("Departure")},
    {"CTR", QObject::tr("Center")},
    {"FSS", QObject::tr("FSS")},
    {"AWOS", QObject::tr("AWOS")},
    {"ASOS", QObject::tr("ASOS")},
    {"CPT", QObject::tr("Clearance pre Taxi")},
    {"RCD", QObject::tr("Remote Clearance Delivery")}
  });

const static QHash<map::MapAirspaceTypes, QString> airspaceTypeNameMap(
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
    {map::ALERT, QObject::tr("Alert")},
    {map::DANGER, QObject::tr("Danger")},
    {map::NATIONAL_PARK, QObject::tr("National Park")},
    {map::MODEC, QObject::tr("Mode-C")},
    {map::RADAR, QObject::tr("Radar")},
    {map::TRAINING, QObject::tr("Training")},

    // Values below only for actions
    {map::AIRSPACE_AT_FLIGHTPLAN, QObject::tr("At flight plan cruise altitude")},
    {map::AIRSPACE_BELOW_10000, QObject::tr("Below 10000 ft only")},
    {map::AIRSPACE_BELOW_18000, QObject::tr("Below 18000 ft only")},
    {map::AIRSPACE_ABOVE_10000, QObject::tr("Above 10000 ft only")},
    {map::AIRSPACE_ABOVE_18000, QObject::tr("Above 18000 ft only")},
    {map::AIRSPACE_ALL_ALTITUDE, QObject::tr("All Altitudes")}
  });

const static QHash<map::MapAirspaceTypes, QString> airspaceRemarkMap(
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
    {map::MOA, QObject::tr("Military operations area. Needs clearance for IFR if active. Check for traffic advisories.")},
    {map::RESTRICTED, QObject::tr("Needs authorization.")},
    {map::PROHIBITED, QObject::tr("No flight allowed.")},
    {map::WARNING, QObject::tr("Contains activity that may be hazardous to aircraft.")},
    {map::ALERT, QObject::tr("High volume of pilot training or an unusual type of aerial activity.")},
    {map::DANGER, QObject::tr("Avoid or proceed with caution.")},
    {map::NATIONAL_PARK, QString()},
    {map::MODEC, QObject::tr("Needs altitude aware transponder.")},
    {map::RADAR, QObject::tr("Terminal radar area. Not controlled.")},
    {map::TRAINING, QString()}
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
    {"W", map::WARNING},
    {"AL", map::ALERT},
    {"DA", map::DANGER},
    {"NP", map::NATIONAL_PARK},
    {"MD", map::MODEC},
    {"RD", map::RADAR},
    {"TR", map::TRAINING},
  });

const static QHash<map::MapAirspaceTypes, QString> airspaceTypeToDatabaseMap(
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
    {map::ALERT, "AL"},
    {map::DANGER, "DA"},
    {map::NATIONAL_PARK, "NP"},
    {map::MODEC, "MD"},
    {map::RADAR, "RD"},
    {map::TRAINING, "TR"},
  });

/* Defines drawing sort order - lower values are drawn first*/
const static QHash<map::MapAirspaceTypes, int> airspacePriorityMap(
  {
    {map::AIRSPACE_NONE, 1},
    {map::CENTER, 1},

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

    {map::RESTRICTED, 100},
    {map::PROHIBITED, 101},

    {map::WARNING, 60},
    {map::ALERT, 61},
    {map::DANGER, 62},

    {map::NATIONAL_PARK, 2},
    {map::MODEC, 5},
    {map::RADAR, 6},
    {map::TRAINING, 59},
  });

int qHash(const map::MapObjectRef& type)
{
  return type.id ^ static_cast<int>(type.type);
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

QString navName(const QString& type)
{
  return navNames.value(type);
}

QString surfaceName(const QString& surface)
{
  return surfaceMap.value(surface);
}

int surfaceQuality(const QString& surface)
{
  return surfaceQualityMap.value(surface, 0);
}

QString parkingGateName(const QString& gate)
{
  return parkingMapGate.value(gate);
}

QString parkingRampName(const QString& ramp)
{
  return parkingMapRamp.value(ramp);
}

QString parkingTypeName(const QString& type)
{
  return parkingTypeMap.value(type);
}

QString parkingName(const QString& name)
{
  return parkingNameMap.value(name);
}

QString parkingDatabaseName(const QString& name)
{
  return parkingDatabaseNameMap.value(name);
}

QString parkingNameNumberType(const map::MapParking& parking)
{
  return map::parkingName(parking.name) + " " + QLocale().toString(parking.number) +
         ", " + map::parkingTypeName(parking.type);
}

QString startType(const map::MapStart& start)
{
  if(start.type == "R")
    return "Runway";
  else if(start.type == "W")
    return "Water";
  else if(start.type == "H")
    return "Helipad";
  else
    return QString();
}

QString parkingNameForFlightplan(const map::MapParking& parking)
{
  return parkingNameMap.value(parking.name).toUpper() + " " + QString::number(parking.number);
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

bool MapAirport::empty() const
{
  return !parking() && !taxiway() && !apron() && !addon();
}

bool MapAirport::addon() const
{
  return flags.testFlag(AP_ADDON);
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

  if(OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS &&
     empty() && !waterOnly() && !objectTypes.testFlag(map::AIRPORT_EMPTY))
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
  if(navType == "V" || navType == "VD" || navType == "D")
    type = map::VOR;
  else if(navType == "N")
    type = map::NDB;
  else if(navType == "W")
    type = map::WAYPOINT;
  return type;
}

QString airwayTypeToShortString(MapAirwayType type)
{
  switch(type)
  {
    case map::NO_AIRWAY:
      break;
    case map::VICTOR:
      return QObject::tr("V");

    case map::JET:
      return QObject::tr("J");

    case map::BOTH:
      return QObject::tr("B");

  }
  return QString();
}

QString airwayTypeToString(MapAirwayType type)
{
  switch(type)
  {
    case map::NO_AIRWAY:
      break;
    case map::VICTOR:
      return QObject::tr("Victor");

    case map::JET:
      return QObject::tr("Jet");

    case map::BOTH:
      return QObject::tr("Both");

  }
  return QString();
}

MapAirwayType airwayTypeFromString(const QString& typeStr)
{
  if(typeStr.startsWith("V"))
    return map::VICTOR;
  else if(typeStr.startsWith("J"))
    return map::JET;
  else if(typeStr.startsWith("B"))
    return map::BOTH;
  else
    return map::NO_AIRWAY;
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
  dataStream >> obj.text >> obj.color >> obj.from >> obj.to >> obj.magvar >> obj.isRhumbLine >> obj.hasMagvar;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj)
{
  dataStream << obj.text << obj.color << obj.from << obj.to << obj.magvar << obj.isRhumbLine << obj.hasMagvar;
  return dataStream;
}

QString vorType(const MapVor& vor)
{
  QString type;
  if(vor.dmeOnly)
    return QObject::tr("DME");
  else if(vor.hasDme)
    return QObject::tr("VORDME");
  else
    return QObject::tr("VOR");
}

QString vorText(const MapVor& vor)
{
  return QObject::tr("%1 %2 (%3)").arg(vorType(vor)).arg(atools::capString(vor.name)).arg(vor.ident);
}

QString ndbText(const MapNdb& ndb)
{
  return QObject::tr("NDB %1 (%2)").arg(atools::capString(ndb.name)).arg(ndb.ident);
}

QString waypointText(const MapWaypoint& waypoint)
{
  return QObject::tr("Waypoint %1").arg(waypoint.ident);
}

QString userpointText(const MapUserpoint& userpoint)
{
  return QObject::tr("User point %1").arg(userpoint.name);
}

QString airwayText(const MapAirway& airway)
{
  return QObject::tr("Airway %1").arg(airway.name);
}

QString airportText(const MapAirport& airport)
{
  return QObject::tr("Airport %1 (%2)").arg(airport.name).arg(airport.ident);
}

QString airportTextShort(const MapAirport& airport)
{
  return QObject::tr("%1 (%2)").arg(airport.name).arg(airport.ident);
}

QString comTypeName(const QString& type)
{
  return comTypeNames.value(type);
}

bool MapObjectRef::operator==(const MapObjectRef& other) const
{
  return id == other.id && type == other.type;
}

bool MapObjectRef::operator!=(const MapObjectRef& other) const
{
  return !operator==(other);
}

QString magvarText(float magvar)
{
  int decimals = 0;
  if(std::remainder(std::abs(magvar), 1.f) > 0.f)
    decimals = 1;

  if(magvar < -0.04f)
    return QObject::tr("%1°%2").
           arg(QLocale().toString(std::abs(magvar), 'f', decimals)).arg(QObject::tr(" West"));
  else if(magvar > 0.04f)
    // positive" (or "easterly") variation
    return QObject::tr("%1°%2").
           arg(QLocale().toString(magvar, 'f', decimals)).arg(QObject::tr(" East"));
  else
    return "0°";
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

bool MapSearchResult::isEmpty(const MapObjectTypes& types) const
{
  bool filled = false;
  filled |= types & map::AIRPORT && !airports.isEmpty();
  filled |= types & map::WAYPOINT && !waypoints.isEmpty();
  filled |= types & map::VOR && !vors.isEmpty();
  filled |= types & map::NDB && !ndbs.isEmpty();
  filled |= types & map::AIRWAY && !airways.isEmpty();
  filled |= types & map::RUNWAYEND && !runwayEnds.isEmpty();
  filled |= types & map::ILS && !ils.isEmpty();
  filled |= types & map::USER && !userPoints.isEmpty();
  return !filled;
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
  QString type = vor.type.at(0);

  if(vor.dmeOnly)
    return QObject::tr("DME (%1)").arg(type);
  else if(vor.hasDme)
    return QObject::tr("VORDME (%1)").arg(type);
  else
    return QObject::tr("VOR (%1)").arg(type);
}

QString ndbFullShortText(const MapNdb& ndb)
{
  QString type = ndb.type == "CP" ? QObject::tr("CL") : ndb.type;
  return QObject::tr("NDB (%1)").arg(type);
}

QString airspaceTypeToString(map::MapAirspaceTypes type)
{
  return airspaceTypeNameMap.value(type);
}

QString airspaceRemark(map::MapAirspaceTypes type)
{
  return airspaceRemarkMap.value(type);
}

int airspaceDrawingOrder(map::MapAirspaceTypes type)
{
  return airspacePriorityMap.value(type);
}

map::MapAirspaceTypes airspaceTypeFromDatabase(const QString& type)
{
  return airspaceTypeFromDatabaseMap.value(type);
}

QString airspaceTypeToDatabase(map::MapAirspaceTypes type)
{
  return airspaceTypeToDatabaseMap.value(type);
}

} // namespace types
