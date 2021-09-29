#ifndef RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H
#define RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H

#include "search/randomdepartureairportpickingbycriteria.h"

#include <QObject>
#include <QThread>

namespace atools {
  namespace geo {
    class Pos;
  }
}

class RandomDestinationAirportPickingByCriteria : public QThread
{
  Q_OBJECT

public:
  RandomDestinationAirportPickingByCriteria(int indexDeparture);

  // required calling !!
  static void initStatics(int countResult, int randomLimit, std::pair<int, atools::geo::Pos>* data, int distanceMinMeter, int distanceMaxMeter);

  void run() override;

signals:
  void resultReady(const bool isSuccess, const int indexDeparture, const int indexDestination);

private:
  friend class RandomDepartureAirportPickingByCriteria;
  static bool stopExecution;                                                    // Qt has no way to finally stop a thread
  int indexDeparture;
  static int countResult;
  static int randomLimit;
  static std::pair<int, atools::geo::Pos>* data;
  static int distanceMin;
  static int distanceMax;
};

#endif // RANDOMDESTINATIONAIRPORTPICKINGBYCRITERIA_H
