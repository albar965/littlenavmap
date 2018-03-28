/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LNM_ONLINECONTROLLER_H
#define LNM_ONLINECONTROLLER_H

#include <QDateTime>
#include <QObject>
#include <QTimer>

namespace atools {
namespace util {
class HttpDownloader;
}
namespace sql {
class SqlDatabase;
}

namespace fs {
namespace online {
class OnlinedataManager;
}
}
}

class MainWindow;
class QTextCodec;

/*
 * Manages recurring download of online network data from the status.txt and whazzup.txt files.
 * Uses options to determine how to download data.
 */
class OnlinedataController :
  public QObject
{
  Q_OBJECT

public:
  OnlinedataController(atools::fs::online::OnlinedataManager *onlineManager, MainWindow *parent);
  virtual ~OnlinedataController();

  /* Start download and after downloading timer which triggers recurring download */
  void startProcessing();

  atools::sql::SqlDatabase *getDatabase();

  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Options dialog changed settings. Will trigger download. */
  void optionsChanged();

  const QDateTime& getLastUpdateTime() const
  {
    return lastUpdateTime;
  }

signals:
  /* Sent whenever new data was downloaded */
  void onlineClientAndAtcUpdated();
  void onlineServersUpdated();

private:
  /* HTTP download signal slots */
  void downloadFinished(const QByteArray& data, QString url);
  void downloadFailed(const QString& error, QString url);

  void startDownloadInternal();
  void startDownloadTimer();
  void stopAllProcesses();

  atools::fs::online::OnlinedataManager *manager;
  atools::util::HttpDownloader *downloader;
  MainWindow *mainWindow;

  enum State
  {
    NONE, /* Not downloading anything */
    DOWNLOADING_STATUS, /* Downloading status.txt */
    DOWNLOADING_WHAZZUP, /* Downloading whazzup.txt */
    DOWNLOADING_WHAZZUP_SERVERS /* Downloading servers */
  };

  State currentState = NONE;

  QTimer downloadTimer; /* Triggers recurring downloads */

  /* Used to check server downloads and limit them to 15 minutes */
  QDateTime lastServerDownload;

  /*  Last update from whazzup */
  QDateTime lastUpdateTime;

  /* Set after parsing status.txt to indicate compressed file */
  bool whazzupGzipped = false;

  QTextCodec *codec = nullptr;
};

#endif // LNM_ONLINECONTROLLER_H
