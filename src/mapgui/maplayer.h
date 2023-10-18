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

#ifndef LITTLENAVMAP_MAPLAYER_H
#define LITTLENAVMAP_MAPLAYER_H

#include <QDebug>

namespace atools {
namespace  util {
class XmlStream;
}
}

namespace layer {

/* Ships considered large above this model radius in feet */
constexpr int LARGE_SHIP_SIZE = 150;

/* Aircraft considered large above this model radius in feet */
constexpr int LARGE_AIRCRAFT_SIZE = 75;

}

/*
 * A map layer defines what should be shown on the map for a certain zoom level. It follows the builder pattern.
 */
class MapLayer
{
public:
  /*
   * @param maximumRange create a layer for the maximum zoom distance
   * All features are enabled per default.
   */
  MapLayer(float maximumRangeKm);

  /*
   * create a clone of this layer with the maximum zoom distance
   */
  MapLayer clone(float maximumRangeKm) const;

  /* @return true if a query for this layer will give the same result set */
  bool hasSameQueryParametersAirport(const MapLayer *other) const;
  bool hasSameQueryParametersAirspace(const MapLayer *other) const;
  bool hasSameQueryParametersAirwayTrack(const MapLayer *other) const;
  bool hasSameQueryParametersVor(const MapLayer *other) const;
  bool hasSameQueryParametersNdb(const MapLayer *other) const;
  bool hasSameQueryParametersWaypoint(const MapLayer *other) const;
  bool hasSameQueryParametersWind(const MapLayer *other) const;
  bool hasSameQueryParametersMarker(const MapLayer *other) const;
  bool hasSameQueryParametersIls(const MapLayer *other) const;
  bool hasSameQueryParametersHolding(const MapLayer *other) const;
  bool hasSameQueryParametersAirportMsa(const MapLayer *other) const;
  bool hasSameQueryParametersAircraft(const MapLayer *other) const;

  bool operator<(const MapLayer& other) const;

  float getMaxRange() const
  {
    return maxRange;
  }

  bool isAirport() const
  {
    return airport;
  }

  bool isApproach() const
  {
    return approach;
  }

  bool isApproachDetail() const
  {
    return approachDetail;
  }

  bool isApproachText() const
  {
    return approachText;
  }

  bool isApproachTextDetails() const
  {
    return approachTextDetail;
  }

  bool isAirportOverviewRunway() const
  {
    return airportOverviewRunway;
  }

  bool isAirportDiagram() const
  {
    return airportDiagram;
  }

  bool isAirportDiagramRunway() const
  {
    return airportDiagramRunway;
  }

  /* Lowest detail */
  bool isAirportDiagramDetail() const
  {
    return airportDiagramDetail;
  }

  /* Higher detail */
  bool isAirportDiagramDetail2() const
  {
    return airportDiagramDetail2;
  }

  /* Highest detail */
  bool isAirportDiagramDetail3() const
  {
    return airportDiagramDetail3;
  }

  bool isAirportMinor() const
  {
    return airportMinor;
  }

  bool isAirportNoRating() const
  {
    return airportNoRating;
  }

  int getAirportSymbolSize() const
  {
    return airportSymbolSize;
  }

  bool isAirportIdent() const
  {
    return airportIdent;
  }

  bool isAirportName() const
  {
    return airportName;
  }

  bool isAirportInfo() const
  {
    return airportInfo;
  }

  bool isAirportRouteInfo() const
  {
    return airportRouteInfo;
  }

  int getMinRunwayLength() const
  {
    return minRunwayLength;
  }

  bool isWaypoint() const
  {
    return waypoint;
  }

  bool isWaypointName() const
  {
    return waypointName;
  }

  bool isWaypointRouteName() const
  {
    return waypointRouteName;
  }

  bool isVor() const
  {
    return vor;
  }

  bool isVorLarge() const
  {
    return vorLarge;
  }

  bool isVorIdent() const
  {
    return vorIdent;
  }

  bool isVorInfo() const
  {
    return vorInfo;
  }

  bool isVorRouteIdent() const
  {
    return vorRouteIdent;
  }

  bool isVorRouteInfo() const
  {
    return vorRouteInfo;
  }

  bool isNdb() const
  {
    return ndb;
  }

  bool isNdbIdent() const
  {
    return ndbIdent;
  }

  bool isNdbInfo() const
  {
    return ndbInfo;
  }

  bool isNdbRouteIdent() const
  {
    return ndbRouteIdent;
  }

  bool isNdbRouteInfo() const
  {
    return ndbRouteInfo;
  }

  bool isHolding() const
  {
    return holding;
  }

  bool isHoldingInfo() const
  {
    return holdingInfo;
  }

  bool isHoldingInfo2() const
  {
    return holdingInfo2;
  }

  bool isMarker() const
  {
    return marker;
  }

  bool isMarkerInfo() const
  {
    return markerInfo;
  }

  bool isUserpoint() const
  {
    return userpoint;
  }

  bool isUserpointInfo() const
  {
    return userpointInfo;
  }

  int getUserPointSymbolSize() const
  {
    return userpointSymbolSize;
  }

  bool isIls() const
  {
    return ils;
  }

  bool isIlsDetail() const
  {
    return ilsDetail;
  }

  bool isIlsIdent() const
  {
    return ilsIdent;
  }

  bool isIlsInfo() const
  {
    return ilsInfo;
  }

  bool isAirway() const
  {
    return airway;
  }

  bool isAirwayDetails() const
  {
    return airwayDetails;
  }

  bool isAirwayWaypoint() const
  {
    return airwayWaypoint;
  }

  bool isAirwayIdent() const
  {
    return airwayIdent;
  }

  bool isAirwayInfo() const
  {
    return airwayInfo;
  }

  bool isTrack() const
  {
    return track;
  }

  bool isTrackWaypoint() const
  {
    return trackWaypoint;
  }

  bool isTrackIdent() const
  {
    return trackIdent;
  }

  bool isTrackInfo() const
  {
    return trackInfo;
  }

  int getWaypointSymbolSize() const
  {
    return waypointSymbolSize;
  }

  int getProcedurePointSymbolSize() const
  {
    return waypointSymbolSize + 3;
  }

  int getVorSymbolSize() const
  {
    return vorSymbolSize;
  }

  int getVorSymbolSizeRoute() const
  {
    return std::max(vorSymbolSize, 8);
  }

  int getVorSymbolSizeLarge() const
  {
    return isVorLarge() ? vorSymbolSize * 5 : 0;
  }

  int getNdbSymbolSize() const
  {
    return ndbSymbolSize;
  }

  int getMarkerSymbolSize() const
  {
    return markerSymbolSize;
  }

  bool isAnyAirspace() const
  {
    return isAirspaceCenter() || isAirspaceFg() || isAirspaceFirUir() || isAirspaceIcao() || isAirspaceOther() ||
           isAirspaceRestricted() || isAirspaceSpecial();
  }

  bool isAnyAirspaceText() const
  {
    return isAirspaceCenterText() || isAirspaceFgText() || isAirspaceFirUirText() || isAirspaceIcaoText() || isAirspaceOtherText() ||
           isAirspaceRestrictedText() || isAirspaceSpecialText();
  }

  bool isAirspaceCenter() const
  {
    return airspaceCenter;
  }

  bool isAirspaceIcao() const
  {
    return airspaceIcao;
  }

  bool isAirspaceFg() const
  {
    return airspaceFg;
  }

  bool isAirspaceFirUir() const
  {
    return airspaceFirUir;
  }

  bool isAirspaceRestricted() const
  {
    return airspaceRestricted;
  }

  bool isAirspaceSpecial() const
  {
    return airspaceSpecial;
  }

  bool isAirspaceOther() const
  {
    return airspaceOther;
  }

  bool isAirspaceCenterText() const
  {
    return airspaceCenterText;
  }

  bool isAirspaceIcaoText() const
  {
    return airspaceIcaoText;
  }

  bool isAirspaceFgText() const
  {
    return airspaceFgText;
  }

  bool isAirspaceFirUirText() const
  {
    return airspaceFirUirText;
  }

  bool isAirspaceRestrictedText() const
  {
    return airspaceRestrictedText;
  }

  bool isAirspaceSpecialText() const
  {
    return airspaceSpecialText;
  }

  bool isAirspaceOtherText() const
  {
    return airspaceOtherText;
  }

  bool isAiAircraftLarge() const
  {
    return aiAircraftLarge;
  }

  bool isAiAircraftGround() const
  {
    return aiAircraftGround;
  }

  bool isAiAircraftSmall() const
  {
    return aiAircraftSmall;
  }

  bool isAiShipLarge() const
  {
    return aiShipLarge;
  }

  bool isAiShipSmall() const
  {
    return aiShipSmall;
  }

  bool isAiAircraftGroundText() const
  {
    return aiAircraftGroundText;
  }

  bool isAiAircraftText() const
  {
    return aiAircraftText;
  }

  /* Lowest detail */
  bool isAiAircraftTextDetail() const
  {
    return aiAircraftTextDetail;
  }

  /* Higher detail */
  bool isAiAircraftTextDetail2() const
  {
    return aiAircraftTextDetail2;
  }

  /* Highest detail */
  bool isAiAircraftTextDetail3() const
  {
    return aiAircraftTextDetail3;
  }

  bool isOnlineAircraft() const
  {
    return onlineAircraft;
  }

  bool isOnlineAircraftText() const
  {
    return onlineAircraftText;
  }

  int getMaxTextLengthAirport() const
  {
    return maximumTextLengthAirport;
  }

  int getMaxTextLengthUserpoint() const
  {
    return maximumTextLengthUserpoint;
  }

  bool isAirportWeather() const
  {
    return airportWeather;
  }

  bool isAirportWeatherDetails() const
  {
    return airportWeatherDetails;
  }

  bool isAirportMsa() const
  {
    return airportMsa;
  }

  bool isAirportMsaDetails() const
  {
    return airportMsaDetails;
  }

  float getAirportMsaSymbolScale() const
  {
    return airportMsaSymbolScale;
  }

  /* minimum off route altitude */
  bool isMora() const
  {
    return mora;
  }

  /* Lowest detail */
  bool isRouteTextAndDetail() const
  {
    return routeTextAndDetail;
  }

  /* Higher detail */
  bool isRouteTextAndDetail2() const
  {
    return routeTextAndDetail2;
  }

  /* 2 = show barbs at every even coordinate, 1 = show at every coordinate */
  int getWindBarbs() const
  {
    return windBarbs;
  }

  int getWindBarbsSymbolSize() const
  {
    return windBarbsSymbolSize;
  }

  int getAiAircraftSize() const
  {
    return aiAircraftSize;
  }

  bool isAirportMinorIdent() const
  {
    return airportMinorIdent;
  }

  bool isAirportMinorName() const
  {
    return airportMinorName;
  }

  bool isAirportMinorInfo() const
  {
    return airportMinorInfo;
  }

  int getAirportMinorSymbolSize() const
  {
    return airportMinorSymbolSize;
  }

  int getMaximumTextLengthAirportMinor() const
  {
    return maximumTextLengthAirportMinor;
  }

  float getAirportMinorFontScale() const
  {
    return airportMinorFontScale;
  }

  float getAirportFontScale() const
  {
    return airportFontScale;
  }

  float getRouteFontScale() const
  {
    return routeFontScale;
  }

  float getAirspaceFontScale() const
  {
    return airspaceFontScale;
  }

  /* Load layer data from stream which is already positioned inside a layer element */
  void loadFromXml(atools::util::XmlStream& xmlStream);

private:
  friend QDebug operator<<(QDebug out, const MapLayer& record);

  float maxRange = -1.; /* KM */

  bool airport = true, airportOverviewRunway = true, airportDiagram = true, airportDiagramRunway = true, airportDiagramDetail = true,
       airportDiagramDetail2 = true, airportDiagramDetail3 = true, airportNoRating = true, airportIdent = true, airportName = true,
       airportInfo = true, approach = true,

       airportMinor = true, airportMinorIdent = true, airportMinorName = true, airportMinorInfo = true,

       approachDetail = true, approachText = true, approachTextDetail = true, routeTextAndDetail = true, routeTextAndDetail2 = true,
       userpoint = true;

  bool airportWeather = true, airportWeatherDetails = true;

  bool airportMsa = true, airportMsaDetails = true;

  int airportSymbolSize = 3, airportMinorSymbolSize = 3, minRunwayLength = 0;

  int windBarbs = 1;
  int windBarbsSymbolSize = 6;

  float airportMsaSymbolScale = 6.f;
  float airportMinorFontScale = 1.f, airportFontScale = 1.f, routeFontScale = 1.f, airspaceFontScale = 1.f;

  bool waypoint = true, waypointName = true, vor = true, vorIdent = true, vorInfo = true, vorLarge = true, ndb = true, ndbIdent = true,
       ndbInfo = true, marker = true, markerInfo = true, userpointInfo = true, ils = true, ilsIdent = true, ilsInfo = true,
       ilsDetail = true, airway = true, airwayDetails = true, airwayWaypoint = true, airwayIdent = true, airwayInfo = true, track = true,
       trackWaypoint = true, trackIdent = true, trackInfo = true, mora = true, holding = true, holdingInfo = true, holdingInfo2 = true;

  bool airportRouteInfo = true;
  bool vorRouteIdent = true, vorRouteInfo = true;
  bool ndbRouteIdent = true, ndbRouteInfo = true;
  bool waypointRouteName = true;

  int waypointSymbolSize = 3, vorSymbolSize = 3, ndbSymbolSize = 4,
      markerSymbolSize = 8, userpointSymbolSize = 12;

  int maximumTextLengthAirport = 16, maximumTextLengthAirportMinor = 16, maximumTextLengthUserpoint = 10;

  bool airspaceCenter = true, airspaceIcao = true, airspaceFg = true, airspaceFirUir = true, airspaceRestricted = true,
       airspaceSpecial = true, airspaceOther = true;

  bool airspaceCenterText = true, airspaceIcaoText = true, airspaceFgText = true, airspaceFirUirText = true, airspaceRestrictedText = true,
       airspaceSpecialText = true, airspaceOtherText = true;

  int aiAircraftSize = 32;
  bool aiAircraftGround = true, aiAircraftLarge = true, aiAircraftSmall = true, onlineAircraft = true, aiShipLarge = true,
       aiShipSmall = true, aiAircraftGroundText = true, aiAircraftText = true, aiAircraftTextDetail = true, aiAircraftTextDetail2 = true,
       aiAircraftTextDetail3 = true, onlineAircraftText = true;
};

#endif // LITTLENAVMAP_MAPLAYER_H
