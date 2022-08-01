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

  AircraftTrackPos(atools::geo::Pos position, qint64 timeMs, bool ground)
    : pos(position), timestampMs(timeMs), onGround(ground)
  {
  }

  /* Create an invalid position which is used as a marker for separate trail segments */
  AircraftTrackPos(qint64 timeMs, bool ground)
    : timestampMs(timeMs), onGround(ground)
  {
  }

  bool isValid() const
  {
    return pos.isValid();
  }

  const atools::geo::Pos& getPosition() const
  {
    return pos;
  }

  /* Milliseconds since Epoch UTC */
  qint64 getTimestampMs() const
  {
    return timestampMs;
  }

  bool isOnGround() const
  {
    return onGround;
  }

private:
  friend QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrackPos& trackPos);
  friend QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrackPos& trackPos);

  atools::geo::Pos pos;
  qint64 timestampMs;
  bool onGround;
};

QDataStream& operator>>(QDataStream& dataStream, at::AircraftTrackPos& obj);
QDataStream& operator<<(QDataStream& dataStream, const at::AircraftTrackPos& obj);

}

Q_DECLARE_TYPEINFO(at::AircraftTrackPos, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(at::AircraftTrackPos);

/*
 * Stores the track of the flight simulator aircraft.
 *
 * Points where the track is interrupted (new flight) are indicated by invalid coordinates.
 * Warping at altitude does not interrupt a track.
 */
class AircraftTrack :
  private QList<at::AircraftTrackPos>
{
public:
  AircraftTrack();
  ~AircraftTrack();

  AircraftTrack(const AircraftTrack& other);

  AircraftTrack& operator=(const AircraftTrack& other);

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

  /* Same size as getLineStrings() but returns the timestamps in milliseconds since Epoch UTC for each position */
  QVector<QVector<qint64> > getTimestampsMs() const;

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
  friend QDataStream& at::operator>>(QDataStream& dataStream, at::AircraftTrackPos& trackPos);

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

  /* Version 2 to adds timstamp and single floating point precision. Uses 32-bit second timestamps */
  static const quint16 FILE_VERSION_32BIT = 2;

  /* Version 3 adds 64-bit millisecond values */
  static const quint16 FILE_VERSION = 3;

  atools::fs::sc::SimConnectUserAircraft *lastUserAircraft;

  /* Needed in RouteExportFormat stream operators to read different formats */
  static quint16 version;
};

#endif // LITTLENAVMAP_AIRCRAFTTRACK_H
