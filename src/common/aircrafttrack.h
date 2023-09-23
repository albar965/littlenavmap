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

  AircraftTrackPos(atools::geo::PosD position, qint64 timeMs, bool ground)
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

  /* Get single old precision coordinate */
  atools::geo::Pos getPos() const
  {
    return pos.asPos();
  }

  /* Get full precision coordinate */
  const atools::geo::PosD& getPosD() const
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

  atools::geo::PosD pos;
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

  /* Saves and restores track into a separate file (little_navmap.track). Creates two additional backup files. */
  void saveState(const QString& suffix, int numBackupFiles);
  void restoreState(const QString& suffix);

  void clearTrack();

  /*
   * Add a track position. Accurracy depends on the ground flag which will cause more
   * or less points skipped.
   * @return true if the track was pruned
   */
  bool appendTrackPos(const atools::fs::sc::SimConnectUserAircraft& userAircraft, bool allowSplit);

  /* Get maximum altitude in all track points */
  float getMaxAltitude() const;

  /* Copies the coordinates from the structs to a list of linestrings.
   * More than one linestring might be returned if the trail is interrupted. */
  const QVector<atools::geo::LineString> getLineStrings() const;

  /* Accurate positions for drawing */
  const QVector<QVector<atools::geo::PosD> > getPositionsD() const;

  /* Same size as getLineStrings() but returns the timestamps in milliseconds since Epoch UTC for each position */
  const QVector<QVector<qint64> > getTimestampsMs() const;

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

  /* Maximum number of track points. If exceeded entries will be removed from beginning of the list */
  int maxTrackEntries = 20000;

  atools::fs::sc::SimConnectUserAircraft *lastUserAircraft;

  /* Needed in RouteExportFormat stream operators to read different formats */
  static quint16 version;
};

#endif // LITTLENAVMAP_AIRCRAFTTRACK_H
