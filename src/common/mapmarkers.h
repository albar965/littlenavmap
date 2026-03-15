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

namespace map {
class MsaMarker;
class HoldingMarker;
class PatternMarker;
class DistanceMarker;
class RangeMarker;
}

namespace atools {
namespace util {
class XmlStreamWriter;
class XmlStreamReader;
}
}

/* ===============================================================================
 * Combines all user features and allows saving and loading from/to XML and settings.
 */
class MapMarkers
{
  Q_DECLARE_TR_FUNCTIONS(MapMarkers)

public:
  /* Save all to XML file */
  void save(const QString& filename, int numBackupFiles);

  /* Load all from XML file. Appends to current list of user features. */
  void restore(const QString& filename);

  /* Restore from legacy settings */
  void restoreFromSettings();

  /* Removes all by type */
  void clear(map::MapTypes types = map::MARK_ALL);

  /* Copies from other by type. Overwrites only if type is given in flags. */
  void copy(const MapMarkers& other, map::MapTypes types = map::MARK_ALL);

  /* Appends from other by type */
  void append(const MapMarkers& other, map::MapTypes types = map::MARK_ALL);

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
  map::DistanceMarker& getDistanceMarker(int id);

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

  bool hasRangeMarkers() const;
  bool hasDistanceMarkers() const;
  bool hasPatternMarkers() const;
  bool hasHoldingMarkers() const;
  bool hasMsaMarkers() const;
  bool hasAnyMarkers() const;

  /* Assign artificial ids to measurement and range rings which allow to identify them.
   * Not thread safe. */
  int getNextMapMarkerId();

  /* true if file is detected as a user feature XML file */
  static bool isMarkersFile(const QString& filename);

  /* Get wrapped map objects from holdings and MSA for painting. */
  const QList<map::MapHolding> getHoldingMarksFiltered() const;
  const QList<map::MapAirportMsa> getMsaMarksFiltered() const;

private:
  /* XML file version */
  static const int MARKERS_VERSION_MAJOR = 1;
  static const int MARKERS_VERSION_MINOR = 0;

  QHash<int, map::RangeMarker> rangeMarkers;
  QHash<int, map::DistanceMarker> distanceMarkers;
  QHash<int, map::PatternMarker> patternMarkers;
  QHash<int, map::HoldingMarker> holdingMarkers;
  QHash<int, map::MsaMarker> msaMarkers;

  int currentMapMarkerId = 0;
};

#endif // LITTLENAVMAP_MARKERS_H
