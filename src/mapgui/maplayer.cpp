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

#include "mapgui/maplayer.h"

#include "util/xmlstream.h"
#include "mapgui/maplayer.h"

#include <QXmlStreamReader>

MapLayer::MapLayer(float maximumRangeKm)
{
  maxRange = maximumRangeKm;
}

MapLayer MapLayer::clone(float maximumRangeKm) const
{
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << QString("    <Layer MaximumRangeKm=\"%1\">").arg(maximumRangeKm) << endl;
#endif
  MapLayer retval = *this;
  retval.maxRange = maximumRangeKm;
  return retval;
}

bool MapLayer::hasSameQueryParametersAirport(const MapLayer *other) const
{
  return layerMinRunwayLength == other->layerMinRunwayLength;
}

bool MapLayer::hasSameQueryParametersAirspace(const MapLayer *other) const
{
  // Or any airspace parameter which needs a new query
  return layerAirspaceCenter == other->layerAirspaceCenter &&
         layerAirspaceIcao == other->layerAirspaceIcao &&
         layerAirspaceFg == other->layerAirspaceFg &&
         layerAirspaceFirUir == other->layerAirspaceFirUir &&
         layerAirspaceRestricted == other->layerAirspaceRestricted &&
         layerAirspaceSpecial == other->layerAirspaceSpecial &&
         layerAirspaceOther == other->layerAirspaceOther;
}

bool MapLayer::hasSameQueryParametersAirwayTrack(const MapLayer *other) const
{
  return layerAirway == other->layerAirway && layerTrack == other->layerTrack;
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

bool MapLayer::hasSameQueryParametersWind(const MapLayer *other) const
{
  return layerWindBarbs == other->layerWindBarbs;
}

bool MapLayer::hasSameQueryParametersMarker(const MapLayer *other) const
{
  return layerMarker == other->layerMarker;
}

bool MapLayer::hasSameQueryParametersIls(const MapLayer *other) const
{
  return layerIls == other->layerIls;
}

bool MapLayer::hasSameQueryParametersHolding(const MapLayer *other) const
{
  return layerHolding == other->layerHolding;
}

bool MapLayer::hasSameQueryParametersAirportMsa(const MapLayer *other) const
{
  return layerAirportMsa == other->layerAirportMsa;
}

MapLayer& MapLayer::airport(bool value)
{
  layerAirport = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Airport>" << layerAirport << "</Airport>";
#endif
  return *this;
}

MapLayer& MapLayer::approach(bool value)
{
  layerApproach = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Approach>" << layerApproach << "</Approach>";
#endif
  return *this;
}

MapLayer& MapLayer::approachDetail(bool value)
{
  layerApproachDetail = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <ApproachDetail>" << layerApproachDetail << "</ApproachDetail>";
#endif
  return *this;
}

MapLayer& MapLayer::approachText(bool value)
{
  layerApproachText = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <ApproachText>" << layerApproachText << "</ApproachText>";
#endif
  return *this;
}

MapLayer& MapLayer::approachTextDetail(bool value)
{
  layerApproachTextDetail = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <ApproachTextDetail>" << layerApproachTextDetail << "</ApproachTextDetail>";
#endif
  return *this;
}

MapLayer& MapLayer::routeTextAndDetail(bool value)
{
  layerRouteTextAndDetail = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <RouteTextAndDetail>" << layerRouteTextAndDetail << "</RouteTextAndDetail>";
#endif
  return *this;
}

MapLayer& MapLayer::airportOverviewRunway(bool value)
{
  layerAirportOverviewRunway = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportOverviewRunway>" << layerAirportOverviewRunway << "</AirportOverviewRunway>";
#endif
  return *this;
}

MapLayer& MapLayer::airportDiagram(bool value)
{
  layerAirportDiagram = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportDiagram>" << layerAirportDiagram << "</AirportDiagram>";
#endif
  return *this;
}

MapLayer& MapLayer::airportDiagramRunway(bool value)
{
  layerAirportDiagramRunway = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportDiagramRunway>" << layerAirportDiagramRunway << "</AirportDiagramRunway>";
#endif
  return *this;
}

MapLayer& MapLayer::airportDiagramDetail(bool value)
{
  layerAirportDiagramDetail = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportDiagramDetail>" << layerAirportDiagramDetail << "</AirportDiagramDetail>";
#endif
  return *this;
}

MapLayer& MapLayer::airportDiagramDetail2(bool value)
{
  layerAirportDiagramDetail2 = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportDiagramDetail2>" << layerAirportDiagramDetail2 << "</AirportDiagramDetail2>";
#endif
  return *this;
}

MapLayer& MapLayer::airportDiagramDetail3(bool value)
{
  layerAirportDiagramDetail3 = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportDiagramDetail3>" << layerAirportDiagramDetail3 << "</AirportDiagramDetail3>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinor(bool value)
{
  layerAirportMinor = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMinor>" << layerAirportMinor << "</AirportMinor>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinorSymbolSize(int size)
{
  layerAirportMinorSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMinorSymbolSize>" << layerAirportMinorSymbolSize << "</AirportMinorSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinorFontScale(float scale)
{
  layerAirportMinorFontScale = scale;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMinorFontScale>" << layerAirportMinorFontScale << "</AirportMinorFontScale>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinorIdent(bool value)
{
  layerAirportMinorIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMinorIdent>" << layerAirportMinorIdent << "</AirportMinorIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinorName(bool value)
{
  layerAirportMinorName = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMinorName>" << layerAirportMinorName << "</AirportMinorName>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinorMaxTextLength(int size)
{
  maximumTextLengthAirportMinor = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <MaximumTextLengthAirportMinor>" << maximumTextLengthAirportMinor <<
    "</MaximumTextLengthAirportMinor>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMinorInfo(bool value)
{
  layerAirportMinorInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMinorInfo>" << layerAirportMinorInfo << "</AirportMinorInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::airportNoRating(bool value)
{
  layerAirportNoRating = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportNoRating>" << layerAirportNoRating << "</AirportNoRating>";
#endif
  return *this;
}

MapLayer& MapLayer::airportSymbolSize(int size)
{
  layerAirportSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportSymbolSize>" << layerAirportSymbolSize << "</AirportSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::airportFontScale(float scale)
{
  layerAirportFontScale = scale;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportFontScale>" << layerAirportFontScale << "</AirportFontScale>";
#endif
  return *this;
}

MapLayer& MapLayer::airportIdent(bool value)
{
  layerAirportIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportIdent>" << layerAirportIdent << "</AirportIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::airportName(bool value)
{
  layerAirportName = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportName>" << layerAirportName << "</AirportName>";
#endif
  return *this;
}

MapLayer& MapLayer::airportInfo(bool value)
{
  layerAirportInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportInfo>" << layerAirportInfo << "</AirportInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::airportRouteInfo(bool value)
{
  layerAirportRouteInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportRouteInfo>" << layerAirportRouteInfo << "</AirportRouteInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::minRunwayLength(int length)
{
  layerMinRunwayLength = length;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <MinRunwayLength>" << layerMinRunwayLength << "</MinRunwayLength>";
#endif
  return *this;
}

MapLayer& MapLayer::waypoint(bool value)
{
  layerWaypoint = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Waypoint>" << layerWaypoint << "</Waypoint>";
#endif
  return *this;
}

MapLayer& MapLayer::waypointName(bool value)
{
  layerWaypointName = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <WaypointName>" << layerWaypointName << "</WaypointName>";
#endif
  return *this;
}

MapLayer& MapLayer::waypointRouteName(bool value)
{
  layerWaypointRouteName = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <WaypointRouteName>" << layerWaypointRouteName << "</WaypointRouteName>";
#endif
  return *this;
}

MapLayer& MapLayer::vor(bool value)
{
  layerVor = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Vor>" << layerVor << "</Vor>";
#endif
  return *this;
}

MapLayer& MapLayer::vorIdent(bool value)
{
  layerVorIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <VorIdent>" << layerVorIdent << "</VorIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::vorInfo(bool value)
{
  layerVorInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <VorInfo>" << layerVorInfo << "</VorInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::vorRouteIdent(bool value)
{
  layerVorRouteIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <VorRouteIdent>" << layerVorRouteIdent << "</VorRouteIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::vorRouteInfo(bool value)
{
  layerVorRouteInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <VorRouteInfo>" << layerVorRouteInfo << "</VorRouteInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::vorLarge(bool value)
{
  layerVorLarge = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <VorLarge>" << layerVorLarge << "</VorLarge>";
#endif
  return *this;
}

MapLayer& MapLayer::ndb(bool value)
{
  layerNdb = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Ndb>" << layerNdb << "</Ndb>";
#endif
  return *this;
}

MapLayer& MapLayer::ndbIdent(bool value)
{
  layerNdbIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <NdbIdent>" << layerNdbIdent << "</NdbIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::ndbInfo(bool value)
{
  layerNdbInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <NdbInfo>" << layerNdbInfo << "</NdbInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::ndbRouteIdent(bool value)
{
  layerNdbRouteIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <NdbRouteIdent>" << layerNdbRouteIdent << "</NdbRouteIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::ndbRouteInfo(bool value)
{
  layerNdbRouteInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <NdbRouteInfo>" << layerNdbRouteInfo << "</NdbRouteInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::marker(bool value)
{
  layerMarker = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Marker>" << layerMarker << "</Marker>";
#endif
  return *this;
}

MapLayer& MapLayer::markerInfo(bool value)
{
  layerMarkerInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <MarkerInfo>" << layerMarkerInfo << "</MarkerInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::ils(bool value)
{
  layerIls = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Ils>" << layerIls << "</Ils>";
#endif
  return *this;
}

MapLayer& MapLayer::ilsIdent(bool value)
{
  layerIlsIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <IlsIdent>" << layerIlsIdent << "</IlsIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::ilsInfo(bool value)
{
  layerIlsInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <IlsInfo>" << layerIlsInfo << "</IlsInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::airway(bool value)
{
  layerAirway = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Airway>" << layerAirway << "</Airway>";
#endif
  return *this;
}

MapLayer& MapLayer::airwayWaypoint(bool value)
{
  layerAirwayWaypoint = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirwayWaypoint>" << layerAirwayWaypoint << "</AirwayWaypoint>";
#endif
  return *this;
}

MapLayer& MapLayer::airwayIdent(bool value)
{
  layerAirwayIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirwayIdent>" << layerAirwayIdent << "</AirwayIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::airwayInfo(bool value)
{
  layerAirwayInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirwayInfo>" << layerAirwayInfo << "</AirwayInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::track(bool value)
{
  layerTrack = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Track>" << layerTrack << "</Track>";
#endif
  return *this;
}

MapLayer& MapLayer::trackWaypoint(bool value)
{
  layerTrackWaypoint = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <TrackWaypoint>" << layerTrackWaypoint << "</TrackWaypoint>";
#endif
  return *this;
}

MapLayer& MapLayer::trackIdent(bool value)
{
  layerTrackIdent = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <TrackIdent>" << layerTrackIdent << "</TrackIdent>";
#endif
  return *this;
}

MapLayer& MapLayer::trackInfo(bool value)
{
  layerTrackInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <TrackInfo>" << layerTrackInfo << "</TrackInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceCenter(bool value)
{
  layerAirspaceCenter = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceCenter>" << layerAirspaceCenter << "</AirspaceCenter>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceIcao(bool value)
{
  layerAirspaceIcao = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceIcao>" << layerAirspaceIcao << "</AirspaceIcao>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceFg(bool value)
{
  layerAirspaceFg = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceFg>" << layerAirspaceFg << "</AirspaceFg>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceFirUir(bool value)
{
  layerAirspaceFirUir = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceFirUir>" << layerAirspaceFirUir << "</AirspaceFirUir>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceRestricted(bool value)
{
  layerAirspaceRestricted = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceRestricted>" << layerAirspaceRestricted << "</AirspaceRestricted>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceSpecial(bool value)
{
  layerAirspaceSpecial = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceSpecial>" << layerAirspaceSpecial << "</AirspaceSpecial>";
#endif
  return *this;
}

MapLayer& MapLayer::airspaceOther(bool value)
{
  layerAirspaceOther = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirspaceOther>" << layerAirspaceOther << "</AirspaceOther>";
#endif
  return *this;
}

MapLayer& MapLayer::aiAircraftLarge(bool value)
{
  layerAiAircraftLarge = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiAircraftLarge>" << layerAiAircraftLarge << "</AiAircraftLarge>";
#endif
  return *this;
}

MapLayer& MapLayer::aiAircraftGround(bool value)
{
  layerAiAircraftGround = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiAircraftGround>" << layerAiAircraftGround << "</AiAircraftGround>";
#endif
  return *this;
}

MapLayer& MapLayer::aiAircraftSmall(bool value)
{
  layerAiAircraftSmall = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiAircraftSmall>" << layerAiAircraftSmall << "</AiAircraftSmall>";
#endif
  return *this;
}

MapLayer& MapLayer::aiShipLarge(bool value)
{
  layerAiShipLarge = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiShipLarge>" << layerAiShipLarge << "</AiShipLarge>";
#endif
  return *this;
}

MapLayer& MapLayer::aiShipSmall(bool value)
{
  layerAiShipSmall = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiShipSmall>" << layerAiShipSmall << "</AiShipSmall>";
#endif
  return *this;
}

MapLayer& MapLayer::aiAircraftGroundText(bool value)
{
  layerAiAircraftGroundText = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiAircraftGroundText>" << layerAiAircraftGroundText << "</AiAircraftGroundText>";
#endif
  return *this;
}

MapLayer& MapLayer::aiAircraftText(bool value)
{
  layerAiAircraftText = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiAircraftText>" << layerAiAircraftText << "</AiAircraftText>";
#endif
  return *this;
}

MapLayer& MapLayer::onlineAircraft(bool value)
{
  layerOnlineAircraft = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <OnlineAircraft>" << layerOnlineAircraft << "</OnlineAircraft>";
#endif
  return *this;
}

MapLayer& MapLayer::onlineAircraftText(bool value)
{
  layerOnlineAircraftText = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <OnlineAircraftText>" << layerOnlineAircraftText << "</OnlineAircraftText>";
#endif
  return *this;
}

MapLayer& MapLayer::mora(bool value)
{
  layerMora = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Mora>" << layerMora << "</Mora>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMaxTextLength(int size)
{
  maximumTextLengthAirport = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <MaximumTextLengthAirport>" << maximumTextLengthAirport << "</MaximumTextLengthAirport>";
#endif
  return *this;
}

MapLayer& MapLayer::airportWeather(bool value)
{
  layerAirportWeather = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportWeather>" << layerAirportWeather << "</AirportWeather>";
#endif
  return *this;
}

MapLayer& MapLayer::windBarbs(bool value)
{
  layerWindBarbs = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <WindBarbs>" << layerWindBarbs << "</WindBarbs>";
#endif
  return *this;
}

MapLayer& MapLayer::windBarbsSymbolSize(int size)
{
  layerWindBarbsSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <WindBarbsSymbolSize>" << layerWindBarbsSymbolSize << "</WindBarbsSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::aiAircraftSize(int value)
{
  layerAiAircraftSize = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AiAircraftSize>" << layerAiAircraftSize << "</AiAircraftSize>";
#endif
  return *this;
}

MapLayer& MapLayer::airportWeatherDetails(bool value)
{
  layerAirportWeatherDetails = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportWeatherDetails>" << layerAirportWeatherDetails << "</AirportWeatherDetails>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMsa(bool value)
{
  layerAirportMsa = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMsa>" << layerAirportMsa << "</AirportMsa>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMsaDetails(bool value)
{
  layerAirportMsaDetails = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMsaDetails>" << layerAirportMsaDetails << "</AirportMsaDetails>";
#endif
  return *this;
}

MapLayer& MapLayer::airportMsaSymbolScale(float scale)
{
  layerAirportMsaSymbolScale = scale;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <AirportMsaSymbolScale>" << layerAirportMsaSymbolScale << "</AirportMsaSymbolScale>";
#endif
  return *this;
}

MapLayer& MapLayer::waypointSymbolSize(int size)
{
  layerWaypointSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <WaypointSymbolSize>" << layerWaypointSymbolSize << "</WaypointSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::userpoint(bool value)
{
  layerUserpoint = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Userpoint>" << layerUserpoint << "</Userpoint>";
#endif
  return *this;
}

MapLayer& MapLayer::userpointInfo(bool value)
{
  layerUserpointInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <UserpointInfo>" << layerUserpointInfo << "</UserpointInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::userpointSymbolSize(int size)
{
  layerUserpointSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <UserpointSymbolSize>" << layerUserpointSymbolSize << "</UserpointSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::userpointMaxTextLength(int length)
{
  maximumTextLengthUserpoint = length;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <MaximumTextLengthUserpoint>" << maximumTextLengthUserpoint << "</MaximumTextLengthUserpoint>";
#endif
  return *this;
}

MapLayer& MapLayer::vorSymbolSize(int size)
{
  layerVorSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <VorSymbolSize>" << layerVorSymbolSize << "</VorSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::ndbSymbolSize(int size)
{
  layerNdbSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <NdbSymbolSize>" << layerNdbSymbolSize << "</NdbSymbolSize>";
#endif
  return *this;
}

MapLayer& MapLayer::holding(bool value)
{
  layerHolding = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <Holding>" << layerHolding << "</Holding>";
#endif
  return *this;
}

MapLayer& MapLayer::holdingInfo(bool value)
{
  layerHoldingInfo = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <HoldingInfo>" << layerHoldingInfo << "</HoldingInfo>";
#endif
  return *this;
}

MapLayer& MapLayer::holdingInfo2(bool value)
{
  layerHoldingInfo2 = value;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <HoldingInfo2>" << layerHoldingInfo2 << "</HoldingInfo2>";
#endif
  return *this;
}

MapLayer& MapLayer::markerSymbolSize(int size)
{
  layerMarkerSymbolSize = size;
#ifdef DEBUG_PRINT_LAYERS
  qInfo().noquote().nospace() << "      <MarkerSymbolSize>" << layerMarkerSymbolSize << "</MarkerSymbolSize>";
#endif
  return *this;
}

bool MapLayer::operator<(const MapLayer& other) const
{
  return maxRange < other.maxRange;
}

void MapLayer::loadFromXml(atools::util::XmlStream& xmlStream)
{
  QXmlStreamReader& reader = xmlStream.getReader();

  while(xmlStream.readNextStartElement())
  {
    if(reader.name() == "MinRunwayLength")
      layerMinRunwayLength = xmlStream.readElementTextInt();
    else if(reader.name() == "MaxRange")
      maxRange = xmlStream.readElementTextInt();
    else if(reader.name() == "AiAircraftGround")
      layerAiAircraftGround = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftGroundText")
      layerAiAircraftGroundText = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftLarge")
      layerAiAircraftLarge = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftSize")
      layerAiAircraftSize = xmlStream.readElementTextInt();
    else if(reader.name() == "AiAircraftSmall")
      layerAiAircraftSmall = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftText")
      layerAiAircraftText = xmlStream.readElementTextBool();
    else if(reader.name() == "AiShipLarge")
      layerAiShipLarge = xmlStream.readElementTextBool();
    else if(reader.name() == "AiShipSmall")
      layerAiShipSmall = xmlStream.readElementTextBool();
    else if(reader.name() == "Airport")
      layerAirport = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagram")
      layerAirportDiagram = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramDetail")
      layerAirportDiagramDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramDetail2")
      layerAirportDiagramDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramDetail3")
      layerAirportDiagramDetail3 = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramRunway")
      layerAirportDiagramRunway = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportIdent")
      layerAirportIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportInfo")
      layerAirportInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMsa")
      layerAirportMsa = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMsaDetails")
      layerAirportMsaDetails = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMsaSymbolScale")
      layerAirportMsaSymbolScale = xmlStream.readElementTextFloat();
    else if(reader.name() == "AirportName")
      layerAirportName = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportNoRating")
      layerAirportNoRating = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportOverviewRunway")
      layerAirportOverviewRunway = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportRouteInfo")
      layerAirportRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinor")
      layerAirportMinor = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorIdent")
      layerAirportMinorIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorInfo")
      layerAirportMinorInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorName")
      layerAirportMinorName = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorSymbolSize")
      layerAirportMinorSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "AirportSymbolSize")
      layerAirportSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "AirportWeather")
      layerAirportWeather = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportWeatherDetails")
      layerAirportWeatherDetails = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceCenter")
      layerAirspaceCenter = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceFg")
      layerAirspaceFg = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceFirUir")
      layerAirspaceFirUir = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceIcao")
      layerAirspaceIcao = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceOther")
      layerAirspaceOther = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceRestricted")
      layerAirspaceRestricted = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceSpecial")
      layerAirspaceSpecial = xmlStream.readElementTextBool();
    else if(reader.name() == "Airway")
      layerAirway = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayIdent")
      layerAirwayIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayInfo")
      layerAirwayInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayWaypoint")
      layerAirwayWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == "Approach")
      layerApproach = xmlStream.readElementTextBool();
    else if(reader.name() == "ApproachDetail")
      layerApproachDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "ApproachText")
      layerApproachText = xmlStream.readElementTextBool();
    else if(reader.name() == "ApproachTextDetail")
      layerApproachTextDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "Holding")
      layerHolding = xmlStream.readElementTextBool();
    else if(reader.name() == "HoldingInfo")
      layerHoldingInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "HoldingInfo2")
      layerHoldingInfo2 = xmlStream.readElementTextBool();
    else if(reader.name() == "Ils")
      layerIls = xmlStream.readElementTextBool();
    else if(reader.name() == "IlsIdent")
      layerIlsIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "IlsInfo")
      layerIlsInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "Marker")
      layerMarker = xmlStream.readElementTextBool();
    else if(reader.name() == "MarkerInfo")
      layerMarkerInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "MarkerSymbolSize")
      layerMarkerSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "Mora")
      layerMora = xmlStream.readElementTextBool();
    else if(reader.name() == "Ndb")
      layerNdb = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbIdent")
      layerNdbIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbInfo")
      layerNdbInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbRouteIdent")
      layerNdbRouteIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbRouteInfo")
      layerNdbRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbSymbolSize")
      layerNdbSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "OnlineAircraft")
      layerOnlineAircraft = xmlStream.readElementTextBool();
    else if(reader.name() == "OnlineAircraftText")
      layerOnlineAircraftText = xmlStream.readElementTextBool();
    else if(reader.name() == "RouteTextAndDetail")
      layerRouteTextAndDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "Track")
      layerTrack = xmlStream.readElementTextBool();
    else if(reader.name() == "TrackIdent")
      layerTrackIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "TrackInfo")
      layerTrackInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "TrackWaypoint")
      layerTrackWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == "Userpoint")
      layerUserpoint = xmlStream.readElementTextBool();
    else if(reader.name() == "UserpointInfo")
      layerUserpointInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "UserpointSymbolSize")
      layerUserpointSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "Vor")
      layerVor = xmlStream.readElementTextBool();
    else if(reader.name() == "VorIdent")
      layerVorIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "VorInfo")
      layerVorInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "VorLarge")
      layerVorLarge = xmlStream.readElementTextBool();
    else if(reader.name() == "VorRouteIdent")
      layerVorRouteIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "VorRouteInfo")
      layerVorRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "VorSymbolSize")
      layerVorSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "Waypoint")
      layerWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == "WaypointName")
      layerWaypointName = xmlStream.readElementTextBool();
    else if(reader.name() == "WaypointRouteName")
      layerWaypointRouteName = xmlStream.readElementTextBool();
    else if(reader.name() == "WaypointSymbolSize")
      layerWaypointSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "WindBarbs")
      layerWindBarbs = xmlStream.readElementTextBool();
    else if(reader.name() == "WindBarbsSymbolSize")
      layerWindBarbsSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "MaximumTextLengthAirport")
      maximumTextLengthAirport = xmlStream.readElementTextInt();
    else if(reader.name() == "MaximumTextLengthAirportMinor")
      maximumTextLengthAirportMinor = xmlStream.readElementTextInt();
    else if(reader.name() == "MaximumTextLengthUserpoint")
      maximumTextLengthUserpoint = xmlStream.readElementTextInt();
    else if(reader.name() == "AirportFontScale")
      layerAirportFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == "AirportMinorFontScale")
      layerAirportMinorFontScale = xmlStream.readElementTextFloat();
    else
      xmlStream.skipCurrentElement(true /* warn */);
  }
}

QDebug operator<<(QDebug out, const MapLayer& record)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote();
  out << "<Layer>" << endl;
  out << "<AiAircraftGround>" << record.layerAiAircraftGround << "</AiAircraftGround>" << endl;
  out << "<AiAircraftGroundText>" << record.layerAiAircraftGroundText << "</AiAircraftGroundText>" << endl;
  out << "<AiAircraftLarge>" << record.layerAiAircraftLarge << "</AiAircraftLarge>" << endl;
  out << "<AiAircraftSize>" << record.layerAiAircraftSize << "</AiAircraftSize>" << endl;
  out << "<AiAircraftSmall>" << record.layerAiAircraftSmall << "</AiAircraftSmall>" << endl;
  out << "<AiAircraftText>" << record.layerAiAircraftText << "</AiAircraftText>" << endl;
  out << "<AiShipLarge>" << record.layerAiShipLarge << "</AiShipLarge>" << endl;
  out << "<AiShipSmall>" << record.layerAiShipSmall << "</AiShipSmall>" << endl;
  out << "<Airport>" << record.layerAirport << "</Airport>" << endl;
  out << "<AirportDiagram>" << record.layerAirportDiagram << "</AirportDiagram>" << endl;
  out << "<AirportDiagramDetail>" << record.layerAirportDiagramDetail << "</AirportDiagramDetail>" << endl;
  out << "<AirportDiagramDetail2>" << record.layerAirportDiagramDetail2 << "</AirportDiagramDetail2>" << endl;
  out << "<AirportDiagramDetail3>" << record.layerAirportDiagramDetail3 << "</AirportDiagramDetail3>" << endl;
  out << "<AirportDiagramRunway>" << record.layerAirportDiagramRunway << "</AirportDiagramRunway>" << endl;
  out << "<AirportFontScale>" << record.layerAirportFontScale << "</AirportFontScale>" << endl;
  out << "<AirportIdent>" << record.layerAirportIdent << "</AirportIdent>" << endl;
  out << "<AirportInfo>" << record.layerAirportInfo << "</AirportInfo>" << endl;
  out << "<AirportMinor>" << record.layerAirportMinor << "</AirportMinor>" << endl;
  out << "<AirportMinorFontScale>" << record.layerAirportMinorFontScale << "</AirportMinorFontScale>" << endl;
  out << "<AirportMinorIdent>" << record.layerAirportMinorIdent << "</AirportMinorIdent>" << endl;
  out << "<AirportMinorInfo>" << record.layerAirportMinorInfo << "</AirportMinorInfo>" << endl;
  out << "<AirportMinorName>" << record.layerAirportMinorName << "</AirportMinorName>" << endl;
  out << "<AirportMinorSymbolSize>" << record.layerAirportMinorSymbolSize << "</AirportMinorSymbolSize>" << endl;
  out << "<AirportMsa>" << record.layerAirportMsa << "</AirportMsa>" << endl;
  out << "<AirportMsaDetails>" << record.layerAirportMsaDetails << "</AirportMsaDetails>" << endl;
  out << "<AirportMsaSymbolScale>" << record.layerAirportMsaSymbolScale << "</AirportMsaSymbolScale>" << endl;
  out << "<AirportName>" << record.layerAirportName << "</AirportName>" << endl;
  out << "<AirportNoRating>" << record.layerAirportNoRating << "</AirportNoRating>" << endl;
  out << "<AirportOverviewRunway>" << record.layerAirportOverviewRunway << "</AirportOverviewRunway>" << endl;
  out << "<AirportRouteInfo>" << record.layerAirportRouteInfo << "</AirportRouteInfo>" << endl;
  out << "<AirportSymbolSize>" << record.layerAirportSymbolSize << "</AirportSymbolSize>" << endl;
  out << "<AirportWeather>" << record.layerAirportWeather << "</AirportWeather>" << endl;
  out << "<AirportWeatherDetails>" << record.layerAirportWeatherDetails << "</AirportWeatherDetails>" << endl;
  out << "<AirspaceCenter>" << record.layerAirspaceCenter << "</AirspaceCenter>" << endl;
  out << "<AirspaceFg>" << record.layerAirspaceFg << "</AirspaceFg>" << endl;
  out << "<AirspaceFirUir>" << record.layerAirspaceFirUir << "</AirspaceFirUir>" << endl;
  out << "<AirspaceIcao>" << record.layerAirspaceIcao << "</AirspaceIcao>" << endl;
  out << "<AirspaceOther>" << record.layerAirspaceOther << "</AirspaceOther>" << endl;
  out << "<AirspaceRestricted>" << record.layerAirspaceRestricted << "</AirspaceRestricted>" << endl;
  out << "<AirspaceSpecial>" << record.layerAirspaceSpecial << "</AirspaceSpecial>" << endl;
  out << "<Airway>" << record.layerAirway << "</Airway>" << endl;
  out << "<AirwayIdent>" << record.layerAirwayIdent << "</AirwayIdent>" << endl;
  out << "<AirwayInfo>" << record.layerAirwayInfo << "</AirwayInfo>" << endl;
  out << "<AirwayWaypoint>" << record.layerAirwayWaypoint << "</AirwayWaypoint>" << endl;
  out << "<Approach>" << record.layerApproach << "</Approach>" << endl;
  out << "<ApproachDetail>" << record.layerApproachDetail << "</ApproachDetail>" << endl;
  out << "<ApproachText>" << record.layerApproachText << "</ApproachText>" << endl;
  out << "<ApproachTextDetail>" << record.layerApproachTextDetail << "</ApproachTextDetail>" << endl;
  out << "<Holding>" << record.layerHolding << "</Holding>" << endl;
  out << "<HoldingInfo>" << record.layerHoldingInfo << "</HoldingInfo>" << endl;
  out << "<HoldingInfo2>" << record.layerHoldingInfo2 << "</HoldingInfo2>" << endl;
  out << "<Ils>" << record.layerIls << "</Ils>" << endl;
  out << "<IlsIdent>" << record.layerIlsIdent << "</IlsIdent>" << endl;
  out << "<IlsInfo>" << record.layerIlsInfo << "</IlsInfo>" << endl;
  out << "<Marker>" << record.layerMarker << "</Marker>" << endl;
  out << "<MarkerInfo>" << record.layerMarkerInfo << "</MarkerInfo>" << endl;
  out << "<MarkerSymbolSize>" << record.layerMarkerSymbolSize << "</MarkerSymbolSize>" << endl;
  out << "<MaxRange>" << record.maxRange << "</MaxRange>" << endl;
  out << "<MaximumTextLengthAirport>" << record.maximumTextLengthAirport << "</MaximumTextLengthAirport>" << endl;
  out << "<MaximumTextLengthAirportMinor>" << record.maximumTextLengthAirportMinor << "</MaximumTextLengthAirportMinor>" << endl;
  out << "<MaximumTextLengthUserpoint>" << record.maximumTextLengthUserpoint << "</MaximumTextLengthUserpoint>" << endl;
  out << "<MinRunwayLength>" << record.layerMinRunwayLength << "</MinRunwayLength>" << endl;
  out << "<Mora>" << record.layerMora << "</Mora>" << endl;
  out << "<Ndb>" << record.layerNdb << "</Ndb>" << endl;
  out << "<NdbIdent>" << record.layerNdbIdent << "</NdbIdent>" << endl;
  out << "<NdbInfo>" << record.layerNdbInfo << "</NdbInfo>" << endl;
  out << "<NdbRouteIdent>" << record.layerNdbRouteIdent << "</NdbRouteIdent>" << endl;
  out << "<NdbRouteInfo>" << record.layerNdbRouteInfo << "</NdbRouteInfo>" << endl;
  out << "<NdbSymbolSize>" << record.layerNdbSymbolSize << "</NdbSymbolSize>" << endl;
  out << "<OnlineAircraft>" << record.layerOnlineAircraft << "</OnlineAircraft>" << endl;
  out << "<OnlineAircraftText>" << record.layerOnlineAircraftText << "</OnlineAircraftText>" << endl;
  out << "<RouteTextAndDetail>" << record.layerRouteTextAndDetail << "</RouteTextAndDetail>" << endl;
  out << "<Track>" << record.layerTrack << "</Track>" << endl;
  out << "<TrackIdent>" << record.layerTrackIdent << "</TrackIdent>" << endl;
  out << "<TrackInfo>" << record.layerTrackInfo << "</TrackInfo>" << endl;
  out << "<TrackWaypoint>" << record.layerTrackWaypoint << "</TrackWaypoint>" << endl;
  out << "<Userpoint>" << record.layerUserpoint << "</Userpoint>" << endl;
  out << "<UserpointInfo>" << record.layerUserpointInfo << "</UserpointInfo>" << endl;
  out << "<UserpointSymbolSize>" << record.layerUserpointSymbolSize << "</UserpointSymbolSize>" << endl;
  out << "<Vor>" << record.layerVor << "</Vor>" << endl;
  out << "<VorIdent>" << record.layerVorIdent << "</VorIdent>" << endl;
  out << "<VorInfo>" << record.layerVorInfo << "</VorInfo>" << endl;
  out << "<VorLarge>" << record.layerVorLarge << "</VorLarge>" << endl;
  out << "<VorRouteIdent>" << record.layerVorRouteIdent << "</VorRouteIdent>" << endl;
  out << "<VorRouteInfo>" << record.layerVorRouteInfo << "</VorRouteInfo>" << endl;
  out << "<VorSymbolSize>" << record.layerVorSymbolSize << "</VorSymbolSize>" << endl;
  out << "<Waypoint>" << record.layerWaypoint << "</Waypoint>" << endl;
  out << "<WaypointName>" << record.layerWaypointName << "</WaypointName>" << endl;
  out << "<WaypointRouteName>" << record.layerWaypointRouteName << "</WaypointRouteName>" << endl;
  out << "<WaypointSymbolSize>" << record.layerWaypointSymbolSize << "</WaypointSymbolSize>" << endl;
  out << "<WindBarbs>" << record.layerWindBarbs << "</WindBarbs>" << endl;
  out << "<WindBarbsSymbolSize>" << record.layerWindBarbsSymbolSize << "</WindBarbsSymbolSize>" << endl;
  out << "</Layer>" << endl;

  return out;
}
