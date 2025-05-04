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


int RandomDestinationAirportPickingByCriteria::distanceMin = 0;
int RandomDestinationAirportPickingByCriteria::distanceMax = 0;
std::pair<int, atools::geo::Pos> *RandomDestinationAirportPickingByCriteria::data = nullptr;
int RandomDestinationAirportPickingByCriteria::indexDeparture = -1;
int *RandomDestinationAirportPickingByCriteria::idsNonGrata = nullptr;
int RandomDestinationAirportPickingByCriteria::lengthIdsNonGrata = 0;

RandomDestinationAirportPickingByCriteria::RandomDestinationAirportPickingByCriteria(int threadIndex,
                                                                                     int dataRangeIndexStart,
                                                                                     int dataRangeLength)
{
  this->threadIndex = threadIndex;
  this->dataRangeIndexStart = dataRangeIndexStart;
  this->dataRangeLength = dataRangeLength;
}

void RandomDestinationAirportPickingByCriteria::initStatics(int distanceMinMeter,
                                                            int distanceMaxMeter)
{
  RandomDestinationAirportPickingByCriteria::distanceMin = distanceMinMeter;
  RandomDestinationAirportPickingByCriteria::distanceMax = distanceMaxMeter;
}

void RandomDestinationAirportPickingByCriteria::initData(std::pair<int, atools::geo::Pos> *data,
                                                         int indexDeparture,
                                                         int *idsNonGrata,
                                                         int lengthIdsNonGrata)
{
  RandomDestinationAirportPickingByCriteria::data = data;
  RandomDestinationAirportPickingByCriteria::indexDeparture = indexDeparture;
  RandomDestinationAirportPickingByCriteria::idsNonGrata = idsNonGrata;
  RandomDestinationAirportPickingByCriteria::lengthIdsNonGrata = lengthIdsNonGrata;
}

void RandomDestinationAirportPickingByCriteria::run()
{
  QHash<int, bool> triedIndexDestination;                    // acts as a lookup which indices have been tried already
  if(indexDeparture >= dataRangeIndexStart && indexDeparture < dataRangeIndexStart + dataRangeLength)
  {
    triedIndexDestination.insert(indexDeparture - dataRangeIndexStart, true);     // destination shall != departure
  }

  int indexDestination = -1;
  // randomLimit :  above this limit values are not tried to be found randomly
  // because there will only be few "space" to "pick" from making random picks
  // take long, instead values are taken linearly from the remaining values
  int randomLimit = (dataRangeLength * 7) / 10;
  std::pair<int, atools::geo::Pos> *offsettedData = data + dataRangeIndexStart;
  atools::geo::Pos *posDeparture = &(data[indexDeparture].second);

  float distMeter;
  bool destinationSuccess;

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
      distMeter = posDeparture->distanceMeterTo(offsettedData[indexDestination].second);    // distanceMeterTo checks for isValid
    }
    while(distMeter == atools::geo::Pos::INVALID_VALUE);
    destinationSuccess = true;
  }
  while(distMeter < distanceMin || distMeter > distanceMax || binary_search(offsettedData[indexDestination].first, idsNonGrata, lengthIdsNonGrata) > -1);

destinationsEnd:
  emit resultReady(destinationSuccess, dataRangeIndexStart + indexDestination, threadIndex);
}

int RandomDestinationAirportPickingByCriteria::binary_search(int searchFor, int* inArray, int arrayLength)
{
  int toIndex = arrayLength - 1;
  arrayLength >>= 1;          // arrayLength becomes the half way length from fromIndex to toIndex
  int fromIndex = 0;

  while (fromIndex <= toIndex)
  {
    int middleIndex = fromIndex + arrayLength;
    int inMiddle = inArray[middleIndex];

    arrayLength >>= 1;
    if (inMiddle < searchFor)
      fromIndex = middleIndex + 1;
    else if (inMiddle > searchFor)
      toIndex = middleIndex - 1;
    else
      return middleIndex;
  }

  return -1;
}
