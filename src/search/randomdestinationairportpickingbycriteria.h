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

#ifndef RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H
#define RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H

#include <QObject>
#include <QThread>

namespace atools {
  namespace geo {
    class Pos;
  }
}

class RandomDestinationAirportPickingByCriteria :
  public QThread
{
  Q_OBJECT

public:
  RandomDestinationAirportPickingByCriteria(int threadId,
                                            int dataRangeIndexStart,
                                            int dataRangeLength);

  // required calling !!! once, before first construction
  static void initStatics(int distanceMinMeter,
                          int distanceMaxMeter);

  // required calling !!! per construction before constructing
  // data is whole set
  static void initData(std::pair<int, atools::geo::Pos> *data,
                       int indexDeparture);

  void run() override;

signals:
  void resultReady(const bool isSuccess, const int indexDestination, const int threadId);

private:
  int threadId;
  int dataRangeIndexStart;
  int dataRangeLength;
  // randomLimit :  above this limit values are not tried to be found randomly
  // because there will only be few "space" to "pick" from making random picks
  // take long, instead values are taken linearly from the remaining values
  int randomLimit;
  std::pair<int, atools::geo::Pos> *offsettedData;
  static std::pair<int, atools::geo::Pos> *data;
  static int distanceMin;
  static int distanceMax;
  static int indexDeparture;
};

#endif // RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H
