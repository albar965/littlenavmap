/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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
  MapLayer retval = *this;
  retval.maxRange = maximumRangeKm;
  return retval;
}

bool MapLayer::hasSameQueryParametersAirport(const MapLayer *other) const
{
  return minRunwayLength == other->minRunwayLength;
}

bool MapLayer::hasSameQueryParametersAirspace(const MapLayer *other) const
{
  // Or any airspace parameter which needs a new query
  return airspaceCenter == other->airspaceCenter &&
         airspaceIcao == other->airspaceIcao &&
         airspaceFg == other->airspaceFg &&
         airspaceFirUir == other->airspaceFirUir &&
         airspaceRestricted == other->airspaceRestricted &&
         airspaceSpecial == other->airspaceSpecial &&
         airspaceOther == other->airspaceOther;
}

bool MapLayer::hasSameQueryParametersAirwayTrack(const MapLayer *other) const
{
  return airway == other->airway && track == other->track;
}

bool MapLayer::hasSameQueryParametersVor(const MapLayer *other) const
{
  return vor == other->vor;
}

bool MapLayer::hasSameQueryParametersNdb(const MapLayer *other) const
{
  return ndb == other->ndb;
}

bool MapLayer::hasSameQueryParametersWaypoint(const MapLayer *other) const
{
  return waypoint == other->waypoint;
}

bool MapLayer::hasSameQueryParametersWind(const MapLayer *other) const
{
  return windBarbs == other->windBarbs;
}

bool MapLayer::hasSameQueryParametersMarker(const MapLayer *other) const
{
  return marker == other->marker;
}

bool MapLayer::hasSameQueryParametersIls(const MapLayer *other) const
{
  return ils == other->ils && ilsDetail == other->ilsDetail;
}

bool MapLayer::hasSameQueryParametersHolding(const MapLayer *other) const
{
  return holding == other->holding;
}

bool MapLayer::hasSameQueryParametersAirportMsa(const MapLayer *other) const
{
  return airportMsa == other->airportMsa;
}

bool MapLayer::hasSameQueryParametersAircraft(const MapLayer *other) const
{
  return onlineAircraft == other->onlineAircraft &&
         aiAircraftLarge == other->aiAircraftLarge &&
         aiAircraftSmall == other->aiAircraftSmall &&
         aiAircraftGround == other->aiAircraftGround;
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
    if(reader.name() == QStringLiteral("MinRunwayLength"))
      minRunwayLength = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("MaxRange"))
      maxRange = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("AiAircraftGround"))
      aiAircraftGround = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftGroundText"))
      aiAircraftGroundText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftLarge"))
      aiAircraftLarge = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftSize"))
      aiAircraftSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("AiAircraftSmall"))
      aiAircraftSmall = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftText"))
      aiAircraftText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftTextDetail"))
      aiAircraftTextDetail = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftTextDetail2"))
      aiAircraftTextDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiAircraftTextDetail3"))
      aiAircraftTextDetail3 = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiShipLarge"))
      aiShipLarge = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AiShipSmall"))
      aiShipSmall = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Airport"))
      airport = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportDiagram"))
      airportDiagram = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportDiagramDetail"))
      airportDiagramDetail = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportDiagramDetail2"))
      airportDiagramDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportDiagramDetail3"))
      airportDiagramDetail3 = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportDiagramRunway"))
      airportDiagramRunway = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportIdent"))
      airportIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportInfo"))
      airportInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMsa"))
      airportMsa = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMsaDetails"))
      airportMsaDetails = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMsaSymbolScale"))
      airportMsaSymbolScale = xmlStream.readElementTextFloat();
    else if(reader.name() == QStringLiteral("AirportName"))
      airportName = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportNoRating"))
      airportNoRating = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportOverviewRunway"))
      airportOverviewRunway = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportRouteInfo"))
      airportRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMinor"))
      airportMinor = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMinorIdent"))
      airportMinorIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMinorInfo"))
      airportMinorInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMinorName"))
      airportMinorName = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportMinorSymbolSize"))
      airportMinorSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("AirportSymbolSize"))
      airportSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("AirportWeather"))
      airportWeather = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirportWeatherDetails"))
      airportWeatherDetails = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceCenter"))
      airspaceCenter = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceFg"))
      airspaceFg = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceFirUir"))
      airspaceFirUir = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceIcao"))
      airspaceIcao = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceOther"))
      airspaceOther = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceRestricted"))
      airspaceRestricted = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceSpecial"))
      airspaceSpecial = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceCenterText"))
      airspaceCenterText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceFgText"))
      airspaceFgText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceFirUirText"))
      airspaceFirUirText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceIcaoText"))
      airspaceIcaoText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceOtherText"))
      airspaceOtherText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceRestrictedText"))
      airspaceRestrictedText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirspaceSpecialText"))
      airspaceSpecialText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Airway"))
      airway = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirwayDetails"))
      airwayDetails = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirwayIdent"))
      airwayIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirwayInfo"))
      airwayInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("AirwayWaypoint"))
      airwayWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Approach"))
      approach = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("ApproachDetail"))
      approachDetail = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("ApproachText"))
      approachText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("ApproachTextDetail"))
      approachTextDetail = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Holding"))
      holding = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("HoldingInfo"))
      holdingInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("HoldingInfo2"))
      holdingInfo2 = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Ils"))
      ils = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("IlsDetail"))
      ilsDetail = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("IlsIdent"))
      ilsIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("IlsInfo"))
      ilsInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Marker"))
      marker = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("MarkerInfo"))
      markerInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("MarkerSymbolSize"))
      markerSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("Mora"))
      mora = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Ndb"))
      ndb = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("NdbIdent"))
      ndbIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("NdbInfo"))
      ndbInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("NdbRouteIdent"))
      ndbRouteIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("NdbRouteInfo"))
      ndbRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("NdbSymbolSize"))
      ndbSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("OnlineAircraft"))
      onlineAircraft = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("OnlineAircraftText"))
      onlineAircraftText = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("RouteTextAndDetail"))
      routeTextAndDetail = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("RouteTextAndDetail2"))
      routeTextAndDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Track"))
      track = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("TrackIdent"))
      trackIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("TrackInfo"))
      trackInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("TrackWaypoint"))
      trackWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("Userpoint"))
      userpoint = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("UserpointInfo"))
      userpointInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("UserpointSymbolSize"))
      userpointSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("Vor"))
      vor = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("VorIdent"))
      vorIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("VorInfo"))
      vorInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("VorLarge"))
      vorLarge = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("VorRouteIdent"))
      vorRouteIdent = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("VorRouteInfo"))
      vorRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("VorSymbolSize"))
      vorSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("Waypoint"))
      waypoint = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("WaypointName"))
      waypointName = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("WaypointRouteName"))
      waypointRouteName = xmlStream.readElementTextBool();
    else if(reader.name() == QStringLiteral("WaypointSymbolSize"))
      waypointSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("WindBarbs"))
      windBarbs = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("WindBarbsSymbolSize"))
      windBarbsSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("MaximumTextLengthAirport"))
      maximumTextLengthAirport = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("MaximumTextLengthAirportMinor"))
      maximumTextLengthAirportMinor = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("MaximumTextLengthUserpoint"))
      maximumTextLengthUserpoint = xmlStream.readElementTextInt();
    else if(reader.name() == QStringLiteral("AirportFontScale"))
      airportFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == QStringLiteral("AirportMinorFontScale"))
      airportMinorFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == QStringLiteral("RouteFontScale"))
      routeFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == QStringLiteral("AirspaceFontScale"))
      airspaceFontScale = xmlStream.readElementTextFloat();
    else
      xmlStream.skipCurrentElement(true /* warn */);
  }
}

QDebug operator<<(QDebug out, const MapLayer& record)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote();
  out << "<Layer>" << Qt::endl;
  out << "<AiAircraftGround>" << record.aiAircraftGround << "</AiAircraftGround>" << Qt::endl;
  out << "<AiAircraftGroundText>" << record.aiAircraftGroundText << "</AiAircraftGroundText>" << Qt::endl;
  out << "<AiAircraftLarge>" << record.aiAircraftLarge << "</AiAircraftLarge>" << Qt::endl;
  out << "<AiAircraftSize>" << record.aiAircraftSize << "</AiAircraftSize>" << Qt::endl;
  out << "<AiAircraftSmall>" << record.aiAircraftSmall << "</AiAircraftSmall>" << Qt::endl;
  out << "<AiAircraftText>" << record.aiAircraftText << "</AiAircraftText>" << Qt::endl;
  out << "<AiAircraftTextDetail>" << record.aiAircraftTextDetail << "</AiAircraftTextDetail>" << Qt::endl;
  out << "<AiAircraftTextDetail2>" << record.aiAircraftTextDetail2 << "</AiAircraftTextDetail2>" << Qt::endl;
  out << "<AiAircraftTextDetail3>" << record.aiAircraftTextDetail2 << "</AiAircraftTextDetail3>" << Qt::endl;
  out << "<AiShipLarge>" << record.aiShipLarge << "</AiShipLarge>" << Qt::endl;
  out << "<AiShipSmall>" << record.aiShipSmall << "</AiShipSmall>" << Qt::endl;
  out << "<Airport>" << record.airport << "</Airport>" << Qt::endl;
  out << "<AirportDiagram>" << record.airportDiagram << "</AirportDiagram>" << Qt::endl;
  out << "<AirportDiagramDetail>" << record.airportDiagramDetail << "</AirportDiagramDetail>" << Qt::endl;
  out << "<AirportDiagramDetail2>" << record.airportDiagramDetail2 << "</AirportDiagramDetail2>" << Qt::endl;
  out << "<AirportDiagramDetail3>" << record.airportDiagramDetail3 << "</AirportDiagramDetail3>" << Qt::endl;
  out << "<AirportDiagramRunway>" << record.airportDiagramRunway << "</AirportDiagramRunway>" << Qt::endl;
  out << "<AirportFontScale>" << record.airportFontScale << "</AirportFontScale>" << Qt::endl;
  out << "<AirportIdent>" << record.airportIdent << "</AirportIdent>" << Qt::endl;
  out << "<AirportInfo>" << record.airportInfo << "</AirportInfo>" << Qt::endl;
  out << "<AirportMinor>" << record.airportMinor << "</AirportMinor>" << Qt::endl;
  out << "<AirportMinorFontScale>" << record.airportMinorFontScale << "</AirportMinorFontScale>" << Qt::endl;
  out << "<AirportMinorIdent>" << record.airportMinorIdent << "</AirportMinorIdent>" << Qt::endl;
  out << "<AirportMinorInfo>" << record.airportMinorInfo << "</AirportMinorInfo>" << Qt::endl;
  out << "<AirportMinorName>" << record.airportMinorName << "</AirportMinorName>" << Qt::endl;
  out << "<AirportMinorSymbolSize>" << record.airportMinorSymbolSize << "</AirportMinorSymbolSize>" << Qt::endl;
  out << "<AirportMsa>" << record.airportMsa << "</AirportMsa>" << Qt::endl;
  out << "<AirportMsaDetails>" << record.airportMsaDetails << "</AirportMsaDetails>" << Qt::endl;
  out << "<AirportMsaSymbolScale>" << record.airportMsaSymbolScale << "</AirportMsaSymbolScale>" << Qt::endl;
  out << "<AirportName>" << record.airportName << "</AirportName>" << Qt::endl;
  out << "<AirportNoRating>" << record.airportNoRating << "</AirportNoRating>" << Qt::endl;
  out << "<AirportOverviewRunway>" << record.airportOverviewRunway << "</AirportOverviewRunway>" << Qt::endl;
  out << "<AirportRouteInfo>" << record.airportRouteInfo << "</AirportRouteInfo>" << Qt::endl;
  out << "<AirportSymbolSize>" << record.airportSymbolSize << "</AirportSymbolSize>" << Qt::endl;
  out << "<AirportWeather>" << record.airportWeather << "</AirportWeather>" << Qt::endl;
  out << "<AirportWeatherDetails>" << record.airportWeatherDetails << "</AirportWeatherDetails>" << Qt::endl;
  out << "<AirspaceCenter>" << record.airspaceCenter << "</AirspaceCenter>" << Qt::endl;
  out << "<AirspaceFg>" << record.airspaceFg << "</AirspaceFg>" << Qt::endl;
  out << "<AirspaceFirUir>" << record.airspaceFirUir << "</AirspaceFirUir>" << Qt::endl;
  out << "<AirspaceIcao>" << record.airspaceIcao << "</AirspaceIcao>" << Qt::endl;
  out << "<AirspaceOther>" << record.airspaceOther << "</AirspaceOther>" << Qt::endl;
  out << "<AirspaceRestricted>" << record.airspaceRestricted << "</AirspaceRestricted>" << Qt::endl;
  out << "<AirspaceSpecial>" << record.airspaceSpecial << "</AirspaceSpecial>" << Qt::endl;
  out << "<AirspaceCenterText>" << record.airspaceCenterText << "</AirspaceCenterText>" << Qt::endl;
  out << "<AirspaceFgText>" << record.airspaceFgText << "</AirspaceFgText>" << Qt::endl;
  out << "<AirspaceFirUirText>" << record.airspaceFirUirText << "</AirspaceFirUirText>" << Qt::endl;
  out << "<AirspaceIcaoText>" << record.airspaceIcaoText << "</AirspaceIcaoText>" << Qt::endl;
  out << "<AirspaceOtherText>" << record.airspaceOtherText << "</AirspaceOtherText>" << Qt::endl;
  out << "<AirspaceRestrictedText>" << record.airspaceRestrictedText << "</AirspaceRestrictedText>" << Qt::endl;
  out << "<AirspaceSpecialText>" << record.airspaceSpecialText << "</AirspaceSpecialText>" << Qt::endl;
  out << "<Airway>" << record.airway << "</Airway>" << Qt::endl;
  out << "<AirwayDetails>" << record.airway << "</AirwayDetails>" << Qt::endl;
  out << "<AirwayIdent>" << record.airwayIdent << "</AirwayIdent>" << Qt::endl;
  out << "<AirwayInfo>" << record.airwayInfo << "</AirwayInfo>" << Qt::endl;
  out << "<AirwayWaypoint>" << record.airwayWaypoint << "</AirwayWaypoint>" << Qt::endl;
  out << "<Approach>" << record.approach << "</Approach>" << Qt::endl;
  out << "<ApproachDetail>" << record.approachDetail << "</ApproachDetail>" << Qt::endl;
  out << "<ApproachText>" << record.approachText << "</ApproachText>" << Qt::endl;
  out << "<ApproachTextDetail>" << record.approachTextDetail << "</ApproachTextDetail>" << Qt::endl;
  out << "<Holding>" << record.holding << "</Holding>" << Qt::endl;
  out << "<HoldingInfo>" << record.holdingInfo << "</HoldingInfo>" << Qt::endl;
  out << "<HoldingInfo2>" << record.holdingInfo2 << "</HoldingInfo2>" << Qt::endl;
  out << "<Ils>" << record.ils << "</Ils>" << Qt::endl;
  out << "<IlsDetail>" << record.ilsDetail << "</IlsDetail>" << Qt::endl;
  out << "<IlsIdent>" << record.ilsIdent << "</IlsIdent>" << Qt::endl;
  out << "<IlsInfo>" << record.ilsInfo << "</IlsInfo>" << Qt::endl;
  out << "<Marker>" << record.marker << "</Marker>" << Qt::endl;
  out << "<MarkerInfo>" << record.markerInfo << "</MarkerInfo>" << Qt::endl;
  out << "<MarkerSymbolSize>" << record.markerSymbolSize << "</MarkerSymbolSize>" << Qt::endl;
  out << "<MaxRange>" << record.maxRange << "</MaxRange>" << Qt::endl;
  out << "<MaximumTextLengthAirport>" << record.maximumTextLengthAirport << "</MaximumTextLengthAirport>" << Qt::endl;
  out << "<MaximumTextLengthAirportMinor>" << record.maximumTextLengthAirportMinor << "</MaximumTextLengthAirportMinor>" << Qt::endl;
  out << "<MaximumTextLengthUserpoint>" << record.maximumTextLengthUserpoint << "</MaximumTextLengthUserpoint>" << Qt::endl;
  out << "<MinRunwayLength>" << record.minRunwayLength << "</MinRunwayLength>" << Qt::endl;
  out << "<Mora>" << record.mora << "</Mora>" << Qt::endl;
  out << "<Ndb>" << record.ndb << "</Ndb>" << Qt::endl;
  out << "<NdbIdent>" << record.ndbIdent << "</NdbIdent>" << Qt::endl;
  out << "<NdbInfo>" << record.ndbInfo << "</NdbInfo>" << Qt::endl;
  out << "<NdbRouteIdent>" << record.ndbRouteIdent << "</NdbRouteIdent>" << Qt::endl;
  out << "<NdbRouteInfo>" << record.ndbRouteInfo << "</NdbRouteInfo>" << Qt::endl;
  out << "<NdbSymbolSize>" << record.ndbSymbolSize << "</NdbSymbolSize>" << Qt::endl;
  out << "<OnlineAircraft>" << record.onlineAircraft << "</OnlineAircraft>" << Qt::endl;
  out << "<OnlineAircraftText>" << record.onlineAircraftText << "</OnlineAircraftText>" << Qt::endl;
  out << "<RouteTextAndDetail>" << record.routeTextAndDetail << "</RouteTextAndDetail>" << Qt::endl;
  out << "<RouteTextAndDetail2>" << record.routeTextAndDetail2 << "</RouteTextAndDetail2>" << Qt::endl;
  out << "<Track>" << record.track << "</Track>" << Qt::endl;
  out << "<TrackIdent>" << record.trackIdent << "</TrackIdent>" << Qt::endl;
  out << "<TrackInfo>" << record.trackInfo << "</TrackInfo>" << Qt::endl;
  out << "<TrackWaypoint>" << record.trackWaypoint << "</TrackWaypoint>" << Qt::endl;
  out << "<Userpoint>" << record.userpoint << "</Userpoint>" << Qt::endl;
  out << "<UserpointInfo>" << record.userpointInfo << "</UserpointInfo>" << Qt::endl;
  out << "<UserpointSymbolSize>" << record.userpointSymbolSize << "</UserpointSymbolSize>" << Qt::endl;
  out << "<Vor>" << record.vor << "</Vor>" << Qt::endl;
  out << "<VorIdent>" << record.vorIdent << "</VorIdent>" << Qt::endl;
  out << "<VorInfo>" << record.vorInfo << "</VorInfo>" << Qt::endl;
  out << "<VorLarge>" << record.vorLarge << "</VorLarge>" << Qt::endl;
  out << "<VorRouteIdent>" << record.vorRouteIdent << "</VorRouteIdent>" << Qt::endl;
  out << "<VorRouteInfo>" << record.vorRouteInfo << "</VorRouteInfo>" << Qt::endl;
  out << "<VorSymbolSize>" << record.vorSymbolSize << "</VorSymbolSize>" << Qt::endl;
  out << "<Waypoint>" << record.waypoint << "</Waypoint>" << Qt::endl;
  out << "<WaypointName>" << record.waypointName << "</WaypointName>" << Qt::endl;
  out << "<WaypointRouteName>" << record.waypointRouteName << "</WaypointRouteName>" << Qt::endl;
  out << "<WaypointSymbolSize>" << record.waypointSymbolSize << "</WaypointSymbolSize>" << Qt::endl;
  out << "<WindBarbs>" << record.windBarbs << "</WindBarbs>" << Qt::endl;
  out << "<WindBarbsSymbolSize>" << record.windBarbsSymbolSize << "</WindBarbsSymbolSize>" << Qt::endl;
  out << "</Layer>" << Qt::endl;

  return out;
}
