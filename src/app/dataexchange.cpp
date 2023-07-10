/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

static const int SHARED_MEMORY_SIZE = 4096;
static QLatin1String PROGRAM_GUID("203abd54-8a6a-4308-a654-6771efec62cd");

DataExchange::DataExchange()
  : QObject(nullptr)
{
  // Shared memory layout:
  // - quint32 Size of serialized property list
  // - qint64 Timestamp milliseconds since Epoch
  // - properties Serialized properties object

  exit = false;

  // Detect other running application instance with same settings - this is unsafe on Unix since shm can remain after crashes
  if(sharedMemory == nullptr)
    sharedMemory = new QSharedMemory(PROGRAM_GUID + "-" +
                                     QDir::cleanPath(QFileInfo(atools::settings::Settings::getPath()).canonicalFilePath()));

  // Create and attach
  if(sharedMemory->create(SHARED_MEMORY_SIZE, QSharedMemory::ReadWrite))
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "Created" << sharedMemory->key() << sharedMemory->nativeKey();
#endif

    // Created =====================================================================
    // Write null size and timestamp into the shared memory segment
    // A crash is detected if timestamp is not updated for ten seconds
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
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "Attached" << sharedMemory->key() << sharedMemory->nativeKey();
#endif

      // Read timestamp to detect freeze or crash
      if(sharedMemory->lock())
      {
        qint64 datetime;
        quint32 size;
        // Create a view on the shared memory for reading - does not copy but needs a lock
        QByteArray sizeBytes = QByteArray::fromRawData(static_cast<const char *>(sharedMemory->constData()), SHARED_MEMORY_SIZE);
        QDataStream in(&sizeBytes, QIODevice::ReadOnly);
        in >> size >> datetime;

        // If timestamp is older than ten seconds other might be crashed or frozen, start normally - otherwise send message
        if(QDateTime::fromMSecsSinceEpoch(datetime).secsTo(QDateTime::currentDateTimeUtc()) < 10)
        {
          // Copy command line parameters and add activate option to bring other to front
          atools::util::Properties properties(NavApp::getStartupOptionsConst());
          properties.setPropertyBool(lnm::STARTUP_COMMAND_ACTIVATE, true); // Raise other window

#ifdef DEBUG_INFORMATION
          qDebug() << Q_FUNC_INFO << "Sending" << properties;
#endif

          // Get properties for size
          QByteArray propBytes = properties.asByteArray();

          // Write off for other process
          QByteArray bytes;
          QDataStream out(&bytes, QIODevice::WriteOnly);
          out << static_cast<quint32>(propBytes.size()) << QDateTime::currentMSecsSinceEpoch();
          out.writeRawData(propBytes.constData(), propBytes.size());

          if(bytes.size() < sharedMemory->size())
          {
            memcpy(sharedMemory->data(), bytes.constData(), static_cast<size_t>(bytes.size()));
            exit = true;
          }
        } // if(QDateTime::fromMSecsSinceEpoch(date ...

        sharedMemory->unlock();
      } // if(sharedMemory->lock())
    }
  }
}

DataExchange::~DataExchange()
{
  sharedMemoryCheckTimer.stop();

  if(sharedMemory != nullptr)
  {
    qDebug() << Q_FUNC_INFO << "delete sharedMemory";
    sharedMemory->detach();
    delete sharedMemory;
    sharedMemory = nullptr;
  }
}

void DataExchange::startTimer()
{
  // Check for commands from other instance =====================
  sharedMemoryCheckTimer.setInterval(500);
  connect(&sharedMemoryCheckTimer, &QTimer::timeout, this, &DataExchange::checkData);
  sharedMemoryCheckTimer.start();
}

void DataExchange::checkData()
{
  // Check for message from other instance
  atools::util::Properties properties = fetchSharedMemory();

  if(!properties.isEmpty())
  {
    // Found message
    qDebug() << Q_FUNC_INFO << properties;

    // Extract filenames from known options ================================
    QString flightplan, flightplanDescr, perf, layout;
    fc::fromStartupProperties(properties, &flightplan, &flightplanDescr, &perf, &layout);

    // Load files if found and exist ===========================================
    if(atools::checkFile(Q_FUNC_INFO, flightplan, true /* warn */))
      emit loadRoute(flightplan);

    if(!flightplanDescr.isEmpty())
      emit loadRouteDescr(flightplanDescr);

    if(atools::checkFile(Q_FUNC_INFO, layout, true /* warn */))
      emit loadLayout(layout);

    if(atools::checkFile(Q_FUNC_INFO, perf, true /* warn */))
      emit loadPerf(perf);

    // Activate window - always sent by other instance =====================================================
    if(properties.getPropertyBool(lnm::STARTUP_COMMAND_ACTIVATE))
      emit activateMain();
  }
}

atools::util::Properties DataExchange::fetchSharedMemory()
{
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
        // Read properties if size is given - ignore timestamp
        qint64 datetimeDummy;
        in >> datetimeDummy >> properties;
      }

      // Write back a null size and a timestamp to allow the other process detect a crash
      QByteArray bytesEmpty;
      QDataStream out(&bytesEmpty, QIODevice::WriteOnly);
      out << static_cast<quint32>(0) << QDateTime::currentMSecsSinceEpoch();
      memcpy(sharedMemory->data(), bytesEmpty.constData(), static_cast<size_t>(bytesEmpty.size()));

      sharedMemory->unlock();
    }
  }

  return properties;
}
