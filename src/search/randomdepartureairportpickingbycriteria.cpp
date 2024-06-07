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

#include "geo/pos.h"

#include <QRandomGenerator>

QVector<std::pair<int, atools::geo::Pos> > *RandomDepartureAirportPickingByCriteria::data = nullptr;
int RandomDepartureAirportPickingByCriteria::predefinedAirportIndex = -1;
int RandomDepartureAirportPickingByCriteria::numberDestinationsSetParts = 0;
bool RandomDepartureAirportPickingByCriteria::stopExecution = false;

RandomDepartureAirportPickingByCriteria::RandomDepartureAirportPickingByCriteria()
{
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

  int size = data->size();

  do
  {
    if(predefinedAirportIndex == -1)
    {
      departureSuccess = false;
      do
      {
        indexDeparture = QRandomGenerator::global()->bounded(size);
        if(data->at(indexDeparture).second.isValid())
        {
          departureSuccess = true;
          break;
        }
        data->remove(indexDeparture);
        --size;
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
      // (at least with a Process Lasso version used)

      destinationPickerState.clear();
      QVector<RandomDestinationAirportPickingByCriteria*> destinationThreads;

      int runningDestinationThreads = 0;

      int lengthDestinationsSetPart = size / numberDestinationsSetParts;
      int counter = numberDestinationsSetParts - 1;
      int lengthLastDestinationsSetPart = size - counter * lengthDestinationsSetPart;

      RandomDestinationAirportPickingByCriteria::initData(data->data(), indexDeparture);

      RandomDestinationAirportPickingByCriteria *destinationPicker = new RandomDestinationAirportPickingByCriteria(runningDestinationThreads,
                                                                                                                   counter * lengthDestinationsSetPart,
                                                                                                                   lengthLastDestinationsSetPart);
      destinationThreads.append(destinationPicker);
      destinationPickerState.append(false);
      connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::resultReady,
              this, &RandomDepartureAirportPickingByCriteria::dataReceived);
      ++runningDestinationThreads;

      if(lengthDestinationsSetPart)
      {
        while(counter && !stopExecution)
        {
          destinationPicker = new RandomDestinationAirportPickingByCriteria(runningDestinationThreads,
                                                                            --counter * lengthDestinationsSetPart,
                                                                            lengthDestinationsSetPart);
          destinationThreads.append(destinationPicker);
          destinationPickerState.append(false);
          connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::resultReady,
                  this, &RandomDepartureAirportPickingByCriteria::dataReceived);
          ++runningDestinationThreads;
        }
      }

      dataDestinationPickerState = destinationPickerState.data();
      foreach (destinationPicker, destinationThreads)
        destinationPicker->start();

      while(destinationPickerState.contains(false))
      {
        emit progressing();
        msleep(15);
      }
      // QThreads run was about to end (no false state anymore) but
      // that it really did and not does wait in a limbo before it
      // finally advances "1 instruction further" and hence QThread
      // object can be safely deleted is not guaranteed.
      // When using deleteLater we would have to wait for this thread
      // to end for all resources be released. I don't want to wait.
      msleep(30);
      do
        delete destinationThreads.takeLast();
      while(!destinationThreads.isEmpty());

      if(success)
        break;
    }

    data->remove(indexDeparture);
    --size;
  }
  while(size && !stopExecution && predefinedAirportIndex == -1);

  emit resultReady(foundIndexDestination > -1, indexDeparture, foundIndexDestination, data);
}

void RandomDepartureAirportPickingByCriteria::dataReceived(const bool isSuccess,
                                                           const int indexDestination,
                                                           const int threadIndex)
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
    success = true;
  }
  // this thread does not have an event loop
  // started threads would only be deleted when this thread is deleted
  // https://doc.qt.io/qt-5/qobject.html#deleteLater
  // this would keep resources in use unneccessarily and which could be used elsewhere better
  // furthermore this thread used to be parented to the main thread and would only
  // be deleted on program exit.
  // QVector.replace is not marked as thread-safe in Qt documentation
  dataDestinationPickerState[threadIndex] = true;
}

void RandomDepartureAirportPickingByCriteria::cancellationReceived()
{
  stopExecution = true;
}
