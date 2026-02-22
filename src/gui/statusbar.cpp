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

#include "statusbar.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "gui/statusbareventfilter.h"
#include "gui/stylehandler.h"
#include "gui/widgetstate.h"
#include "mapgui/mappaintwidget.h"
#include "settings/settings.h"
#include "util/htmlbuilder.h"

#include <QActionGroup>
#include <QMenu>
#include <QStatusBar>
#include <QTimeZone>
#include <QToolTip>

// Clear render status after time
const static int RENDER_STATUS_TIMER_MS = 5000;

// Reduce size of status bar fields after inactivity
const static int SHRINK_STATUS_BAR_TIMER_MS = 10000;

// Maximum number of status messages to show in tooltip
const static int MAXIMUM_NUM_STATUS_MESSAGES = 20;

// Refresh clock seconds period
const static int CLOCK_TIMER_MS = 1000;

// Minimum label width
const static int MINIMUM_LABEL_WIDTH = 20;

StatusBar::StatusBar(QStatusBar *statusBarParam)
  : QObject(statusBarParam),
  GMT(tr("GMT", "Replaces wrong GMT indication in statusbar with UTC")),
  UTC(tr("UTC", "Replaces wrong GMT indication in statusbar with UTC"))
{
  statusBar = statusBarParam;
}

StatusBar::~StatusBar()
{

}

void StatusBar::init()
{
  qDebug() << Q_FUNC_INFO;
  setupLabels();

  // Update clock =====================
  clockTimer.setInterval(CLOCK_TIMER_MS);
  connect(&clockTimer, &QTimer::timeout, this, &StatusBar::updateClock);
  clockTimer.start();

  // Reset render status - change to done after ten seconds =====================
  renderStatusTimer.setInterval(RENDER_STATUS_TIMER_MS);
  renderStatusTimer.setSingleShot(true);
  connect(&renderStatusTimer, &QTimer::timeout, this, &StatusBar::renderStatusReset);

  shrinkStatusBarTimer.setInterval(SHRINK_STATUS_BAR_TIMER_MS);
  shrinkStatusBarTimer.setSingleShot(true);
  connect(&shrinkStatusBarTimer, &QTimer::timeout, this, &StatusBar::shrinkStatusBar);

  connect(NavApp::getMapPaintWidgetGui(), &MapPaintWidget::renderStateChanged, this, &StatusBar::renderStateChanged);
  connect(NavApp::getMapPaintWidgetGui(), &MapPaintWidget::distanceChanged, this, &StatusBar::distanceChanged);

  styleChanged();
}

void StatusBar::deInit()
{
  qDebug() << Q_FUNC_INFO;
  clockTimer.stop();
  renderStatusTimer.stop();
  shrinkStatusBarTimer.stop();
}

void StatusBar::optionsChanged()
{
  distanceChanged();
}

void StatusBar::updateClock() const
{
  if(!atools::gui::Application::isShuttingDown())
  {
    // Get timedate depending on settings in menu ===================================
    QDateTime datetimeText, // Label and first line in tooltip
              datetimeAlt; // Alternate time in tooltip
    QString timeTypeStr, invalidStr;
    switch(timeType)
    {
      case TIME_UTC_REAL:
        datetimeText = QDateTime::currentDateTimeUtc();
        datetimeAlt = QDateTime::currentDateTime();
        timeTypeStr = tr("UTC real");
        invalidStr = tr(" — ");
        break;

      case TIME_LOCAL_REAL:
        datetimeText = QDateTime::currentDateTime();
        datetimeAlt = QDateTime::currentDateTimeUtc();
        timeTypeStr = tr("local real");
        invalidStr = tr(" — ");
        break;

      case TIME_UTC_SIM:
        if(NavApp::isConnectedAndAircraft())
        {
          datetimeText = NavApp::getUserAircraft().getZuluTime();
          datetimeAlt = NavApp::getUserAircraft().getLocalTime();
        }
        invalidStr = tr("Not connected to simulator");
        timeTypeStr = tr("UTC simulator");
        break;

      case TIME_LOCAL_SIM:
        if(NavApp::isConnectedAndAircraft())
        {
          datetimeText = NavApp::getUserAircraft().getLocalTime();
          datetimeAlt = NavApp::getUserAircraft().getZuluTime();
        }
        invalidStr = tr("Not connected to simulator");
        timeTypeStr = tr("local simulator");
        break;
    }

    // Label text ===========================
    if(datetimeText.isNull())
      timeLabel->setText(tr(" — "));
    else
      timeLabel->setText(datetimeText.toString(QStringLiteral("d   HH:mm:ss %1 ").arg(datetimeText.timeZoneAbbreviation())));

    // Tooltip text ===========================
    if(timeLabel->isVisible())
    {
      atools::util::HtmlBuilder html;
      html.p(atools::util::html::NOBR_WHITESPACE);
      if(datetimeText.isNull())
      {
        // Time not valid - sim time selected and not connected to simulator
        html.textBr(tr("Day of month and %1 time.").arg(timeTypeStr), atools::util::html::BOLD);
        html.text(invalidStr);
      }
      else
      {
        // Real time or sim time and connected
        html.textBr(tr("Day of month and %1 time.").arg(timeTypeStr), atools::util::html::BOLD);
        html.textBr(dateTimeString(datetimeText, invalidStr, timeType == TIME_UTC_SIM || timeType == TIME_LOCAL_SIM),
                    atools::util::html::ITALIC);
        html.text(dateTimeString(datetimeAlt, invalidStr, timeType == TIME_UTC_SIM || timeType == TIME_LOCAL_SIM));
      }
      html.pEnd();

      // Do not use setToolTip(). This allows to update time on the fly with seconds ticking
      if(timeLabel->rect().contains(timeLabel->mapFromGlobal(QCursor::pos())))
        QToolTip::showText(QCursor::pos(), html.getHtml());
    }

    timeLabel->setMinimumWidth(timeLabel->width());
  }
}

QString StatusBar::dateTimeString(const QDateTime& datetime, const QString& invalidStr, bool sim) const
{
  if(datetime.isValid())
    return tr("%1%2 (%3)").
           arg(datetime.timeZone() == QDateTime::currentDateTime().timeZone() ?
               tr("Local%1: ").arg(sim ? tr(" simulator") : tr(" real")) :
               tr("UTC%1: ").arg(sim ? tr(" simulator") : tr(" real"))).
           arg(QLocale().toString(datetime).replace(GMT, UTC).replace(UTC % QStringLiteral(" ") % UTC, UTC)).
           arg(formatter::formatTimeZoneOffset(datetime.timeZone().standardTimeOffset(datetime)));
  else
    return invalidStr;
}

void StatusBar::addLabel(QLabel *& label, const QString& objectName, const QString& text, const QString& tooltip)
{
  label = new QLabel();
  label->setObjectName(objectName);
  label->setText(text);
  label->setToolTip(tooltip);
  label->setMinimumWidth(label->width());

  statusBar->addPermanentWidget(label);
  labels.insert(objectName, label);
}

void StatusBar::setupLabels()
{
  // ==============================================================
  // Create labels for the statusbar
  addLabel(connectStatusLabel, "connectStatusLabel", tr("Not connected."),
           tr("Simulator connection status."));
  addLabel(mapVisibleLabel, "mapVisibleLabel");
  addLabel(mapDetailLabel, "mapDetailLabel", QStringLiteral(),
           tr("Map detail level / text label level."));
  addLabel(mapRenderStatusLabel, "mapRenderStatusLabel", QStringLiteral(),
           tr("Map rendering and download status."));
  addLabel(mapDistanceLabel, "mapDistanceLabel", QStringLiteral(),
           tr("Map view distance to ground."));
  addLabel(mapPositionLabel, "mapPositionLabel", QStringLiteral(),
           tr("Coordinates and elevation at cursor position."));
  addLabel(mapMagvarLabel, "mapMagvarLabel", QStringLiteral(),
           tr("Magnetic declination at cursor position."));
  addLabel(timeZoneLabel, "timeZoneLabel", QStringLiteral(),
           tr("Time Zone country and offset from UTC (excluding DST) at cursor position."));
  addLabel(timeLabel, "timeLabel");

  // Status bar takes ownership of filter which handles tooltip on click
  statusBar->installEventFilter(new StatusBarEventFilter(statusBar, connectStatusLabel));

  connect(statusBar, &QStatusBar::messageChanged, this, &StatusBar::statusMessageChanged);
  connect(statusBar, &QStatusBar::customContextMenuRequested, this, &StatusBar::customContextMenuRequested);
}

QAction *StatusBar::addMenuAction(QMenu& menu, QList<QAction *>& labelActions, const QLabel *label, const QString& text,
                                  const QString& tooltip) const
{
  QAction *action = new QAction(text, &menu);
  action->setToolTip(tooltip);
  action->setObjectName(label->objectName());
  action->setCheckable(true);
  action->setChecked(label->isVisible());
  labelActions.append(action);
  return action;
}

void StatusBar::customContextMenuRequested(const QPoint& point)
{
  qDebug() << Q_FUNC_INFO;

  // Label show/hide actions =======================
  QList<QAction *> labelActions;
  QMenu menu(statusBar);
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  addMenuAction(menu, labelActions, connectStatusLabel, tr("&Simulator connection status"),
                tr("Shows if Little Navmap is connected to the simulator"));
  addMenuAction(menu, labelActions, mapVisibleLabel, tr("Map &content indicator"),
                tr("Shows visible features on the map"));
  addMenuAction(menu, labelActions, mapDetailLabel, tr("Map &detail level and map label level"),
                tr("Shows map detail levels as set in menu \"View\""));
  addMenuAction(menu, labelActions, mapRenderStatusLabel, tr("&Online map download progress indicator"),
                tr("Show the download status for the background map"));
  addMenuAction(menu, labelActions, mapDistanceLabel, tr("&Zoom Distance"),
                tr("Zoom distance to map surface"));
  addMenuAction(menu, labelActions, mapPositionLabel, tr("&Cursor Coordinates"),
                tr("Map coordinates and altitude at cursor position"));
  addMenuAction(menu, labelActions, mapMagvarLabel, tr("&Magnetic Declination"),
                tr("Magnetic declination at map cursor position"));
  addMenuAction(menu, labelActions, timeZoneLabel, tr("&Country and Time Zone"),
                tr("Country and time zone at map cursor position"));
  menu.addActions(labelActions);
  menu.addSeparator();
  QAction *actionTimeLabel = addMenuAction(menu, labelActions, timeLabel, tr("&Time"),
                                           tr("Current or simulator time"));
  menu.addAction(actionTimeLabel);
  menu.addSeparator();

  // Time type actions =======================
  QActionGroup *group = new QActionGroup(&menu);
  QAction *actionUtcReal = new QAction(tr("UTC Real Time"), group);
  actionUtcReal->setToolTip(tr(""));
  actionUtcReal->setCheckable(true);
  actionUtcReal->setChecked(timeType == TIME_UTC_REAL);
  actionUtcReal->setEnabled(actionTimeLabel->isChecked());
  group->addAction(actionUtcReal);
  menu.addAction(actionUtcReal);

  QAction *actionLocalReal = new QAction(tr("Local Real Time"), group);
  actionLocalReal->setToolTip(tr(""));
  actionLocalReal->setCheckable(true);
  actionLocalReal->setChecked(timeType == TIME_LOCAL_REAL);
  actionLocalReal->setEnabled(actionTimeLabel->isChecked());
  group->addAction(actionLocalReal);
  menu.addAction(actionLocalReal);

  QAction *actionUtcSim = new QAction(tr("UTC Simulator Time"), group);
  actionUtcSim->setToolTip(tr(""));
  actionUtcSim->setCheckable(true);
  actionUtcSim->setChecked(timeType == TIME_UTC_SIM);
  actionUtcSim->setEnabled(actionTimeLabel->isChecked());
  group->addAction(actionUtcSim);
  menu.addAction(actionUtcSim);

  QAction *actionLocalSim = new QAction(tr("Local Simulator Time"), group);
  actionLocalSim->setToolTip(tr(""));
  actionLocalSim->setCheckable(true);
  actionLocalSim->setChecked(timeType == TIME_LOCAL_SIM);
  actionLocalSim->setEnabled(actionTimeLabel->isChecked());
  group->addAction(actionLocalSim);
  menu.addAction(actionLocalSim);

  // Show menu =======================================================
  menu.exec(statusBar->mapToGlobal(point));

  // Load values from menu =======================================================
  for(const QAction *action : std::as_const(labelActions))
    labels.value(action->objectName())->setVisible(action->isChecked());

  if(actionUtcReal->isChecked())
    timeType = TIME_UTC_REAL;
  else if(actionLocalReal->isChecked())
    timeType = TIME_LOCAL_REAL;
  else if(actionUtcSim->isChecked())
    timeType = TIME_UTC_SIM;
  else if(actionLocalSim->isChecked())
    timeType = TIME_LOCAL_SIM;

  updateClock();
}

void StatusBar::styleChanged()
{
  Qt::AlignmentFlag align = Qt::AlignCenter;

  // Adjust shadow and shape of status bar labels but not for macOS
#ifndef Q_OS_MACOS
  bool adjustFrame = false;
  QFrame::Shadow shadow = connectStatusLabel->frameShadow();
  QFrame::Shape shape = connectStatusLabel->frameShape();

  if(NavApp::getStyleHandler() != nullptr)
  {
    QString style = NavApp::getStyleHandler()->getCurrentGuiStyleDisplayName();
    if(NavApp::isGuiStyleDark())
    {
      shadow = QFrame::Sunken;
      shape = QFrame::Box;
      adjustFrame = true;
    }
    else
    {
      shadow = QFrame::Sunken;
      shape = QFrame::StyledPanel;
      adjustFrame = true;
    }
    // Windows styles already use a box
  }

  if(adjustFrame)
  {
    for(auto it = labels.begin(); it != labels.end(); ++it)
    {
      it.value()->setFrameShadow(shadow);
      it.value()->setFrameShape(shape);
      it.value()->setMargin(1);
    }
  }
#endif

  // Set a minimum width - the labels grow (but do not shrink) with content changes
  for(auto it = labels.begin(); it != labels.end(); ++it)
  {
    it.value()->setAlignment(align);
    it.value()->setMinimumWidth(MINIMUM_LABEL_WIDTH);
  }

  mapPositionLabel->setText(tr(" — "));
  mapMagvarLabel->setText(tr(" — "));
  timeZoneLabel->setText(tr(" — "));
}

void StatusBar::setConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  if(!NavApp::isShuttingDown())
  {
    if(!text.isEmpty())
      connectionStatus = text;
    connectionStatusTooltip = tooltipText;
    updateConnectionStatusMessageText();
  }
}

void StatusBar::setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  if(!NavApp::isShuttingDown())
  {
    onlineConnectionStatus = text;
    onlineConnectionStatusTooltip = tooltipText;
    updateConnectionStatusMessageText();
  }
}

void StatusBar::saveState() const
{
  atools::gui::WidgetState state(lnm::MAINWINDOW_STATUSBAR, true /* visibility */);

  for(auto it = labels.constBegin(); it != labels.constEnd(); ++it)
    state.save(it.value());

  atools::settings::Settings::instance().setValueEnum(lnm::MAINWINDOW_STATUSBAR_TIME_TYPE, timeType);
}

void StatusBar::restoreState()
{
  atools::gui::WidgetState state(lnm::MAINWINDOW_STATUSBAR, true /* visibility */);

  for(auto it = labels.begin(); it != labels.end(); ++it)
    state.restore(it.value());

  timeType = atools::settings::Settings::instance().valueEnum(lnm::MAINWINDOW_STATUSBAR_TIME_TYPE, TIME_UTC_REAL);
}

void StatusBar::updateConnectionStatusMessageText()
{
  if(!NavApp::isShuttingDown())
  {
    if(onlineConnectionStatus.isEmpty())
      connectStatusLabel->setText(connectionStatus);
    else
      connectStatusLabel->setText(tr("%1/%2").arg(connectionStatus).arg(onlineConnectionStatus));

    if(onlineConnectionStatusTooltip.isEmpty())
      connectStatusLabel->setToolTip(connectionStatusTooltip);
    else
      connectStatusLabel->setToolTip(tr("Simulator:\n%1\n\nOnline Network:\n%2").
                                     arg(connectionStatusTooltip).arg(onlineConnectionStatusTooltip));
    connectStatusLabel->setMinimumWidth(connectStatusLabel->width());
  }
}

void StatusBar::setMapObjectsShownMessageText(const QString& text, const QString& tooltipText)
{
  if(!NavApp::isShuttingDown())
  {
    mapVisibleLabel->setText(text);
    mapVisibleLabel->setToolTip(tooltipText);
    mapVisibleLabel->setMinimumWidth(mapVisibleLabel->width());
  }
}

void StatusBar::resultTruncated()
{
  if(!NavApp::isShuttingDown())
  {
    mapVisibleLabel->setText(atools::util::HtmlBuilder::errorMessage(tr("Too many objects")));
    mapVisibleLabel->setToolTip(tr("Too many objects to show on map.\n"
                                   "Display might be incomplete.\n"
                                   "Reduce map details in the \"View\" menu.",
                                   "Keep menu item in sync with menu translation"));
    mapVisibleLabel->setMinimumWidth(mapVisibleLabel->width());
  }
}

void StatusBar::distanceChanged()
{
  if(!NavApp::isShuttingDown())
  {
    const MapPaintWidget *mapWidget = NavApp::getMapPaintWidgetGui();
    if(mapWidget != nullptr)
    {
      QString text = Unit::distMeter(mapWidget->distance() * 1000.f);

#ifdef DEBUG_INFORMATION
      text += QStringLiteral(" [%1km][%2z]").arg(mapWidget->distance(), 0, 'f', 2).arg(mapWidget->zoom());
#endif

      mapDistanceLabel->setText(text);
      mapDistanceLabel->setMinimumWidth(mapDistanceLabel->width());
    }
  }
}

void StatusBar::renderStatusReset()
{
  // Force reset to complete to avoid forever "Waiting"
  renderStatusUpdateLabel(Marble::Complete, false /* forceUpdate */);
}

QString StatusBar::renderStatusString(Marble::RenderStatus status)
{
  switch(status)
  {
    // All data is there and up to date
    case Marble::Complete:
      return tr("Done");

    // Rendering is based on complete, but outdated data, data update was requested
    case Marble::WaitingForUpdate:
      return tr("Updating");

    // Rendering is based on no or partial data, more data was requested (e.g. pending network queries)
    case Marble::WaitingForData:
      return tr("Loading");

    // Data is missing and some error occurred when trying to retrieve it (e.g. network failure)
    case Marble::Incomplete:
      return tr("Incomplete");
  }

  return tr("Unknown");
}

void StatusBar::renderStatusUpdateLabel(Marble::RenderStatus status, bool forceUpdate)
{
  if(!atools::gui::Application::isShuttingDown())
  {
    if(status != lastRenderStatus || forceUpdate)
    {
      mapRenderStatusLabel->setText(renderStatusString(status));
      lastRenderStatus = status;
      mapRenderStatusLabel->setMinimumWidth(mapRenderStatusLabel->width());
    }
  }
}

void StatusBar::renderStateChanged(const Marble::RenderState& state)
{
  if(!atools::gui::Application::isShuttingDown())
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << renderStatusString(state.status());

    for(int i = 0; i < state.children(); i++)
      qDebug() << Q_FUNC_INFO << state.childAt(i).name() << renderStatusString(state.status());
#endif

    Marble::RenderStatus status = state.status();
    renderStatusUpdateLabel(status, false /* forceUpdate */);

    if(status == Marble::WaitingForUpdate || status == Marble::WaitingForData)
      // Reset forever lasting waiting status if Marble cannot fetch tiles
      renderStatusTimer.start();
    else if(status == Marble::Complete || status == Marble::Incomplete)
      renderStatusTimer.stop();
  }
}

void StatusBar::shrinkStatusBar()
{
  if(!atools::gui::Application::isShuttingDown())
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << statusBar->geometry() << QCursor::pos();
#endif

    // Do not shrink status bar if cursor is above
    if(!statusBar->rect().contains(statusBar->mapFromGlobal(QCursor::pos())))
    {
      mapPositionLabel->clear();
      mapPositionLabel->setText(tr(" — "));
      mapPositionLabel->setMinimumWidth(MINIMUM_LABEL_WIDTH);
      mapPositionLabel->resize(MINIMUM_LABEL_WIDTH, mapPositionLabel->height());

      mapMagvarLabel->clear();
      mapMagvarLabel->setText(tr(" — "));
      mapMagvarLabel->setMinimumWidth(MINIMUM_LABEL_WIDTH);
      mapMagvarLabel->resize(MINIMUM_LABEL_WIDTH, mapMagvarLabel->height());

      timeZoneLabel->clear();
      timeZoneLabel->setText(tr(" — "));
      timeZoneLabel->setMinimumWidth(MINIMUM_LABEL_WIDTH);
      timeZoneLabel->resize(MINIMUM_LABEL_WIDTH, timeZoneLabel->height());
    }
    else
      shrinkStatusBarTimer.start();
  }
}

void StatusBar::updateMapPositionLabel(const atools::geo::Pos& pos, const QPoint& point)
{
  if(!atools::gui::Application::isShuttingDown() && pos.isValid())
  {
    // Coordinates ============================
    QString text(Unit::coords(pos));

    if(NavApp::isGlobeOfflineProvider() && pos.getAltitude() < map::INVALID_ALTITUDE_VALUE)
      text += tr(" / ") % Unit::altMeter(pos.getAltitude());
#ifdef DEBUG_INFORMATION
    text.append(QStringLiteral(" [L %1,%2/G %3,%4]").arg(point.x()).arg(point.y()).arg(QCursor::pos().x()).arg(QCursor::pos().y()));
#endif

    mapPositionLabel->setText(text);
    mapPositionLabel->setMinimumWidth(mapPositionLabel->width());

    // Declination ============================
    float magVar = NavApp::getMagVar(pos);
    QString magVarText = map::magvarText(magVar, true /* short text */);

#ifdef DEBUG_INFORMATION
    magVarText += QStringLiteral(" [%1]").arg(magVar, 0, 'f', 2);
#endif

    mapMagvarLabel->setText(magVarText);
    mapMagvarLabel->setMinimumWidth(mapMagvarLabel->width());

    // Time Zone  ============================
    const QTimeZone zone = NavApp::getTimeZone(pos);
    const QString offset = formatter::formatTimeZoneOffset(zone.standardTimeOffset(NavApp::getUtcDateTimeSimOrCurrent()));
    QLocale::Territory territory = zone.territory();
    if(territory != QLocale::AnyTerritory)
      timeZoneLabel->setText(tr("%1 / %2").arg(QLocale::territoryToString(territory)).arg(offset));
    else
      timeZoneLabel->setText(tr("%1").arg(offset));

    timeZoneLabel->setMinimumWidth(timeZoneLabel->width());

    // Stop status bar time to avoid shrinking =====================
    shrinkStatusBarTimer.stop();
  }
  else
  {
    mapPositionLabel->setText(tr(" — "));
    mapPositionLabel->setMinimumWidth(mapPositionLabel->width());
    mapMagvarLabel->setText(tr(" — "));
    timeZoneLabel->setText(tr(" — "));

    // Reduce status fields bar after timeout =====================
    shrinkStatusBarTimer.start();
  }
}

void StatusBar::setStatusMessage(const QString& message, bool addToLog, bool popup)
{
  if(addToLog && !message.isEmpty() && !NavApp::isShuttingDown())
  {
    qInfo() << Q_FUNC_INFO << "Message" << message;

    statusMessages.append({QDateTime::currentDateTime(), message});

    bool removed = false;
    while(statusMessages.size() > MAXIMUM_NUM_STATUS_MESSAGES)
    {
      statusMessages.removeFirst();
      removed = true;
    }

    QStringList msg(tr("<p style='white-space:pre'><b>Messages:</b>"));

    if(removed)
      // Indicate overflow
      msg.append(tr("..."));

    for(int i = 0; i < statusMessages.size(); i++)
      msg.append(tr("%1: %2").
                 arg(QLocale().toString(statusMessages.at(i).getTimestamp().time(), tr("hh:mm:ss"))).
                 arg(statusMessages.at(i).getMessage()));

    statusBar->setToolTip(msg.join(tr("<br/>")) % tr("</p>"));

    if(popup)
      // Always shown with offset relative to point
      QToolTip::showText(statusBar->mapToGlobal(QPoint(0, 0)), message, statusBar, QRect(), 4000);
  }

  statusBar->showMessage(message);
}

void StatusBar::statusMessageChanged(const QString& text)
{
  if(text.isEmpty() && !NavApp::isShuttingDown())
  {
    // Field is cleared. Show number of messages in otherwise empty field.
    if(statusMessages.isEmpty())
      statusBar->showMessage(tr("No Messages"));
    else
      statusBar->showMessage(tr("%1 %2").
                             arg(statusMessages.size()).
                             arg(statusMessages.size() > 1 ? tr("Messages") : tr("Message")));
  }
}

void StatusBar::setDetailLabelText(const QString& text)
{
  if(!atools::gui::Application::isShuttingDown())
  {
    mapDetailLabel->setText(text);
    mapDetailLabel->setMinimumWidth(mapDetailLabel->width());
  }
}

void StatusBar::printDebugInformation()
{
#ifdef DEBUG_INFORMATION
  qDebug() << "mapDistanceLabel->size()" << mapDistanceLabel->size();
  qDebug() << "mapPositionLabel->size()" << mapPositionLabel->size();
  qDebug() << "mapMagvarLabel->size()" << mapMagvarLabel->size();
  qDebug() << "timeZoneLabel ->size()" << timeZoneLabel->size();
  qDebug() << "mapRenderStatusLabel->size()" << mapRenderStatusLabel->size();
  qDebug() << "mapDetailLabel->size()" << mapDetailLabel->size();
  qDebug() << "mapVisibleLabel->size()" << mapVisibleLabel->size();
  qDebug() << "connectStatusLabel->size()" << connectStatusLabel->size();
  qDebug() << "timeLabel->size()" << timeLabel->size();
#endif
}
