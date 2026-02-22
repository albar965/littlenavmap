/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_GUI_STATUSBAR_H
#define LNM_GUI_STATUSBAR_H

#include <QDateTime>
#include <QLabel>
#include <QObject>
#include <QTimer>

#include <marble/MarbleGlobal.h>
#include <marble/RenderState.h>

class QStatusBar;
namespace atools {
namespace geo {
class Pos;
}
}

/*
 * Wraps the status bar of the main window and provides configurations and update functionality
 * for status bar labels.
 */
class StatusBar
  : public QObject
{
  Q_OBJECT

public:
  /* parent should be MainWindow */
  explicit StatusBar(QStatusBar *statusBarParam);
  virtual ~StatusBar() override;

  /* Allocate labels, connect and start timers */
  void init();

  /* Stops timers */
  void deInit();

  void saveState() const;
  void restoreState();

  /* Update style for dark and other styles */
  void styleChanged();

  /* Update distance in case of unit changes */
  void optionsChanged();

  /* Print label sizes to log */
  void printDebugInformation();

  /* Sets a general status bar message which is shared with all widgets/actions status text
   * Set a general status message */
  void setStatusMessage(const QString& message, bool addToLog = false, bool popup = false);

  /* Updates label and tooltip for connection status */
  void setConnectionStatusMessageText(const QString& text, const QString& tooltipText);

  /* Update label for Marble download and file load status. */
  void renderStatusUpdateLabel(Marble::RenderStatus status, bool forceUpdate);

  /* Show "Too many objects" label if number of map features was truncated */
  /* Called after each query */
  void resultTruncated();

  /* Sets the text and tooltip of the statusbar label that indicates what objects are shown on the map */
  /* Updates label and tooltip for objects shown on map */
  void setMapObjectsShownMessageText(const QString& text, const QString& tooltipText);

  /* Set text for map details */
  void setDetailLabelText(const QString& text);

  /* Update coordinate, declination and time zone displays in status bar */
  void updateMapPositionLabel(const atools::geo::Pos& pos, const QPoint& point);

  /* Message label showing "Connected" and other simulator connection related messages */
  void setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText);

private:
  /* Map zoom changed */
  void distanceChanged();

  /* Render state from marble widget. Get the more detailed state since it updates more often */
  void renderStateChanged(const Marble::RenderState& state);

  /* Status message has changed from QStatusBar::messageChanged() signal. */
  void statusMessageChanged(const QString& text);

  /* Clear render status if no updates appear */
  void renderStatusReset();

  /* Localized display string for render status */
  QString renderStatusString(Marble::RenderStatus status);

  /* Update status bar section for online status */
  void updateConnectionStatusMessageText();

  /* Reduce status bar size if no mouse movement */
  void shrinkStatusBar();

  /* Update clock widget by timer */
  void updateClock() const;

  /* Allocate all labels */
  void setupLabels();

  void addLabel(QLabel *& label, const QString& objectName, const QString& text = QString(), const QString& tooltip = QString());
  QAction *addMenuAction(QMenu& menu, QList<QAction *>& labelActions, const QLabel *label, const QString& text = QString(),
                         const QString& tooltip = QString()) const;
  void customContextMenuRequested(const QPoint& point);

  QString dateTimeString(const QDateTime& datetime, const QString& invalidStr, bool sim) const;

  /* List of status bar messages. First is shown and others are shown in tooltip. */
  class StatusMessage
  {
public:
    StatusMessage(const QDateTime& timestampParam, const QString& messageParam)
      : timestamp(timestampParam), message(messageParam)
    {
    }

    const QDateTime& getTimestamp() const
    {
      return timestamp;
    }

    const QString& getMessage() const
    {
      return message;
    }

private:
    QDateTime timestamp;
    QString message;
  };

  /* Status bar labels */
  QLabel *mapDistanceLabel = nullptr, *mapPositionLabel = nullptr, *mapMagvarLabel = nullptr, *timeZoneLabel = nullptr,
         *mapRenderStatusLabel = nullptr, *mapDetailLabel = nullptr, *mapVisibleLabel = nullptr, *connectStatusLabel = nullptr,
         *timeLabel = nullptr;

  /* Connection field and tooltip in statusbar */
  QString connectionStatus, connectionStatusTooltip, onlineConnectionStatus, onlineConnectionStatusTooltip;
  const QString GMT, UTC;

  QList<StatusMessage> statusMessages;

  QTimer renderStatusTimer /* MainWindow::renderStatusReset() if render status is stalled */,
         shrinkStatusBarTimer /* calls MainWindow::shrinkStatusBar() once map pos and magvar are "-" */;

  Marble::RenderStatus lastRenderStatus = Marble::Incomplete;

  QHash<QString, QLabel *> labels;

  QTimer clockTimer /* MainWindow::updateClock() every second */;

  enum TimeType
  {
    TIME_UTC_REAL,
    TIME_LOCAL_REAL,
    TIME_UTC_SIM,
    TIME_LOCAL_SIM
  };

  TimeType timeType = TIME_UTC_REAL;
  QStatusBar *statusBar = nullptr;
};

#endif // LNM_GUI_STATUSBAR_H
