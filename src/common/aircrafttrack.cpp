/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include <QDataStream>
#include <QFile>

AircraftTrack::AircraftTrack()
{

}

AircraftTrack::~AircraftTrack()
{

}

namespace at {

QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrackPos& obj)
{
  dataStream >> obj.pos >> obj.onGround;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrackPos& obj)
{
  dataStream << obj.pos << obj.onGround;
  return dataStream;
}

}

void AircraftTrack::saveState()
{
  QFile trackFile(atools::settings::Settings::getConfigFilename(".track"));

  if(trackFile.open(QIODevice::WriteOnly))
  {
    QDataStream out(&trackFile);
    out.setVersion(QDataStream::Qt_5_5);

    out << FILE_MAGIC_NUMBER << FILE_VERSION << *this;
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
      quint32 magic;
      quint16 version;
      QDataStream in(&trackFile);
      in.setVersion(QDataStream::Qt_5_5);
      in >> magic;

      if(magic == FILE_MAGIC_NUMBER)
      {
        in >> version;
        if(version == FILE_VERSION)
          in >> *this;
        else
          qWarning() << "Cannot read track" << trackFile.fileName() << ". Invalid version number:" << version;
      }
      else
        qWarning() << "Cannot read track" << trackFile.fileName() << ". Invalid magic number:" << magic;

      trackFile.close();
    }
    else
      qWarning() << "Cannot read track" << trackFile.fileName() << ":" << trackFile.errorString();
  }
}

bool AircraftTrack::appendTrackPos(const atools::geo::Pos& pos, bool onGround)
{
  bool pruned = false;
  // Use a larger distance on ground before storing position
  float epsilon = onGround ? atools::geo::Pos::POS_EPSILON_1M : atools::geo::Pos::POS_EPSILON_100M;
  if(isEmpty())
    append({pos, onGround});
  else if(!pos.almostEqual(last().pos, epsilon))
  {
    if(pos.distanceMeterTo(last().pos) > MAX_POINT_DISTANCE_METER)
    {
      clear();
      pruned = true;
    }
    else
    {
      if(size() > MAX_TRACK_ENTRIES)
      {
        for(int i = 0; i < PRUNE_TRACK_ENTRIES; i++)
          removeFirst();
        pruned = true;
      }
    }
    append({pos, onGround});
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
