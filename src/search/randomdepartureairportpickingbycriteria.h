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

#ifndef RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
#define RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H

#include "search/randomdestinationairportpickingbycriteria.h"

#include <QObject>
#include <QThread>
#include <QMap>

namespace atools {
namespace geo {
class Pos;
}
}

// 1 departure thread creates many destination threads.
// destination threads each search on a different part of the data.
// when a match is found, all other threads are timed out.
// when no destination is found for a departure, that departure
// is removed from the set of destinations for the next departure.
// the departure thread should only be instantiated 1 at a time.
class RandomDepartureAirportPickingByCriteria :
  public QThread
{
  Q_OBJECT

public:
  explicit RandomDepartureAirportPickingByCriteria();
  virtual ~RandomDepartureAirportPickingByCriteria() override;

  // required calling !!!
  // the number of items in data can have been reduced when search
  // is done or cancelled
  static void initStatics(QList<std::pair<int, atools::geo::Pos> > *data,
                          QList<std::pair<int, int> > *antiData,
                          int distanceMinMeter,
                          int distanceMaxMeter,
                          int predefinedAirportIndex);

  void run() override;

public slots:
  void dataReceived(const bool isSuccess,
                    const int indexDestination,
                    const int threadIndex);
  void cancellationReceived();

signals:
  void resultReady(const bool isSuccess,
                   const int indexDeparture,
                   const int indexDestination,
                   QList<std::pair<int, atools::geo::Pos> > *data);
  void progressing();

private:
  friend class RandomDestinationAirportPickingByCriteria;

  QList<int> map_sort(int *array, int arrayLength);

  QList<bool> destinationPickerState; // false = running, true = done
  bool *dataDestinationPickerState;
  int foundIndexDestination = -1;
  bool success = false;
  static QList<std::pair<int, atools::geo::Pos> > *data;
  static QList<std::pair<int, int> > *antiData;
  static int predefinedAirportIndex;
  static int numberDestinationsSetParts;
  static bool stopExecution;
};

#endif // RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
