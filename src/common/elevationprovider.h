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

#ifndef LITTLENAVMAP_ELEVATIONPROVIDER_H
#define LITTLENAVMAP_ELEVATIONPROVIDER_H

#include <QMutex>
#include <QObject>

namespace Marble {
class ElevationModel;
}

namespace atools {
namespace fs {
namespace common {
class GlobeReader;
}
}

namespace geo {
class Pos;
class LineString;
class Line;
}
}

/*
 * Wraps the slow Marble online elevation provider and the fast offline GLOBE data provider.
 * Use GLOBE data if all paramters are set properly in settings.
 *
 * Class is thread safe.
 */
class ElevationProvider :
  public QObject
{
  Q_OBJECT

public:
  ElevationProvider(QObject *parent, const Marble::ElevationModel *model);
  virtual ~ElevationProvider();

  /* Elevation in meter. Only for offline data. */
  float getElevationMeter(const atools::geo::Pos& pos);

  /* Get elevations along a great circle line. Will create a point every 500 meters and delete
   * consecutive ones with same elevation. Elevation given in meter */
  void getElevations(atools::geo::LineString& elevations, const atools::geo::Line& line);

  /* true if the data is provided from the fast offline source */
  bool isGlobeOfflineProvider() const
  {
    return globeReader != nullptr;
  }

  /* True if directory is valid and contains at least one valid GLOBE file */
  bool isGlobeDirectoryValid(const QString& path) const;

  void optionsChanged();

signals:
  /*  Elevation tiles loaded. You will get more accurate results when querying height
   * for at least one that was queried before. Only sent for online data. */
  void updateAvailable();

private:
  void marbleUpdateAvailable();
  void updateReader();

  const Marble::ElevationModel *marbleModel = nullptr;
  atools::fs::common::GlobeReader *globeReader = nullptr;

  /* Need to synchronize here since it is called from profile widget thread */
  mutable QMutex mutex;

};

#endif // LITTLENAVMAP_ELEVATIONPROVIDER_H
