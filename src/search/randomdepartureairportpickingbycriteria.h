#ifndef RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
#define RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H

#include "search/randomdestinationairportpickingbycriteria.h"

#include <QObject>
#include <QThread>

namespace atools {
  namespace geo {
    class Pos;
  }
}

// should only be instantiated 1 at a time
class RandomDepartureAirportPickingByCriteria : public QThread
{
  Q_OBJECT

public:
  RandomDepartureAirportPickingByCriteria(QObject *parent);

  // required calling !!
  static void initStatics(const int &countResult, const int &randomLimit, QVector<std::pair<int, atools::geo::Pos>>* data, const int &distanceMin, const int &distanceMax);

  void run() override;

public slots:
  void dataReceived(const bool isSuccess, const int indexDeparture, const int indexDestination);
  void cancellationReceived();

signals:
  void resultReady(const bool isSuccess, const int indexDeparture, const int indexDestination, QVector<std::pair<int, atools::geo::Pos>>* data);

private:
  bool noSuccess;
  int indexDestination;
  int associatedIndexDeparture;
  QObject* parent;
  static int countResult;
  static int randomLimit;
  static QVector<std::pair<int, atools::geo::Pos>>* data;
  static int distanceMin;
  static int distanceMax;
  int runningDestinationThreads;
};

#endif // RANDOMDEPARTUREAIRPORTPICKINGBYCRITERIA_H
