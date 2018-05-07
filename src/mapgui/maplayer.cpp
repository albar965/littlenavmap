/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "mapgui/maplayer.h"

#include "maplayer.h"

MapLayer::MapLayer(float maximumRange)
{
  maxRange = maximumRange;
}

MapLayer MapLayer::clone(float maximumRange) const
{
  MapLayer retval = *this;
  retval.maxRange = maximumRange;
  return retval;
}

bool MapLayer::hasSameQueryParametersAirport(const MapLayer *other) const
{
  // Either the source query changes or minimum runway length which need a new query
  return src == other->src && layerMinRunwayLength == other->layerMinRunwayLength;
}

bool MapLayer::hasSameQueryParametersAirspace(const MapLayer *other) const
{
  // Or any airspace parameter which needs a new query
  return layerAirspaceCenter == other->layerAirspaceCenter &&
         layerAirspaceIcao == other->layerAirspaceIcao &&
         layerAirspaceFir == other->layerAirspaceFir &&
         layerAirspaceRestricted == other->layerAirspaceRestricted &&
         layerAirspaceSpecial == other->layerAirspaceSpecial &&
         layerAirspaceOther == other->layerAirspaceOther;
}

bool MapLayer::hasSameQueryParametersAirway(const MapLayer *other) const
{
  return layerAirway == other->layerAirway;
}

bool MapLayer::hasSameQueryParametersVor(const MapLayer *other) const
{
  return layerVor == other->layerVor;
}

bool MapLayer::hasSameQueryParametersNdb(const MapLayer *other) const
{
  return layerNdb == other->layerNdb;
}

bool MapLayer::hasSameQueryParametersWaypoint(const MapLayer *other) const
{
  return layerWaypoint == other->layerWaypoint;
}

bool MapLayer::hasSameQueryParametersMarker(const MapLayer *other) const
{
  return layerMarker == other->layerMarker;
}

bool MapLayer::hasSameQueryParametersIls(const MapLayer *other) const
{
  return layerIls == other->layerIls;
}

MapLayer& MapLayer::airport(bool value)
{
  layerAirport = value;
  return *this;
}

MapLayer& MapLayer::approach(bool value)
{
  layerApproach = value;
  return *this;
}

MapLayer& MapLayer::approachTextAndDetail(bool value)
{
  layerApproachTextAndDetail = value;
  return *this;
}

MapLayer& MapLayer::airportSource(layer::AirportSource source)
{
  src = source;
  return *this;
}

MapLayer& MapLayer::airportOverviewRunway(bool value)
{
  layerAirportOverviewRunway = value;
  return *this;
}

MapLayer& MapLayer::airportDiagram(bool value)
{
  layerAirportDiagram = value;
  return *this;
}

MapLayer& MapLayer::airportDiagramDetail(bool value)
{
  layerAirportDiagramDetail = value;
  return *this;
}

MapLayer& MapLayer::airportDiagramDetail2(bool value)
{
  layerAirportDiagramDetail2 = value;
  return *this;
}

MapLayer& MapLayer::airportDiagramDetail3(bool value)
{
  layerAirportDiagramDetail3 = value;
  return *this;
}

MapLayer& MapLayer::airportSoft(bool value)
{
  layerAirportSoft = value;
  return *this;
}

MapLayer& MapLayer::airportNoRating(bool value)
{
  layerAirportNoRating = value;
  return *this;
}

MapLayer& MapLayer::airportSymbolSize(int size)
{
  layerAirportSymbolSize = size;
  return *this;
}

MapLayer& MapLayer::airportIdent(bool value)
{
  layerAirportIdent = value;
  return *this;
}

MapLayer& MapLayer::airportName(bool value)
{
  layerAirportName = value;
  return *this;
}

MapLayer& MapLayer::airportInfo(bool value)
{
  layerAirportInfo = value;
  return *this;
}

MapLayer& MapLayer::airportRouteInfo(bool value)
{
  layerAirportRouteInfo = value;
  return *this;
}

MapLayer& MapLayer::minRunwayLength(int length)
{
  layerMinRunwayLength = length;
  return *this;
}

bool MapLayer::operator<(const MapLayer& other) const
{
  return maxRange < other.maxRange;
}

MapLayer& MapLayer::waypoint(bool value)
{
  layerWaypoint = value;
  return *this;
}

MapLayer& MapLayer::waypointName(bool value)
{
  layerWaypointName = value;
  return *this;
}

MapLayer& MapLayer::waypointRouteName(bool value)
{
  layerWaypointRouteName = value;
  return *this;
}

MapLayer& MapLayer::vor(bool value)
{
  layerVor = value;
  return *this;
}

MapLayer& MapLayer::vorIdent(bool value)
{
  layerVorIdent = value;
  return *this;
}

MapLayer& MapLayer::vorInfo(bool value)
{
  layerVorInfo = value;
  return *this;
}

MapLayer& MapLayer::vorRouteIdent(bool value)
{
  layerVorRouteIdent = value;
  return *this;
}

MapLayer& MapLayer::vorRouteInfo(bool value)
{
  layerVorRouteInfo = value;
  return *this;
}

MapLayer& MapLayer::vorLarge(bool value)
{
  layerVorLarge = value;
  return *this;
}

MapLayer& MapLayer::ndb(bool value)
{
  layerNdb = value;
  return *this;
}

MapLayer& MapLayer::ndbIdent(bool value)
{
  layerNdbIdent = value;
  return *this;
}

MapLayer& MapLayer::ndbInfo(bool value)
{
  layerNdbInfo = value;
  return *this;
}

MapLayer& MapLayer::ndbRouteIdent(bool value)
{
  layerNdbRouteIdent = value;
  return *this;
}

MapLayer& MapLayer::ndbRouteInfo(bool value)
{
  layerNdbRouteInfo = value;
  return *this;
}

MapLayer& MapLayer::marker(bool value)
{
  layerMarker = value;
  return *this;
}

MapLayer& MapLayer::markerInfo(bool value)
{
  layerMarkerInfo = value;
  return *this;
}

MapLayer& MapLayer::ils(bool value)
{
  layerIls = value;
  return *this;
}

MapLayer& MapLayer::ilsIdent(bool value)
{
  layerIlsIdent = value;
  return *this;
}

MapLayer& MapLayer::ilsInfo(bool value)
{
  layerIlsInfo = value;
  return *this;
}

MapLayer& MapLayer::airway(bool value)
{
  layerAirway = value;
  return *this;
}

MapLayer& MapLayer::airwayWaypoint(bool value)
{
  layerAirwayWaypoint = value;
  return *this;
}

MapLayer& MapLayer::airwayIdent(bool value)
{
  layerAirwayIdent = value;
  return *this;
}

MapLayer& MapLayer::airwayInfo(bool value)
{
  layerAirwayInfo = value;
  return *this;
}

MapLayer& MapLayer::airspaceCenter(bool value)
{
  layerAirspaceCenter = value;
  return *this;
}

MapLayer& MapLayer::airspaceIcao(bool value)
{
  layerAirspaceIcao = value;
  return *this;
}

MapLayer& MapLayer::airspaceFir(bool value)
{
  layerAirspaceFir = value;
  return *this;
}

MapLayer& MapLayer::airspaceRestricted(bool value)
{
  layerAirspaceRestricted = value;
  return *this;
}

MapLayer& MapLayer::airspaceSpecial(bool value)
{
  layerAirspaceSpecial = value;
  return *this;
}

MapLayer& MapLayer::airspaceOther(bool value)
{
  layerAirspaceOther = value;
  return *this;
}

MapLayer& MapLayer::aiAircraftLarge(bool value)
{
  layerAiAircraftLarge = value;
  return *this;
}

MapLayer& MapLayer::aiAircraftGround(bool value)
{
  layerAiAircraftGround = value;
  return *this;
}

MapLayer& MapLayer::aiAircraftSmall(bool value)
{
  layerAiAircraftSmall = value;
  return *this;
}

MapLayer& MapLayer::aiShipLarge(bool value)
{
  layerAiShipLarge = value;
  return *this;
}

MapLayer& MapLayer::aiShipSmall(bool value)
{
  layerAiShipSmall = value;
  return *this;
}

MapLayer& MapLayer::aiAircraftGroundText(bool value)
{
  layerAiAircraftGroundText = value;
  return *this;
}

MapLayer& MapLayer::aiAircraftText(bool value)
{
  layerAiAircraftText = value;
  return *this;
}

MapLayer& MapLayer::onlineAircraft(bool value)
{
  layerOnlineAircraft = value;
  return *this;
}

MapLayer& MapLayer::onlineAircraftText(bool value)
{
  layerOnlineAircraftText = value;
  return *this;
}

MapLayer& MapLayer::airportMaxTextLength(int size)
{
  maximumTextLengthAirport = size;
  return *this;
}

MapLayer& MapLayer::waypointSymbolSize(int size)
{
  layerWaypointSymbolSize = size;
  return *this;
}

MapLayer& MapLayer::userpoint(bool value)
{
  layerUserpoint = value;
  return *this;
}

MapLayer& MapLayer::userpointInfo(bool value)
{
  layerUserpointInfo = value;
  return *this;
}

MapLayer& MapLayer::userpoinSymbolSize(int size)
{
  layerUserpointSymbolSize = size;
  return *this;
}

MapLayer& MapLayer::userpointMaxTextLength(int length)
{
  maximumTextLengthUserpoint = length;
  return *this;
}

MapLayer& MapLayer::vorSymbolSize(int size)
{
  layerVorSymbolSize = size;
  return *this;
}

MapLayer& MapLayer::ndbSymbolSize(int size)
{
  layerNdbSymbolSize = size;
  return *this;
}

MapLayer& MapLayer::markerSymbolSize(int size)
{
  layerMarkerSymbolSize = size;
  return *this;
}

QDebug operator<<(QDebug out, const MapLayer& record)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote() << "MapLayer[" << record.maxRange << "]";

  return out;
}
