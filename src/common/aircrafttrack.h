/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
  quint32 timestamp;
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

  /* Saves and restores track into a separate file (little_navmap.track) */
  void saveState();
  void restoreState();

  void clearTrack()
  {
    clear();
  }

  /*
   * Add a track position. Accurracy depends on the ground flag which will cause more
   * or less points skipped.
   * @return true if the track was pruned
   */
  bool appendTrackPos(const atools::geo::Pos& pos, const QDateTime& timestamp, bool onGround);

  float getMaxAltitude() const;

  /* Pull only needed methods into public space */
  using QList::isEmpty;
  using QList::first;
  using QList::last;
  using QList::size;
  using QList::at;
  using QList::begin;
  using QList::end;

  void setMaxTrackEntries(int value)
  {
    maxTrackEntries = value;
  }

private:
  /* Maximum number of track points. If exceeded entries will be removed from beginning of the list */
  int maxTrackEntries = 20000;
  /* Number of entries to remove at once */
  static Q_DECL_CONSTEXPR int PRUNE_TRACK_ENTRIES = 200;

  /* Minimum time difference between recordings */
  static Q_DECL_CONSTEXPR int MIN_POSITION_TIME_DIFF_MS = 1000;
  static Q_DECL_CONSTEXPR int MIN_POSITION_TIME_DIFF_GROUND_MS = 250;

  /* Clear track if aircraft jumps too far */
  static Q_DECL_CONSTEXPR int MAX_POINT_DISTANCE_NM = 2000;

  static Q_DECL_CONSTEXPR quint32 FILE_MAGIC_NUMBER = 0x5B6C1A2B;

  /* Version 2 to adds timstamp and single floating point precision */
  static Q_DECL_CONSTEXPR quint16 FILE_VERSION = 2;
};

#endif // LITTLENAVMAP_AIRCRAFTTRACK_H
