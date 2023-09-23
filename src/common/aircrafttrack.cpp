/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "common/aircrafttrack.h"

#include "settings/settings.h"
#include "atools.h"
#include "geo/calculations.h"
#include "geo/linestring.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "io/fileroller.h"

#include <QDataStream>
#include <QDateTime>
#include <QFile>

quint16 AircraftTrack::version = 0;

/* Insert an invalid position as an break indicator if aircraft jumps too far on ground. */
static const float MAX_POINT_DISTANCE_NM = 5.f;

/* Number of entries to remove at once */
static const int PRUNE_TRACK_ENTRIES = 200;

/* Minimum time difference between recordings */
static const int MIN_POSITION_TIME_DIFF_MS = 1000;
static const int MIN_POSITION_TIME_DIFF_GROUND_MS = 250;

static const quint32 FILE_MAGIC_NUMBER = 0x5B6C1A2B;

/* Version 2 to adds timstamp and single floating point precision. Uses 32-bit second timestamps */
static const quint16 FILE_VERSION_OLD = 2;

/* Version 3 adds 64-bit millisecond values - still 32 bit coordinates */
static const quint16 FILE_VERSION_64BIT_TS = 3;

/* Version 4 adds double floating point precision for coordinates */
static const quint16 FILE_VERSION_64BIT_COORDS = 4;

namespace at {

QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrackPos& trackPos)
{
  if(AircraftTrack::version == FILE_VERSION_64BIT_COORDS)
  {
    // Newest format with 64 bit coordinates and 64 bit timestamp
    dataStream >> trackPos.pos >> trackPos.timestampMs >> trackPos.onGround;

    // Fix invalid positions from wrong decoding of earlier versions
    if(!trackPos.pos.isValidRange() || trackPos.pos.isNull())
      // Add separator
      trackPos.pos = atools::geo::PosD();
  }
  else if(AircraftTrack::version == FILE_VERSION_64BIT_TS)
  {
    // Convert 32 bit coordinates
    atools::geo::Pos pos;
    dataStream >> pos >> trackPos.timestampMs >> trackPos.onGround;

    // Convert invalid float positions to invalid double positions to indicate split track
    trackPos.pos = atools::geo::PosD(pos);
  }
  else if(AircraftTrack::version == FILE_VERSION_OLD)
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

QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrackPos& trackPos)
{
#ifdef DEBUG_SAVE_TRACK_OLD
  dataStream << trackPos.pos.asPos() << trackPos.timestampMs << trackPos.onGround;
#else
  dataStream << trackPos.pos << trackPos.timestampMs << trackPos.onGround;
#endif
  return dataStream;
}

}

AircraftTrack::AircraftTrack()
{
  lastUserAircraft = new atools::fs::sc::SimConnectUserAircraft;
}

AircraftTrack::~AircraftTrack()
{
  delete lastUserAircraft;
}

AircraftTrack::AircraftTrack(const AircraftTrack& other)
  : QList<at::AircraftTrackPos>(other)
{
  lastUserAircraft = new atools::fs::sc::SimConnectUserAircraft;
  this->operator=(other);
}

AircraftTrack& AircraftTrack::operator=(const AircraftTrack& other)
{
  clear();
  append(other);
  maxTrackEntries = other.maxTrackEntries;
  *lastUserAircraft = *other.lastUserAircraft;
  return *this;
}

void AircraftTrack::saveState(const QString& suffix, int numBackupFiles)
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

void AircraftTrack::restoreState(const QString& suffix)
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
}

void AircraftTrack::clearTrack()
{
  clear();
}

void AircraftTrack::saveToStream(QDataStream& out)
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

bool AircraftTrack::readFromStream(QDataStream& in)
{
  bool retval = false;
  clear();

  quint32 magic;
  in.setVersion(QDataStream::Qt_5_5);
  in >> magic;

  if(magic == FILE_MAGIC_NUMBER)
  {
    in >> AircraftTrack::version;
    if(AircraftTrack::version == FILE_VERSION_64BIT_TS || AircraftTrack::version == FILE_VERSION_OLD)
    {
      // Read old float coordinates
      in.setFloatingPointPrecision(QDataStream::SinglePrecision);
      in >> *this;
      retval = true;
    }
    else if(AircraftTrack::version == FILE_VERSION_64BIT_COORDS)
    {
      // Read new double coordinates
      in.setFloatingPointPrecision(QDataStream::DoublePrecision);
      in >> *this;
      retval = true;
    }
    else
      qWarning() << "Cannot read track. Invalid version number:" << AircraftTrack::version;
  }
  else
    qWarning() << "Cannot read track. Invalid magic number:" << magic;

  return retval;
}

bool AircraftTrack::appendTrackPos(const atools::fs::sc::SimConnectUserAircraft& userAircraft, bool allowSplit)
{
  if(!userAircraft.isValid())
  {
    qDebug() << Q_FUNC_INFO << "Aircraft not valid";
    return false;
  }

  if(!lastUserAircraft->isValid() && !userAircraft.isFullyValid())
  {
    // Avoid spurious aircraft repositioning to 0/0 like done by MSFS
    qDebug() << Q_FUNC_INFO << "Aircraft not fully valid";
    return false;
  }

  bool pruned = false;
  const QDateTime timestamp = userAircraft.getZuluTime();
  atools::geo::PosD posD = userAircraft.getPositionD();
  bool onGround = userAircraft.isOnGround();

  if(isEmpty() && userAircraft.isValid())
    append(at::AircraftTrackPos(posD, timestamp.toMSecsSinceEpoch(), onGround));
  else
  {
    // Use a smaller distance on ground before storing position
    float epsilonPos = onGround ? atools::geo::Pos::POS_EPSILON_5M : atools::geo::Pos::POS_EPSILON_100M;
    qint64 epsilonTime = onGround ? MIN_POSITION_TIME_DIFF_GROUND_MS : MIN_POSITION_TIME_DIFF_MS;

    qint64 timeMs = timestamp.toMSecsSinceEpoch();
    const at::AircraftTrackPos& last = constLast();
    qint64 lastTimeMs = last.getTimestampMs();

    atools::geo::Pos pos = posD.asPos();
    if(!pos.almostEqual(last.getPos(), epsilonPos) && !atools::almostEqual(lastTimeMs, timeMs, epsilonTime))
    {
      bool lastValid = lastUserAircraft->isValid();
      bool aircraftChanged = lastValid && lastUserAircraft->hasAircraftChanged(userAircraft);
      bool jumped = !isEmpty() && pos.distanceMeterTo(last.getPos()) > atools::geo::nmToMeter(MAX_POINT_DISTANCE_NM);

      // Points where the track is interrupted (new flight) are indicated by invalid coordinates.
      // Warping at altitude does not interrupt a track.
      if(allowSplit && jumped && (last.isOnGround() || onGround || aircraftChanged))
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "Splitting trail" << "allowSplit" << allowSplit << "jumped" << jumped
                 << "last.onGround" << last.isOnGround() << "onGround" << onGround << "aircraftChanged" << aircraftChanged;
#endif

        // Add an invalid position before indicating a break
        append(at::AircraftTrackPos(timestamp.toMSecsSinceEpoch(), onGround));
        append(at::AircraftTrackPos(posD, timestamp.toMSecsSinceEpoch(), onGround));
      }
      else
      {
        if(size() > maxTrackEntries)
        {
          for(int i = 0; i < PRUNE_TRACK_ENTRIES; i++)
            removeFirst();

          // Remove invalid segments
          while(!isEmpty() && !constFirst().isValid())
            removeFirst();

          pruned = true;
        }
        append(at::AircraftTrackPos(posD, timestamp.toMSecsSinceEpoch(), onGround));
      }

      *lastUserAircraft = userAircraft;
    }
  }
  return pruned;
}

float AircraftTrack::getMaxAltitude() const
{
  double maxAlt = 0.;
  for(const at::AircraftTrackPos& trackPos : *this)
    maxAlt = std::max(maxAlt, trackPos.getPosD().getAltitude());
  return static_cast<float>(maxAlt);
}

const QVector<atools::geo::LineString> AircraftTrack::getLineStrings() const
{
  QVector<atools::geo::LineString> linestrings;

  if(!isEmpty())
  {
    atools::geo::LineString line;
    linestrings.reserve(size());

    for(const at::AircraftTrackPos& trackPos : *this)
    {
      if(!trackPos.isValid())
      {
        // An invalid position shows a break in the lines - add line and start a new one
        linestrings.append(line);
        line.clear();
      }
      else
        line.append(trackPos.getPos());
    }

    // Add rest
    if(!line.isEmpty())
      linestrings.append(line);
  }
  return linestrings;
}

const QVector<QVector<atools::geo::PosD> > AircraftTrack::getPositionsD() const
{
  QVector<QVector<atools::geo::PosD> > linestrings;

  if(!isEmpty())
  {
    QVector<atools::geo::PosD> line;
    linestrings.reserve(size());

    for(const at::AircraftTrackPos& trackPos : *this)
    {
      if(!trackPos.isValid())
      {
        // An invalid position shows a break in the lines - add line and start a new one
        linestrings.append(line);
        line.clear();
      }
      else
        line.append(trackPos.getPosD());
    }

    // Add rest
    if(!line.isEmpty())
      linestrings.append(line);
  }
  return linestrings;
}

const QVector<QVector<qint64> > AircraftTrack::getTimestampsMs() const
{
  QVector<QVector<qint64> > timestamps;

  if(!isEmpty())
  {
    QVector<qint64> times;
    timestamps.reserve(size());

    for(const at::AircraftTrackPos& trackPos : *this)
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
