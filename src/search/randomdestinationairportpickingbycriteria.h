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

#ifndef RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H
#define RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H

#include "geo/pos.h"

#include <QObject>
#include <QThread>

namespace atools {
}

class RandomDestinationAirportPickingByCriteria :
  public QThread
{
  Q_OBJECT

public:
  RandomDestinationAirportPickingByCriteria(int threadIndex,
                                            int dataRangeIndexStart,
                                            int dataRangeLength);
  virtual ~RandomDestinationAirportPickingByCriteria() override;

  // required calling !!! once, before first construction
  static void initStatics(int distanceMinMeter,
                          int distanceMaxMeter);

  // required calling !!! per construction before constructing
  // data is whole set
  static void initData(std::pair<int, atools::geo::Pos> *data,
                       int indexDeparture,
                       int *idsNonGrata,
                       int lengthIdsNonGrata);

  void run() override;

signals:
  void resultReady(const bool isSuccess, const int indexDestination, const int threadIndex);

private:
  // do the thing no C++ brain (std specifier) wanted to do before
  // returns index when found searchFor, else -1
  int binary_search(int searchFor, int *inArray, int arrayLength);

  int threadIndex;
  int dataRangeIndexStart;
  int dataRangeLength;
  static int distanceMin;
  static int distanceMax;
  static std::pair<int, atools::geo::Pos> *data;
  static int indexDeparture;
  static int *idsNonGrata;
  static int lengthIdsNonGrata;
};

#endif // RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H
