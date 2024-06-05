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
int RandomDepartureAirportPickingByCriteria::predefinedAirportIndex = -1;
int RandomDepartureAirportPickingByCriteria::numberDestinationsSetParts = 0;
bool RandomDepartureAirportPickingByCriteria::stopExecution = false;

RandomDepartureAirportPickingByCriteria::RandomDepartureAirportPickingByCriteria()
{
  runningDestinationThreads = 0;
}

void RandomDepartureAirportPickingByCriteria::initStatics(QVector<std::pair<int, atools::geo::Pos> > *data,
                                                          int distanceMinMeter,
                                                          int distanceMaxMeter,
                                                          int predefinedAirportIndex)
{
  RandomDepartureAirportPickingByCriteria::data = data;
  RandomDepartureAirportPickingByCriteria::predefinedAirportIndex = predefinedAirportIndex;
  RandomDepartureAirportPickingByCriteria::numberDestinationsSetParts = QThread::idealThreadCount();
  RandomDepartureAirportPickingByCriteria::stopExecution = false;
  RandomDestinationAirportPickingByCriteria::initStatics(distanceMinMeter,
                                                         distanceMaxMeter);
}

void RandomDepartureAirportPickingByCriteria::run()
{
  int indexDeparture;
  bool departureSuccess;

  int numberDestinationThreadsStarted = 0;
  int size = data->size();

  do
  {
    if(predefinedAirportIndex == -1)
    {
      do
      {
        departureSuccess = false;
        indexDeparture = QRandomGenerator::global()->bounded(size);
        if(data->at(indexDeparture).second.isValid())
        {
          departureSuccess = true;
          break;
        }
        data->remove(indexDeparture);
        size = data->size();
      }
      while(size && !stopExecution);
    }
    else
    {
      indexDeparture = predefinedAirportIndex;
      departureSuccess = true;
    }

    if(departureSuccess)
    {
      // if a CPU affinity tool is used on lnm.exe, all threads created by Qt
      // might be run on the designated core instead of being evenly distributed
      // (at least with Process Lasso)
      int lengthDestinationsSetPart = size / numberDestinationsSetParts;
      int counter = numberDestinationsSetParts - 1;
      int lengthLastDestinationsSetPart = size - counter * lengthDestinationsSetPart;

      RandomDestinationAirportPickingByCriteria::initData(data->data(), indexDeparture);

      ++runningDestinationThreads;
      ++numberDestinationThreadsStarted;
      RandomDestinationAirportPickingByCriteria *destinationPicker = new RandomDestinationAirportPickingByCriteria(numberDestinationThreadsStarted,
                                                                                                                   counter * lengthDestinationsSetPart,
                                                                                                                   lengthLastDestinationsSetPart);
      departureThreads.insert(numberDestinationThreadsStarted, destinationPicker);
      connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::resultReady,
              this, &RandomDepartureAirportPickingByCriteria::dataReceived);
      destinationPicker->start();

      if(lengthDestinationsSetPart)
      {
        while(counter && !stopExecution)
        {
          ++runningDestinationThreads;
          ++numberDestinationThreadsStarted;
          RandomDestinationAirportPickingByCriteria *destinationPicker = new RandomDestinationAirportPickingByCriteria(numberDestinationThreadsStarted,
                                                                                                                        --counter * lengthDestinationsSetPart,
                                                                                                                       lengthDestinationsSetPart);
          departureThreads.insert(numberDestinationThreadsStarted, destinationPicker);
          connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::resultReady,
                  this, &RandomDepartureAirportPickingByCriteria::dataReceived);
          destinationPicker->start();
        }
      }

      while(noSuccess && runningDestinationThreads && !stopExecution)
      {
        emit progressing();
        msleep(15);
      }

      if(!noSuccess)
        break;
    }

    data->remove(indexDeparture);
    size = data->size();
  }
  while(size && !stopExecution && predefinedAirportIndex == -1);

  stopExecution = true;
  while(runningDestinationThreads)
  {
    // make sure all destination threads which still might
    // send data have exited but not be a tight loop
    // (1ms is not tight)
    msleep(1);
  }

  emit resultReady(foundIndexDestination > -1, indexDeparture, foundIndexDestination, data);
}

void RandomDepartureAirportPickingByCriteria::dataReceived(const bool isSuccess,
                                                           const int indexDestination,
                                                           const int threadId)
{
  // the object this method belongs to lives in the main thread
  // the main thread runs methods called in it sequentially
  // https://doc.qt.io/qt-5/qobject.html#thread-affinity
  // the caller is a thread which was created in this run call which is a different thread from the main thread
  // hence the signal-slot-connection type should be Qt::QueuedConnection automatically
  // furthermore associatedIndexDeparture and foundIndexDestination are read after all potantial callers exited
  // thus they ought always be from 1 caller and not possibly be from 2 caller (ie. the following ought not occur:
  // caller thread 2 sets associatedIndexDeparture before it is read and foundIndexDestination is still from
  // caller thread 1 and was read before)
  if(isSuccess)
  {
    foundIndexDestination = indexDestination;
    noSuccess = false;
  }
  // this thread does not have an event loop
  // started threads would only be deleted when this thread is deleted
  // https://doc.qt.io/qt-5/qobject.html#deleteLater
  // this would keep resources in use unneccessarily and which could be used elsewhere better
  // furthermore this thread used to be parented to the main thread and would only
  // be deleted on program exit.
  delete departureThreads.take(threadId);
  --runningDestinationThreads;
}

void RandomDepartureAirportPickingByCriteria::cancellationReceived()
{
  stopExecution = true;
}
