/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "profile/profilescrollarea.h"
#include "profile/profilewidget.h"
#include "profile/profilelabelwidget.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "gui/helphandler.h"

#include "navapp.h"
#include "atools.h"

#include <QDebug>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QWidget>
#include "ui_mainwindow.h"

ProfileScrollArea::ProfileScrollArea(ProfileWidget *parent, QScrollArea *scrollAreaParam)
  : QObject(parent), profileWidget(parent),
  horizScrollBar(scrollAreaParam->horizontalScrollBar()), vertScrollBar(scrollAreaParam->verticalScrollBar()),
  scrollArea(scrollAreaParam), viewport(scrollAreaParam->viewport())
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  scrollArea->setContextMenuPolicy(Qt::DefaultContextMenu);
  scrollArea->setFocusPolicy(Qt::WheelFocus);

  // Create label widget on the left
  labelWidget = new ProfileLabelWidget(profileWidget, this);
  labelWidget->setMinimumWidth(65);
  ui->gridLayoutProfileDrawing->replaceWidget(ui->widgetProfileLabelLeft, labelWidget);

  // Update splitter icon and color
  styleChanged();

  // Disallow collapsing of the profile view
  ui->splitterProfile->setCollapsible(0, false);
  if(ui->splitterProfile->handle(1) != nullptr)
  {
    ui->splitterProfile->handle(1)->setToolTip(tr("Show, hide or resize zoom button area."));
    ui->splitterProfile->handle(1)->setStatusTip(ui->splitterProfile->handle(1)->toolTip());
  }

  // Connect zoom sliders
  connect(ui->horizontalSliderProfileZoom, &QSlider::valueChanged,
          this, &ProfileScrollArea::horizontalZoomSliderValueChanged);
  connect(ui->verticalSliderProfileZoom, &QSlider::valueChanged,
          this, &ProfileScrollArea::verticalZoomSliderValueChanged);

  // Update label on scroll bar changes and remember position
  connect(horizScrollBar, &QScrollBar::rangeChanged, this, &ProfileScrollArea::horizScrollBarChanged);
  connect(horizScrollBar, &QScrollBar::valueChanged, this, &ProfileScrollArea::horizScrollBarValueChanged);

  connect(vertScrollBar, &QScrollBar::rangeChanged, this, &ProfileScrollArea::vertScrollBarChanged);
  connect(vertScrollBar, &QScrollBar::valueChanged, this, &ProfileScrollArea::vertScrollBarValueChanged);

  // Buttons and menu actions
  connect(ui->pushButtonProfileExpand, &QPushButton::clicked, this, &ProfileScrollArea::expandWidget);
  connect(ui->pushButtonProfileHelp, &QPushButton::clicked, this, &ProfileScrollArea::helpClicked);
  connect(ui->actionProfileExpand, &QAction::triggered, this, &ProfileScrollArea::expandWidget);

  connect(ui->actionProfileShowLabels, &QAction::toggled, this, &ProfileScrollArea::showLabels);
  connect(ui->actionProfileShowScrollbars, &QAction::toggled, this, &ProfileScrollArea::showScrollbars);
  connect(ui->actionProfileShowZoom, &QAction::toggled, this, &ProfileScrollArea::showZoom);

  // Need to resize scroll area
  connect(ui->splitterProfile, &QSplitter::splitterMoved, this, &ProfileScrollArea::splitterMoved);

  // Install event filter to area and viewport widgets avoid subclassing
  scrollArea->installEventFilter(this);
  scrollArea->viewport()->installEventFilter(this);
}

ProfileScrollArea::~ProfileScrollArea()
{
  scrollArea->removeEventFilter(this);
  scrollArea->viewport()->removeEventFilter(this);
  delete labelWidget;
}

void ProfileScrollArea::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrlWeb(scrollArea, lnm::helpOnlineUrl + "PROFILE.html",
                                           lnm::helpLanguageOnline());
}

void ProfileScrollArea::expandWidget()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  // Reset scale factor to zoom slider widget minimum
  vertScaleFactor = horizScaleFactor = 1;

  // Reset zooming to default to center
  lastHorizScrollPos = lastVertScrollPos = 0.5;

  // Resize widget temporarily and then switch it off again
  scrollArea->setWidgetResizable(true);
  ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->minimum());
  ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->minimum());
  scrollArea->setWidgetResizable(false);
}

void ProfileScrollArea::splitterMoved(int pos, int index)
{
  Q_UNUSED(pos)
  Q_UNUSED(index)

  // Uncheck menu action if the split containing the zoom sliders is collapsed
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionProfileShowZoom->blockSignals(true);
  ui->actionProfileShowZoom->setChecked(ui->splitterProfile->sizes().at(1) > 0);
  ui->actionProfileShowZoom->blockSignals(false);

  resizeEvent();
}

void ProfileScrollArea::vertScrollBarValueChanged()
{
  vertScrollBarChanged();

  if(!centeringAircraft)
    emit jumpBackToAircraftStart();
}

void ProfileScrollArea::vertScrollBarChanged()
{
  if(!noLastScrollPosUpdate && vertScrollBar->maximum() > 0)
  {
    if(vertScrollBar->value() == vertScrollBar->minimum())
      // Stick to the top
      lastVertScrollPos = 0.;
    else if(vertScrollBar->value() == vertScrollBar->maximum())
      // Stick to the bottom
      lastVertScrollPos = 1.;
    else
      // Remember position
      lastVertScrollPos = toScrollPos(vertScrollBar);
  }

  // Update y scale
  labelWidget->update();
}

void ProfileScrollArea::horizScrollBarValueChanged()
{
  horizScrollBarChanged();

  if(!centeringAircraft)
    emit jumpBackToAircraftStart();
}

void ProfileScrollArea::horizScrollBarChanged()
{
  if(!noLastScrollPosUpdate && horizScrollBar->maximum() > 0)
  {
    if(horizScrollBar->value() == horizScrollBar->minimum())
      // Stick to the left
      lastHorizScrollPos = 0.;
    else if(horizScrollBar->value() == horizScrollBar->maximum())
      // Stick to the right
      lastHorizScrollPos = 1.;
    else
      // Remember position
      lastHorizScrollPos = toScrollPos(horizScrollBar);
  }
}

void ProfileScrollArea::update()
{
  scrollArea->update();
  updateLabelWidget();
}

void ProfileScrollArea::updateLabelWidget()
{
  labelWidget->update();
}

void ProfileScrollArea::routeChanged(bool geometryChanged)
{
  maxWindowAlt = profileWidget->getMaxWindowAlt();

  if(geometryChanged)
    setMaxHorizZoom();

  routeAltitudeChanged();
  updateWidgets();

  // Graphic update will be triggered later
  // profileWidget->update();
  // update();
}

void ProfileScrollArea::routeAltitudeChanged()
{
  maxWindowAlt = profileWidget->getMaxWindowAlt();

  setMaxVertZoom();
  updateWidgets();

  // Graphic update will be triggered later
  // profileWidget->update();
  // update();
}

void ProfileScrollArea::updateWidgets()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool routeEmpty = NavApp::getRoute().getSizeWithoutAlternates() < 2;

  if(routeEmpty)
  {
    // Reset zoom sliders
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->minimum());
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->minimum());
    lastVertScrollPos = 0.5;
    lastHorizScrollPos = 0.5;
  }

  // Disable scrolling and zooming
  ui->labelProfileHorizontalSlider->setDisabled(routeEmpty);
  ui->labelProfileVerticalSlider->setDisabled(routeEmpty);
  ui->pushButtonProfileExpand->setDisabled(routeEmpty);
  ui->actionProfileExpand->setDisabled(routeEmpty);
  ui->horizontalSliderProfileZoom->setDisabled(routeEmpty);
  ui->verticalSliderProfileZoom->setDisabled(routeEmpty);
}

bool ProfileScrollArea::eventFilter(QObject *object, QEvent *event)
{
  bool consumed = false;
  if(object == scrollArea->viewport())
  {
    // Do not let wheel event propagate from the viewport to the scroll bars
    if(event->type() == QEvent::Wheel)
      return true;
  }
  else if(object == scrollArea)
  {
    // Work on own events
    // qDebug() << Q_FUNC_INFO << event->type();
    if(event->type() == QEvent::Resize)
      consumed = resizeEvent();
    else if(event->type() == QEvent::Wheel)
      consumed = wheelEvent(dynamic_cast<QWheelEvent *>(event));
    else if(event->type() == QEvent::MouseButtonPress)
      consumed = mousePressEvent(dynamic_cast<QMouseEvent *>(event));
    else if(event->type() == QEvent::MouseButtonRelease)
      consumed = mouseReleaseEvent(dynamic_cast<QMouseEvent *>(event));
    else if(event->type() == QEvent::MouseMove)
      consumed = mouseMoveEvent(dynamic_cast<QMouseEvent *>(event));
    else if(event->type() == QEvent::MouseButtonDblClick)
      consumed = mouseDoubleClickEvent(dynamic_cast<QMouseEvent *>(event));
    else if(event->type() == QEvent::KeyPress)
      consumed = keyEvent(dynamic_cast<QKeyEvent *>(event));
  }

  if(!consumed)
    // if you want to filter the event out, i.e. stop it being handled further, return true
    return QObject::eventFilter(object, event);
  else
    return true;
}

bool ProfileScrollArea::keyEvent(QKeyEvent *event)
{
  if(NavApp::getRoute().getSizeWithoutAlternates() < 2)
    return false;

  bool consumed = false;
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(event->key() == Qt::Key_Home)
  {
    // Scroll to left beginning
    horizScrollBar->setValue(horizScrollBar->minimum());
    consumed = true;
  }
  else if(event->key() == Qt::Key_End)
  {
    // Scroll to right end
    horizScrollBar->setValue(horizScrollBar->maximum());
    consumed = true;
  }
  if(event->key() == Qt::Key_PageUp)
  {
    horizScrollBar->setValue(horizScrollBar->value() - horizScrollBar->pageStep());
    consumed = true;
  }
  else if(event->key() == Qt::Key_PageDown)
  {
    horizScrollBar->setValue(horizScrollBar->value() + horizScrollBar->pageStep());
    consumed = true;
  }
  else if(event->key() == Qt::Key_Plus)
  {
    // Zoom in vertically
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() + 1);
    consumed = true;
  }
  else if(event->key() == Qt::Key_Minus)
  {
    // Zoom out vertically
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() - 1);
    consumed = true;
  }
  else if(event->key() == Qt::Key_Asterisk)
  {
    // Zoom in horizontally
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() + 1);
    consumed = true;
  }
  else if(event->key() == Qt::Key_Slash)
  {
    // Zoom out horizontally
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() - 1);
    consumed = true;
  }
  else if(event->key() == Qt::Key_0 || event->key() == Qt::Key_Insert)
  {
    // Reset using 0 or 0/Ins on numpad
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->minimum());
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->minimum());
    consumed = true;
  }

  if(consumed && !centeringAircraft)
    emit jumpBackToAircraftStart();

  return consumed;
}

bool ProfileScrollArea::mouseDoubleClickEvent(QMouseEvent *event)
{
  qDebug() << Q_FUNC_INFO << (event->pos() + getOffset());
  emit showPosAlongFlightplan(event->pos().x() + getOffset().x(), true);
  return true;
}

bool ProfileScrollArea::mouseMoveEvent(QMouseEvent *event)
{
  if(NavApp::getRoute().getSizeWithoutAlternates() < 2)
    return false;

  if(!startDragPos.isNull())
  {
    // Mouse down and dragging
    QPoint diff = event->pos() - startDragPos;

    horizScrollBar->setValue(horizScrollBar->value() - diff.x());
    vertScrollBar->setValue(vertScrollBar->value() - diff.y());
    startDragPos = event->pos();

    // Event consumed - do not propagate
    if(!centeringAircraft)
      emit jumpBackToAircraftStart();
    return true;
  }
  return false;
}

bool ProfileScrollArea::mousePressEvent(QMouseEvent *event)
{
  if(NavApp::getRoute().getSizeWithoutAlternates() < 2 || event->button() != Qt::LeftButton)
    return false;

  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->verticalSliderProfileZoom->value() == ui->verticalSliderProfileZoom->minimum() &&
     ui->horizontalSliderProfileZoom->value() == ui->horizontalSliderProfileZoom->minimum())
    return false;

  // Initiate mouse dragging
  startDragPos = event->pos();
  scrollArea->setCursor(Qt::OpenHandCursor);

  return false;
}

bool ProfileScrollArea::mouseReleaseEvent(QMouseEvent *event)
{
  if(NavApp::getRoute().getSizeWithoutAlternates() < 2 || event->button() != Qt::LeftButton)
    return false;

  // End mouse dragging
  startDragPos = QPoint();
  scrollArea->setCursor(Qt::ArrowCursor);
  return false;
}

bool ProfileScrollArea::wheelEvent(QWheelEvent *event)
{
  if(!viewport->geometry().contains(event->pos()))
    // Ignore wheel events that appear outside of the view and on the scrollbars
    return false;

  if(NavApp::getRoute().getSizeWithoutAlternates() > 1)
  {
    emit hideRubberBand();

    Ui::MainWindow *ui = NavApp::getMainUi();

    int zoom = 0;
    QPoint numDegrees = event->angleDelta() / 8;
    if(!event->pixelDelta().isNull())
      zoom = event->pixelDelta().y();
    else if(!numDegrees.isNull())
      zoom = numDegrees.y() / 15;

    QPoint mouse = (event->pos() + getOffset());
    QPoint mouseToCenter = event->pos() - viewport->geometry().center(); // left neg, right pos

    if(event->modifiers() == Qt::NoModifier)
    {
      // Zoom in horizontally ==================================

      // Center object at mouse position first
      lastHorizScrollPos = static_cast<double>(mouse.x()) /
                           static_cast<double>(horizScrollBar->maximum() + horizScrollBar->pageStep());

      // Remember old zoom value
      double oldZoomVal = static_cast<double>(viewport->width()) / profileWidget->width();

      // Set to zoom slider
      ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() + zoom);

      // Calculate the correction for the fixed, not scaled offset in the profile widget
      double correction = profileLeftOffset * oldZoomVal * -mouseToCenter.x() / (viewport->width() / 2.);

      // Calculate difference if scroll bar was at the window edge and adjusted its value
      int diffPos = horizScrollBar->value() - calculatedHorizScrollPos;

      // Put position below mouse back
      horizScrollBar->setValue(static_cast<int>(horizScrollBar->value() - (diffPos + mouseToCenter.x() + correction)));
    }
    else if(event->modifiers() == Qt::ShiftModifier)
    {
      // Zoom in vertically ==================================

      // Center object at mouse position first
      lastVertScrollPos = static_cast<double>(mouse.y()) /
                          static_cast<double>(vertScrollBar->maximum() + vertScrollBar->pageStep());

      // Remember old zoom value
      double oldZoomVal = static_cast<double>(viewport->height()) / profileWidget->height();

      // Set to zoom slider
      ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() + zoom);

      // Calculate the correction for the fixed, not scaled offset in the profile widget
      double correction = profileTopOffset * oldZoomVal * -mouseToCenter.y() / (viewport->height() / 2.);

      // Calculate difference if scroll bar was at the window edge and adjusted its value
      int diffPos = vertScrollBar->value() - calculatedVertScrollPos;

      // Put position below mouse back
      vertScrollBar->setValue(static_cast<int>(vertScrollBar->value() - (diffPos + mouseToCenter.y() + correction)));
    }
  }

  if(!centeringAircraft)
    emit jumpBackToAircraftStart();

  // Consume all events
  return true;
}

bool ProfileScrollArea::resizeEvent()
{
  // Event on scroll area only

  // Let event finishe the resizing first and then scale the view
  QTimer::singleShot(0, this, &ProfileScrollArea::scaleViewAll);

  // Event not consumed
  return false;
}

void ProfileScrollArea::scaleViewAll()
{
  scaleView(horizScrollBar);
  scaleView(vertScrollBar);
}

void ProfileScrollArea::scaleView(QScrollBar *scrollBar)
{
  QSize size(horizScaleFactor * scrollArea->viewport()->width(), vertScaleFactor *scrollArea->viewport()->height());

  // Do not update the last scroll position values by vertScrollBarChanged or horizScrollBarChanged
  noLastScrollPosUpdate = true;
  profileWidget->resize(size);
  noLastScrollPosUpdate = false;

  // Keep scroll bar positions
  if(scrollBar->orientation() == Qt::Vertical)
  {
    // Remember the intended position change to allow calculation of difference to actual scroll bar position
    calculatedVertScrollPos = toScrollBarValue(vertScrollBar, lastVertScrollPos);
    scrollBar->setValue(calculatedVertScrollPos);
  }
  else if(scrollBar->orientation() == Qt::Horizontal)
  {
    calculatedHorizScrollPos = toScrollBarValue(horizScrollBar, lastHorizScrollPos);
    scrollBar->setValue(calculatedHorizScrollPos);
  }
}

int ProfileScrollArea::toScrollBarValue(const QScrollBar *scrollBar, double scrollPos) const
{
  return atools::roundToInt(scrollPos * (scrollBar->maximum() + scrollBar->pageStep()) - scrollBar->pageStep() / 2.);
}

double ProfileScrollArea::toScrollPos(const QScrollBar *scrollBar)
{
  return (scrollBar->value() + scrollBar->pageStep() / 2.) / (scrollBar->maximum() + scrollBar->pageStep());
}

void ProfileScrollArea::horizontalZoomSliderValueChanged(int value)
{
  // 1 = out = route length, route length / 3 NM = in
  horizScaleFactor = value;
  scaleView(horizScrollBar);
}

void ProfileScrollArea::verticalZoomSliderValueChanged(int value)
{
  // 1 = out = max altitude, max altitude / 500  ft = in
  vertScaleFactor = value;
  scaleView(vertScrollBar);
}

QPoint ProfileScrollArea::getOffset() const
{
  return profileWidget->pos() * -1;
}

void ProfileScrollArea::showZoom(bool show)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Use arbitrary large value for the profile part - the widget will redistribute it automatically
  if(show)
    ui->splitterProfile->setSizes({10000, ui->gridLayoutProfileButtons->minimumSize().width()});
  else
    ui->splitterProfile->setSizes({10000, 0});
  resizeEvent();
}

void ProfileScrollArea::showScrollbars(bool show)
{
  scrollArea->setVerticalScrollBarPolicy(show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
  scrollArea->setHorizontalScrollBarPolicy(show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
  resizeEvent();
}

void ProfileScrollArea::showLabels(bool show)
{
  labelWidget->setVisible(show);

  // Signal later in the event queue to give widget a chance to resize
  QTimer::singleShot(0, this, &ProfileScrollArea::resizeEvent);
}

void ProfileScrollArea::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::PROFILE_WINDOW_OPTIONS).save({
    ui->splitterProfile,
    ui->actionProfileCenterAircraft,
    ui->actionProfileFollow,
    ui->actionProfileShowLabels,
    ui->actionProfileShowScrollbars,
    ui->actionProfileShowZoom,
    ui->actionProfileShowIls,
    ui->actionProfileShowVasi
  });
}

void ProfileScrollArea::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::PROFILE_WINDOW_OPTIONS).restore({
    ui->splitterProfile,
    ui->actionProfileCenterAircraft,
    ui->actionProfileFollow,
    ui->actionProfileShowLabels,
    ui->actionProfileShowScrollbars,
    ui->actionProfileShowZoom,
    ui->actionProfileShowIls,
    ui->actionProfileShowVasi
  });
  ui->splitterProfile->setHandleWidth(6);
}

bool ProfileScrollArea::centerAircraft(const QPoint& screenPoint, float verticalSpeed)
{
  // Use rectangle on the left side of the view
  int xmarginLeft = viewport->width() / 20;
  int xmarginRight = viewport->width() * 8 / 10;

  int ymarginTop, ymarginBottom, vertPos;
  if(verticalSpeed > 250)
  {
    // Climb - keep aircraft in the bottom left corner
    ymarginTop = viewport->height() * 8 / 10;
    ymarginBottom = viewport->height() / 10;
    vertPos = viewport->height() - ymarginBottom;
  }
  else if(verticalSpeed < -250)
  {
    // Descent - keep aircraft in the top left corner
    ymarginTop = viewport->height() / 10;
    ymarginBottom = viewport->height() * 8 / 10;
    vertPos = ymarginTop;
  }
  else
  {
    // Keep aircraft in the center left corner
    ymarginTop = viewport->height() / 4;
    ymarginBottom = viewport->height() / 4;
    vertPos = viewport->height() / 2;
  }

  int x = screenPoint.x();
  int y = screenPoint.y();

  bool centered = false;
  centeringAircraft = true;
  if(x - xmarginLeft < horizScrollBar->value() || x > horizScrollBar->value() + viewport->width() - xmarginRight)
  {
    centered = true;
    horizScrollBar->setValue(std::min(std::max(horizScrollBar->minimum(), x - xmarginLeft), horizScrollBar->maximum()));
  }

  if(y - ymarginTop < vertScrollBar->value() || y > vertScrollBar->value() + viewport->height() - ymarginBottom)
  {
    centered = true;
    vertScrollBar->setValue(std::min(std::max(vertScrollBar->minimum(), y - vertPos), vertScrollBar->maximum()));
  }
  centeringAircraft = false;
  return centered;
}

void ProfileScrollArea::styleChanged()
{
  // Styles cascade to children and mess up UI themes on linux - even if widget is selected by name
#ifndef Q_OS_LINUX
  // Make the elevation profile splitter handle better visible - update background color
  NavApp::getMainUi()->splitterProfile->setStyleSheet(
    QString("QSplitter::handle { "
            "background: %1;"
            "image: url(:/littlenavmap/resources/icons/splitterhandhoriz.png);"
            " }").
    arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
#endif
}

void ProfileScrollArea::setMaxVertZoom()
{
  // Max vertical scale for window height 500 ft
  NavApp::getMainUi()->verticalSliderProfileZoom->setMaximum(std::min(atools::roundToInt(maxWindowAlt / 500.f), 500));

#ifdef DEBUG_INFORMATION
  Ui::MainWindow *ui = NavApp::getMainUi();
  qDebug() << Q_FUNC_INFO << ui->verticalSliderProfileZoom->minimum() << ui->verticalSliderProfileZoom->maximum();
#endif
}

void ProfileScrollArea::setMaxHorizZoom()
{
  // Max horizontal scale for window width 4 NM
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->horizontalSliderProfileZoom->setMaximum(
    atools::roundToInt(std::max(NavApp::getRoute().getTotalDistance() / 4.f, 4.f)));

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << ui->horizontalSliderProfileZoom->minimum() << ui->horizontalSliderProfileZoom->maximum();
#endif
}
