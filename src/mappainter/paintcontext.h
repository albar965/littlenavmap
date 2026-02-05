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

#ifndef LITTLENAVMAP_PAINTCONTEXT_H
#define LITTLENAVMAP_PAINTCONTEXT_H

#include "common/mapflags.h"
#include "geo/rect.h"
#include "options/optiondata.h"

#include <marble/MarbleGlobal.h>

#include <QPen>
#include <QFont>
#include <QDateTime>
#include <QCoreApplication>

namespace Marble {
class GeoPainter;
class ViewportParams;
}

class MapLayer;
class Route;

namespace map {
struct MapRef;
}

/* Struct that is passed to all painters. It is created from scratch for each paint event. */
struct PaintContext
{
  const MapLayer *mapLayer, /* Layer for the current zoom distance also affected by detail level
                             * Used for visibility of map objects */
                 *mapLayerText, /* layer for the current zoom distance also affected by text and label detail level
                                 * Used for visibility of labels */
                 *mapLayerEffective, /* Layer for the current zoom distance not affected by detail level.
                                      *  Used to determine text visibility and object sizes. */
                 *mapLayerRoute, /* Layer for the current zoom distance and more details for route. */
                 *mapLayerRouteText; /* Layer for the current zoom distance and more details for route labels. */

  Marble::GeoPainter *painter;
  Marble::ViewportParams *viewport;
  Marble::ViewContext viewContext;
  float zoomDistanceMeter;

  bool drawFast; /* true if reduced details should be used */
  bool lazyUpdate; /* postpone reloading until map is still */
  bool darkMap; /* CartoDark or similar. Not Night mode */

  map::MapTypes objectTypes; /* Object types that should be drawn */
  map::MapDisplayTypes objectDisplayTypes; /* Object types that should be drawn */
  map::MapAirspaceFilter airspaceFilterByLayer; /* Airspaces */
  map::MapAirspaceType airspaceTextsByLayer;

  atools::geo::Rect viewportRect; /* Rectangle of current viewport */
  QRect screenRect; /* Screen coordinate rect */

  opts::MapScrollDetail mapScrollDetail; /* Option that indicates the detail level when drawFast is true */
  QFont defaultFont /* Default widget font */;
  float distanceNm; /* Zoom distance in NM */
  float distanceKm; /* Zoom distance in KM as in map widget */
  QStringList userPointTypes, /* In menu selected types */
              userPointTypesAll; /* All available tyes */
  bool userPointTypeUnknown; /* Show unknown types */

  const Route *route;

  optsac::DisplayOptionsUserAircraft dispOptsUser;
  optsac::DisplayOptionsAiAircraft dispOptsAi;
  optsd::DisplayOptionsAirport dispOptsAirport;
  optsd::DisplayOptionsRose dispOptsRose;
  optsd::DisplayOptionsMeasurement dispOptsMeasurement;
  optsd::DisplayOptionsRoute dispOptsRoute;

  /* ===============================================================================
   * Flags from options dialog */
  opts::Flags flags;
  opts2::Flags2 flags2;

  map::MapWeatherSource weatherSource;
  bool visibleWidget;
  bool paintCopyright = true, paintWindHeader = true, webMap = false;
  int mimimumRunwayLengthFt = -1; /* Value from toolbar */
  int currentDistanceMarkerId = -1;

  /* ===============================================================================
   * Ids which are filled during painting and are passes between painters */

  // All waypoints from the route and add them to the map to avoid duplicate drawing
  // Same for procedure preview
  QSet<map::MapRef> routeProcIdMap, /* Navaids on plan */
                    routeProcIdMapRec /* Recommended navaids */;

  /* Airports drawn having parking spots which require tooltips and more */
  QSet<int> *shownDetailAirportIds;

  /* All navaids drawn for route and procedures. Points to vector in MapScreenIndex */
  QList<map::MapRef> *routeDrawnNavaids;

  /* Global scale applied to all features, labels and symbols */
  float sizeAll = 1.f;

  /* ===============================================================================
   * Text sizes and line thickness in percent / 100 as set in options dialog. To be used with szF() or szFont() */
  float textSizeAircraftAi = 1.f;
  float symbolSizeNavaid = 1.f;
  float symbolSizeUserpoint = 1.f;
  float symbolSizeHighlight = 1.f;
  float thicknessFlightplan = 1.f;
  float textSizeNavaid = 1.f;
  float textSizeAirspace = 1.f;
  float textSizeUserpoint = 1.f;
  float textSizeAirway = 1.f;
  float thicknessAirway = 1.f;
  float textSizeCompassRose = 1.f;
  float textSizeRangeUserFeature = 1.f;
  float textSizeRangeMeasurement = 1.f;
  float symbolSizeAirport = 1.f;
  float symbolSizeAirportWeather = 1.f;
  float symbolSizeWindBarbs = 1.f;
  float symbolSizeAircraftAi = 1.f;
  float textSizeFlightplan = 1.f;
  float textSizeAircraftUser = 1.f;
  float symbolSizeAircraftUser = 1.f;
  float textSizeAirport = 1.f;
  float textSizeAirportRunway = 1.f;
  float textSizeAirportTaxiway = 1.f;
  float thicknessTrail = 1.f;
  float thicknessUserFeature = 1.f;
  float thicknessMeasurement = 1.f;
  float thicknessCompassRose = 1.f;
  float textSizeMora = 1.f;
  float transparencyMora = 1.f;
  float textSizeAirportMsa = 1.f;
  float transparencyAirportMsa = 1.f;
  float transparencyFlightplan = 1.f;
  float transparencyHighlight = 1.f;

  int objectCount = 0;
  bool queryOverflow = false;

  /* Increase drawn object count and return true if exceeded */
  bool objCount()
  {
    objectCount++;
    return objectCount >= map::MAX_MAP_OBJECTS;
  }

  bool isObjectOverflow() const
  {
    return objectCount >= map::MAX_MAP_OBJECTS;
  }

  int getObjectCount() const
  {
    return objectCount;
  }

  void setQueryOverflow(bool overflow)
  {
    queryOverflow |= overflow;
  }

  bool isQueryOverflow() const
  {
    return queryOverflow;
  }

  bool  dOptUserAc(optsac::DisplayOptionsUserAircraft opts) const
  {
    return dispOptsUser & opts;
  }

  bool  dOptAiAc(optsac::DisplayOptionsAiAircraft opts) const
  {
    return dispOptsAi & opts;
  }

  bool  dOptAp(optsd::DisplayOptionsAirport opts) const
  {
    return dispOptsAirport & opts;
  }

  bool  dOptRose(optsd::DisplayOptionsRose opts) const
  {
    return dispOptsRose & opts;
  }

  bool  dOptMeasurement(optsd::DisplayOptionsMeasurement opts) const
  {
    return dispOptsMeasurement & opts;
  }

  bool  dOptRoute(optsd::DisplayOptionsRoute opts) const
  {
    return dispOptsRoute & opts;
  }

  /* Calculate real symbol size */
  int sz(float scale, int size) const
  {
    return static_cast<int>(std::round(scale * size * sizeAll));
  }

  int sz(float scale, float size) const
  {
    return static_cast<int>(std::round(scale * size * sizeAll));
  }

  int sz(float scale, double size) const
  {
    return static_cast<int>(std::round(scale * size * sizeAll));
  }

  float szF(float scale, int size) const
  {
    return scale * size * sizeAll;
  }

  float szF(float scale, float size) const
  {
    return scale * size * sizeAll;
  }

  float szF(float scale, double size) const
  {
    return scale * static_cast<float>(size) * sizeAll;
  }

  /* Calculate and set font based on scale */
  void szFont(float scale) const;

  /* Calculate label text flags for route waypoints depending on layer settings */
  textflags::TextFlags airportTextFlags() const;
  textflags::TextFlags airportTextFlagsMinor() const;
  textflags::TextFlags airportTextFlagsRoute(bool drawAsRoute, bool drawAsLog) const;

  void startTimer(const QString& label)
  {
    if(verboseDraw)
      renderTimesMs.insert(label, QDateTime::currentMSecsSinceEpoch());
  }

  void endTimer(const QString& label)
  {
    if(verboseDraw)
      renderTimesMs.insert(label, QDateTime::currentMSecsSinceEpoch() - renderTimesMs.value(label));
  }

  void clearTimer()
  {
    if(verboseDraw)
      renderTimesMs.clear();
  }

  bool verboseDraw = false;
  QMap<QString, qint64> renderTimesMs;
};

#endif // LITTLENAVMAP_PAINTCONTEXT_H
