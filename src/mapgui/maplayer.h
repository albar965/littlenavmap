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

#ifndef LAYER_H
#define LAYER_H

#include "logging/loggingdefs.h"

namespace layer {
enum AirportSource
{
  ALL,
  MEDIUM,
  LARGE
};

}

class MapLayer
{
public:
  MapLayer(float maximumRange);

  MapLayer clone(float maximumRange) const;

  MapLayer& airports(bool value = true);

  /* Draw fuel ticks, etc. */
  MapLayer& airportDetail(bool value = true);
  MapLayer& airportSource(layer::AirportSource source);
  MapLayer& airportOverviewRunway(bool value = true);
  MapLayer& airportDiagram(bool value = true);
  MapLayer& airportDiagramDetail(bool value = true);
  MapLayer& airportDiagramDetail2(bool value = true);
  MapLayer& airportSoft(bool value = true);
  MapLayer& airportNoRating(bool value = true);
  MapLayer& airportSymbolSize(int size);
  MapLayer& airportIdent(bool = true);
  MapLayer& airportName(bool = true);
  MapLayer& airportInfo(bool = true);
  MapLayer& minRunwayLength(int length);

  bool operator<(const MapLayer& other) const;

  float getMaxRange() const
  {
    return maxRange;
  }

  bool isAirport() const
  {
    return layerAirport;
  }

  bool isAirportDetail() const
  {
    return layerAirportDetail;
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

  layer::AirportSource getDataSource() const
  {
    return src;
  }

  int getMinRunwayLength() const
  {
    return layerMinRunwayLength;
  }

  bool hasSameQueryParameters(const MapLayer *other) const
  {
    return src == other->src && layerMinRunwayLength == other->layerMinRunwayLength;
  }

private:
  friend QDebug operator<<(QDebug out, const MapLayer& record);

  float maxRange = -1.;

  layer::AirportSource src;
  bool layerAirport = false, layerAirportDetail = false, layerAirportOverviewRunway = false,
       layerAirportDiagram = false, layerAirportDiagramDetail = false, layerAirportDiagramDetail2 = false,
       layerAirportSoft = false, layerAirportNoRating = false, layerAirportIdent = false,
       layerAirportName = false, layerAirportInfo = false;
  int layerAirportSymbolSize = 10, layerMinRunwayLength = 0;

};

#endif // LAYER_H
