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
#include "common/formatter.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "gui/statusbareventfilter.h"
#include "gui/stylehandler.h"
#include "mapgui/mappaintwidget.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"

#include <QTimeZone>
#include <QToolTip>

// Clear render status after time
const static int RENDER_STATUS_TIMER_MS = 5000;

// Reduce size of status bar fields after inactivity
const static int SHRINK_STATUS_BAR_TIMER_MS = 10000;

// Maximum number of status messages to show in tooltip
const static int MAX_STATUS_MESSAGES = 20;

// Refresh clock seconds period
const static int CLOCK_TIMER_MS = 1000;

StatusBar::StatusBar(QObject *parent)
  : QObject(parent)
{

}

StatusBar::~StatusBar()
{

}

void StatusBar::init()
{
  setupUi();

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
    timeLabel->setText(QDateTime::currentDateTimeUtc().toString("d   HH:mm:ss UTC "));
    timeLabel->setToolTip(tr("Day of month and UTC time.\n%1\nLocal: %2 %3").
                          arg(QDateTime::currentDateTimeUtc().toString().
                              replace(tr("GMT", "Replaces wrong GMT indication in statusbar with UTC"),
                                      tr("UTC", "Replaces wrong GMT indication in statusbar with UTC"))).
                          arg(QDateTime::currentDateTime().toString()).
                          arg(QDateTime::currentDateTime().timeZoneAbbreviation()));
    timeLabel->setMinimumWidth(timeLabel->width());
  }
}

void StatusBar::setupUi()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // ==============================================================
  // Create labels for the statusbar
  connectStatusLabel = new QLabel();
  connectStatusLabel->setText(tr("Not connected."));
  connectStatusLabel->setToolTip(tr("Simulator connection status."));
  ui->statusBar->addPermanentWidget(connectStatusLabel);
  connectStatusLabel->setMinimumWidth(connectStatusLabel->width());

  mapVisibleLabel = new QLabel();
  ui->statusBar->addPermanentWidget(mapVisibleLabel);

  mapDetailLabel = new QLabel();
  mapDetailLabel->setToolTip(tr("Map detail level / text label level."));
  ui->statusBar->addPermanentWidget(mapDetailLabel);

  mapRenderStatusLabel = new QLabel();
  mapRenderStatusLabel->setToolTip(tr("Map rendering and download status."));
  ui->statusBar->addPermanentWidget(mapRenderStatusLabel);

  mapDistanceLabel = new QLabel();
  mapDistanceLabel->setToolTip(tr("Map view distance to ground."));
  ui->statusBar->addPermanentWidget(mapDistanceLabel);

  mapPositionLabel = new QLabel();
  mapPositionLabel->setToolTip(tr("Coordinates and elevation at cursor position."));
  ui->statusBar->addPermanentWidget(mapPositionLabel);

  mapMagvarLabel = new QLabel();
  mapMagvarLabel->setToolTip(tr("Magnetic declination at cursor position."));
  ui->statusBar->addPermanentWidget(mapMagvarLabel);

  timeZoneLabel = new QLabel();
  timeZoneLabel->setToolTip(tr("Time Zone country and offset from UTC (excluding DST) at cursor position."));
  ui->statusBar->addPermanentWidget(timeZoneLabel);

  timeLabel = new QLabel();
  timeLabel->setToolTip(tr("Day of month and UTC time."));
  ui->statusBar->addPermanentWidget(timeLabel);

  // Status bar takes ownership of filter which handles tooltip on click
  ui->statusBar->installEventFilter(new StatusBarEventFilter(ui->statusBar, connectStatusLabel));

  connect(ui->statusBar, &QStatusBar::messageChanged, this, &StatusBar::statusMessageChanged);
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
    connectStatusLabel->setFrameShadow(shadow);
    connectStatusLabel->setFrameShape(shape);
    connectStatusLabel->setMargin(1);

    mapVisibleLabel->setFrameShadow(shadow);
    mapVisibleLabel->setFrameShape(shape);
    mapVisibleLabel->setMargin(1);

    mapDetailLabel->setFrameShadow(shadow);
    mapDetailLabel->setFrameShape(shape);
    mapDetailLabel->setMargin(1);

    mapRenderStatusLabel->setFrameShadow(shadow);
    mapRenderStatusLabel->setFrameShape(shape);
    mapRenderStatusLabel->setMargin(1);

    mapDistanceLabel->setFrameShadow(shadow);
    mapDistanceLabel->setFrameShape(shape);
    mapDistanceLabel->setMargin(1);

    mapPositionLabel->setFrameShadow(shadow);
    mapPositionLabel->setFrameShape(shape);
    mapPositionLabel->setMargin(1);

    mapMagvarLabel->setFrameShadow(shadow);
    mapMagvarLabel->setFrameShape(shape);
    mapMagvarLabel->setMargin(1);

    timeZoneLabel->setFrameShadow(shadow);
    timeZoneLabel->setFrameShape(shape);
    timeZoneLabel->setMargin(1);

    timeLabel->setFrameShadow(shadow);
    timeLabel->setFrameShape(shape);
    timeLabel->setMargin(1);
  }
#endif

  // Set a minimum width - the labels grow (but do not shrink) with content changes
  connectStatusLabel->setAlignment(align);
  connectStatusLabel->setMinimumWidth(20);

  mapVisibleLabel->setAlignment(align);
  mapVisibleLabel->setMinimumWidth(20);

  mapDetailLabel->setAlignment(align);
  mapDetailLabel->setMinimumWidth(20);

  mapRenderStatusLabel->setAlignment(align);
  mapRenderStatusLabel->setMinimumWidth(20);

  mapDistanceLabel->setAlignment(align);
  mapDistanceLabel->setMinimumWidth(20);

  mapPositionLabel->setAlignment(align);
  mapPositionLabel->setMinimumWidth(20);
  mapPositionLabel->setText(tr(" — "));

  mapMagvarLabel->setAlignment(align);
  mapMagvarLabel->setMinimumWidth(20);
  mapMagvarLabel->setText(tr(" — "));

  timeZoneLabel->setAlignment(align);
  timeZoneLabel->setMinimumWidth(20);
  timeZoneLabel->setText(tr(" — "));

  timeLabel->setAlignment(align);
  timeLabel->setMinimumWidth(20);
}

void StatusBar::setConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  if(!text.isEmpty())
    connectionStatus = text;
  connectionStatusTooltip = tooltipText;
  updateConnectionStatusMessageText();
}

void StatusBar::setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText)
{
  onlineConnectionStatus = text;
  onlineConnectionStatusTooltip = tooltipText;
  updateConnectionStatusMessageText();
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
  mapVisibleLabel->setText(text);
  mapVisibleLabel->setToolTip(tooltipText);
  mapVisibleLabel->setMinimumWidth(mapVisibleLabel->width());
}

void StatusBar::resultTruncated()
{
  mapVisibleLabel->setText(atools::util::HtmlBuilder::errorMessage(tr("Too many objects")));
  mapVisibleLabel->setToolTip(tr("Too many objects to show on map.\n"
                                 "Display might be incomplete.\n"
                                 "Reduce map details in the \"View\" menu.",
                                 "Keep menu item in sync with menu translation"));
  mapVisibleLabel->setMinimumWidth(mapVisibleLabel->width());
}

void StatusBar::distanceChanged()
{
  const MapPaintWidget *mapWidget = NavApp::getMapPaintWidgetGui();
  float dist = Unit::distMeterF(static_cast<float>(mapWidget->distance() * 1000.f));
  QString distStr = QLocale().toString(dist, 'f', dist < 20.f ? (dist < 0.2f ? 2 : 1) : 0);
  if(distStr.endsWith(QString(QLocale().decimalPoint()) % "0"))
    distStr.chop(2);

  QString text = distStr % " " % Unit::getUnitDistStr();

#ifdef DEBUG_INFORMATION
  text += QStringLiteral("[%1km][%2z]").arg(mapWidget->distance(), 0, 'f', 2).arg(mapWidget->zoom());
#endif

  mapDistanceLabel->setText(text);
  mapDistanceLabel->setMinimumWidth(mapDistanceLabel->width());
}

void StatusBar::renderStatusReset()
{
  if(!atools::gui::Application::isShuttingDown())
    // Force reset to complete to avoid forever "Waiting"
    renderStatusUpdateLabel(Marble::Complete, false /* forceUpdate */);
}

QString StatusBar::renderStatusString(Marble::RenderStatus status)
{
  switch(status)
  {
    case Marble::Complete:
      // All data is there and up to date
      return tr("Done");

    case Marble::WaitingForUpdate:
      // Rendering is based on complete, but outdated data, data update was requested
      return tr("Updating");

    case Marble::WaitingForData:
      // Rendering is based on no or partial data, more data was requested (e.g. pending network queries)
      return tr("Loading");

    case Marble::Incomplete:
      // Data is missing and some error occurred when trying to retrieve it (e.g. network failure)
      return tr("Incomplete");
  }

  return tr("Unknown");
}

void StatusBar::renderStatusUpdateLabel(Marble::RenderStatus status, bool forceUpdate)
{
  if(status != lastRenderStatus || forceUpdate)
  {
    mapRenderStatusLabel->setText(renderStatusString(status));
    lastRenderStatus = status;
    mapRenderStatusLabel->setMinimumWidth(mapRenderStatusLabel->width());
  }
}

void StatusBar::renderStateChanged(const Marble::RenderState& state)
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

void StatusBar::shrinkStatusBar()
{
  if(!atools::gui::Application::isShuttingDown())
  {
    Ui::MainWindow *ui = NavApp::getMainUi();

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << ui->statusBar->geometry() << QCursor::pos();
#endif

    // Do not shrink status bar if cursor is above
    if(!ui->statusBar->rect().contains(ui->statusBar->mapFromGlobal(QCursor::pos())))
    {
      mapPositionLabel->clear();
      mapPositionLabel->setText(tr(" — "));
      mapPositionLabel->setMinimumWidth(20);
      mapPositionLabel->resize(20, mapPositionLabel->height());

      mapMagvarLabel->clear();
      mapMagvarLabel->setText(tr(" — "));
      mapMagvarLabel->setMinimumWidth(20);
      mapMagvarLabel->resize(20, mapMagvarLabel->height());

      timeZoneLabel->clear();
      timeZoneLabel->setText(tr(" — "));
      timeZoneLabel->setMinimumWidth(20);
      timeZoneLabel->resize(20, timeZoneLabel->height());
    }
    else
      shrinkStatusBarTimer.start();
  }
}

void StatusBar::updateMapPositionLabel(const atools::geo::Pos& pos, const QPoint& point)
{
  if(pos.isValid())
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
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(addToLog && !message.isEmpty() && !NavApp::isShuttingDown())
  {
    qInfo() << Q_FUNC_INFO << "Message" << message;

    statusMessages.append({QDateTime::currentDateTime(), message});

    bool removed = false;
    while(statusMessages.size() > MAX_STATUS_MESSAGES)
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

    ui->statusBar->setToolTip(msg.join(tr("<br/>")) % tr("</p>"));

    if(popup)
      // Always shown with offset relative to point
      QToolTip::showText(ui->statusBar->mapToGlobal(QPoint(0, 0)), message, ui->statusBar, QRect(), 4000);
  }

  ui->statusBar->showMessage(message);
}

void StatusBar::statusMessageChanged(const QString& text)
{
  if(text.isEmpty() && !NavApp::isShuttingDown())
  {
    Ui::MainWindow *ui = NavApp::getMainUi();
    // Field is cleared. Show number of messages in otherwise empty field.
    if(statusMessages.isEmpty())
      ui->statusBar->showMessage(tr("No Messages"));
    else
      ui->statusBar->showMessage(tr("%1 %2").
                                 arg(statusMessages.size()).
                                 arg(statusMessages.size() > 1 ? tr("Messages") : tr("Message")));
  }
}

void StatusBar::setDetailLabelText(const QString& text)
{
  mapDetailLabel->setText(text);
  mapDetailLabel->setMinimumWidth(mapDetailLabel->width());
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
