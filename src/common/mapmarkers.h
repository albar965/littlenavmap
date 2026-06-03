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
 * Combines all map markers and allows saving and loading from/to XML and settings.
 */
class MapMarkers
{
  Q_DECLARE_TR_FUNCTIONS(MapMarkers)

public:
  /* Save all to XML file */
  void save(const QString& filename, int numBackupFiles);

  /* Load all from XML file. Appends to current list of map markers. */
  void restore(const QString& filename);

  /* Restore from legacy settings */
  void restoreFromSettings();

  /* Removes all by type */
  void clear(map::MapTypes types = map::MARK_ALL);

  /* Copies from other by type. Overwrites only if type is given in flags. */
  void copy(const MapMarkers& other, map::MapTypes types = map::MARK_ALL);

  /* Appends from other by type */
  void append(const MapMarkers& other, map::MapTypes types = map::MARK_ALL);

  /* Add map markers. Id has to be set before using getNextMarkerId(). */
  void addRangeMark(const map::RangeMarker& obj);
  void addPatternMark(const map::PatternMarker& obj);
  void addDistanceMark(const map::DistanceMarker& obj);
  void addHoldingMark(const map::HoldingMarker& obj);
  void addMsaMark(const map::MsaMarker& obj);

  /* Remove map markers by generated id from getNextMarkerId() */
  void removeRangeMark(int id);
  void removePatternMark(int id);
  void removeDistanceMark(int id);
  void removeHoldingMark(int id);
  void removeMsaMark(int id);

  /* Update measurement lines positions for either end. Ignored if id is -1  */
  void updateDistanceMarkerFromPos(int id, const atools::geo::Pos& pos);
  void updateDistanceMarkerToPos(int id, const atools::geo::Pos& pos);

  /* Also altitude in pos. Ignored if id is -1  */
  void updateHoldingMarkerPosAndAlt(int id, const atools::geo::Pos& pos);

  /* Update position. Ignored if id is -1 */
  void updateRangeMarkerPos(int id, const atools::geo::Pos& pos);

  /* Update markers including id */
  void updateDistanceMarker(const map::DistanceMarker& marker);
  void updateHoldingMarker(const map::HoldingMarker& marker);
  void updateRangeMarker(const map::RangeMarker& marker);

  /* Get range rings */
  const QMap<int, map::RangeMarker>& getRangeMarkers() const
  {
    return rangeMarkers;
  }

  /* Get distance measurement lines */
  const QMap<int, map::DistanceMarker>& getDistanceMarkers() const
  {
    return distanceMarkers;
  }

  /* Get for editing. Do not pass not existing ids since this creates empty objects. */
  map::DistanceMarker& getDistanceMarkerRef(int id);
  map::HoldingMarker& getHoldingMarkerRef(int id);
  map::RangeMarker& getRangeMarkerRef(int id);
  map::PatternMarker& getPatternMarkerRef(int id);

  /* Airfield traffic patterns. */
  const QMap<int, map::PatternMarker>& getPatternMarkers() const
  {
    return patternMarkers;
  }

  /* Holdings. */
  const QMap<int, map::HoldingMarker>& getHoldingMarkers() const
  {
    return holdingMarkers;
  }

  /* Airport MSA. */
  const QMap<int, map::MsaMarker>& getMsaMarkers() const
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

  /* true if file is detected as a map marker XML file */
  static bool isMarkersFile(const QString& filename);

  /* Get wrapped map objects from holdings and MSA for painting. markers with
   * currentId are currenly edited and are drawn as last on top, Sett currentId to -1 if nothing is edited.
   * texts are marker texts. */
  const QList<const map::MapHolding *> getHoldingMarksFiltered(int currentId, QStringList& texts) const;

  /* Get wrapped map objects from holdings and MSA for painting. markers with
   * currentId are currenly edited and are drawn as last on top, Sett currentId to -1 if nothing is edited. */
  const QList<const map::MapAirportMsa *> getMsaMarksFiltered(int currentId) const;

private:
  /* XML file version */
  static const int MARKERS_VERSION_MAJOR = 1;
  static const int MARKERS_VERSION_MINOR = 0;

  /* Use maps to ensure stable draw order as entered by the user */
  QMap<int, map::RangeMarker> rangeMarkers;
  QMap<int, map::DistanceMarker> distanceMarkers;
  QMap<int, map::PatternMarker> patternMarkers;
  QMap<int, map::HoldingMarker> holdingMarkers;
  QMap<int, map::MsaMarker> msaMarkers;

  /* Used to assign a unique id at runtime */
  int currentMapMarkerId = 0;
};

#endif // LITTLENAVMAP_MARKERS_H
