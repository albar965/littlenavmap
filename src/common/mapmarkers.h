/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MARKERS_H
#define LITTLENAVMAP_MARKERS_H

#include "maptypes.h"

#include <QCoreApplication>

namespace atools {
namespace util {

class XmlStreamWriter;
class XmlStreamReader;
}
}
namespace map {

/* =====================================================================
 * All markers called user features in the user interface.
 * Can be converted to QVariant as well as loaded and saved from/to XML
 * ===================================================================== */

// =====================================================================
/* All information for complete traffic pattern structure */
/* Threshold position (end of final) and runway altitude MSL */
struct PatternMarker
  : public MapBase
{
  PatternMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_PATTERNS;
  }

  float magCourse() const;

  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  QString airportIcao, runwayName;
  QColor color;
  bool turnRight,
       base45Degree /* calculate base turn from 45 deg after threshold */,
       showEntryExit /* Entry and exit indicators */;
  int runwayLength; /* ft Does not include displaced threshold */

  float downwindParallelDistance, finalDistance, departureDistance; /* NM */
  float courseTrue; /* degree true final course*/
  float magvar;
};

QDataStream& operator>>(QDataStream& dataStream, map::PatternMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::PatternMarker& obj);

// =====================================================================
/* Holding marker.
 * Uses its own type distinct from holdings. */
struct HoldingMarker
  : public MapBase
{
  HoldingMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_HOLDING;
  }

  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  /* Aggregate the database holding structure */
  map::MapHolding holding;
};

/* Save only information for user defined holds */
QDataStream& operator>>(QDataStream& dataStream, map::HoldingMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::HoldingMarker& obj);

// =====================================================================
/* MSA marker as placed by user.
 * Uses its own type distinct from database MSA. */
struct MsaMarker
  : public MapBase
{
  MsaMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_MSA;
  }

  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  /*   Aggregate the database MSA structure */
  map::MapAirportMsa msa;
};

/* Save only information for user defined holds */
QDataStream& operator>>(QDataStream& dataStream, map::MsaMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::MsaMarker& obj);

// =====================================================================
/* Range rings marker. Covers user defined rings as well as radio-navaid ranges */
struct RangeMarker
  : public MapBase
{
  RangeMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_RANGE;
  }

  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  QString text; /* Text to display like VOR name and frequency */
  QList<float> ranges; /* Range ring list (NM) */
  MapType navType; /* VOR, NDB, AIRPORT, etc. */
  QColor color;
};

QDataStream& operator>>(QDataStream& dataStream, map::RangeMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj);

// =====================================================================
/* Distance measurement line. */
struct DistanceMarker
  : public MapBase
{
  DistanceMarker()
    : MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_DISTANCE;
  }

  bool isValid() const
  {
    return from.isValid();
  }

  const atools::geo::Pos& getPositionTo() const
  {
    return to;
  }

  const atools::geo::Pos& getPositionFrom() const
  {
    return from;
  }

  float getDistanceMeter() const
  {
    return from.distanceMeterTo(to);
  }

  float getDistanceNm() const;

  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  QString text; /* Text to display like VOR name and frequency */
  QColor color; /* Line color depends on origin (airport or navaid type */
  atools::geo::Pos from, to;
  float magvar = 0.f; /* Declination from world or navaid */
  map::DistanceMarkerFlags flags = map::DIST_MARK_NONE;
};

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj);

/* ===============================================================================
 * Combines all user features and allows saving and loading from/to XML and settings.
 */
class MapMarkers
{
  Q_DECLARE_TR_FUNCTIONS(MapMarkers)

public:
  /* Save and load to/from XML file */
  void save(const QString& filename, int numBackupFiles);
  void restore(const QString& filename);

  /* Add user features. Id has to be set before using getNextUserFeatureId(). */
  void addRangeMark(const map::RangeMarker& obj);
  void addPatternMark(const map::PatternMarker& obj);
  void addDistanceMark(const map::DistanceMarker& obj);
  void addHoldingMark(const map::HoldingMarker& obj);
  void addMsaMark(const map::MsaMarker& obj);

  /* Remove user features by generated id from getNextUserFeatureId() */
  void removeRangeMark(int id);
  void removePatternMark(int id);
  void removeDistanceMark(int id);
  void removeHoldingMark(int id);
  void removeMsaMark(int id);

  /* Removes all by type */
  void clearAllMarkers(map::MapTypes types);

  /* Update measurement lines positions for either end */
  void updateDistanceMarkerFromPos(int id, const atools::geo::Pos& pos);
  void updateDistanceMarkerToPos(int id, const atools::geo::Pos& pos);

  /* Update measurement lines all fields except id where the parameter is used. */
  void updateDistanceMarker(int id, const map::DistanceMarker& marker);

  /* Get range rings */
  const QHash<int, map::RangeMarker>& getRangeMarkers() const
  {
    return rangeMarkers;
  }

  /* Get distance measurement lines */
  const QHash<int, map::DistanceMarker>& getDistanceMarkers() const
  {
    return distanceMarkers;
  }

  /* Get for editing */
  map::DistanceMarker& getDistanceMarker(int id)
  {
    return distanceMarkers[id];
  }

  /* Airfield traffic patterns. */
  const QHash<int, map::PatternMarker>& getPatternMarkers() const
  {
    return patternMarkers;
  }

  /* Holdings. */
  const QHash<int, map::HoldingMarker>& getHoldingMarkers() const
  {
    return holdingMarkers;
  }

  /* Airport MSA. */
  const QHash<int, map::MsaMarker>& getMsaMarkers() const
  {
    return msaMarkers;
  }

  /* Assign artificial ids to measurement and range rings which allow to identify them.
   * Not thread safe. */
  static int getNextUserFeatureId();

  /* true if file is detected as a user feature XML file */
  static bool isMarkersFile(const QString& filename);

private:
  /* XML file version */
  static const int MARKERS_VERSION_MAJOR = 1;
  static const int MARKERS_VERSION_MINOR = 0;

  QHash<int, map::RangeMarker> rangeMarkers;
  QHash<int, map::DistanceMarker> distanceMarkers;
  QHash<int, map::PatternMarker> patternMarkers;
  QHash<int, map::HoldingMarker> holdingMarkers;
  QHash<int, map::MsaMarker> msaMarkers;
};

/* Register serializable objects */
void registerMarkerMetaTypes();

} // namespace map

/* Type information =============================================== */
Q_DECLARE_TYPEINFO(map::RangeMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::RangeMarker)
Q_DECLARE_METATYPE(QList<map::RangeMarker>)

Q_DECLARE_TYPEINFO(map::DistanceMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::DistanceMarker)
Q_DECLARE_METATYPE(QList<map::DistanceMarker>)

Q_DECLARE_TYPEINFO(map::PatternMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::PatternMarker)
Q_DECLARE_METATYPE(QList<map::PatternMarker>)

Q_DECLARE_TYPEINFO(map::HoldingMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::HoldingMarker)
Q_DECLARE_METATYPE(QList<map::HoldingMarker>)

Q_DECLARE_TYPEINFO(map::MsaMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::MsaMarker)
Q_DECLARE_METATYPE(QList<map::MsaMarker>)

#endif // LITTLENAVMAP_MARKERS_H
