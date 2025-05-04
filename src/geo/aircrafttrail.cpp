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

#include "geo/aircrafttrail.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptypes.h"
#include "fs/gpx/gpxtypes.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "geo/calculations.h"
#include "geo/linestring.h"
#include "geo/marbleconverter.h"
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

/* Split lines up for display */
static const int PARTITION_POINT_SIZE = 200;

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
  lineStringsIndex = other.lineStringsIndex;

  return *this;
}

map::AircraftTrailSegment AircraftTrail::findNearest(const QPoint& point, const atools::geo::Pos& position, int maxScreenDistance,
                                                     const Marble::GeoDataLatLonAltBox& viewportBox,
                                                     ScreenCoordinatesFunc screenCoordinates) const
{
  map::AircraftTrailSegment trailSegment;
  if(size() <= 1)
    return trailSegment;

  atools::geo::Rect viewportRect = mconvert::fromGdc(viewportBox);

  int trackIndexNearest = -1;
  atools::geo::LineDistance resultNearest, resultNearestLine;
  resultNearest.distance = map::INVALID_DISTANCE_VALUE;

  for(int i = 0; i < lineStrings.size(); i++)
  {
    const atools::geo::LineString& lineString = lineStrings.at(i);
    atools::geo::Rect trackBounding = lineString.boundingRect();

#ifdef DEBUG_INFORMATION_TRACK_NEAREST
    qDebug() << Q_FUNC_INFO << "###############" << "bounding" << boundingRect << "viewportRect" << viewportRect;
#endif

    if(trackBounding.overlaps(viewportRect))
    {
      int posInLineIdx = -1;
      atools::geo::LineDistance result, resultLine;
      lineString.distanceMeterToLineString(position, result, &resultLine, &posInLineIdx, &viewportRect);

      if(std::abs(result.distance) < std::abs(resultNearest.distance) && result.status == atools::geo::ALONG_TRACK)
      {
        resultNearest = result; // Numbers related to full linestring
        resultNearestLine = resultLine; // Numbers related to found line segment
        trackIndexNearest = lineStringsIndex.at(i) + posInLineIdx;
      }
    }
  }

#ifdef DEBUG_INFORMATION_TRACK_NEAREST
  qDebug() << Q_FUNC_INFO << "resultNearest" << resultNearest << "trackIndexNearest" << trackIndexNearest << "size()" << size();
  qDebug() << Q_FUNC_INFO << "resultNearestLine" << resultNearestLine;
#endif

  AircraftTrailPos posFrom, posTo;
  Pos pos;

  if(trackIndexNearest >= 0 && trackIndexNearest < size() - 1 && resultNearest.status == atools::geo::ALONG_TRACK)
  {
    posFrom = at(trackIndexNearest);
    posTo = at(trackIndexNearest + 1);
    atools::geo::Line line(posFrom.getPosition(), posTo.getPosition());

    double length = posFrom.getPosD().distanceMeterTo(posTo.getPosD());
    double fraction = resultNearestLine.distanceFrom1 / length;
    pos = line.interpolate(static_cast<float>(length), static_cast<float>(fraction));
    trackIndexNearest++;

    double xs, ys;
    screenCoordinates(pos.getLonX(), pos.getLatY(), xs, ys);

    if(atools::geo::manhattanDistance(point.x(), point.y(), atools::roundToInt(xs), atools::roundToInt(ys)) < maxScreenDistance)
    {
#ifdef DEBUG_INFORMATION_TRACK_NEAREST
      qDebug() << Q_FUNC_INFO << "NEAR" << pos << point << QPoint(static_cast<int>(xs), static_cast<int>(ys));
#endif
      double distFrom1 = static_cast<double>(resultNearestLine.distanceFrom1);
      double travelTime = static_cast<double>(posTo.getTimestampMs() - posFrom.getTimestampMs());

      // Set interpolated altitude
      pos.setAltitude(static_cast<float>(atools::interpolate(posFrom.getAltitudeD(), posTo.getAltitudeD(), 0., length, distFrom1)));
      trailSegment.position = pos;
      trailSegment.index = trackIndexNearest;
      trailSegment.from = posFrom.getPosition();
      trailSegment.to = posTo.getPosition();
      trailSegment.length = static_cast<float>(length);
      trailSegment.distanceFromStart = resultNearest.distanceFrom1;
      trailSegment.speed = static_cast<float>(length / travelTime * 1000.f);
      trailSegment.headingTrue = static_cast<float>(posFrom.getPosD().angleDegTo(posTo.getPosD()));
      trailSegment.timestampPos = posFrom.getTimestampMs() + std::lround(travelTime * fraction);
      trailSegment.onGround = atools::roundToInt(atools::interpolate(static_cast<double>(posFrom.isOnGround()),
                                                                     static_cast<double>(posTo.isOnGround()), 0., length, distFrom1));
    }
    else
    {
#ifdef DEBUG_INFORMATION_TRACK_NEAREST
      qDebug() << Q_FUNC_INFO << "MISSED" << pos << point << QPoint(static_cast<int>(xs), static_cast<int>(ys));
#endif
      trackIndexNearest = -1;
      trailSegment.position = Pos();
    }
  }
  return trailSegment;
}

atools::fs::gpx::GpxData AircraftTrail::toGpxData(const atools::fs::pln::Flightplan& flightplan) const
{
  atools::fs::gpx::GpxData gpxData;
  gpxData.setFlightplan(flightplan);

  const QVector<QVector<atools::geo::PosD> >& positionList = positionsD();
  const QVector<QVector<qint64> >& timestampsList = timestampsMs();

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

  cleanDuplicates();
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

  cleanDuplicates();
  qDebug() << Q_FUNC_INFO << "Trail size" << size();

#ifdef  DEBUG_INFORMATION_TRAIL
  for(int i = 0; i < size(); i++)
  {
    const AircraftTrailPos& trackPos = at(i);
    if(!trackPos.isValid())
      qDebug() << Q_FUNC_INFO << "SPLIT AT" << i << value(i - 1).getPosition() << value(i + 1).getPosition();
  }
#endif

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

  int numTruncated = 0;
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
    if(split || numTruncated > 0)
    {
      // Trail was split or truncated - do a full update
      cleanDuplicates();
      updateBoundary();
      updateLineStrings();
    }
    else if(changed)
    {
      // Last one is always valid
      updateBoundaryLast();
      updateLineStringsLast();
    }
  }
  else
  {
    // Trail is empty - clear cached values
    clearBoundaries();
    lineStrings.clear();
    lineStringsIndex.clear();
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
  int numTruncated = 0, truncateBatch = std::max(size() / 50, 5); // Calculate batch based on current size
  int maxEntries = maxTrackEntries <= 0 || maxTrackEntries > MAX_TRACK_ENTRIES ? MAX_TRACK_ENTRIES : maxTrackEntries;

  while(size() > maxEntries && !isEmpty())
  {
    for(int i = 0; i < truncateBatch; i++)
    {
      if(isEmpty())
        break;

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
  lineStringsIndex.clear();
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

void AircraftTrail::updateBoundaryLast()
{
  if(!isEmpty())
  {
    const AircraftTrailPos& lastTrailPos = constLast();
    if(lastTrailPos.isValid())
    {
      float alt = static_cast<float>(lastTrailPos.getPosD().getAltitude());
      minAltitude = std::min(minAltitude, alt);
      maxAltitude = std::max(maxAltitude, alt);
      boundingRect.extend(lastTrailPos.getPosition());
    }
  }
}

void AircraftTrail::cleanDuplicates()
{
  // Remove consecutive invalid and with similar position
  erase(std::unique(begin(), end(), [](const AircraftTrailPos& p1, const AircraftTrailPos& p2) -> bool {
    return (!p1.isValid() && !p2.isValid()) ||
           (p1.getPosD().almostEqual(p2.getPosD(), atools::geo::Pos::POS_EPSILON_1M) &&
            atools::almostEqual(p1.getAltitudeD(), p2.getAltitudeD(), 10.));
  }), end());
}

void AircraftTrail::updateLineStringsLast()
{
  if(!isEmpty())
  {
    const AircraftTrailPos& lastTrailPos = constLast();
    if(lastTrailPos.isValid())
    {
      if(lineStrings.isEmpty())
      {
        // Initial append add new line string =======================================
        lineStrings.append(atools::geo::LineString(lastTrailPos.getPosition()));
        lineStringsIndex.append(0);
      }
      else
      {
        // Append to filled list =======================================
        atools::geo::LineString& lastLineString = lineStrings.last();

        if(lastLineString.size() > PARTITION_POINT_SIZE)
        {
          // Add new partition since last is too large - add position to both lines for closing polyline
          lastLineString.append(lastTrailPos.getPosition());

          lineStrings.append(atools::geo::LineString(lastTrailPos.getPosition()));
          lineStringsIndex.append(size() - 1);
        }
        else
          lastLineString.append(lastTrailPos.getPosition());
      }
    }
  }
}

void AircraftTrail::updateLineStrings()
{
  lineStrings.clear();
  lineStringsIndex.clear();

  if(!isEmpty())
  {
    atools::geo::LineString linestring;
    int startIndex = 0;
    for(int i = 0; i < size(); i++)
    {
      const AircraftTrailPos& trailPos = at(i);

      if(!trailPos.isValid())
      {
        // An invalid position shows a break in the lines - add line and start a new one without connection =================
        lineStrings.append(linestring);
        lineStringsIndex.append(startIndex);
        startIndex += linestring.size() + 1; // Index for next one - this point is skipped - consecutive invalid are already filtered
        linestring.clear();
      }
      else if(linestring.size() > PARTITION_POINT_SIZE)
      {
        // Line string too long - add a new one as partition being connected =============================
        linestring.append(trailPos.getPosition());

        // Add next position to keep lines attached
        if(i < size() - 1)
        {
          const AircraftTrailPos& nextTrailPos = at(i + 1);
          if(nextTrailPos.isValid())
            linestring.append(nextTrailPos.getPosition());
        }

        lineStrings.append(linestring);
        lineStringsIndex.append(startIndex);
        startIndex += linestring.size() - 1; // Next one starts overlapping at nextTrailPos
        linestring.clear();
      }
      else
        linestring.append(trailPos.getPosition());
    }

    // Add rest
    if(!linestring.isEmpty())
    {
      lineStrings.append(linestring);
      lineStringsIndex.append(startIndex);
    }
  }

  if(lineStrings.size() != lineStringsIndex.size())
    qWarning() << Q_FUNC_INFO << "lineStrings.size() != lineStringsIndex.size()" << lineStrings.size() << lineStringsIndex.size();

#ifdef  DEBUG_INFORMATION_TRAIL
  qDebug() << Q_FUNC_INFO << "========================================";

  for(int i = 0; i < lineStrings.size(); i++)
  {
    const atools::geo::LineString& ls = lineStrings.at(i);
    qDebug() << "#" << i << "size" << ls.size() << "index" << lineStringsIndex.at(i);
    qDebug() << "From" << at(lineStringsIndex.at(i)).getPosition() << "to" << at(lineStringsIndex.at(i) + ls.size() - 1).getPosition();

    if(ls.size() > 2)
      qDebug() << ls.constFirst() << "..." << ls.constLast();
    else if(ls.size() > 1)
      qDebug() << ls.constFirst() << ls.constLast();
    else if(ls.size() > 0)
      qDebug() << ls.constFirst();
    else if(ls.size() > 0)
      qDebug() << "empty";
  }

  // qDebug() << Q_FUNC_INFO << lineStrings;
  qDebug() << Q_FUNC_INFO << lineStringsIndex;
#endif
}

const QVector<QVector<atools::geo::PosD> > AircraftTrail::positionsD() const
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

const QVector<QVector<qint64> > AircraftTrail::timestampsMs() const
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
