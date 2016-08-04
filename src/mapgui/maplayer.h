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

#ifndef LITTLENAVMAP_LAYER_H
#define LITTLENAVMAP_LAYER_H

#include <QDebug>

namespace layer {
/* Defines which table to use for airport queries */
enum AirportSource
{
  ALL, /* use airport table as source */
  MEDIUM, /* use airport_medium table as source */
  LARGE /* use airport_large table as source */
};

}

/*
 * A map layer defines what should be shown on the map for a certain zoom level. It follows the builder pattern.
 */
class MapLayer
{
public:
  /*
   * @param maximumRange create a layer for the maximum zoom distance
   */
  MapLayer(float maximumRange);

  /*
   * create a clone of this layer with the maximum zoom distance
   */
  MapLayer clone(float maximumRange) const;

  /* Show airports */
  MapLayer& airport(bool value = true);

  /* Define source table for airports */
  MapLayer& airportSource(layer::AirportSource source);

  /* Show airport runway overview symbol for airports with runways > 8000 ft */
  MapLayer& airportOverviewRunway(bool value = true);

  /* Show airport diagram with runways, taxiways, etc. */
  MapLayer& airportDiagram(bool value = true);
  MapLayer& airportDiagramDetail(bool value = true);
  MapLayer& airportDiagramDetail2(bool value = true);

  /* Show airports having only soft runways */
  MapLayer& airportSoft(bool value = true);

  /* Show empty airports */
  MapLayer& airportNoRating(bool value = true);

  /* Symbol size in pixel */
  MapLayer& airportSymbolSize(int size);
  MapLayer& airportIdent(bool = true);
  MapLayer& airportName(bool = true);

  /* Show Tower, ATIS, etc. */
  MapLayer& airportInfo(bool = true);

  /* Show airport information if it is part of the route */
  MapLayer& airportRouteInfo(bool = true);

  /* Display only airport that have a minimum runway length in feet */
  MapLayer& minRunwayLength(int length);

  /* Waypoint options */
  MapLayer& waypoint(bool value = true);
  MapLayer& waypointName(bool value = true);
  MapLayer& waypointRouteName(bool value = true);
  MapLayer& waypointSymbolSize(int size);

  /* VOR options */
  MapLayer& vor(bool value = true);
  MapLayer& vorLarge(bool value = true); /* Show large with compass circle and ticks */
  MapLayer& vorIdent(bool value = true);
  MapLayer& vorInfo(bool value = true);
  MapLayer& vorRouteIdent(bool value = true);
  MapLayer& vorRouteInfo(bool value = true);
  MapLayer& vorSymbolSize(int size);

  /* NDB options */
  MapLayer& ndb(bool value = true);
  MapLayer& ndbIdent(bool value = true);
  MapLayer& ndbInfo(bool value = true);
  MapLayer& ndbRouteIdent(bool value = true);
  MapLayer& ndbRouteInfo(bool value = true);
  MapLayer& ndbSymbolSize(int size);

  /* Marker options */
  MapLayer& marker(bool value = true);
  MapLayer& markerInfo(bool value = true);
  MapLayer& markerSymbolSize(int size);

  /* ILS options */
  MapLayer& ils(bool value = true);
  MapLayer& ilsIdent(bool value = true);
  MapLayer& ilsInfo(bool value = true);

  /* Airway options (Jet and Victor airways are filtered out in the paint method) */
  MapLayer& airway(bool value = true);
  MapLayer& airwayIdent(bool value = true);
  MapLayer& airwayInfo(bool value = true);

  bool operator<(const MapLayer& other) const;

  float getMaxRange() const
  {
    return maxRange;
  }

  bool isAirport() const
  {
    return layerAirport;
  }

  bool isAirportOverviewRunway() const
  {
    return layerAirportOverviewRunway;
  }

  bool isAirportDiagram() const
  {
    return layerAirportDiagram;
  }

  bool isAirportDiagramDetail() const
  {
    return layerAirportDiagramDetail;
  }

  bool isAirportDiagramDetail2() const
  {
    return layerAirportDiagramDetail2;
  }

  bool isAirportSoft() const
  {
    return layerAirportSoft;
  }

  bool isAirportNoRating() const
  {
    return layerAirportNoRating;
  }

  int getAirportSymbolSize() const
  {
    return layerAirportSymbolSize;
  }

  bool isAirportIdent() const
  {
    return layerAirportIdent;
  }

  bool isAirportName() const
  {
    return layerAirportName;
  }

  bool isAirportInfo() const
  {
    return layerAirportInfo;
  }

  bool isAirportRouteInfo() const
  {
    return layerAirportRouteInfo;
  }

  layer::AirportSource getDataSource() const
  {
    return src;
  }

  int getMinRunwayLength() const
  {
    return layerMinRunwayLength;
  }

  bool isWaypoint() const
  {
    return layerWaypoint;
  }

  bool isWaypointName() const
  {
    return layerWaypointName;
  }

  bool isWaypointRouteName() const
  {
    return layerWaypointRouteName;
  }

  bool isVor() const
  {
    return layerVor;
  }

  bool isVorLarge() const
  {
    return layerVorLarge;
  }

  bool isVorIdent() const
  {
    return layerVorIdent;
  }

  bool isVorInfo() const
  {
    return layerVorInfo;
  }

  bool isVorRouteIdent() const
  {
    return layerVorRouteIdent;
  }

  bool isVorRouteInfo() const
  {
    return layerVorRouteInfo;
  }

  bool isNdb() const
  {
    return layerNdb;
  }

  bool isNdbIdent() const
  {
    return layerNdbIdent;
  }

  bool isNdbInfo() const
  {
    return layerNdbInfo;
  }

  bool isNdbRouteIdent() const
  {
    return layerNdbRouteIdent;
  }

  bool isNdbRouteInfo() const
  {
    return layerNdbRouteInfo;
  }

  bool isMarker() const
  {
    return layerMarker;
  }

  bool isMarkerInfo() const
  {
    return layerMarkerInfo;
  }

  bool isIls() const
  {
    return layerIls;
  }

  bool isIlsIdent() const
  {
    return layerIlsIdent;
  }

  bool isIlsInfo() const
  {
    return layerIlsInfo;
  }

  bool isAirway() const
  {
    return layerAirway;
  }

  bool isAirwayIdent() const
  {
    return layerAirwayIdent;
  }

  bool isAirwayInfo() const
  {
    return layerAirwayInfo;
  }

  bool hasSameQueryParameters(const MapLayer *other) const
  {
    return src == other->src && layerMinRunwayLength == other->layerMinRunwayLength;
  }

  int getWaypointSymbolSize() const
  {
    return layerWaypointSymbolSize;
  }

  int getVorSymbolSize() const
  {
    return layerVorSymbolSize;
  }

  int getNdbSymbolSize() const
  {
    return layerNdbSymbolSize;
  }

  int getMarkerSymbolSize() const
  {
    return layerMarkerSymbolSize;
  }

private:
  friend QDebug operator<<(QDebug out, const MapLayer& record);

  float maxRange = -1.;

  layer::AirportSource src;
  bool layerAirport = false, layerAirportOverviewRunway = false,
       layerAirportDiagram = false, layerAirportDiagramDetail = false, layerAirportDiagramDetail2 = false,
       layerAirportSoft = false, layerAirportNoRating = false, layerAirportIdent = false,
       layerAirportName = false, layerAirportInfo = false;
  int layerAirportSymbolSize = 5, layerMinRunwayLength = 0;

  bool layerWaypoint = false, layerWaypointName = false,
       layerVor = false, layerVorIdent = false, layerVorInfo = false, layerVorLarge = false,
       layerNdb = false, layerNdbIdent = false, layerNdbInfo = false,
       layerMarker = false, layerMarkerInfo = false,
       layerIls = false, layerIlsIdent = false, layerIlsInfo = false,
       layerAirway = false, layerAirwayIdent = false, layerAirwayInfo = false;

  bool layerAirportRouteInfo = false;
  bool layerVorRouteIdent = false, layerVorRouteInfo = false;
  bool layerNdbRouteIdent = false, layerNdbRouteInfo = false;
  bool layerWaypointRouteName = false;

  int layerWaypointSymbolSize = 8, layerVorSymbolSize = 8, layerNdbSymbolSize = 8,
      layerMarkerSymbolSize = 8;

};

#endif // LITTLENAVMAP_LAYER_H
