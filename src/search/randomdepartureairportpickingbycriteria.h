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

// should only be instantiated 1 at a time
class RandomDepartureAirportPickingByCriteria :
  public QThread
{
  Q_OBJECT

public:
  explicit RandomDepartureAirportPickingByCriteria(QObject *parent);

  // required calling !!
  static void initStatics(int countResult, int randomLimit, QVector<std::pair<int, atools::geo::Pos> > *data, int distanceMinMeter,
                          int distanceMaxMeter);

  void run() override;

public slots:
  void dataReceived(const bool isSuccess, const int indexDeparture, const int indexDestination);
  void cancellationReceived();

signals:
  void resultReady(const bool isSuccess, const int indexDeparture, const int indexDestination, QVector<std::pair<int,
                                                                                                                 atools::geo::Pos> > *data);
  void progressing();

private:
  bool noSuccess;
  int indexDestination;
  int associatedIndexDeparture;
  static int countResult;
  static int randomLimit;
  static QVector<std::pair<int, atools::geo::Pos> > *data;
  static int distanceMin;
  static int distanceMax;
  int runningDestinationThreads;
};

#endif // RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
