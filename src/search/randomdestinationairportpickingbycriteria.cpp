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

#include "search/randomdestinationairportpickingbycriteria.h"
#include "search/randomdepartureairportpickingbycriteria.h"

#include "geo/rect.h"

#include <QRandomGenerator>


std::pair<int, atools::geo::Pos> *RandomDestinationAirportPickingByCriteria::data = nullptr;
int RandomDestinationAirportPickingByCriteria::distanceMin = 0;
int RandomDestinationAirportPickingByCriteria::distanceMax = 0;
int RandomDestinationAirportPickingByCriteria::indexDeparture = -1;

RandomDestinationAirportPickingByCriteria::RandomDestinationAirportPickingByCriteria(int threadId,
                                                                                     int dataRangeIndexStart,
                                                                                     int dataRangeLength)
{
  this->threadId = threadId;
  this->dataRangeIndexStart = dataRangeIndexStart;
  this->dataRangeLength = dataRangeLength;
  randomLimit = (dataRangeLength * 7) / 10;
  this->offsettedData = RandomDestinationAirportPickingByCriteria::data + dataRangeIndexStart;
}

void RandomDestinationAirportPickingByCriteria::initStatics(int distanceMinMeter,
                                                            int distanceMaxMeter)
{
  RandomDestinationAirportPickingByCriteria::distanceMin = distanceMinMeter;
  RandomDestinationAirportPickingByCriteria::distanceMax = distanceMaxMeter;
}

void RandomDestinationAirportPickingByCriteria::initData(std::pair<int, atools::geo::Pos> *data,
                                                         int indexDeparture)
{
  RandomDestinationAirportPickingByCriteria::data = data;
  RandomDestinationAirportPickingByCriteria::indexDeparture = indexDeparture;
}

void RandomDestinationAirportPickingByCriteria::run()
{
  QMap<int, bool> triedIndexDestination;                    // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast
  if(indexDeparture >= dataRangeIndexStart && indexDeparture < dataRangeIndexStart + dataRangeLength)
  {
    triedIndexDestination.insert(indexDeparture, true);     // destination shall != departure
  }
  int indexDestination = -1;
  bool destinationSuccess;

  atools::geo::Pos departureSecond = data[indexDeparture].second;
  float distMeter;

  do
  {
    destinationSuccess = false;
    do
    {
      if(triedIndexDestination.count() == dataRangeLength || RandomDepartureAirportPickingByCriteria::stopExecution)
        goto destinationsEnd;
      if(triedIndexDestination.count() < randomLimit)
      {
        do
        {
          indexDestination = QRandomGenerator::global()->bounded(dataRangeLength);
        }
        while(triedIndexDestination.contains(indexDestination));
      }
      else
      {
        indexDestination = QRandomGenerator::global()->bounded(dataRangeLength);
        while(triedIndexDestination.contains(indexDestination))
        {
            ++indexDestination %= dataRangeLength;
        }
      }
      triedIndexDestination.insert(indexDestination, true);
      distMeter = departureSecond.distanceMeterTo(offsettedData[indexDestination].second); // distanceMeterTo checks for isValid
    }
    while(distMeter == atools::geo::Pos::INVALID_VALUE);
    destinationSuccess = true;
  }
  while(distMeter < distanceMin || distMeter > distanceMax);

destinationsEnd:
  emit resultReady(destinationSuccess, indexDestination, threadId);
}
