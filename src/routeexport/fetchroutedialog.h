/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_FETCHROUTEDIALOG_H
#define LNM_FETCHROUTEDIALOG_H

#include <QDialog>

namespace atools {

namespace fs {
namespace pln {
class Flightplan;
}
}

namespace util {
class HttpDownloader;
}
}

namespace Ui {
class FetchRouteDialog;
}

class QAbstractButton;

/*
 * Downloads a flight plan asynchronously from SimBriefs OFP.
 *
 * Loads and saves state automatically.
 */
class FetchRouteDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit FetchRouteDialog(QWidget *parent);
  virtual ~FetchRouteDialog() override;

signals:
  /* User clicked "Create Flight Plan" */
  void routeNewFromFlightplan(const atools::fs::pln::Flightplan& flightplan, bool adjustAltitude, bool changed, bool undo);

  /* User clicked on "Open in Route Description" */
  void routeNewFromString(const QString& routeString);

private:
  void restoreState();
  void saveState();

  void buttonBoxClicked(QAbstractButton *button);

  void startDownload();
  void updateButtonStates();

  /* HttpDownloader slots */
  void downloadFinished(const QByteArray& data, QString);
  void downloadFailed(const QString& error, int errorCode, QString downloadUrl);
  void downloadSslErrors(const QStringList& errors, const QString& downloadUrl);

  atools::util::HttpDownloader *downloader;

  /* Plan is empty if parsing failed */
  atools::fs::pln::Flightplan *flightplan;

  QString routeString, /* Route string is empty if download failed */
          fetcherUrl; /* URL can be overridden in settings */

  Ui::FetchRouteDialog *ui;

};

#endif // LNM_FETCHROUTEDIALOG_H
