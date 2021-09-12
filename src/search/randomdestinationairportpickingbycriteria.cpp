#include "randomdestinationairportpickingbycriteria.h"

#include "geo/rect.h"

#include <QRandomGenerator>

int RandomDestinationAirportPickingByCriteria::countResult = 0;                 // required to get memory allocated in C++ (the initStatics function is not guaranteed to get called from a compiler/linker perspective)
int RandomDestinationAirportPickingByCriteria::randomLimit = 0;
std::pair<int, atools::geo::Pos>* RandomDestinationAirportPickingByCriteria::data = nullptr;
int RandomDestinationAirportPickingByCriteria::distanceMin = 0;
int RandomDestinationAirportPickingByCriteria::distanceMax = 0;
bool RandomDestinationAirportPickingByCriteria::stopExecution = false;

RandomDestinationAirportPickingByCriteria::RandomDestinationAirportPickingByCriteria(int indexDeparture)
{
  this->indexDeparture = indexDeparture;
}

void RandomDestinationAirportPickingByCriteria::initStatics(int countResult, int randomLimit, std::pair<int, atools::geo::Pos>* data, int distanceMinMeter, int distanceMaxMeter)
{
  RandomDestinationAirportPickingByCriteria::countResult = countResult;
  RandomDestinationAirportPickingByCriteria::randomLimit = randomLimit;
  RandomDestinationAirportPickingByCriteria::data = data;
  RandomDestinationAirportPickingByCriteria::distanceMin = distanceMinMeter;
  RandomDestinationAirportPickingByCriteria::distanceMax = distanceMaxMeter;
  stopExecution = false;
}

void RandomDestinationAirportPickingByCriteria::run()
{
  int indexDestination;

  QMap<int, bool> triedIndexDestination;                                        // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast
  triedIndexDestination.insert(indexDeparture, true);                           // destination shall != departure

  float distMeter=atools::geo::Pos::INVALID_VALUE;

  bool destinationSuccess;

  do
  {
    destinationSuccess = false;
    do
    {
      if(triedIndexDestination.count() == countResult || stopExecution)
        goto destinationsEnd;                                                   // all destinations have been depleted, try a different departure
      if(triedIndexDestination.count() < randomLimit)                           // the "if" is inside the "while" for the destination because the indices get used up during "picking"
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
          indexDestination = (indexDestination + 1) % countResult;
        }
      }
      triedIndexDestination.insert(indexDestination, true);
      distMeter = data[indexDeparture].second.distanceMeterTo(data[indexDestination].second);         // distanceMeterTo checks for isValid
    }
    while(distMeter == atools::geo::Pos::INVALID_VALUE);
    destinationSuccess = true;
  }
  while(distMeter < distanceMin || distMeter > distanceMax);
destinationsEnd:
  if(destinationSuccess)                                                        // the last triedIndexDestination might be taken but it might have passed the last condition (thus checking a bool and not the map count being countResult)
  {
    emit resultReady(true, indexDeparture, indexDestination);
  }
  else
  {
    emit resultReady(false, -1, -1);
  }
}
