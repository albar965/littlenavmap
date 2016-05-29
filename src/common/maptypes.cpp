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
#include "common/formatter.h"
#include "atools.h"

#include <QDataStream>
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

const QHash<QString, QString> navTypeNames(
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

const QHash<QString, QString> navNames(
  {
    {"VORDME", "VORDME"},
    {"VOR", "VOR"},
    {"DME", "DME"},
    {"NDB", "NDB"},
    {"WAYPOINT", "Waypoint"}
  });

const QHash<QString, QString> comTypeNames(
  {
    {"NONE", "None"},
    {"ATIS", "ATIS"},
    {"MULTICOM", "Multicom"},
    {"UNICOM", "Unicom"},
    {"CTAF", "CTAF"},
    {"GROUND", "Ground"},
    {"TOWER", "Tower"},
    {"CLEARANCE", "Clearance"},
    {"APPROACH", "Approach"},
    {"DEPARTURE", "Departure"},
    {"CENTER", "Center"},
    {"FSS", "FSS"},
    {"AWOS", "AWOS"},
    {"ASOS", "ASOS"},
    {"CLEARANCE_PRE_TAXI", "Clearance pre Taxi"},
    {"REMOTE_CLEARANCE_DELIVERY", "Remote Clearance Delivery"}
  });

int qHash(const maptypes::MapObjectRef& type)
{
  return type.id ^ type.type;
}

QString navTypeName(const QString& type)
{
  return navTypeNames.value(type);
}

QString navName(const QString& type)
{
  return navNames.value(type);
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

QString parkingTypeName(const QString& type)
{
  return parkingTypeMap.value(type);
}

QString parkingName(const QString& name)
{
  return parkingNameMap.value(name);
}

QString parkingNameForFlightplan(const maptypes::MapParking& parking)
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
  if(addon() && objectTypes.testFlag(maptypes::AIRPORT_ADDON))
    return true;

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
      return "Victor";

    case maptypes::JET:
      return "Jet";

    case maptypes::BOTH:
      return "Both";

  }
  return QString();
}

MapAirwayType airwayTypeFromString(const QString& typeStr)
{
  if(typeStr.startsWith("V"))
    return maptypes::VICTOR;
  else if(typeStr.startsWith("J"))
    return maptypes::JET;
  else if(typeStr.startsWith("B"))
    return maptypes::BOTH;
  else
    return maptypes::NO_AIRWAY;
}

QDataStream& operator>>(QDataStream& dataStream, maptypes::RangeMarker& obj)
{
  dataStream >> obj.text >> obj.ranges;

  float lon, lat;
  dataStream >> lon >> lat;
  obj.center = atools::geo::Pos(lon, lat);

  qint32 t;
  dataStream >> t;
  obj.type = static_cast<maptypes::MapObjectTypes>(t);
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const maptypes::RangeMarker& obj)
{
  dataStream << obj.text
  << obj.ranges
  << obj.center.getLonX() << obj.center.getLatY()
  << static_cast<qint32>(obj.type);
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, maptypes::DistanceMarker& obj)
{
  dataStream >> obj.text >> obj.color;

  float lon, lat;
  dataStream >> lon >> lat;
  obj.from = atools::geo::Pos(lon, lat);
  dataStream >> lon >> lat;
  obj.to = atools::geo::Pos(lon, lat);

  dataStream >> obj.magvar;
  qint32 t;
  dataStream >> t;
  obj.type = static_cast<maptypes::MapObjectTypes>(t);
  dataStream >> obj.rhumbLine >> obj.hasMagvar;

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const maptypes::DistanceMarker& obj)
{
  dataStream << obj.text << obj.color
  << obj.from.getLonX() << obj.from.getLatY() << obj.to.getLonX() << obj.to.getLatY();

  dataStream << obj.magvar << static_cast<qint32>(obj.type) << obj.rhumbLine << obj.hasMagvar;

  return dataStream;
}

QString vorType(const MapVor& vor)
{
  QString type;
  if(vor.dmeOnly)
    return "DME";
  else if(vor.hasDme)
    return "VORDME";
  else
    return "VOR";
}

QString vorText(const MapVor& vor)
{
  return vorType(vor) + " " + atools::capString(vor.name) + " (" + vor.ident + ")";
}

QString ndbText(const MapNdb& ndb)
{
  return "NDB " + atools::capString(ndb.name) + " (" + ndb.ident + ")";
}

QString waypointText(const MapWaypoint& waypoint)
{
  return "Waypoint " + waypoint.ident;
}

QString userpointText(const MapUserpoint& userpoint)
{
  return "User point " + userpoint.name;
}

QString airportText(const MapAirport& airport)
{
  return "Airport " + airport.name + " (" + airport.ident + ")";
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

} // namespace types
