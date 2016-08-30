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
    // No need for versioning here - if version is older migrate::checkAndMigrateSettings()
    // will simply delete the file
    QDataStream out(&trackFile);
    out.setVersion(QDataStream::Qt_5_5);
    out << *this;
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
      in.setVersion(QDataStream::Qt_5_5);
      in >> *this;
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

  if(isEmpty() || !pos.almostEqual(last().pos, epsilon))
  {
    if(size() > MAX_TRACK_ENTRIES)
    {
      for(int i = 0; i < PRUNE_TRACK_ENTRIES; i++)
        removeFirst();
      pruned = true;
    }

    append({pos, onGround});
  }
  return pruned;
}
