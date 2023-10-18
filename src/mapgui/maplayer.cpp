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
    if(reader.name() == "MinRunwayLength")
      minRunwayLength = xmlStream.readElementTextInt();
    else if(reader.name() == "MaxRange")
      maxRange = xmlStream.readElementTextInt();
    else if(reader.name() == "AiAircraftGround")
      aiAircraftGround = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftGroundText")
      aiAircraftGroundText = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftLarge")
      aiAircraftLarge = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftSize")
      aiAircraftSize = xmlStream.readElementTextInt();
    else if(reader.name() == "AiAircraftSmall")
      aiAircraftSmall = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftText")
      aiAircraftText = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftTextDetail")
      aiAircraftTextDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftTextDetail2")
      aiAircraftTextDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == "AiAircraftTextDetail3")
      aiAircraftTextDetail3 = xmlStream.readElementTextBool();
    else if(reader.name() == "AiShipLarge")
      aiShipLarge = xmlStream.readElementTextBool();
    else if(reader.name() == "AiShipSmall")
      aiShipSmall = xmlStream.readElementTextBool();
    else if(reader.name() == "Airport")
      airport = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagram")
      airportDiagram = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramDetail")
      airportDiagramDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramDetail2")
      airportDiagramDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramDetail3")
      airportDiagramDetail3 = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportDiagramRunway")
      airportDiagramRunway = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportIdent")
      airportIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportInfo")
      airportInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMsa")
      airportMsa = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMsaDetails")
      airportMsaDetails = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMsaSymbolScale")
      airportMsaSymbolScale = xmlStream.readElementTextFloat();
    else if(reader.name() == "AirportName")
      airportName = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportNoRating")
      airportNoRating = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportOverviewRunway")
      airportOverviewRunway = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportRouteInfo")
      airportRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinor")
      airportMinor = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorIdent")
      airportMinorIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorInfo")
      airportMinorInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorName")
      airportMinorName = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportMinorSymbolSize")
      airportMinorSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "AirportSymbolSize")
      airportSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "AirportWeather")
      airportWeather = xmlStream.readElementTextBool();
    else if(reader.name() == "AirportWeatherDetails")
      airportWeatherDetails = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceCenter")
      airspaceCenter = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceFg")
      airspaceFg = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceFirUir")
      airspaceFirUir = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceIcao")
      airspaceIcao = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceOther")
      airspaceOther = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceRestricted")
      airspaceRestricted = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceSpecial")
      airspaceSpecial = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceCenterText")
      airspaceCenterText = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceFgText")
      airspaceFgText = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceFirUirText")
      airspaceFirUirText = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceIcaoText")
      airspaceIcaoText = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceOtherText")
      airspaceOtherText = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceRestrictedText")
      airspaceRestrictedText = xmlStream.readElementTextBool();
    else if(reader.name() == "AirspaceSpecialText")
      airspaceSpecialText = xmlStream.readElementTextBool();
    else if(reader.name() == "Airway")
      airway = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayDetails")
      airwayDetails = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayIdent")
      airwayIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayInfo")
      airwayInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "AirwayWaypoint")
      airwayWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == "Approach")
      approach = xmlStream.readElementTextBool();
    else if(reader.name() == "ApproachDetail")
      approachDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "ApproachText")
      approachText = xmlStream.readElementTextBool();
    else if(reader.name() == "ApproachTextDetail")
      approachTextDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "Holding")
      holding = xmlStream.readElementTextBool();
    else if(reader.name() == "HoldingInfo")
      holdingInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "HoldingInfo2")
      holdingInfo2 = xmlStream.readElementTextBool();
    else if(reader.name() == "Ils")
      ils = xmlStream.readElementTextBool();
    else if(reader.name() == "IlsDetail")
      ilsDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "IlsIdent")
      ilsIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "IlsInfo")
      ilsInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "Marker")
      marker = xmlStream.readElementTextBool();
    else if(reader.name() == "MarkerInfo")
      markerInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "MarkerSymbolSize")
      markerSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "Mora")
      mora = xmlStream.readElementTextBool();
    else if(reader.name() == "Ndb")
      ndb = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbIdent")
      ndbIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbInfo")
      ndbInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbRouteIdent")
      ndbRouteIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbRouteInfo")
      ndbRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "NdbSymbolSize")
      ndbSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "OnlineAircraft")
      onlineAircraft = xmlStream.readElementTextBool();
    else if(reader.name() == "OnlineAircraftText")
      onlineAircraftText = xmlStream.readElementTextBool();
    else if(reader.name() == "RouteTextAndDetail")
      routeTextAndDetail = xmlStream.readElementTextBool();
    else if(reader.name() == "RouteTextAndDetail2")
      routeTextAndDetail2 = xmlStream.readElementTextBool();
    else if(reader.name() == "Track")
      track = xmlStream.readElementTextBool();
    else if(reader.name() == "TrackIdent")
      trackIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "TrackInfo")
      trackInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "TrackWaypoint")
      trackWaypoint = xmlStream.readElementTextBool();
    else if(reader.name() == "Userpoint")
      userpoint = xmlStream.readElementTextBool();
    else if(reader.name() == "UserpointInfo")
      userpointInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "UserpointSymbolSize")
      userpointSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "Vor")
      vor = xmlStream.readElementTextBool();
    else if(reader.name() == "VorIdent")
      vorIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "VorInfo")
      vorInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "VorLarge")
      vorLarge = xmlStream.readElementTextBool();
    else if(reader.name() == "VorRouteIdent")
      vorRouteIdent = xmlStream.readElementTextBool();
    else if(reader.name() == "VorRouteInfo")
      vorRouteInfo = xmlStream.readElementTextBool();
    else if(reader.name() == "VorSymbolSize")
      vorSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "Waypoint")
      waypoint = xmlStream.readElementTextBool();
    else if(reader.name() == "WaypointName")
      waypointName = xmlStream.readElementTextBool();
    else if(reader.name() == "WaypointRouteName")
      waypointRouteName = xmlStream.readElementTextBool();
    else if(reader.name() == "WaypointSymbolSize")
      waypointSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "WindBarbs")
      windBarbs = xmlStream.readElementTextInt();
    else if(reader.name() == "WindBarbsSymbolSize")
      windBarbsSymbolSize = xmlStream.readElementTextInt();
    else if(reader.name() == "MaximumTextLengthAirport")
      maximumTextLengthAirport = xmlStream.readElementTextInt();
    else if(reader.name() == "MaximumTextLengthAirportMinor")
      maximumTextLengthAirportMinor = xmlStream.readElementTextInt();
    else if(reader.name() == "MaximumTextLengthUserpoint")
      maximumTextLengthUserpoint = xmlStream.readElementTextInt();
    else if(reader.name() == "AirportFontScale")
      airportFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == "AirportMinorFontScale")
      airportMinorFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == "RouteFontScale")
      routeFontScale = xmlStream.readElementTextFloat();
    else if(reader.name() == "AirspaceFontScale")
      airspaceFontScale = xmlStream.readElementTextFloat();
    else
      xmlStream.skipCurrentElement(true /* warn */);
  }
}

QDebug operator<<(QDebug out, const MapLayer& record)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote();
  out << "<Layer>" << endl;
  out << "<AiAircraftGround>" << record.aiAircraftGround << "</AiAircraftGround>" << endl;
  out << "<AiAircraftGroundText>" << record.aiAircraftGroundText << "</AiAircraftGroundText>" << endl;
  out << "<AiAircraftLarge>" << record.aiAircraftLarge << "</AiAircraftLarge>" << endl;
  out << "<AiAircraftSize>" << record.aiAircraftSize << "</AiAircraftSize>" << endl;
  out << "<AiAircraftSmall>" << record.aiAircraftSmall << "</AiAircraftSmall>" << endl;
  out << "<AiAircraftText>" << record.aiAircraftText << "</AiAircraftText>" << endl;
  out << "<AiAircraftTextDetail>" << record.aiAircraftTextDetail << "</AiAircraftTextDetail>" << endl;
  out << "<AiAircraftTextDetail2>" << record.aiAircraftTextDetail2 << "</AiAircraftTextDetail2>" << endl;
  out << "<AiAircraftTextDetail3>" << record.aiAircraftTextDetail2 << "</AiAircraftTextDetail3>" << endl;
  out << "<AiShipLarge>" << record.aiShipLarge << "</AiShipLarge>" << endl;
  out << "<AiShipSmall>" << record.aiShipSmall << "</AiShipSmall>" << endl;
  out << "<Airport>" << record.airport << "</Airport>" << endl;
  out << "<AirportDiagram>" << record.airportDiagram << "</AirportDiagram>" << endl;
  out << "<AirportDiagramDetail>" << record.airportDiagramDetail << "</AirportDiagramDetail>" << endl;
  out << "<AirportDiagramDetail2>" << record.airportDiagramDetail2 << "</AirportDiagramDetail2>" << endl;
  out << "<AirportDiagramDetail3>" << record.airportDiagramDetail3 << "</AirportDiagramDetail3>" << endl;
  out << "<AirportDiagramRunway>" << record.airportDiagramRunway << "</AirportDiagramRunway>" << endl;
  out << "<AirportFontScale>" << record.airportFontScale << "</AirportFontScale>" << endl;
  out << "<AirportIdent>" << record.airportIdent << "</AirportIdent>" << endl;
  out << "<AirportInfo>" << record.airportInfo << "</AirportInfo>" << endl;
  out << "<AirportMinor>" << record.airportMinor << "</AirportMinor>" << endl;
  out << "<AirportMinorFontScale>" << record.airportMinorFontScale << "</AirportMinorFontScale>" << endl;
  out << "<AirportMinorIdent>" << record.airportMinorIdent << "</AirportMinorIdent>" << endl;
  out << "<AirportMinorInfo>" << record.airportMinorInfo << "</AirportMinorInfo>" << endl;
  out << "<AirportMinorName>" << record.airportMinorName << "</AirportMinorName>" << endl;
  out << "<AirportMinorSymbolSize>" << record.airportMinorSymbolSize << "</AirportMinorSymbolSize>" << endl;
  out << "<AirportMsa>" << record.airportMsa << "</AirportMsa>" << endl;
  out << "<AirportMsaDetails>" << record.airportMsaDetails << "</AirportMsaDetails>" << endl;
  out << "<AirportMsaSymbolScale>" << record.airportMsaSymbolScale << "</AirportMsaSymbolScale>" << endl;
  out << "<AirportName>" << record.airportName << "</AirportName>" << endl;
  out << "<AirportNoRating>" << record.airportNoRating << "</AirportNoRating>" << endl;
  out << "<AirportOverviewRunway>" << record.airportOverviewRunway << "</AirportOverviewRunway>" << endl;
  out << "<AirportRouteInfo>" << record.airportRouteInfo << "</AirportRouteInfo>" << endl;
  out << "<AirportSymbolSize>" << record.airportSymbolSize << "</AirportSymbolSize>" << endl;
  out << "<AirportWeather>" << record.airportWeather << "</AirportWeather>" << endl;
  out << "<AirportWeatherDetails>" << record.airportWeatherDetails << "</AirportWeatherDetails>" << endl;
  out << "<AirspaceCenter>" << record.airspaceCenter << "</AirspaceCenter>" << endl;
  out << "<AirspaceFg>" << record.airspaceFg << "</AirspaceFg>" << endl;
  out << "<AirspaceFirUir>" << record.airspaceFirUir << "</AirspaceFirUir>" << endl;
  out << "<AirspaceIcao>" << record.airspaceIcao << "</AirspaceIcao>" << endl;
  out << "<AirspaceOther>" << record.airspaceOther << "</AirspaceOther>" << endl;
  out << "<AirspaceRestricted>" << record.airspaceRestricted << "</AirspaceRestricted>" << endl;
  out << "<AirspaceSpecial>" << record.airspaceSpecial << "</AirspaceSpecial>" << endl;
  out << "<AirspaceCenterText>" << record.airspaceCenterText << "</AirspaceCenterText>" << endl;
  out << "<AirspaceFgText>" << record.airspaceFgText << "</AirspaceFgText>" << endl;
  out << "<AirspaceFirUirText>" << record.airspaceFirUirText << "</AirspaceFirUirText>" << endl;
  out << "<AirspaceIcaoText>" << record.airspaceIcaoText << "</AirspaceIcaoText>" << endl;
  out << "<AirspaceOtherText>" << record.airspaceOtherText << "</AirspaceOtherText>" << endl;
  out << "<AirspaceRestrictedText>" << record.airspaceRestrictedText << "</AirspaceRestrictedText>" << endl;
  out << "<AirspaceSpecialText>" << record.airspaceSpecialText << "</AirspaceSpecialText>" << endl;
  out << "<Airway>" << record.airway << "</Airway>" << endl;
  out << "<AirwayDetails>" << record.airway << "</AirwayDetails>" << endl;
  out << "<AirwayIdent>" << record.airwayIdent << "</AirwayIdent>" << endl;
  out << "<AirwayInfo>" << record.airwayInfo << "</AirwayInfo>" << endl;
  out << "<AirwayWaypoint>" << record.airwayWaypoint << "</AirwayWaypoint>" << endl;
  out << "<Approach>" << record.approach << "</Approach>" << endl;
  out << "<ApproachDetail>" << record.approachDetail << "</ApproachDetail>" << endl;
  out << "<ApproachText>" << record.approachText << "</ApproachText>" << endl;
  out << "<ApproachTextDetail>" << record.approachTextDetail << "</ApproachTextDetail>" << endl;
  out << "<Holding>" << record.holding << "</Holding>" << endl;
  out << "<HoldingInfo>" << record.holdingInfo << "</HoldingInfo>" << endl;
  out << "<HoldingInfo2>" << record.holdingInfo2 << "</HoldingInfo2>" << endl;
  out << "<Ils>" << record.ils << "</Ils>" << endl;
  out << "<IlsDetail>" << record.ilsDetail << "</IlsDetail>" << endl;
  out << "<IlsIdent>" << record.ilsIdent << "</IlsIdent>" << endl;
  out << "<IlsInfo>" << record.ilsInfo << "</IlsInfo>" << endl;
  out << "<Marker>" << record.marker << "</Marker>" << endl;
  out << "<MarkerInfo>" << record.markerInfo << "</MarkerInfo>" << endl;
  out << "<MarkerSymbolSize>" << record.markerSymbolSize << "</MarkerSymbolSize>" << endl;
  out << "<MaxRange>" << record.maxRange << "</MaxRange>" << endl;
  out << "<MaximumTextLengthAirport>" << record.maximumTextLengthAirport << "</MaximumTextLengthAirport>" << endl;
  out << "<MaximumTextLengthAirportMinor>" << record.maximumTextLengthAirportMinor << "</MaximumTextLengthAirportMinor>" << endl;
  out << "<MaximumTextLengthUserpoint>" << record.maximumTextLengthUserpoint << "</MaximumTextLengthUserpoint>" << endl;
  out << "<MinRunwayLength>" << record.minRunwayLength << "</MinRunwayLength>" << endl;
  out << "<Mora>" << record.mora << "</Mora>" << endl;
  out << "<Ndb>" << record.ndb << "</Ndb>" << endl;
  out << "<NdbIdent>" << record.ndbIdent << "</NdbIdent>" << endl;
  out << "<NdbInfo>" << record.ndbInfo << "</NdbInfo>" << endl;
  out << "<NdbRouteIdent>" << record.ndbRouteIdent << "</NdbRouteIdent>" << endl;
  out << "<NdbRouteInfo>" << record.ndbRouteInfo << "</NdbRouteInfo>" << endl;
  out << "<NdbSymbolSize>" << record.ndbSymbolSize << "</NdbSymbolSize>" << endl;
  out << "<OnlineAircraft>" << record.onlineAircraft << "</OnlineAircraft>" << endl;
  out << "<OnlineAircraftText>" << record.onlineAircraftText << "</OnlineAircraftText>" << endl;
  out << "<RouteTextAndDetail>" << record.routeTextAndDetail << "</RouteTextAndDetail>" << endl;
  out << "<RouteTextAndDetail2>" << record.routeTextAndDetail2 << "</RouteTextAndDetail2>" << endl;
  out << "<Track>" << record.track << "</Track>" << endl;
  out << "<TrackIdent>" << record.trackIdent << "</TrackIdent>" << endl;
  out << "<TrackInfo>" << record.trackInfo << "</TrackInfo>" << endl;
  out << "<TrackWaypoint>" << record.trackWaypoint << "</TrackWaypoint>" << endl;
  out << "<Userpoint>" << record.userpoint << "</Userpoint>" << endl;
  out << "<UserpointInfo>" << record.userpointInfo << "</UserpointInfo>" << endl;
  out << "<UserpointSymbolSize>" << record.userpointSymbolSize << "</UserpointSymbolSize>" << endl;
  out << "<Vor>" << record.vor << "</Vor>" << endl;
  out << "<VorIdent>" << record.vorIdent << "</VorIdent>" << endl;
  out << "<VorInfo>" << record.vorInfo << "</VorInfo>" << endl;
  out << "<VorLarge>" << record.vorLarge << "</VorLarge>" << endl;
  out << "<VorRouteIdent>" << record.vorRouteIdent << "</VorRouteIdent>" << endl;
  out << "<VorRouteInfo>" << record.vorRouteInfo << "</VorRouteInfo>" << endl;
  out << "<VorSymbolSize>" << record.vorSymbolSize << "</VorSymbolSize>" << endl;
  out << "<Waypoint>" << record.waypoint << "</Waypoint>" << endl;
  out << "<WaypointName>" << record.waypointName << "</WaypointName>" << endl;
  out << "<WaypointRouteName>" << record.waypointRouteName << "</WaypointRouteName>" << endl;
  out << "<WaypointSymbolSize>" << record.waypointSymbolSize << "</WaypointSymbolSize>" << endl;
  out << "<WindBarbs>" << record.windBarbs << "</WindBarbs>" << endl;
  out << "<WindBarbsSymbolSize>" << record.windBarbsSymbolSize << "</WindBarbsSymbolSize>" << endl;
  out << "</Layer>" << endl;

  return out;
}
