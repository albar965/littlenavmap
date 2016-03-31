/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "maptypes.h"

#include <QHash>
#include <QObject>

namespace maptypes {

const QHash<QString, QString> surfaceMap(
  {
    {"CONCRETE", QObject::tr("Concrete")},
    {"GRASS", QObject::tr("Grass")},
    {"WATER", QObject::tr("Water")},
    {"ASPHALT", QObject::tr("Asphalt")},
    {"CEMENT", QObject::tr("Cement")},
    {"CLAY", QObject::tr("Clay")},
    {"SNOW", QObject::tr("Snow")},
    {"ICE", QObject::tr("Ice")},
    {"DIRT", QObject::tr("Dirt")},
    {"CORAL", QObject::tr("Coral")},
    {"GRAVEL", QObject::tr("Gravel")},
    {"OIL_TREATED", QObject::tr("Oil treated")},
    {"STEEL_MATS", QObject::tr("Steel Mats")},
    {"BITUMINOUS", QObject::tr("Bituminous")},
    {"BRICK", QObject::tr("Brick")},
    {"MACADAM", QObject::tr("Macadam")},
    {"PLANKS", QObject::tr("Planks")},
    {"SAND", QObject::tr("Sand")},
    {"SHALE", QObject::tr("Shale")},
    {"TARMAC", QObject::tr("Tarmac")},
    {"UNKNOWN", QObject::tr("Unknown")}
  });

const QHash<QString, QString> parkingMapGate(
  {
    {"UNKNOWN", QObject::tr("Unknown")},
    {"RAMP_GA", QObject::tr("Ramp GA")},
    {"RAMP_GA_SMALL", QObject::tr("Ramp GA Small")},
    {"RAMP_GA_MEDIUM", QObject::tr("Ramp GA Medium")},
    {"RAMP_GA_LARGE", QObject::tr("Ramp GA Large")},
    {"RAMP_CARGO", QObject::tr("Ramp Cargo")},
    {"RAMP_MIL_CARGO", QObject::tr("Ramp Mil Cargo")},
    {"RAMP_MIL_COMBAT", QObject::tr("Ramp Mil Combat")},
    {"GATE_SMALL", QObject::tr("Small")},
    {"GATE_MEDIUM", QObject::tr("Medium")},
    {"GATE_HEAVY", QObject::tr("Heavy")},
    {"DOCK_GA", QObject::tr("Dock GA")},
    {"FUEL", QObject::tr("Fuel")},
    {"VEHICLES", QObject::tr("Vehicles")}
  });

const QHash<QString, QString> parkingMapRamp(
  {
    {"UNKNOWN", QObject::tr("Unknown")},
    {"RAMP_GA", QObject::tr("Ramp GA")},
    {"RAMP_GA_SMALL", QObject::tr("Small")},
    {"RAMP_GA_MEDIUM", QObject::tr("Medium")},
    {"RAMP_GA_LARGE", QObject::tr("Large")},
    {"RAMP_CARGO", QObject::tr("Ramp Cargo")},
    {"RAMP_MIL_CARGO", QObject::tr("Ramp Mil Cargo")},
    {"RAMP_MIL_COMBAT", QObject::tr("Ramp Mil Combat")},
    {"GATE_SMALL", QObject::tr("Gate Small")},
    {"GATE_MEDIUM", QObject::tr("Gate Medium")},
    {"GATE_HEAVY", QObject::tr("Gate Heavy")},
    {"DOCK_GA", QObject::tr("Dock GA")},
    {"FUEL", QObject::tr("Fuel")},
    {"VEHICLES", QObject::tr("Vehicles")}
  });

const QHash<QString, QString> parkingTypeMap(
  {
    {"UNKNOWN", QObject::tr("Unknown")},
    {"RAMP_GA", QObject::tr("Ramp GA")},
    {"RAMP_GA_SMALL", QObject::tr("Ramp GA Small")},
    {"RAMP_GA_MEDIUM", QObject::tr("Ramp GA Medium")},
    {"RAMP_GA_LARGE", QObject::tr("Ramp GA Large")},
    {"RAMP_CARGO", QObject::tr("Ramp Cargo")},
    {"RAMP_MIL_CARGO", QObject::tr("Ramp Mil Cargo")},
    {"RAMP_MIL_COMBAT", QObject::tr("Ramp Mil Combat")},
    {"GATE_SMALL", QObject::tr("Gate Small")},
    {"GATE_MEDIUM", QObject::tr("Gate Medium")},
    {"GATE_HEAVY", QObject::tr("Gate Heavy")},
    {"DOCK_GA", QObject::tr("Dock GA")},
    {"FUEL", QObject::tr("Fuel")},
    {"VEHICLES", QObject::tr("Vehicles")}
  });

const QHash<QString, QString> parkingNameMap(
  {
    {"UNKNOWN", QObject::tr("Unknown")},
    {"NO_PARKING", QObject::tr("No Parking")},
    {"PARKING", QObject::tr("Parking")},
    {"N_PARKING", QObject::tr("N Parking")},
    {"NE_PARKING", QObject::tr("NE Parking")},
    {"E_PARKING", QObject::tr("E Parking")},
    {"SE_PARKING", QObject::tr("SE Parking")},
    {"S_PARKING", QObject::tr("S Parking")},
    {"SW_PARKING", QObject::tr("SW Parking")},
    {"W_PARKING", QObject::tr("W Parking")},
    {"NW_PARKING", QObject::tr("NW Parking")},
    {"GATE", QObject::tr("Gate")},
    {"DOCK", QObject::tr("Dock")},
    {"GATE_A", QObject::tr("Gate A")},
    {"GATE_B", QObject::tr("Gate B")},
    {"GATE_C", QObject::tr("Gate C")},
    {"GATE_D", QObject::tr("Gate D")},
    {"GATE_E", QObject::tr("Gate E")},
    {"GATE_F", QObject::tr("Gate F")},
    {"GATE_G", QObject::tr("Gate G")},
    {"GATE_H", QObject::tr("Gate H")},
    {"GATE_I", QObject::tr("Gate I")},
    {"GATE_J", QObject::tr("Gate J")},
    {"GATE_K", QObject::tr("Gate K")},
    {"GATE_L", QObject::tr("Gate L")},
    {"GATE_M", QObject::tr("Gate M")},
    {"GATE_N", QObject::tr("Gate N")},
    {"GATE_O", QObject::tr("Gate O")},
    {"GATE_P", QObject::tr("Gate P")},
    {"GATE_Q", QObject::tr("Gate Q")},
    {"GATE_R", QObject::tr("Gate R")},
    {"GATE_S", QObject::tr("Gate S")},
    {"GATE_T", QObject::tr("Gate T")},
    {"GATE_U", QObject::tr("Gate U")},
    {"GATE_V", QObject::tr("Gate V")},
    {"GATE_W", QObject::tr("Gate W")},
    {"GATE_X", QObject::tr("Gate X")},
    {"GATE_Y", QObject::tr("Gate Y")},
    {"GATE_Z", QObject::tr("Gate Z")}
  });

const QHash<QString, QString> typeNames(
  {
    {"HIGH", "High"},
    {"LOW", "Low"},
    {"TERMINAL", "Terminal"},
    {"HH", "HH"},
    {"H", "H"},
    {"MH", "MH"},
    {"COMPASS_POINT", "Compass Point"},
    {"NAMED", "Named"},
    {"UNNAMED", "Unnamed"},
    {"VOR", "VOR"},
    {"NDB", "NDB"}
  });

const QHash<QString, QString> navTypeNames(
  {
    {"VORDME", "VORDME"},
    {"VOR", "VOR"},
    {"DME", "DME"},
    {"NDB", "NDB"},
    {"WAYPOINT", "Waypoint"}
  });

QString navTypeName(const QString& type)
{
  return typeNames.value(type);
}

QString navName(const QString& type)
{
  return navTypeNames.value(type);
}

QString surfaceName(const QString& surface)
{
  return surfaceMap.value(surface);
}

QString parkingGateName(const QString& gate)
{
  return parkingMapGate.value(gate);
}

QString parkingRampName(const QString& ramp)
{
  return parkingMapRamp.value(ramp);
}

QString parkingTypeName(const QString& ramp)
{
  return parkingTypeMap.value(ramp);
}

QString parkingName(const QString& ramp)
{
  return parkingNameMap.value(ramp);
}

void MapSearchResult::deleteAllObjects()
{
  if(needsDelete)
  {
    qDeleteAll(airports);
    qDeleteAll(towers);
    qDeleteAll(parkings);
    qDeleteAll(helipads);
    qDeleteAll(waypoints);
    qDeleteAll(vors);
    qDeleteAll(ndbs);
    qDeleteAll(markers);
    qDeleteAll(ils);
    qDeleteAll(airways);

    airports.clear();
    airportIds.clear();
    towers.clear();
    parkings.clear();
    helipads.clear();
    waypoints.clear();
    waypointIds.clear();
    vors.clear();
    vorIds.clear();
    ndbs.clear();
    ndbIds.clear();
    markers.clear();
    ils.clear();
    userPoints.clear();
  }
}

bool MapAirport::hard() const
{
  return flags.testFlag(AP_HARD);
}

bool MapAirport::scenery() const
{
  return flags.testFlag(AP_SCENERY);
}

bool MapAirport::tower() const
{
  return flags.testFlag(AP_TOWER);
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

bool MapAirport::isVisible(maptypes::MapObjectTypes objectTypes) const
{
  if(!scenery() && !waterOnly() && !objectTypes.testFlag(maptypes::AIRPORT_EMPTY))
    return false;

  if(hard() && !objectTypes.testFlag(maptypes::AIRPORT_HARD))
    return false;

  if((softOnly() || noRunways()) && !waterOnly() && !objectTypes.testFlag(maptypes::AIRPORT_SOFT))
    return false;

  return true;
}

/* Convert nav_search type */
maptypes::MapObjectTypes navTypeToMapObjectType(const QString& navType)
{
  maptypes::MapObjectTypes type = NONE;
  if(navType == "VOR" || navType == "VORDME" || navType == "DME")
    type = maptypes::VOR;
  else if(navType == "NDB")
    type = maptypes::NDB;
  else if(navType == "WAYPOINT")
    type = maptypes::WAYPOINT;
  return type;
}

QString airwayTypeToShortString(MapAirwayType type)
{
  switch(type)
  {
    case maptypes::NO_AIRWAY:
      break;
    case maptypes::VICTOR:
      return "V";

    case maptypes::JET:
      return "J";

    case maptypes::BOTH:
      return "B";

  }
  return QString();
}

QString airwayTypeToString(MapAirwayType type)
{
  switch(type)
  {
    case maptypes::NO_AIRWAY:
      break;
    case maptypes::VICTOR:
      return "VICTOR";

    case maptypes::JET:
      return "JET";

    case maptypes::BOTH:
      return "BOTH";

  }
  return QString();
}

MapAirwayType airwayTypeFromString(const QString& typeStr)
{
  if(typeStr == "VICTOR")
    return maptypes::VICTOR;
  else if(typeStr == "JET")
    return maptypes::JET;
  else if(typeStr == "BOTH")
    return maptypes::BOTH;
  else
    return maptypes::NO_AIRWAY;
}

} // namespace types
