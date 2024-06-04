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

#ifndef RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
#define RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H

#include <QObject>
#include <QThread>

namespace atools {
  namespace geo {
    class Pos;
  }
}

// per destination thread a different departure airport is used
// destination threads don't sync themselves
// hence the multi-threading here is like multiple single searches
// parallely because we can. Whichever thread first finds a match
// results in all other threads getting timed out
// this departure thread should only be instantiated 1 at a time
class RandomDepartureAirportPickingByCriteria :
  public QThread
{
  Q_OBJECT

public:
  explicit RandomDepartureAirportPickingByCriteria(QObject *parent,
                                                   int predefinedAirportIndex);

  // required calling !!!
  static void initStatics(QVector<std::pair<int, atools::geo::Pos> > *data,
                          int distanceMinMeter,
                          int distanceMaxMeter);

  void run() override;

public slots:
  void dataReceived(const bool isSuccess, const int indexDeparture, const int indexDestination);
  void cancellationReceived();

signals:
  void resultReady(const bool isSuccess,
                   const int indexDeparture,
                   const int indexDestination,
                   QVector<std::pair<int, atools::geo::Pos> > *data);
  void progressing();

private:
  int predefinedAirportIndex;
  bool noSuccess;
  int foundIndexDestination;
  int associatedIndexDeparture;
  static QVector<std::pair<int, atools::geo::Pos> > *data;
  static int distanceMin;
  static int distanceMax;
  static int countResult;
  // randomLimit :  above this limit values are not tried to be found randomly
  // because there will only be few "space" to "pick" from making random picks
  // take long, instead values are taken linearly from the remaining values
  static int randomLimit;
  int runningDestinationThreads;
};

#endif // RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
