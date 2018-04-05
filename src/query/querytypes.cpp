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

#include "query/querytypes.h"

#include "sql/sqlquery.h"

using namespace Marble;

namespace query {

void inflateQueryRect(Marble::GeoDataLatLonBox& rect, double factor, double increment)
{
  rect.scale(1. + factor, 1. + factor);

  if(rect.east(GeoDataCoordinates::Degree) + increment < 180.f)
    rect.setEast(rect.east(GeoDataCoordinates::Degree) + increment, GeoDataCoordinates::Degree);

  if(rect.west(GeoDataCoordinates::Degree) - increment > -180.f)
    rect.setWest(rect.west(GeoDataCoordinates::Degree) - increment, GeoDataCoordinates::Degree);

  if(rect.north(GeoDataCoordinates::Degree) + increment < 90.f)
    rect.setNorth(rect.north(GeoDataCoordinates::Degree) + increment, GeoDataCoordinates::Degree);
  if(rect.south(GeoDataCoordinates::Degree) - increment > -90.f)
    rect.setSouth(rect.south(GeoDataCoordinates::Degree) - increment, GeoDataCoordinates::Degree);

  // qDebug() << rect.toString(GeoDataCoordinates::Degree);
}

/*
 * Bind rectangle coordinates to a query.
 * @param rect
 * @param query
 * @param prefix used to prefix each bind variable
 */
void bindCoordinatePointInRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                               const QString& prefix)
{
  query->bindValue(":" + prefix + "leftx", rect.west(GeoDataCoordinates::Degree));
  query->bindValue(":" + prefix + "rightx", rect.east(GeoDataCoordinates::Degree));
  query->bindValue(":" + prefix + "bottomy", rect.south(GeoDataCoordinates::Degree));
  query->bindValue(":" + prefix + "topy", rect.north(GeoDataCoordinates::Degree));
}

/* Inflates the rectangle and splits it at the antimeridian (date line) if it overlaps */
QList<Marble::GeoDataLatLonBox> splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect, double factor,
                                                    double increment)
{
  GeoDataLatLonBox newRect = rect;
  inflateQueryRect(newRect, factor, increment);

  if(newRect.crossesDateLine())
  {
    // Split in western and eastern part
    GeoDataLatLonBox westOf;
    westOf.setBoundaries(newRect.north(GeoDataCoordinates::Degree),
                         newRect.south(GeoDataCoordinates::Degree),
                         180.,
                         newRect.west(GeoDataCoordinates::Degree), GeoDataCoordinates::Degree);

    GeoDataLatLonBox eastOf;
    eastOf.setBoundaries(newRect.north(GeoDataCoordinates::Degree),
                         newRect.south(GeoDataCoordinates::Degree),
                         newRect.east(GeoDataCoordinates::Degree),
                         -180., GeoDataCoordinates::Degree);

    return QList<GeoDataLatLonBox>({westOf, eastOf});
  }
  else
    return QList<GeoDataLatLonBox>({newRect});
}

}
