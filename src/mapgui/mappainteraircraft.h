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

#ifndef LITTLENAVMAP_MAPPAINTERAIRCRAFT_H
#define LITTLENAVMAP_MAPPAINTERAIRCRAFT_H

#include "mapgui/mappainter.h"

#include <QCache>

namespace Marble {
class GeoDataLineString;
}

namespace atools {
namespace fs {
namespace sc {
class SimConnectAircraft;
class SimConnectUserAircraft;
}

}

}

class MapWidget;

/*
 * Draws the simulator user aircraft and aircraft track
 */
class MapPainterAircraft :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterAircraft(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale);
  virtual ~MapPainterAircraft();

  virtual void render(const PaintContext *context) override;

  enum AircraftType
  {
    AC_SMALL,
    AC_JET,
    AC_HELICOPTER
  };

  struct Key
  {
    bool operator==(const MapPainterAircraft::Key& other) const;

    AircraftType type;
    bool ground;
    bool user;
    int size;
  };

private:
  void paintAircraftTrack(Marble::GeoPainter *painter);
  void paintUserAircraft(const PaintContext *context,
                         const atools::fs::sc::SimConnectUserAircraft& userAircraft);
  void paintAiAircraft(const PaintContext *context,
                       const atools::fs::sc::SimConnectAircraft& aiAircraft);
  const QPixmap *pixmapFromCache(const Key& key);
  const QPixmap *pixmapFromCache(const atools::fs::sc::SimConnectAircraft& ac, int size, bool user);

  /* Minimum length in pixel of a track segment to be drawn */
  static Q_DECL_CONSTEXPR int AIRCRAFT_TRACK_MIN_LINE_LENGTH = 5;

  static Q_DECL_CONSTEXPR int DISTANCE_CUT_OFF_AI_LIMIT = 500;

  QCache<Key, QPixmap> pixmaps;

  void paintTextLabel(int size, const PaintContext *context, float x, float y,
                      const atools::fs::sc::SimConnectAircraft& aircraft);

};

#endif // LITTLENAVMAP_MAPPAINTERMARKAIRCRAFT_H
