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

#ifndef LITTLENAVMAP_AIRCRAFTTRACK_H
#define LITTLENAVMAP_AIRCRAFTTRACK_H

#include "geo/pos.h"

namespace atools {
namespace fs {
namespace sc {
class SimConnectUserAircraft;
}
}
namespace geo {
class LineString;
}
}

namespace at {
/* Track position. Can be converted to QVariant and thus be saved to settings.
 *
 * An invalid position indicates a break of the tracks.
 * Aircraft warp to destination above ground is not broken but a movement on ground or airport change is.
 * Done to avoid format changes. */
struct AircraftTrackPos
{
  AircraftTrackPos()
  {
  }

  AircraftTrackPos(atools::geo::Pos position, quint32 time, bool ground)
    : pos(position), timestamp(time), onGround(ground)
  {
  }

  AircraftTrackPos(quint32 time, bool ground)
    : timestamp(time), onGround(ground)
  {
  }

  bool isValid() const
  {
    return pos.isValid();
  }

  atools::geo::Pos pos;
  quint32 timestamp; // TODO future overflow
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
  void saveState(const QString& suffix);
  void restoreState(const QString& suffix);

  void clearTrack();

  /*
   * Add a track position. Accurracy depends on the ground flag which will cause more
   * or less points skipped.
   * @return true if the track was pruned
   */
  bool appendTrackPos(const atools::fs::sc::SimConnectUserAircraft& userAircraft, bool allowSplit);

  float getMaxAltitude() const;

  /* Copies the coordinates from the structs to a list of linestrings.
   * More than one linestring might be returned if the trail is interrupted. */
  QVector<atools::geo::LineString> getLineStrings() const;

  /* Same as getLineStrings() but returns the timestamps for each position */
  QVector<QVector<quint32> > getTimestamps() const;

  /* Convert to linestring and timestamp values for export functions like GPX */
  // void convert(atools::geo::LineString *track) const;

  /* Pull only needed methods of the base class into public space */
  using QList::isEmpty;

  /* Track will be pruned if it contains more track entries than this value. Default is 20000. */
  void setMaxTrackEntries(int value)
  {
    maxTrackEntries = value;
  }

  /* Write and read the whole track to and from a binary stream */
  void saveToStream(QDataStream& out);

  bool readFromStream(QDataStream & in);

private:
  /* Insert an invalid position as an break indicator if aircraft jumps too far on ground. */
  static const int MAX_POINT_DISTANCE_NM = 5;

  /* Maximum number of track points. If exceeded entries will be removed from beginning of the list */
  int maxTrackEntries = 20000;
  /* Number of entries to remove at once */
  static const int PRUNE_TRACK_ENTRIES = 200;

  /* Minimum time difference between recordings */
  static const int MIN_POSITION_TIME_DIFF_MS = 1000;
  static const int MIN_POSITION_TIME_DIFF_GROUND_MS = 250;

  static const quint32 FILE_MAGIC_NUMBER = 0x5B6C1A2B;

  /* Version 2 to adds timstamp and single floating point precision */
  static const quint16 FILE_VERSION = 2;

  atools::fs::sc::SimConnectUserAircraft *lastUserAircraft;
};

#endif // LITTLENAVMAP_AIRCRAFTTRACK_H
