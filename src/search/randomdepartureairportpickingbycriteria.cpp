#include "randomdepartureairportpickingbycriteria.h"

#include "search/airportsearch.h"

#include <QRandomGenerator>

int RandomDepartureAirportPickingByCriteria::countResult = 0;
int RandomDepartureAirportPickingByCriteria::randomLimit = 0;
QVector<std::pair<int, atools::geo::Pos>>* RandomDepartureAirportPickingByCriteria::data = nullptr;
int RandomDepartureAirportPickingByCriteria::distanceMin = 0;
int RandomDepartureAirportPickingByCriteria::distanceMax = 0;

RandomDepartureAirportPickingByCriteria::RandomDepartureAirportPickingByCriteria(QObject *parent) : QThread(parent)
{
  indexDestination = -1;
  noSuccess = true;
}

void RandomDepartureAirportPickingByCriteria::initStatics(int countResult, int randomLimit, QVector<std::pair<int, atools::geo::Pos>>* data, int distanceMinMeter,  int distanceMaxMeter)
{
  RandomDepartureAirportPickingByCriteria::countResult = countResult;
  RandomDepartureAirportPickingByCriteria::randomLimit = randomLimit;
  RandomDepartureAirportPickingByCriteria::data = data;
  RandomDepartureAirportPickingByCriteria::distanceMin = distanceMinMeter;
  RandomDepartureAirportPickingByCriteria::distanceMax = distanceMaxMeter;
  RandomDestinationAirportPickingByCriteria::initStatics(countResult, randomLimit, data->data(), distanceMinMeter, distanceMaxMeter);
}

void RandomDepartureAirportPickingByCriteria::run()
{
  std::pair<int, atools::geo::Pos>* data = this->data->data();

  QMap<int, bool> triedIndexDeparture;                                          // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast

  bool departureSuccess;

  int indexDeparture = 0;

  runningDestinationThreads = 0;

  do
  {
    // split index finding into 2 approaches (if(while) rather than while(if) for performance reason)
    if(triedIndexDeparture.count() < randomLimit)
    {
      // random picking
      // on small result sets, if all are invalid value, we wouldn't switch to the incremented random approach (because the "if" is outside), but being a small result set, this approach might still try every index after some time
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
    else {
      // random pick, then increment
      indexDeparture = QRandomGenerator::global()->bounded(countResult);
      do
      {
        departureSuccess = false;
        if(triedIndexDeparture.count() == countResult)
          break;
        while(triedIndexDeparture.contains(indexDeparture))
        {
          indexDeparture = (indexDeparture + 1) % countResult;
        }
        triedIndexDeparture.insert(indexDeparture, true);
        departureSuccess = true;
      }
      while(!data[indexDeparture].second.isValid());
    }

    if(departureSuccess)
    {
      runningDestinationThreads++;
      RandomDestinationAirportPickingByCriteria* destinationPicker = new RandomDestinationAirportPickingByCriteria(indexDeparture);
      connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::resultReady, this, &RandomDepartureAirportPickingByCriteria::dataReceived);
      connect(destinationPicker, &RandomDestinationAirportPickingByCriteria::finished, destinationPicker, &QObject::deleteLater);
      destinationPicker->start();
    }

    while(noSuccess && ((runningDestinationThreads >= QThread::idealThreadCount()) || ((triedIndexDeparture.count() == countResult) && (runningDestinationThreads != 0))))             // if a CPU affinity tool is used on lnm exe, all threads created by lnm might be run on the designated core instead of being evenly distributed (at least with Process Lasso)
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
    sleep(1);                                                                   // make sure all destination threads which still might use data have exited but not be a tight loop
  }

  if(departureSuccess && indexDestination > -1)                                 // on cancellation departureSuccess is true and noSuccess is false but indexDestination is still -1
  {
    emit resultReady(true, associatedIndexDeparture, indexDestination, this->data);
  }
  else
  {
    emit resultReady(false, -1, -1, this->data);
  }
}

void RandomDepartureAirportPickingByCriteria::dataReceived(const bool isSuccess, const int indexDeparture, const int indexDestination)
{
  if(isSuccess)
  {
    noSuccess = false;
    associatedIndexDeparture = indexDeparture;
    this->indexDestination = indexDestination;
  }
  runningDestinationThreads--;
}

void RandomDepartureAirportPickingByCriteria::cancellationReceived()
{
  noSuccess = false;
  RandomDestinationAirportPickingByCriteria::stopExecution = true;
}
