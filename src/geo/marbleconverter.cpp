/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "geo/marbleconverter.h"
#include "geo/line.h"
#include "geo/linestring.h"
#include "geo/pos.h"
#include "geo/rect.h"
#include "marble/GeoDataLatLonBox.h"
#include "marble/GeoDataLineString.h"

#include <marble/GeoDataLinearRing.h>

namespace mconvert {

Marble::GeoDataCoordinates toGdc(const atools::geo::Pos& coords)
{
  return Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0., DEG);
}

Marble::GeoDataCoordinates toGdc(const atools::geo::PosD& coords)
{
  return Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0., DEG);
}

Marble::GeoDataLatLonBox toGdc(const atools::geo::Rect& coords)
{
  return Marble::GeoDataLatLonBox(coords.getNorth(), coords.getSouth(), coords.getEast(), coords.getWest(), DEG);
}

atools::geo::Rect fromGdc(const Marble::GeoDataLatLonBox& coords)
{
  return atools::geo::Rect(coords.west(DEG), coords.north(DEG), coords.east(DEG), coords.south(DEG));
}

Marble::GeoDataCoordinates toGdc(float lonX, float latY)
{
  return Marble::GeoDataCoordinates(lonX, latY, 0., DEG);
}

Marble::GeoDataCoordinates toGdc(double lonX, double latY)
{
  return Marble::GeoDataCoordinates(lonX, latY, 0., DEG);
}

Marble::GeoDataLineString toGdcStr(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2)
{
  Marble::GeoDataLineString linestring;
  linestring.setTessellate(true);
  linestring.append(toGdc(pos1));
  linestring.append(toGdc(pos2));
  return linestring;
}

Marble::GeoDataLineString toGdcStr(const atools::geo::Line& coords)
{
  Marble::GeoDataLineString linestring;
  linestring.setTessellate(true);
  linestring.append(toGdc(coords.getPos1()));
  linestring.append(toGdc(coords.getPos2()));
  return linestring;
}

Marble::GeoDataLineString toGdcStr(const atools::geo::LineString& coords)
{
  Marble::GeoDataLineString linestring;
  linestring.setTessellate(true);

  for(const atools::geo::Pos& pos : coords)
    linestring.append(toGdc(pos));
  return linestring;
}

atools::geo::Pos fromGdc(const Marble::GeoDataCoordinates& coords)
{
  return atools::geo::Pos(coords.longitude(DEG), coords.latitude(DEG));
}

const atools::geo::LineString fromGdcStr(const Marble::GeoDataLineString& coords)
{
  atools::geo::LineString linestring;
  for(const Marble::GeoDataCoordinates& gdc : coords)
    linestring.append(fromGdc(gdc));
  return linestring;
}

Marble::GeoDataLinearRing toGdcRing(const atools::geo::LineString& linestring)
{
  Marble::GeoDataLinearRing linearRing;
  linearRing.setTessellate(true);

  for(const atools::geo::Pos& pos : linestring)
    linearRing.append(mconvert::toGdc(pos));
  return linearRing;
}

} // namespace marbleconverter
