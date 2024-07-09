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

#include "common/aircrafttrail.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptypes.h"
#include "fs/gpx/gpxtypes.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "geo/calculations.h"
#include "geo/linestring.h"
#include "io/fileroller.h"
#include "settings/settings.h"

#include <QDataStream>
#include <QDateTime>
#include <QFile>

#include <marble/GeoDataLatLonAltBox.h>

using atools::geo::Pos;

quint16 AircraftTrail::version = 0;

/* Insert an invalid position as an break indicator if aircraft jumps too far on ground. */
static const float MAX_POINT_DISTANCE_NM = 5.f;

/* Number of entries to remove at once */
static const int TRUNCATE_TRACK_ENTRIES = 200;

static const quint32 FILE_MAGIC_NUMBER = 0x5B6C1A2B;

/* Version 2 to adds timstamp and single floating point precision. Uses 32-bit second timestamps */
static const quint16 FILE_VERSION_OLD = 2;

/* Version 3 adds 64-bit millisecond values - still 32 bit coordinates */
static const quint16 FILE_VERSION_64BIT_TS = 3;

/* Version 4 adds double floating point precision for coordinates */
static const quint16 FILE_VERSION_64BIT_COORDS = 4;

QDataStream& operator>>(QDataStream& dataStream, AircraftTrailPos& trackPos)
{
  if(AircraftTrail::version == FILE_VERSION_64BIT_COORDS)
  {
    // Newest format with 64 bit coordinates and 64 bit timestamp
    dataStream >> trackPos.pos >> trackPos.timestampMs >> trackPos.onGround;

    // Fix invalid positions from wrong decoding of earlier versions
    if(!trackPos.pos.isValidRange() || trackPos.pos.isNull())
      // Add separator
      trackPos.pos = atools::geo::PosD();
  }
  else if(AircraftTrail::version == FILE_VERSION_64BIT_TS)
  {
    // Convert 32 bit coordinates
    atools::geo::Pos pos;
    dataStream >> pos >> trackPos.timestampMs >> trackPos.onGround;

    // Convert invalid float positions to invalid double positions to indicate split track
    trackPos.pos = atools::geo::PosD(pos);
  }
  else if(AircraftTrail::version == FILE_VERSION_OLD)
  {
    // Convert 32 bit timestamp and coordinates
    atools::geo::Pos pos;
    quint32 oldTimestampSeconds;
    dataStream >> pos >> oldTimestampSeconds >> trackPos.onGround;
    trackPos.timestampMs = oldTimestampSeconds * 1000L;

    // Convert invalid float positions to invalid double positions to indicate split track
    trackPos.pos = atools::geo::PosD(pos);
  }

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const AircraftTrailPos& trackPos)
{
#ifdef DEBUG_SAVE_TRACK_OLD
  dataStream << trackPos.pos.asPos() << trackPos.timestampMs << trackPos.onGround;
#else
  dataStream << trackPos.pos << trackPos.timestampMs << trackPos.onGround;
#endif
  return dataStream;
}

// ==========================================================================================
AircraftTrail::AircraftTrail()
{
  lastUserAircraft = new atools::fs::sc::SimConnectUserAircraft;

  // Load settings for trail point density on ground ===========================
  // All conditions are combined using OR
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Force point after time passed
  maxGroundTimeMs = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "MaxGroundTimeMs", 10000).toInt();

  // Load settings for trail point density when flying ===========================
  // Force point after time passed
  maxFlyingTimeMs = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "MaxFlyingTimeMs", 30000).toInt();

  // Changes in aircraft parameters trigger a new point
  maxHeadingDiffDeg = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "MaxHeadingDiffDeg", 5.).toFloat();
  maxSpeedDiffKts = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "MaxGsDiffKts", 10.).toFloat();

  maxAltDiffFtUpper = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "MaxAltDiffFtUpper", 500.).toFloat();
  maxAltDiffFtLower = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "MaxAltDiffFtLower", 50.).toFloat();

  // Use maxAltDiffFtLower if below or maxAltDiffFtUpper if above - interpolated
  aglThresholdFt = settings.getAndStoreValue(lnm::SETTINGS_AIRCRAFT_TRAIL + "AglThresholdFt", 10000.).toFloat();
}

AircraftTrail::~AircraftTrail()
{
  delete lastUserAircraft;
}

AircraftTrail::AircraftTrail(const AircraftTrail& other)
  : QList<AircraftTrailPos>(other)
{
  lastUserAircraft = new atools::fs::sc::SimConnectUserAircraft;
  this->operator=(other);
}

AircraftTrail& AircraftTrail::operator=(const AircraftTrail& other)
{
  clear();
  append(other);
  maxTrackEntries = other.maxTrackEntries;
  *lastUserAircraft = *other.lastUserAircraft;

  boundingRect = other.boundingRect;
  maxHeadingDiffDeg = other.maxHeadingDiffDeg;
  maxSpeedDiffKts = other.maxSpeedDiffKts;
  maxAltDiffFtUpper = other.maxAltDiffFtUpper;
  maxAltDiffFtLower = other.maxAltDiffFtLower;
  aglThresholdFt = other.aglThresholdFt;
  maxFlyingTimeMs = other.maxFlyingTimeMs;
  maxGroundTimeMs = other.maxGroundTimeMs;

  lastGroundSpeedKts = other.lastGroundSpeedKts;
  lastActualAltitudeFt = other.lastActualAltitudeFt;
  lastHeadingDegTrue = other.lastHeadingDegTrue;

  lineStrings = other.lineStrings;

  return *this;
}

map::AircraftTrailSegment AircraftTrail::findNearest(const QPoint& point, const atools::geo::Pos& position, int maxScreenDistance,
                                                     const Marble::GeoDataLatLonAltBox& viewportBox,
                                                     ScreenCoordinatesFunc screenCoordinates) const
{
  map::AircraftTrailSegment trail;
  if(size() <= 1)
    return trail;

  atools::geo::Rect viewportRect(viewportBox.west(Marble::GeoDataCoordinates::Degree),
                                 viewportBox.north(Marble::GeoDataCoordinates::Degree),
                                 viewportBox.east(Marble::GeoDataCoordinates::Degree),
                                 viewportBox.south(Marble::GeoDataCoordinates::Degree));

  int trackIndex = -1;
  int indexTracks = 0;
  atools::geo::LineDistance result, resultLine, resultShortest, resultShortestLine;
  resultShortest.distance = map::INVALID_DISTANCE_VALUE;

  for(const atools::geo::LineString& lineString : lineStrings)
  {
    atools::geo::Rect trackBounding = lineString.boundingRect();

#ifdef DEBUG_INFORMATION_TRACK_NEAREST
    qDebug() << Q_FUNC_INFO << "###############" << "bounding" << boundingRect << "viewportRect" << viewportRect;
#endif

    if(trackBounding.overlaps(viewportRect))
    {
      int idx = -1;
      lineString.distanceMeterToLineString(position, result, &resultLine, &idx, &viewportRect);

      if(std::abs(result.distance) < std::abs(resultShortest.distance) && result.status == atools::geo::ALONG_TRACK)
      {
        resultShortest = result;
        resultShortestLine = resultLine;
        trackIndex = idx + indexTracks;
      }
    }

    indexTracks += lineString.size() + 1;
  }

#ifdef DEBUG_INFORMATION_TRACK_NEAREST
  qDebug() << Q_FUNC_INFO << "resultShortest" << resultShortest << "trackIndex" << trackIndex << "size()" << size();
  qDebug() << Q_FUNC_INFO << "resultLine" << resultLine;
#endif

  AircraftTrailPos posFrom, posTo;
  Pos pos;

  if(trackIndex >= 0 && trackIndex < size() - 1 && resultShortest.status == atools::geo::ALONG_TRACK)
  {
    posFrom = at(trackIndex);
    posTo = at(trackIndex + 1);
    atools::geo::Line line(posFrom.getPosition(), posTo.getPosition());

    double length = posFrom.getPosD().distanceMeterTo(posTo.getPosD());
    double fraction = resultShortestLine.distanceFrom1 / length;
    pos = line.interpolate(static_cast<float>(length), static_cast<float>(fraction));
    trackIndex++;

    double xs, ys;
    screenCoordinates(pos.getLonX(), pos.getLatY(), xs, ys);

    if(atools::geo::manhattanDistance(point.x(), point.y(), atools::roundToInt(xs), atools::roundToInt(ys)) < maxScreenDistance)
    {
#ifdef DEBUG_INFORMATION_TRACK_NEAREST
      qDebug() << Q_FUNC_INFO << "NEAR" << pos << point << QPoint(static_cast<int>(xs), static_cast<int>(ys));
#endif
      double distFrom1 = static_cast<double>(resultShortestLine.distanceFrom1);
      double travelTime = static_cast<double>(posTo.getTimestampMs() - posFrom.getTimestampMs());

      // Set interpolated altitude
      pos.setAltitude(static_cast<float>(atools::interpolate(posFrom.getAltitudeD(), posTo.getAltitudeD(), 0., length, distFrom1)));
      trail.position = pos;
      trail.index = trackIndex;
      trail.from = posFrom.getPosition();
      trail.to = posTo.getPosition();
      trail.length = static_cast<float>(length);
      trail.distanceFromStart = resultShortest.distanceFrom1;
      trail.speed = static_cast<float>(length / travelTime * 1000.f);
      trail.headingTrue = static_cast<float>(posFrom.getPosD().angleDegTo(posTo.getPosD()));
      trail.timestampPos = posFrom.getTimestampMs() + std::lround(travelTime * fraction);
      trail.onGround = atools::roundToInt(atools::interpolate(static_cast<double>(posFrom.isOnGround()),
                                                              static_cast<double>(posTo.isOnGround()), 0., length, distFrom1));
    }
    else
    {
#ifdef DEBUG_INFORMATION_TRACK_NEAREST
      qDebug() << Q_FUNC_INFO << "MISSED" << pos << point << QPoint(static_cast<int>(xs), static_cast<int>(ys));
#endif
      trackIndex = -1;
      trail.position = Pos();
    }
  }
  return trail;
}

atools::fs::gpx::GpxData AircraftTrail::toGpxData(const atools::fs::pln::Flightplan& flightplan) const
{
  atools::fs::gpx::GpxData gpxData;
  gpxData.setFlightplan(flightplan);

  const QVector<QVector<atools::geo::PosD> >& positionList = getPositionsD();
  const QVector<QVector<qint64> >& timestampsList = getTimestampsMs();

  Q_ASSERT(positionList.size() == timestampsList.size());

  for(int i = 0; i < positionList.size(); i++)
  {
    const QVector<atools::geo::PosD>& positions = positionList.at(i);
    const QVector<qint64>& timestamps = timestampsList.at(i);

    Q_ASSERT(positions.size() == timestamps.size());

    atools::fs::gpx::TrailPoints points;
    for(int j = 0; j < positions.size(); j++)
      points.append(atools::fs::gpx::TrailPoint(positions.at(j), timestamps.at(j)));
    gpxData.appendTrailPoints(points);
  }

  return gpxData;
}

int AircraftTrail::fillTrailFromGpxData(const atools::fs::gpx::GpxData& gpxData)
{
  clear();
  return appendTrailFromGpxData(gpxData);
}

int AircraftTrail::appendTrailFromGpxData(const atools::fs::gpx::GpxData& gpxData)
{
  // Add separator
  if(!isEmpty())
    append(AircraftTrailPos());

  // Add track points
  for(const atools::fs::gpx::TrailPoints& points : gpxData.getTrails())
  {
    if(!points.isEmpty())
    {
      for(const atools::fs::gpx::TrailPoint& point : qAsConst(points))
        append(AircraftTrailPos(point.pos, point.timestampMs, false));
      append(AircraftTrailPos());
    }
  }
  int numTruncated = truncateTrail();

  updateBoundary();
  updateLineStrings();

  return numTruncated;
}

void AircraftTrail::saveState(const QString& suffix, int numBackupFiles)
{
  QFile trackFile(atools::settings::Settings::getConfigFilename(suffix));

  if(numBackupFiles > 0)
    atools::io::FileRoller(numBackupFiles).rollFile(trackFile.fileName());

  if(trackFile.open(QIODevice::WriteOnly))
  {
    QDataStream out(&trackFile);
    saveToStream(out);
    trackFile.close();
  }
  else
    qWarning() << "Cannot write track" << trackFile.fileName() << ":" << trackFile.errorString();
}

void AircraftTrail::restoreState(const QString& suffix)
{
  clear();

  QFile trackFile(atools::settings::Settings::getConfigFilename(suffix));
  if(trackFile.exists())
  {
    if(trackFile.open(QIODevice::ReadOnly))
    {
      QDataStream in(&trackFile);
      readFromStream(in);
      trackFile.close();
    }
    else
      qWarning() << "Cannot read track" << trackFile.fileName() << ":" << trackFile.errorString();
  }

  qDebug() << Q_FUNC_INFO << "Trail size" << size();

  updateBoundary();
  updateLineStrings();
}

void AircraftTrail::saveToStream(QDataStream& out)
{
  out.setVersion(QDataStream::Qt_5_5);

#ifdef DEBUG_SAVE_TRACK_OLD
  out.setFloatingPointPrecision(QDataStream::SinglePrecision);
  out << FILE_MAGIC_NUMBER << FILE_VERSION_64BIT_TS << *this;
#else
  out.setFloatingPointPrecision(QDataStream::DoublePrecision);
  out << FILE_MAGIC_NUMBER << FILE_VERSION_64BIT_COORDS << *this;
#endif
}

bool AircraftTrail::readFromStream(QDataStream& in)
{
  bool retval = false;
  clear();

  quint32 magic;
  in.setVersion(QDataStream::Qt_5_5);
  in >> magic;

  if(magic == FILE_MAGIC_NUMBER)
  {
    in >> AircraftTrail::version;
    if(AircraftTrail::version == FILE_VERSION_64BIT_TS || AircraftTrail::version == FILE_VERSION_OLD)
    {
      // Read old float coordinates
      in.setFloatingPointPrecision(QDataStream::SinglePrecision);
      in >> *this;
      retval = true;
    }
    else if(AircraftTrail::version == FILE_VERSION_64BIT_COORDS)
    {
      // Read new double coordinates
      in.setFloatingPointPrecision(QDataStream::DoublePrecision);
      in >> *this;
      retval = true;
    }
    else
      qWarning() << "Cannot read track. Invalid version number:" << AircraftTrail::version;
  }
  else
    qWarning() << "Cannot read track. Invalid magic number:" << magic;

  return retval;
}

bool AircraftTrail::almostEqual(const AircraftTrail& other) const
{
  return size() == other.size() &&
         getBounding().almostEqual(other.getBounding(), atools::geo::Pos::POS_EPSILON_1M) &&
         atools::almostEqual(getMaxAltitude(), other.getMaxAltitude(), 10.f) &&
         atools::almostEqual(getMinAltitude(), other.getMinAltitude(), 10.f);
}

int AircraftTrail::appendTrailPos(const atools::fs::sc::SimConnectUserAircraft& userAircraft, bool allowSplit)
{
  using atools::almostNotEqual;
  using atools::geo::angleAbsDiff;

  if(!userAircraft.isValid())
  {
#ifdef DEBUG_INFORMATION_TRAIL
    qDebug() << Q_FUNC_INFO << "Aircraft not valid";
#endif
    return false;
  }

  if(!lastUserAircraft->isValid() && !userAircraft.isFullyValid())
  {
    // Avoid spurious aircraft repositioning to 0/0 like done by MSFS
#ifdef DEBUG_INFORMATION_TRAIL
    qDebug() << Q_FUNC_INFO << "Aircraft not fully valid";
#endif
    return false;
  }

  int numTruncated = false;
  bool changed = false, split = false;
  qint64 timestampMs = userAircraft.getZuluTime().toMSecsSinceEpoch();
  atools::geo::PosD posD = userAircraft.getPositionD();
  bool onGround = userAircraft.isOnGround();

  if(isEmpty() && userAircraft.isValid())
  {
    // First point
    append(AircraftTrailPos(posD, timestampMs, onGround));
    *lastUserAircraft = userAircraft;
    changed = true;
  }
  else
  {
    // Use a smaller distance on ground before storing position - distance depends on ground speed
    qint64 maxTimeMs = onGround ? maxGroundTimeMs : maxFlyingTimeMs;
    float actualAltitudeFt = userAircraft.getActualAltitudeFt();

    float maxAltDiffFt;
    if(userAircraft.getAltitudeAboveGroundFt() < aglThresholdFt)
      maxAltDiffFt =
        atools::interpolate(maxAltDiffFtLower, maxAltDiffFtUpper, 0.f, aglThresholdFt, userAircraft.getAltitudeAboveGroundFt());
    else
      maxAltDiffFt = maxAltDiffFtUpper;

#ifdef DEBUG_INFORMATION_TRAIL
    qDebug() << Q_FUNC_INFO << "maxAltDiffFt" << maxAltDiffFt;
#endif

    // Test if any aircraft parameters have changed to create a point
    bool speedChanged = almostNotEqual(lastGroundSpeedKts, userAircraft.getGroundSpeedKts(), maxSpeedDiffKts);
    bool altChanged = almostNotEqual(lastActualAltitudeFt, actualAltitudeFt, maxAltDiffFt);
    bool headingChanged = atools::geo::angleAbsDiff(lastHeadingDegTrue, userAircraft.getHeadingDegTrue()) > maxHeadingDiffDeg;

    // bool aboveMinDistance = pos.distanceMeterTo(last.getPosition()) > minDistanceMeter;
    const AircraftTrailPos& last = constLast();
    bool maxTimeExceeded = almostNotEqual(last.getTimestampMs(), timestampMs, maxTimeMs);

#ifdef DEBUG_INFORMATION_TRAIL
    if(maxTimeExceeded)
      qDebug() << Q_FUNC_INFO << "MAX TIME" << last.getTimestampMs() << "-" << timestampMs << ">" << maxTimeMs;
    if(speedChanged)
      qDebug() << Q_FUNC_INFO << "SPEED CHANGED" << lastUserAircraft->getGroundSpeedKts() << userAircraft.getGroundSpeedKts();
    if(altChanged)
      qDebug() << Q_FUNC_INFO << "ALT CHANGED" << lastUserAircraft->getActualAltitudeFt() << userAircraft.getActualAltitudeFt();
    if(headingChanged)
      qDebug() << Q_FUNC_INFO << "HDG CHANGED" << lastUserAircraft->getHeadingDegTrue() << userAircraft.getHeadingDegTrue();
#endif

    // New point if maximum time is exceeded or parameter change above minimum point distance
    if(maxTimeExceeded || speedChanged || altChanged || headingChanged)
    {
#ifdef DEBUG_INFORMATION_TRAIL
      qDebug() << Q_FUNC_INFO << "CHANGED ########";
#endif
      bool lastValid = lastUserAircraft->isValid();
      bool aircraftChanged = lastValid && lastUserAircraft->hasAircraftChanged(userAircraft);
      bool jumped = !isEmpty() && posD.asPos().distanceMeterTo(last.getPosition()) > atools::geo::nmToMeter(MAX_POINT_DISTANCE_NM);

      // Points where the track is interrupted (new flight) are indicated by invalid coordinates.
      // Warping at altitude does not interrupt a track.
      if(allowSplit && jumped && (last.isOnGround() || onGround || aircraftChanged))
      {
#ifdef DEBUG_INFORMATION_TRAIL
        qDebug() << Q_FUNC_INFO << "SPLITTING TRAIL ###################" << "allowSplit" << allowSplit << "jumped" << jumped
                 << "last.onGround" << last.isOnGround() << "onGround" << onGround << "aircraftChanged" << aircraftChanged;
#endif
        // Add an invalid position before indicating a break
        append(AircraftTrailPos(timestampMs, onGround));
        split = true;
      }
      else
        numTruncated += truncateTrail();

      append(AircraftTrailPos(posD, timestampMs, onGround));
      changed = true;

      // Update changed values for new added point
      lastGroundSpeedKts = userAircraft.getGroundSpeedKts();
      lastActualAltitudeFt = userAircraft.getActualAltitudeFt();
      lastHeadingDegTrue = userAircraft.getHeadingDegTrue();
    } // if(maxTimeExceeded || speedChanged || altChanged || headingChanged)

    *lastUserAircraft = userAircraft;
  } // if(isEmpty() && userAircraft.isValid()) ... else

  if(!isEmpty())
  {
    if(split)
    {
      // Trail was split - do a full update
      updateBoundary();
      updateLineStrings();
    }
    else if(changed)
    {
      // Last one is always valid
      updateBoundary(constLast());
      updateLineStrings(constLast());
    }
  }
  else
  {
    // Trail is empty - clear cached values
    clearBoundaries();
    lineStrings.clear();
  }

#ifdef DEBUG_INFORMATION_TRAIL
  qDebug() << Q_FUNC_INFO << size();
  qDebug() << Q_FUNC_INFO << constLast().getPosition();
  qDebug() << Q_FUNC_INFO << lineStrings;
#endif

  return numTruncated;
}

int AircraftTrail::truncateTrail()
{
  int numTruncated = 0;
  int maxEntries = maxTrackEntries <= 0 || maxTrackEntries > MAX_TRACK_ENTRIES ? MAX_TRACK_ENTRIES : maxTrackEntries;

  while(size() > maxEntries)
  {
    for(int i = 0; i < TRUNCATE_TRACK_ENTRIES; i++)
    {
      numTruncated++;
      removeFirst();
    }

    // Remove invalid segments
    while(!isEmpty() && !constFirst().isValid())
    {
      numTruncated++;
      removeFirst();
    }
  }

  return numTruncated;
}

void AircraftTrail::clearTrail()
{
  clear();
  clearBoundaries();
  lineStrings.clear();
}

void AircraftTrail::clearBoundaries()
{
  boundingRect = atools::geo::Rect();
  minAltitude = std::numeric_limits<float>::max();
  maxAltitude = std::numeric_limits<float>::min();
}

void AircraftTrail::updateBoundary()
{
  clearBoundaries();

  atools::geo::LineString positions;
  for(const AircraftTrailPos& trackPos : qAsConst(*this))
  {
    if(trackPos.isValid())
    {
      positions.append(trackPos.getPosition());
      float alt = static_cast<float>(trackPos.getPosD().getAltitude());
      minAltitude = std::min(minAltitude, alt);
      maxAltitude = std::max(maxAltitude, alt);
    }
  }

  boundingRect = atools::geo::bounding(positions);
}

void AircraftTrail::updateBoundary(const AircraftTrailPos& trackPos)
{
  if(trackPos.isValid())
  {
    float alt = static_cast<float>(trackPos.getPosD().getAltitude());
    minAltitude = std::min(minAltitude, alt);
    maxAltitude = std::max(maxAltitude, alt);
    boundingRect.extend(trackPos.getPosition());
  }
}

void AircraftTrail::updateLineStrings(const AircraftTrailPos& trackPos)
{
  if(trackPos.isValid())
  {
    if(lineStrings.isEmpty())
      lineStrings.append(atools::geo::LineString(trackPos.getPosition()));
    else
      lineStrings.last().append(trackPos.getPosition());
  }
}

void AircraftTrail::updateLineStrings()
{
  lineStrings.clear();

  if(!isEmpty())
  {
    atools::geo::LineString linestring;
    lineStrings.reserve(size());

    for(const AircraftTrailPos& trackPos : qAsConst(*this))
    {
      if(!trackPos.isValid())
      {
        // An invalid position shows a break in the lines - add line and start a new one
        lineStrings.append(linestring);
        linestring.clear();
      }
      else
        linestring.append(trackPos.getPosition());
    }

    // Add rest
    if(!linestring.isEmpty())
      lineStrings.append(linestring);
  }
}

const QVector<QVector<atools::geo::PosD> > AircraftTrail::getPositionsD() const
{
  QVector<QVector<atools::geo::PosD> > linestringsD;

  if(!isEmpty())
  {
    QVector<atools::geo::PosD> line;
    linestringsD.reserve(size());

    for(const AircraftTrailPos& trackPos : *this)
    {
      if(!trackPos.isValid())
      {
        // An invalid position shows a break in the lines - add line and start a new one
        linestringsD.append(line);
        line.clear();
      }
      else
        line.append(trackPos.getPosD());
    }

    // Add rest
    if(!line.isEmpty())
      linestringsD.append(line);
  }
  return linestringsD;
}

const QVector<QVector<qint64> > AircraftTrail::getTimestampsMs() const
{
  QVector<QVector<qint64> > timestamps;

  if(!isEmpty())
  {
    QVector<qint64> times;
    timestamps.reserve(size());

    for(const AircraftTrailPos& trackPos : *this)
    {
      if(!trackPos.isValid())
      {
        // An invalid position shows a break in the lines - start a new list
        timestamps.append(times);
        times.clear();
      }
      else
        times.append(trackPos.getTimestampMs());
    }

    // Add rest
    if(!times.isEmpty())
      timestamps.append(times);
  }
  return timestamps;
}
