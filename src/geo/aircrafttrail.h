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

#ifndef LITTLENAVMAP_AIRCRAFTTRAIL_H
#define LITTLENAVMAP_AIRCRAFTTRAIL_H

#include "geo/pos.h"
#include "geo/rect.h"

namespace Marble {
class GeoDataLatLonAltBox;
}
namespace map {
struct AircraftTrailSegment;
}
namespace atools {
namespace fs {

namespace pln {
class Flightplan;
}

namespace gpx {
class GpxData;
}
namespace sc {
class SimConnectUserAircraft;
}
}
namespace geo {
class Rect;
class LineString;
}
}

/* User aircraft trail position. Can be converted to QVariant and thus be saved to settings.
 *
 * An invalid position indicates a break of the trails.
 * Aircraft warp to destination above ground is not broken but a movement on ground or airport change is.
 * Done to avoid format changes. */
struct AircraftTrailPos
{
  AircraftTrailPos()
  {
  }

  AircraftTrailPos(atools::geo::PosD position, qint64 timeMs, bool ground)
    : pos(position), timestampMs(timeMs), onGround(ground)
  {
  }

  /* Create an invalid position which is used as a marker for separate trail segments */
  AircraftTrailPos(qint64 timeMs, bool ground)
    : timestampMs(timeMs), onGround(ground)
  {
  }

  bool isValid() const
  {
    return pos.isValid();
  }

  /* Get single old precision coordinate */
  const atools::geo::Pos getPosition() const
  {
    return pos.asPos();
  }

  /* Get full precision coordinate */
  const atools::geo::PosD& getPosD() const
  {
    return pos;
  }

  double getAltitudeD() const
  {
    return pos.getAltitude();
  }

  /* Milliseconds since Epoch UTC */
  qint64 getTimestampMs() const
  {
    return timestampMs;
  }

  bool isOnGround() const
  {
    return onGround;
  }

private:
  friend QDataStream& operator<<(QDataStream& dataStream, const AircraftTrailPos& trailPos);

  friend QDataStream& operator>>(QDataStream& dataStream, AircraftTrailPos& trailPos);

  atools::geo::PosD pos;
  qint64 timestampMs;
  bool onGround;
};

QDataStream& operator>>(QDataStream& dataStream, AircraftTrailPos& obj);
QDataStream& operator<<(QDataStream& dataStream, const AircraftTrailPos& obj);

Q_DECLARE_TYPEINFO(AircraftTrailPos, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(AircraftTrailPos)

/*
 * Stores the trail points of the flight simulator user aircraft.
 *
 * Points where the trail is interrupted (new flight) are indicated by invalid coordinates.
 * Warping at altitude does not interrupt a trail.
 *
 * Latest position is appended at end of list.
 */
class AircraftTrail :
  private QList<AircraftTrailPos>
{
public:
  AircraftTrail();
  ~AircraftTrail();

  AircraftTrail(const AircraftTrail& other);
  AircraftTrail& operator=(const AircraftTrail& other);

  /* Callback to convert lat/lon to x/y screen coordinates in findNearest() */
  typedef std::function<void (float lon, float lat, double& x, double& y)> ScreenCoordinatesFunc;

  /* Find one nearest trail segment. Screen point is equal to global position and used to check maxScreenDistance. */
  map::AircraftTrailSegment findNearest(const QPoint& point, const atools::geo::Pos& position, int maxScreenDistance,
                                        const Marble::GeoDataLatLonAltBox& viewportBox, ScreenCoordinatesFunc screenCoordinates) const;

  /* Build a GpxData from this object and assigns flight plan to it. */
  atools::fs::gpx::GpxData toGpxData(const atools::fs::pln::Flightplan& flightplan) const;

  /* Erases trail and populates this with the given gpxData.
   * @return number of removed points if the trail was truncated */
  int fillTrailFromGpxData(const atools::fs::gpx::GpxData& gpxData);

  /* Appends the given gpxData as new trail segment without deleting the current one.
   * @return number of removed points if the trail was truncated */
  int appendTrailFromGpxData(const atools::fs::gpx::GpxData& gpxData);

  /* Saves and restores trail into a separate file (little_navmap.trail). Creates two additional backup files. */
  void saveState(const QString& suffix, int numBackupFiles);
  void restoreState(const QString& suffix);

  void clearTrail();

  /*
   * Add a trail position. Accurracy depends on the ground flag which will cause more
   * or less points skipped.
   * @return number of removed points if the trail was truncated
   */
  int appendTrailPos(const atools::fs::sc::SimConnectUserAircraft& userAircraft, bool allowSplit);

  /* Get maximum altitude in all trail points */
  float getMaxAltitude() const
  {
    return maxAltitude;
  }

  float getMinAltitude() const
  {
    return minAltitude;
  }

  const atools::geo::Rect& getBounding() const
  {
    return boundingRect;
  }

  /* More than one linestring might be returned if the trail is interrupted.
   * Lines are also partitioned to avoid too large strings. */
  const QList<atools::geo::LineString>& getLineStrings() const
  {
    return lineStrings;
  }

  /* Shown trail will be truncated if it contains more entries than this value. Does not affect stored trail points. */
  void setMaxNumShownEntries(int value);

  /* Write and read the whole trail to and from a binary stream */
  void saveToStream(QDataStream& out);

  bool readFromStream(QDataStream& in);

  /* Pull only needed read-only methods of the base class into public space */
  using QList<AircraftTrailPos>::isEmpty;
  using QList<AircraftTrailPos>::at;
  using QList<AircraftTrailPos>::size;
  using QList<AircraftTrailPos>::constBegin;
  using QList<AircraftTrailPos>::constEnd;
  using QList<AircraftTrailPos>::constFirst;
  using QList<AircraftTrailPos>::constLast;

  /* Shallow compare. Compares min, max, size and bounding rect */
  bool almostEqual(const AircraftTrail& other) const;

  static int getMaxStoredTrailEntries()
  {
    return MAX_STORED_TRAIL_ENTRIES;
  }

private:
  friend QDataStream& operator>>(QDataStream& dataStream, AircraftTrailPos& trailPos);

  void clearBoundaries();
  void updateBoundary();
  void updateBoundaryLast();

  /* Remove consecutive invalid and with similar position */
  void cleanDuplicates();

  /* Copies the coordinates from the structs to a list of lineStrings. */
  void updateLineStrings();

  /* As above but incrementally. lastTrackPos must be added before */
  void updateLineStringsLast();

  /* Accurate positions for GPX export */
  const QList<QList<atools::geo::PosD> > positionsD() const;

  /* Same size as getLineStrings() but returns the timestamps in milliseconds since Epoch UTC for each position */
  const QList<QList<qint64> > timestampsMs() const;

  /* truncate to maxNumStoredEntries and return removed number */
  int truncateTrail();

  /* truncate lineStrings to maxNumShownEntries */
  void removeLineStringFirst();

  /* Absolute maximum, never exceeded for display and storage */
  const static int MAX_STORED_TRAIL_ENTRIES = 5000000;

  /* Split lines up into partitions for display */
  const static int PARTITION_POINT_SIZE = 500;

  /* Maximum number of trail points. If exceeded entries will be removed from beginning of the list. */
  int maxNumShownEntries = MAX_STORED_TRAIL_ENTRIES;

  float maxAltitude, minAltitude;
  atools::geo::Rect boundingRect;

  /* Trail density settings which depends on ground speed */
  float maxHeadingDiffDeg, maxSpeedDiffKts, maxAltDiffFtUpper, maxAltDiffFtLower, aglThresholdFt;

  /* Limit number of stored points. Default is same as in user interface in spinBoxSimMaxTrailPoints. */
  int maxNumStoredEntries = MAX_STORED_TRAIL_ENTRIES;

  int partitionPointSize = PARTITION_POINT_SIZE;

  /* Force point after time passed for ground and flying */
  qint64 maxFlyingTimeMs, maxGroundTimeMs;

  atools::fs::sc::SimConnectUserAircraft *lastUserAircraft;

  // Remember last values to detect changes in altitude or direction when recording points
  float lastGroundSpeedKts = 0.f, lastActualAltitudeFt = 0.f, lastHeadingDegTrue = 0.f;

  // Cached for display
  QList<atools::geo::LineString> lineStrings;
  int lineStringsSize = 0;

  // Points to the first AircraftTrailPos in this - in sync with lineStrings
  QList<int> lineStringsIndex;

  /* Needed in RouteExportFormat stream operators to read different formats */
  static quint16 version;
};

#endif // LITTLENAVMAP_AIRCRAFTTRAIL_H
