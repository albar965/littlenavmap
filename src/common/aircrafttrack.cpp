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
#include "common/constants.h"

#include <QDataStream>

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
  atools::settings::Settings& s = atools::settings::Settings::instance();

  QVariant var = QVariant::fromValue<QList<at::AircraftTrackPos> >(*this);
  s.setValueVar(lnm::MAP_AIRCRAFT_TRACK, var);
}

void AircraftTrack::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  clear();

  QVariant var = s.valueVar(lnm::MAP_AIRCRAFT_TRACK);
  QList<at::AircraftTrackPos> list = var.value<QList<at::AircraftTrackPos> >();
  append(list);
}

void AircraftTrack::appendTrackPos(const atools::geo::Pos& pos, bool onGround)
{
  // Use a larger distance on ground before storing position
  float epsilon = onGround ? atools::geo::Pos::POS_EPSILON_1M : atools::geo::Pos::POS_EPSILON_100M;

  if(isEmpty() || !pos.almostEqual(last().pos, epsilon))
  {
    if(size() > MAX_TRACK_ENTRIES)
      removeFirst();

    append({pos, onGround});
  }
}
