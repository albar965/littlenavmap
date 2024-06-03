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

#include "search/randomdepartureairportpickingbycriteria.h"
#include "search/randomdestinationairportpickingbycriteria.h"

#include "search/airportsearch.h"
#include "geo/pos.h"

#include <QRandomGenerator>

QVector<std::pair<int, atools::geo::Pos> > *RandomDepartureAirportPickingByCriteria::data = nullptr;
int RandomDepartureAirportPickingByCriteria::distanceMin = 0;
int RandomDepartureAirportPickingByCriteria::distanceMax = 0;
int RandomDepartureAirportPickingByCriteria::countResult = 0;
int RandomDepartureAirportPickingByCriteria::randomLimit = 0;

RandomDepartureAirportPickingByCriteria::RandomDepartureAirportPickingByCriteria(QObject *parent,
                                                                                 int predefinedAirportIndex) : QThread(parent)
{
  this->predefinedAirportIndex = predefinedAirportIndex;
  foundIndexDestination = -1;
  noSuccess = true;
}

void RandomDepartureAirportPickingByCriteria::initStatics(QVector<std::pair<int, atools::geo::Pos> > *data,
                                                          int distanceMinMeter,
                                                          int distanceMaxMeter)
{
  RandomDepartureAirportPickingByCriteria::data = data;
  RandomDepartureAirportPickingByCriteria::distanceMin = distanceMinMeter;
  RandomDepartureAirportPickingByCriteria::distanceMax = distanceMaxMeter;
  RandomDepartureAirportPickingByCriteria::countResult = data->size();
  RandomDepartureAirportPickingByCriteria::randomLimit = (RandomDepartureAirportPickingByCriteria::countResult * 7) / 10;
  RandomDestinationAirportPickingByCriteria::initStatics(data->data(),
                                                         distanceMinMeter,
                                                         distanceMaxMeter,
                                                         RandomDepartureAirportPickingByCriteria::countResult);
}

void RandomDepartureAirportPickingByCriteria::run()
{
  std::pair<int, atools::geo::Pos> *data = this->data->data();
  QMap<int, bool> triedIndexDeparture;      // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast
  int indexDeparture;
  bool departureSuccess;

  runningDestinationThreads = 0;

  do
  {
    if(predefinedAirportIndex == -1)
    {
        // split index finding into 2 approaches: random below threshold and linear above
        if(triedIndexDeparture.count() < randomLimit)
        {
          // random picking
          // on small data sets, if all are invalid value, we wouldn't
          // switch to the incremented random approach (because we
          // never exit the while and the "if < randomLimit" is outside),
          // but being a small data set, this approach might still
          // try every index after some time
          do
          {
            departureSuccess = false;
            if(triedIndexDeparture.count() == countResult)
              break;
            do
            {
              indexDeparture = QRandomGenerator::global()->bounded(countResult);
            }
            while(triedIndexDeparture.contains(indexDeparture));
            triedIndexDeparture.insert(indexDeparture, true);
            departureSuccess = true;
          }
          while(!data[indexDeparture].second.isValid());
        }
        else
        {
          // pick start index randomly, then increment
          indexDeparture = QRandomGenerator::global()->bounded(countResult);
          do
          {
            departureSuccess = false;
            if(triedIndexDeparture.count() == countResult)
              break;
            while(triedIndexDeparture.contains(indexDeparture))
            {
                ++indexDeparture %= countResult;
            }
            triedIndexDeparture.insert(indexDeparture, true);
            departureSuccess = true;
          }
          while(!data[indexDeparture].second.isValid());
        }
    }
    else
    {
      indexDeparture = predefinedAirportIndex;
      departureSuccess = true;
    }

    if(departureSuccess)
    {
      ++runningDestinationThreads;
      RandomDestinationAirportPickingByCriteria *destinationPicker = new RandomDestinationAirportPickingByCriteria(indexDeparture);
      connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::resultReady,
              this, &RandomDepartureAirportPickingByCriteria::dataReceived);
      connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::finished,
              destinationPicker, &QObject::deleteLater);
      // if a CPU affinity tool is used on lnm.exe, all threads created by Qt
      // might be run on the designated core instead of being evenly distributed
      // (at least with Process Lasso)
      destinationPicker->start();
    }

    while(noSuccess &&
          (
             runningDestinationThreads >= QThread::idealThreadCount() ||
             (triedIndexDeparture.count() == countResult && runningDestinationThreads != 0)
          )
         )
    {
      emit progressing();
      sleep(1);
    }

    if(!noSuccess || triedIndexDeparture.count() == countResult)
      break;
  }
  while(true);

  RandomDestinationAirportPickingByCriteria::stopExecution = true;
  while(runningDestinationThreads > 0)
  {
    sleep(1); // make sure all destination threads which still might use data have exited but not be a tight loop
  }

  if(departureSuccess && foundIndexDestination > -1)
  {
    emit resultReady(true, associatedIndexDeparture, foundIndexDestination, this->data);
  }
  else
  {
    // on cancellation departureSuccess is true and noSuccess is false but foundIndexDestination is still -1
    emit resultReady(false, -1, -1, this->data);
  }
}

void RandomDepartureAirportPickingByCriteria::dataReceived(const bool isSuccess, const int indexDeparture, const int indexDestination)
{
  if(isSuccess)
  {
    associatedIndexDeparture = indexDeparture;
    foundIndexDestination = indexDestination;
    noSuccess = false;
  }
  --runningDestinationThreads;
}

void RandomDepartureAirportPickingByCriteria::cancellationReceived()
{
  noSuccess = false;
  RandomDestinationAirportPickingByCriteria::stopExecution = true;
}
