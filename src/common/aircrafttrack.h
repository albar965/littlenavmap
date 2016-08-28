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

#ifndef LITTLENAVMAP_AIRCRAFTTRACK_H
#define LITTLENAVMAP_AIRCRAFTTRACK_H

#include "geo/pos.h"

namespace at {
/* Track position. Can be converted to QVariant and thus be saved to settings */
struct AircraftTrackPos
{
  atools::geo::Pos pos;
  bool onGround;
};

QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrackPos& obj);
QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrackPos& obj);

}

Q_DECLARE_TYPEINFO(at::AircraftTrackPos, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(at::AircraftTrackPos);

/*
 * Stores the track of the flight simulator aircraft
 */
class AircraftTrack :
  private QList<at::AircraftTrackPos>
{
public:
  AircraftTrack();
  ~AircraftTrack();

  void saveState();
  void restoreState();

  void clearTrack()
  {
    clear();
  }

  /*
   * Add a track position. Accurracy depends on the ground flag which will cause more
   * or less points skipped.
   */
  void appendTrackPos(const atools::geo::Pos& pos, bool onGround);

  /* Pull only needed methods into public space */
  using QList::isEmpty;
  using QList::first;
  using QList::last;
  using QList::size;
  using QList::at;

  static Q_DECL_CONSTEXPR int MAX_TRACK_ENTRIES = 1000;
};

#endif // LITTLENAVMAP_AIRCRAFTTRACK_H
