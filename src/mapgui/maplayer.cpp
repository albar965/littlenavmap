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
