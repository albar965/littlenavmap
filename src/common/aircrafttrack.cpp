/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include <QDataStream>
#include <QDateTime>
#include <QFile>

AircraftTrack::AircraftTrack()
{

}

AircraftTrack::~AircraftTrack()
{

}

namespace at {

QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrackPos& trackPos)
{
  dataStream >> trackPos.pos >> trackPos.timestamp >> trackPos.onGround;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrackPos& trackPos)
{
  dataStream << trackPos.pos << trackPos.timestamp << trackPos.onGround;
  return dataStream;
}

}

void AircraftTrack::saveState()
{
  QFile trackFile(atools::settings::Settings::getConfigFilename(".track"));

  if(trackFile.open(QIODevice::WriteOnly))
  {
    QDataStream out(&trackFile);
    saveToStream(out);
    trackFile.close();
  }
  else
    qWarning() << "Cannot write track" << trackFile.fileName() << ":" << trackFile.errorString();
}

void AircraftTrack::restoreState()
{
  clear();

  QFile trackFile(atools::settings::Settings::getConfigFilename(".track"));
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

void AircraftTrack::saveToStream(QDataStream& out)
{
  out.setVersion(QDataStream::Qt_5_5);
  out.setFloatingPointPrecision(QDataStream::SinglePrecision);
  out << FILE_MAGIC_NUMBER << FILE_VERSION << *this;
}

bool AircraftTrack::readFromStream(QDataStream& in)
{
  bool retval = false;
  clear();

  quint32 magic;
  quint16 version;
  in.setVersion(QDataStream::Qt_5_5);
  in.setFloatingPointPrecision(QDataStream::SinglePrecision);
  in >> magic;

  if(magic == FILE_MAGIC_NUMBER)
  {
    in >> version;
    if(version == FILE_VERSION)
    {
      in >> *this;
      retval = true;
    }
    else
      qWarning() << "Cannot read track. Invalid version number:" << version;
  }
  else
    qWarning() << "Cannot read track. Invalid magic number:" << magic;

  return retval;
}

void AircraftTrack::convert(atools::geo::LineString *track, QVector<quint32> *timestamps) const
{
  for(const at::AircraftTrackPos& pos : (*this))
  {
    if(track != nullptr)
      track->append(pos.pos);
    if(timestamps != nullptr)
      timestamps->append(pos.timestamp);
  }
}

bool AircraftTrack::appendTrackPos(const atools::geo::Pos& pos, const QDateTime& timestamp, bool onGround)
{
  bool pruned = false;
  // Use a larger distance on ground before storing position
  float epsilon = onGround ? atools::geo::Pos::POS_EPSILON_5M : atools::geo::Pos::POS_EPSILON_100M;
  long timeDiff = onGround ? MIN_POSITION_TIME_DIFF_GROUND_MS : MIN_POSITION_TIME_DIFF_MS;

  if(isEmpty())
    append({pos, timestamp.toTime_t(), onGround});
  else
  {
    long time = timestamp.toMSecsSinceEpoch();
    long lastTime = last().timestamp * 1000L;

    if(!pos.almostEqual(last().pos, epsilon) && !atools::almostEqual(lastTime, time, timeDiff))
    {
      if(pos.distanceMeterTo(last().pos) > atools::geo::nmToMeter(MAX_POINT_DISTANCE_NM))
      {
        clear();
        pruned = true;
      }
      else
      {
        if(size() > maxTrackEntries)
        {
          for(int i = 0; i < PRUNE_TRACK_ENTRIES; i++)
            removeFirst();
          pruned = true;
        }
      }
      append({pos, timestamp.toTime_t(), onGround});
    }
  }
  return pruned;
}

float AircraftTrack::getMaxAltitude() const
{
  float maxAlt = 0.f;
  for(const at::AircraftTrackPos& trackPos : *this)
    maxAlt = std::max(maxAlt, trackPos.pos.getAltitude());
  return maxAlt;
}
