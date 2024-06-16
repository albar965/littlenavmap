/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "app/dataexchange.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/filecheck.h"
#include "util/properties.h"
#include "common/constants.h"
#include "settings/settings.h"

#include <QDataStream>
#include <QSharedMemory>
#include <QDateTime>
#include <QDir>
#include <QThread>

using atools::settings::Settings;

static const int SHARED_MEMORY_SIZE = 8192;
static const int FETCH_TIMER_INTERVAL_MS = 500;
static const int MAX_TIME_DIFFENCE_MS = 1000;

DataExchangeFetcher::DataExchangeFetcher(bool verboseParam)
  : verbose(verboseParam)
{
  setObjectName("DataExchangeFetcher");
  timer = new QTimer(this);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, this, &DataExchangeFetcher::fetchSharedMemory);
}

DataExchangeFetcher::~DataExchangeFetcher()
{
  timer->stop();
  ATOOLS_DELETE_LOG(timer);
}

void DataExchangeFetcher::startTimer()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;

  timer->start(FETCH_TIMER_INTERVAL_MS);
}

void DataExchangeFetcher::fetchSharedMemory()
{
  // sharedMemory is accessed exclusively by the thread here

  atools::util::Properties properties;

  if(sharedMemory != nullptr)
  {
    // Already attached
    if(sharedMemory->lock())
    {
      // Read size only for now
      quint32 size;
      // Create a view on the shared memory for reading - does not copy
      QByteArray sizeBytes = QByteArray::fromRawData(static_cast<const char *>(sharedMemory->constData()), SHARED_MEMORY_SIZE);
      QDataStream in(&sizeBytes, QIODevice::ReadOnly);
      in >> size;

      if(size > 0)
      {
        // Read properties if size is given - skip timestamp
        in.skipRawData(sizeof(qint64));
        in >> properties;
      }

      // Write back a null size and update timestamp to allow the other process detect a crash
      QByteArray bytesEmpty;
      QDataStream out(&bytesEmpty, QIODevice::WriteOnly);
      out << static_cast<quint32>(0) << QDateTime::currentMSecsSinceEpoch();
      memcpy(sharedMemory->data(), bytesEmpty.constData(), static_cast<size_t>(bytesEmpty.size()));

      sharedMemory->unlock();
    }
  }
  else
    qWarning() << Q_FUNC_INFO << "SHM is null";

  if(!properties.isEmpty())
  {
    // Found properties, i.e. a message - send signal
    qInfo() << Q_FUNC_INFO << properties;
    emit dataFetched(properties);
  }

  timer->start(FETCH_TIMER_INTERVAL_MS);
}

// ================================================================================================
DataExchange::DataExchange(bool verboseParam, const QString& programGuid)
  : QObject(nullptr), verbose(verboseParam)
{
  exit = false;

  // sharedMemory is accessess exclusively by the constructor here
  // Detect other running application instance with same settings - this is unsafe on Unix since shm can remain after crashes
  if(sharedMemory == nullptr)
    sharedMemory = new QSharedMemory(programGuid + "-" + atools::cleanPath(QFileInfo(Settings::getPath()).canonicalFilePath()));

  // Create and attach
  if(sharedMemory->create(SHARED_MEMORY_SIZE, QSharedMemory::ReadWrite))
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << "Created" << sharedMemory->key() << sharedMemory->nativeKey();

    // Created =====================================================================
    // Write null size and timestamp into the shared memory segment - not sending message here
    // A crash is detected if timestamp is not updated
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << static_cast<quint32>(0) << QDateTime::currentMSecsSinceEpoch();

    if(sharedMemory->lock())
    {
      memcpy(sharedMemory->data(), bytes.constData(), static_cast<size_t>(bytes.size()));
      sharedMemory->unlock();
    }
  }
  else
  {
    // Attach to already present one if not attached =========================================
    if(sharedMemory->attach(QSharedMemory::ReadWrite))
    {
      if(verbose)
        qDebug() << Q_FUNC_INFO << "Attached" << sharedMemory->key() << sharedMemory->nativeKey();

      // Read timestamp to detect freeze or crash
      if(sharedMemory->lock())
      {
        qint64 datetime;
        quint32 size;
        // Create a view on the shared memory for reading - does not copy but needs a lock
        QByteArray sizeBytes = QByteArray::fromRawData(static_cast<const char *>(sharedMemory->constData()), SHARED_MEMORY_SIZE);
        QDataStream in(&sizeBytes, QIODevice::ReadOnly);
        in >> size >> datetime;

        // If timestamp is older than MAX_TIME_DIFFENCE_MS other might be crashed or frozen, start normally - otherwise send message
        if(QDateTime::fromMSecsSinceEpoch(datetime).msecsTo(QDateTime::currentDateTimeUtc()) < MAX_TIME_DIFFENCE_MS)
        {
          // Copy command line parameters and add activate option to bring other to front
          atools::util::Properties properties(NavApp::getStartupOptionsConst());
          if(!properties.contains(lnm::STARTUP_COMMAND_QUIT))
            properties.setPropertyBool(lnm::STARTUP_COMMAND_ACTIVATE, true); // Always raise other window except on qut

          if(verbose)
            qDebug() << Q_FUNC_INFO << "Sending" << properties;

          // Get properties for size
          QByteArray propBytes = properties.asByteArray();

          // Write off for other process
          QByteArray bytes;
          QDataStream out(&bytes, QIODevice::WriteOnly);
          out << static_cast<quint32>(propBytes.size()) << datetime; // Use original unchanged timestamp here to avoid updating due to restarts
          out.writeRawData(propBytes.constData(), propBytes.size());

          if(bytes.size() < sharedMemory->size())
          {
            memcpy(sharedMemory->data(), bytes.constData(), static_cast<size_t>(bytes.size()));
            exit = true;
          }
          else
            qWarning() << Q_FUNC_INFO << "SHM is too small" << sharedMemory->size();

        } // if(QDateTime::fromMSecsSinceEpoch(date ...

        sharedMemory->unlock();
      } // if(sharedMemory->lock())
    }
  }

  if(verbose)
    qDebug() << Q_FUNC_INFO << "exit" << exit;
}

DataExchange::~DataExchange()
{
  if(dataFetcherThread != nullptr)
  {
    dataFetcherThread->quit();
    dataFetcherThread->wait();
  }
  ATOOLS_DELETE_LATER_LOG(dataFetcherThread);
  ATOOLS_DELETE_LATER_LOG(dataFetcher);

  if(sharedMemory != nullptr)
    sharedMemory->detach();
  ATOOLS_DELETE_LATER_LOG(sharedMemory);
}

void DataExchange::startTimer()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;

  dataFetcher = new DataExchangeFetcher(verbose);
  dataFetcher->setSharedMemory(sharedMemory);

  // Connect thread signal to main thread slot
  connect(dataFetcher, &DataExchangeFetcher::dataFetched, this, &DataExchange::loadData, Qt::QueuedConnection);

  dataFetcherThread = new QThread(this);
  dataFetcherThread->setObjectName("DataFetcherThread");

  // Move worker to thread
  dataFetcher->moveToThread(dataFetcherThread);

  // Start timer in thread context once started
  connect(dataFetcherThread, &QThread::started, dataFetcher, &DataExchangeFetcher::startTimer);
  dataFetcherThread->start();
}

void DataExchange::loadData(atools::util::Properties properties)
{
  // Check for message from other instance
  if(!properties.isEmpty())
  {
    // Found message
    if(verbose)
      qDebug() << Q_FUNC_INFO << properties;

    // Extract filenames from known options ================================
    QString flightplan, flightplanDescr, perf, layout;
    fc::fromStartupProperties(properties, &flightplan, &flightplanDescr, &perf, &layout);

    // Quit without activate =====================================================
    if(properties.contains(lnm::STARTUP_COMMAND_QUIT))
      emit quit();
    else
    {
      // Load files if found and exist ===========================================
      if(!flightplan.isEmpty() && atools::checkFile(Q_FUNC_INFO, flightplan, true /* warn */))
        emit loadRoute(flightplan);

      if(!flightplanDescr.isEmpty())
        emit loadRouteDescr(flightplanDescr);

      if(!layout.isEmpty() && atools::checkFile(Q_FUNC_INFO, layout, true /* warn */))
        emit loadLayout(layout);

      if(!perf.isEmpty() && atools::checkFile(Q_FUNC_INFO, perf, true /* warn */))
        emit loadPerf(perf);

      // Activate window - always sent by other instance =====================================================
      if(properties.getPropertyBool(lnm::STARTUP_COMMAND_ACTIVATE))
        emit activateMain();
    }
  }
  else if(verbose)
    qDebug() << Q_FUNC_INFO << "properties empty";
}
