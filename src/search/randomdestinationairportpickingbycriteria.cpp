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

#include "geo/rect.h"

#include <QRandomGenerator>

int RandomDestinationAirportPickingByCriteria::randomLimit = 0;
std::pair<int, atools::geo::Pos> *RandomDestinationAirportPickingByCriteria::data = nullptr;
int RandomDestinationAirportPickingByCriteria::distanceMin = 0;
int RandomDestinationAirportPickingByCriteria::distanceMax = 0;
int RandomDestinationAirportPickingByCriteria::countResult = 0;
bool RandomDestinationAirportPickingByCriteria::stopExecution = false;

RandomDestinationAirportPickingByCriteria::RandomDestinationAirportPickingByCriteria(int indexDeparture)
{
  this->indexDeparture = indexDeparture;
}

void RandomDestinationAirportPickingByCriteria::initStatics(std::pair<int, atools::geo::Pos> *data,
                                                            int distanceMinMeter,
                                                            int distanceMaxMeter,
                                                            int countResult)
{
  RandomDestinationAirportPickingByCriteria::data = data;
  RandomDestinationAirportPickingByCriteria::distanceMin = distanceMinMeter;
  RandomDestinationAirportPickingByCriteria::distanceMax = distanceMaxMeter;
  RandomDestinationAirportPickingByCriteria::countResult = countResult;
  RandomDestinationAirportPickingByCriteria::randomLimit = (countResult * 7) / 10;
  stopExecution = false;
}

void RandomDestinationAirportPickingByCriteria::run()
{
  QMap<int, bool> triedIndexDestination;                    // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast
  triedIndexDestination.insert(indexDeparture, true);       // destination shall != departure
  int indexDestination;
  bool destinationSuccess;

  float distMeter = atools::geo::Pos::INVALID_VALUE;

  do
  {
    destinationSuccess = false;
    do
    {
      if(triedIndexDestination.count() == countResult || stopExecution)
        goto destinationsEnd;
      // the "if" is inside the "while" for the destination because the indices get "used up" during "picking"
      if(triedIndexDestination.count() < randomLimit)
      {
        do
        {
          indexDestination = QRandomGenerator::global()->bounded(countResult);
        }
        while(triedIndexDestination.contains(indexDestination));
      }
      else
      {
        indexDestination = QRandomGenerator::global()->bounded(countResult);
        while(triedIndexDestination.contains(indexDestination))
        {
          ++indexDestination %= countResult;
        }
      }
      triedIndexDestination.insert(indexDestination, true);
      distMeter = data[indexDeparture].second.distanceMeterTo(data[indexDestination].second); // distanceMeterTo checks for isValid
    }
    while(distMeter == atools::geo::Pos::INVALID_VALUE);
    destinationSuccess = true;
  }
  while(distMeter < distanceMin || distMeter > distanceMax);

destinationsEnd:
  if(destinationSuccess) // the last triedIndexDestination might be taken but it might have passed the last condition (thus checking a bool and not the map count being countResult)
  {
    emit resultReady(true, indexDeparture, indexDestination);
  }
  else
  {
    emit resultReady(false, -1, -1);
  }
}
