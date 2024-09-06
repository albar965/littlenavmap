/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_WEBMAPCONTROLLER_H
#define LNM_WEBMAPCONTROLLER_H

#include "web/webflags.h"

#include "geo/rect.h"

#include <QPixmap>

class QPixmap;
class MapPaintWidget;

/*
 * Result of a map image creating also covering error messages, center position, zoom distance and shown rectangle.
 */
struct MapPixmap
{
  QPixmap pixmap;
  atools::geo::Pos pos; /* Map center */
  float requestedDistanceKm, /* Requested zoom distance */
        correctedDistanceKm; /* Actual zoom distance which can differ from above due to blur avoidance. */

  /* Empty if no error occured . */
  QString error;

  bool hasError() const
  {
    return !error.isEmpty();
  }

  bool hasNoError() const
  {
    return error.isEmpty();
  }

  bool isValid() const
  {
    return !pixmap.isNull();
  }

  bool isInvalid() const
  {
    return pixmap.isNull();
  }

};

/*
 * Wraps the MapPaintWidget and provides methods to retreive map images.
 *
 * The map widget has a state, i.e. it remains in the last shown position and zoom value.
 * Settings are copied from normal visible map window before rendering.
 *
 * This has to run in the main thread and event queue. Therefore, it is necessary to use queued signals to separate
 * a thread from the HTTP server.
 *
 * All methods avoid a blurry map by zoomin out to the next best level. This can result in different distances
 * than expected.
 */
class WebMapController :
  public QObject
{
  Q_OBJECT

public:
  explicit WebMapController(QWidget *parent, bool verboseParam);
  virtual ~WebMapController() override;

  WebMapController(const WebMapController& other) = delete;
  WebMapController& operator=(const WebMapController& other) = delete;

  /* Create or delete the map paint widget */
  void initMapPaintWidget();
  void deInitMapPaintWidget();

  /* Get pixmap with given width and height from current position. Qt::BlockingQueuedConnection */
  MapPixmap getPixmap(int width, int height);

  /* Get pixmap with given width and height for a map object like an airport, the user aircraft or a route.
   * Qt::BlockingQueuedConnection */
  MapPixmap getPixmapObject(int width, int height, web::ObjectType type, const QString& ident, float distanceKm);

  /* Get map at given position and distance. Command can be used to zoom in/out or scroll from the given position:
   * "in", "out", "left", "right", "up" and "down".
   * Qt::BlockingQueuedConnection */
  MapPixmap getPixmapPosDistance(int width, int height, atools::geo::Pos pos, float distanceKm, const QString& mapCommand,
                                 const QString& errorCase = QLatin1String(""));

  /* Zoom to rectangle on map. Qt::BlockingQueuedConnection */
  MapPixmap getPixmapRect(int width, int height, atools::geo::Rect rect, const QString& errorCase = tr("Invalid rectangle"));

  /* Get the map paint widget */
  MapPaintWidget *getMapPaintWidget() const;

  /* Need to clear caches and tear down queries before switching database */
  void preDatabaseLoad();

  /* Initialize queries again after a database change */
  void postDatabaseLoad();

private:
  MapPaintWidget *mapPaintWidget = nullptr;
  QWidget *parentWidget;
  bool verbose = false;
};

#endif // LNM_WEBMAPCONTROLLER_H
