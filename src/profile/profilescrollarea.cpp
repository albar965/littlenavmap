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

#include "profile/profilescrollarea.h"
#include "profile/profilewidget.h"
#include "profile/profilelabelwidgetvert.h"
#include "profile/profilelabelwidgethoriz.h"
#include "profile/profileoptions.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "gui/helphandler.h"
#include "route/route.h"
#include "options/optiondata.h"

#include "app/navapp.h"
#include "atools.h"

#include <QDebug>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QWidget>
#include <QToolTip>
#include "ui_mainwindow.h"

// Need to use fixed value for minimum since Qt arbitrarily resets min to 0
const static int PROFILE_SLIDER_MINIMUM = 1;

ProfileScrollArea::ProfileScrollArea(ProfileWidget *parent, QScrollArea *scrollAreaParam)
  : QObject(parent), profileWidget(parent),
  horizScrollBar(scrollAreaParam->horizontalScrollBar()), vertScrollBar(scrollAreaParam->verticalScrollBar()),
  scrollArea(scrollAreaParam), viewport(scrollAreaParam->viewport())
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  scrollArea->setContextMenuPolicy(Qt::DefaultContextMenu);
  scrollArea->setFocusPolicy(Qt::WheelFocus);

  // Create label widget on the left
  profileLabelWidgetVert = new ProfileLabelWidgetVert(profileWidget, this);
  profileLabelWidgetVert->setMinimumWidth(1); // Setting to 0 hides the widget
  ui->gridLayoutProfileDrawing->replaceWidget(ui->widgetProfileLabelVert, profileLabelWidgetVert);

  // Create label widget on the top
  profileLabelWidgetHoriz = new ProfileLabelWidgetHoriz(profileWidget, this);
  profileLabelWidgetHoriz->setMinimumHeight(1); // Setting to 0 hides the widget
  ui->gridLayoutProfileDrawing->replaceWidget(ui->widgetProfileLabelHoriz, profileLabelWidgetHoriz);

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
  connect(ui->horizontalSliderProfileZoom, &QSlider::valueChanged, this, &ProfileScrollArea::horizontalZoomSliderValueChanged);
  connect(ui->verticalSliderProfileZoom, &QSlider::valueChanged, this, &ProfileScrollArea::verticalZoomSliderValueChanged);

  // Update label on scroll bar changes and remember position
  connect(horizScrollBar, &QScrollBar::rangeChanged, this, &ProfileScrollArea::horizScrollBarChanged);
  connect(horizScrollBar, &QScrollBar::valueChanged, this, &ProfileScrollArea::horizScrollBarValueChanged);

  connect(vertScrollBar, &QScrollBar::rangeChanged, this, &ProfileScrollArea::vertScrollBarChanged);
  connect(vertScrollBar, &QScrollBar::valueChanged, this, &ProfileScrollArea::vertScrollBarValueChanged);

  // Buttons and menu actions
  connect(ui->pushButtonProfileExpand, &QPushButton::clicked, this, &ProfileScrollArea::expandWidget);
  connect(ui->pushButtonProfileHelp, &QPushButton::clicked, this, &ProfileScrollArea::helpClicked);
  connect(ui->actionProfileExpand, &QAction::triggered, this, &ProfileScrollArea::expandWidget);
  connect(ui->actionProfileShowScrollbars, &QAction::toggled, this, &ProfileScrollArea::showScrollbarsToggled);

  connect(ui->actionProfileShowZoom, &QAction::toggled, this, &ProfileScrollArea::showZoomToggled);

  // Need to resize scroll area
  connect(ui->splitterProfile, &QSplitter::splitterMoved, this, &ProfileScrollArea::splitterMoved);

  // Install event filter to area and viewport widgets avoid subclassing ====================
  scrollArea->installEventFilter(this);
  scrollArea->viewport()->installEventFilter(this);

  // Use a label as top level window styled as a tooltip ====================
  tooltipLabel = new QLabel(viewport, Qt::FramelessWindowHint);

  // Keep inactive
  tooltipLabel->setAttribute(Qt::WA_ShowWithoutActivating);
  tooltipLabel->setAutoFillBackground(true);
  tooltipLabel->setFrameShape(QFrame::Box); // Black border
  tooltipLabel->setMargin(0);

  // Use tooltip palette
  tooltipLabel->setPalette(QToolTip::palette());
  tooltipLabel->setForegroundRole(QPalette::ToolTipText);
  tooltipLabel->setBackgroundRole(QPalette::ToolTipBase);

  // Shrink font a bit
  QFont font = tooltipLabel->font();
  if(font.pointSizeF() > 1.)
  {
    font.setPointSizeF(font.pointSizeF() * 0.9);
    tooltipLabel->setFont(font);
  }
  tooltipLabel->hide();
}

ProfileScrollArea::~ProfileScrollArea()
{
  scrollArea->removeEventFilter(this);
  scrollArea->viewport()->removeEventFilter(this);
  delete tooltipLabel;
  delete profileLabelWidgetVert;
  delete profileLabelWidgetHoriz;
}

void ProfileScrollArea::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrlWeb(scrollArea, lnm::helpOnlineUrl + "PROFILE.html", lnm::helpLanguageOnline());
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
  ui->horizontalSliderProfileZoom->setValue(PROFILE_SLIDER_MINIMUM);
  ui->horizontalSliderProfileZoom->setMinimum(PROFILE_SLIDER_MINIMUM);
  ui->verticalSliderProfileZoom->setValue(PROFILE_SLIDER_MINIMUM);
  ui->verticalSliderProfileZoom->setMinimum(PROFILE_SLIDER_MINIMUM);
  scrollArea->setWidgetResizable(false);
}

void ProfileScrollArea::showTooltip(const QPoint& globalPos, const QString& text)
{
  if(profileWidget->getProfileOptions()->getDisplayOptions().testFlag(optsp::PROFILE_TOOLTIP))
  {
    // Set text if changed and adjust window size
    if(text != tooltipLabel->text())
    {
      tooltipLabel->setText(text);
      tooltipLabel->adjustSize();
    }

    int cursorX = viewport->mapFromGlobal(globalPos).x();
    if(tooltipLabel->isVisible())
    {
      // Cut off width and/or height if the tooltip is bigger than the viewport
      if(tooltipLabel->height() > viewport->height())
        tooltipLabel->resize(tooltipLabel->width(), viewport->height());

      if(tooltipLabel->width() > viewport->width())
        tooltipLabel->resize(viewport->width(), tooltipLabel->height());

      // Check if tooltip has to jump to other side
      int buffer = std::min(tooltipLabel->width() * 5 / 4, viewport->width() / 3);
      int leftMargin = buffer;
      int rightMargin = viewport->width() - buffer;

      bool tooltipIsLeft = tooltipLabel->pos().x() <= 2;
      if(!tooltipIsLeft && cursorX >= rightMargin)
        // Tooltip at right side and cursor above right margin - move to left side
        tooltipLabel->move(QPoint());
      else if(tooltipIsLeft && cursorX <= leftMargin)
        // Tooltip at left side and cursor below left margin - move to right side
        tooltipLabel->move(QPoint(viewport->width() - tooltipLabel->width(), 0));
      else if(!tooltipIsLeft && tooltipLabel->x() + tooltipLabel->width() > viewport->width())
        // Tooltip grew above parent width - reposition to be fully visible
        tooltipLabel->move(QPoint(viewport->width() - tooltipLabel->width(), 0));
    }
    else
    {
      // Not visible yet. Move to either side depending on cursor position
      if(cursorX > viewport->width() / 2)
        // Cursor right half of window - move tooltip to left side
        tooltipLabel->move(QPoint());
      else
        // Cursor left half of window - move tooltip to right side
        tooltipLabel->move(QPoint(viewport->width() - tooltipLabel->width(), 0));

      // Show it
      tooltipLabel->setVisible(true);
      tooltipLabel->raise();
    }
  }
  else if(tooltipLabel->isVisible())
    tooltipLabel->setVisible(false);
}

void ProfileScrollArea::hideTooltip()
{
  if(tooltipLabel != nullptr)
    tooltipLabel->setVisible(false);
}

bool ProfileScrollArea::isTooltipVisible() const
{
  return tooltipLabel->isVisible();
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

  if(!changingView)
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
  profileLabelWidgetVert->update();
}

void ProfileScrollArea::horizScrollBarValueChanged()
{
  horizScrollBarChanged();

  if(!changingView)
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

  // Update y scale
  profileLabelWidgetHoriz->update();
}

void ProfileScrollArea::update()
{
  scrollArea->update();
  updateLabelWidgets();
}

void ProfileScrollArea::updateLabelWidgets()
{
  profileLabelWidgetVert->update();
  profileLabelWidgetHoriz->update();
}

void ProfileScrollArea::routeChanged(bool geometryChanged)
{
  maxWindowAlt = profileWidget->getMaxWindowAlt();

  if(geometryChanged)
    setMaxHorizZoom();

  routeAltitudeChanged();

  profileLabelWidgetVert->routeChanged();
  profileLabelWidgetHoriz->routeChanged();

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
  bool routeValid = profileWidget->hasValidRouteForDisplay();

  if(!routeValid)
  {
    // Reset zoom sliders
    ui->horizontalSliderProfileZoom->setValue(PROFILE_SLIDER_MINIMUM);
    ui->horizontalSliderProfileZoom->setMinimum(PROFILE_SLIDER_MINIMUM);
    ui->verticalSliderProfileZoom->setValue(PROFILE_SLIDER_MINIMUM);
    ui->verticalSliderProfileZoom->setMinimum(PROFILE_SLIDER_MINIMUM);
    lastVertScrollPos = 0.5;
    lastHorizScrollPos = 0.5;
  }

  // Disable scrolling and zooming
  ui->labelProfileHorizontalSlider->setEnabled(routeValid);
  ui->labelProfileVerticalSlider->setEnabled(routeValid);
  ui->pushButtonProfileExpand->setEnabled(routeValid);
  ui->actionProfileExpand->setEnabled(routeValid);
  ui->horizontalSliderProfileZoom->setEnabled(routeValid);
  ui->verticalSliderProfileZoom->setEnabled(routeValid);
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
  if(NavApp::getRouteConst().getSizeWithoutAlternates() < 2)
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
    ui->horizontalSliderProfileZoom->setValue(PROFILE_SLIDER_MINIMUM);
    ui->verticalSliderProfileZoom->setValue(PROFILE_SLIDER_MINIMUM);
    consumed = true;
  }

  if(consumed && !changingView)
    emit jumpBackToAircraftStart();

  return consumed;
}

bool ProfileScrollArea::mouseDoubleClickEvent(QMouseEvent *event)
{
  if(!profileWidget->hasValidRouteForDisplay())
    return false;

  emit showPosAlongFlightplan(event->pos().x() + getOffset().x(), true);
  return true;
}

bool ProfileScrollArea::mouseMoveEvent(QMouseEvent *event)
{
  if(!profileWidget->hasValidRouteForDisplay())
    return false;

  if(!startDragPos.isNull())
  {
    // Mouse down and dragging
    QPoint diff = event->pos() - startDragPos;

    horizScrollBar->setValue(horizScrollBar->value() - diff.x());
    vertScrollBar->setValue(vertScrollBar->value() - diff.y());
    startDragPos = event->pos();

    // Event consumed - do not propagate
    if(!changingView)
      emit jumpBackToAircraftStart();
    return true;
  }
  return false;
}

bool ProfileScrollArea::mousePressEvent(QMouseEvent *event)
{
  if(NavApp::getRouteConst().getSizeWithoutAlternates() < 2 || event->button() != Qt::LeftButton)
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
  if(NavApp::getRouteConst().getSizeWithoutAlternates() < 2 || event->button() != Qt::LeftButton)
    return false;

  // End mouse dragging
  startDragPos = QPoint();
  scrollArea->setCursor(Qt::ArrowCursor);
  return false;
}

bool ProfileScrollArea::wheelEvent(QWheelEvent *event)
{
  static const int ANGLE_THRESHOLD = 120;

  if(!viewport->geometry().contains(event->pos()))
    // Ignore wheel events that appear outside of the view and on the scrollbars
    return false;

  if(NavApp::getRouteConst().getSizeWithoutAlternates() > 1)
  {
    emit hideRubberBand();

    Ui::MainWindow *ui = NavApp::getMainUi();

    // Pixel is null for mouse wheel - otherwise touchpad
    int angleDelta = event->angleDelta().y();

    // Sum up wheel events to start action one threshold is exceeded
    lastWheelAngle += angleDelta;

    if(atools::sign(lastWheelAngle) != atools::sign(angleDelta))
      // User changed direction while moving - reverse direction
      // to allow immediate scroll direction change
      lastWheelAngle = ANGLE_THRESHOLD * atools::sign(angleDelta);

    if(std::abs(lastWheelAngle) >= ANGLE_THRESHOLD)
    {
      int zoom = lastWheelAngle > 0 ? 1 : -1;
      if(OptionData::instance().getFlags().testFlag(opts::GUI_REVERSE_WHEEL))
        zoom = -zoom;

      // Reset summed up values if accepted
      lastWheelAngle = 0;

      QPoint mouse = (event->pos() + getOffset());
      QPoint mouseToCenter = event->pos() - viewport->geometry().center(); // left neg, right pos

      if(event->modifiers() == Qt::NoModifier)
      {
        // Zoom in horizontally ==================================
        int value = ui->horizontalSliderProfileZoom->value();
        if((zoom > 0 && value < ui->horizontalSliderProfileZoom->maximum()) ||
           (zoom < 0 && value > ui->horizontalSliderProfileZoom->minimum()))
        {
          // Center object at mouse position first
          lastHorizScrollPos = static_cast<double>(mouse.x()) / static_cast<double>(horizScrollBar->maximum() + horizScrollBar->pageStep());

          // Remember old zoom value
          double oldZoomVal = static_cast<double>(viewport->width()) / profileWidget->width();

          // Set to zoom slider
          ui->horizontalSliderProfileZoom->setValue(value + zoom);

          // Calculate the correction for the fixed, not scaled offset in the profile widget
          double correction = profileLeftOffset * oldZoomVal * -mouseToCenter.x() / (viewport->width() / 2.);

          // Calculate difference if scroll bar was at the window edge and adjusted its value
          int diffPos = horizScrollBar->value() - calculatedHorizScrollPos;

          // Put position below mouse back
          horizScrollBar->setValue(static_cast<int>(horizScrollBar->value() - (diffPos + mouseToCenter.x() + correction)));
        }
      }
      else if(event->modifiers() == Qt::ShiftModifier)
      {
        // Zoom in vertically ==================================
        int value = ui->verticalSliderProfileZoom->value();
        if((zoom > 0 && value < ui->verticalSliderProfileZoom->maximum()) ||
           (zoom < 0 && value > ui->verticalSliderProfileZoom->minimum()))
        {

          // Center object at mouse position first
          lastVertScrollPos = static_cast<double>(mouse.y()) / static_cast<double>(vertScrollBar->maximum() + vertScrollBar->pageStep());

          // Remember old zoom value
          double oldZoomVal = static_cast<double>(viewport->height()) / profileWidget->height();

          // Set to zoom slider
          ui->verticalSliderProfileZoom->setValue(value + zoom);

          // Calculate the correction for the fixed, not scaled offset in the profile widget
          double correction = profileTopOffset * oldZoomVal * -mouseToCenter.y() / (viewport->height() / 2.);

          // Calculate difference if scroll bar was at the window edge and adjusted its value
          int diffPos = vertScrollBar->value() - calculatedVertScrollPos;

          // Put position below mouse back
          vertScrollBar->setValue(static_cast<int>(vertScrollBar->value() - (diffPos + mouseToCenter.y() + correction)));
        }
      }
    }
  }
  if(!changingView)
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
  QSize size(horizScaleFactor * scrollArea->viewport()->width(), vertScaleFactor * scrollArea->viewport()->height());

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

#ifdef DEBUG_INFORMATION
  debugPrintValues();
#endif
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

void ProfileScrollArea::showZoomToggled(bool show)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Use arbitrary large value for the profile part - the widget will redistribute it automatically
  if(show)
    ui->splitterProfile->setSizes({10000, ui->verticalLayoutProfile->minimumSize().width()});
  else
    ui->splitterProfile->setSizes({10000, 0});
  resizeEvent();
}

void ProfileScrollArea::showScrollbarsToggled(bool show)
{
  scrollArea->setVerticalScrollBarPolicy(show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
  scrollArea->setHorizontalScrollBarPolicy(show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
  resizeEvent();
}

void ProfileScrollArea::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::PROFILE_WINDOW_OPTIONS).save({ui->splitterProfile, ui->actionProfileCenterAircraft,
                                                              ui->actionProfileZoomAircraft, ui->actionProfileFollow,
                                                              ui->actionProfileShowScrollbars, ui->actionProfileShowZoom,
                                                              ui->actionProfileShowIls, ui->actionProfileShowVasi,
                                                              ui->actionProfileShowVerticalTrack});
}

void ProfileScrollArea::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::PROFILE_WINDOW_OPTIONS).restore({ui->splitterProfile, ui->actionProfileCenterAircraft,
                                                                 ui->actionProfileZoomAircraft, ui->actionProfileFollow,
                                                                 ui->actionProfileShowScrollbars, ui->actionProfileShowZoom,
                                                                 ui->actionProfileShowIls, ui->actionProfileShowVasi,
                                                                 ui->actionProfileShowVerticalTrack});
  ui->splitterProfile->setHandleWidth(6);
}

void ProfileScrollArea::restoreSplitter()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::PROFILE_WINDOW_OPTIONS);
  if(!state.contains(ui->splitterProfile))
  {
    // First start - splitter not saved yet

    // Adjust splitter size to a reasonable value by setting size for the widget on the right
    QList<int> sizes = ui->splitterProfile->sizes();

    if(sizes.size() >= 2)
      sizes[1] = 20; // Use minimum - will stick to widget size hints for minimum
    ui->splitterProfile->setSizes(sizes);

    // Save splitter size - user can resize freely after next start
    state.save(ui->splitterProfile);
  }
}

bool ProfileScrollArea::isPointVisible(const QPoint& point)
{
  return atools::inRange(horizScrollBar->value(), horizScrollBar->value() + viewport->width(), point.x()) &&
         atools::inRange(vertScrollBar->value(), vertScrollBar->value() + viewport->height(), point.y());
}

void ProfileScrollArea::centerRect(const QPoint& leftScreenPoint, const QPoint& rightScreenPoint, bool zoomVertically, bool force)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "leftScreenPoint" << leftScreenPoint << "rightScreenPoint" << rightScreenPoint
           << "zoomVertically" << zoomVertically << "force" << force;
#endif

  const static double MIN_VIEWPORT_LENGTH_NM = 8.;
  const static double MIN_VIEWPORT_HEIGHT_FT = 3000.;
  const static int MIN_UPDATE_SECONDS = 5;

  // point1 and point2 are relative to profile widget rect
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(!leftScreenPoint.isNull() && !rightScreenPoint.isNull())
  {
    // Convert points to floating point
    QPointF aircraftPt(leftScreenPoint), destPt(rightScreenPoint);

    // Add margins to left point to avoid zooming in too deep
    aircraftPt.rx() -= viewport->width() / 10.f;
    aircraftPt.ry() -= viewport->height() / 10.f;

    // Calculate relative coordinates where 0 = left edge and 1 = right edge - these are independent of the zoom
    double relative1X = atools::minmax(0., 1., aircraftPt.x() / profileWidget->width()),
           relative1Y = atools::minmax(0., 1., aircraftPt.y() / profileWidget->height()),
           relative2X = atools::minmax(0., 1., destPt.x() / profileWidget->width()),
           relative2Y = atools::minmax(0., 1., destPt.y() / profileWidget->height());

    // Calculate the viewport dimensions to avoid zooming in too close
    double viewportWidthNm = viewport->width() / profileWidget->getHorizontalScale();
    double viewportHeightFt = viewport->height() / profileWidget->getVerticalScale();

#ifdef DEBUG_INFORMATION_PROFILE_CENTER_RECT
    qDebug() << Q_FUNC_INFO << leftScreenPoint << rightScreenPoint << aircraftPt << destPt
             << "widthNm" << viewportWidthNm << "heightFt" << viewportHeightFt;
    qDebug() << Q_FUNC_INFO << "rel1X" << relative1X << "rel1Y" << relative1Y << "rel2X" << relative2X << "rel2Y" << relative2Y;
    qDebug() << Q_FUNC_INFO << "viewport" << viewport->rect() << "profile widget" << profileWidget->rect();
    qDebug() << Q_FUNC_INFO << "VSCROLL" << vertScrollBar->value() << "HSCROLL" << horizScrollBar->value();
#endif

    // Force update if aircraft is not visible
    if(!isPointVisible(leftScreenPoint))
      force = true;

    // Do not update more often than five seconds
    QDateTime now = QDateTime::currentDateTime();
    if(force || !lastCenterAircraftAndDest.isValid() || now > lastCenterAircraftAndDest.addSecs(MIN_UPDATE_SECONDS))
    {
      lastCenterAircraftAndDest = now;
      changingView = true;

      // Zoom and postion only if value has changed and current zoom is not closer than 8 NM for the viewport
      double relHoriz = std::abs(relative1X - relative2X);
      if(relHoriz > 0.)
      {
        int zoomHoriz = static_cast<int>(1. / relHoriz);
        if(force || (ui->horizontalSliderProfileZoom->value() != zoomHoriz && viewportWidthNm > MIN_VIEWPORT_LENGTH_NM))
        {
          ui->horizontalSliderProfileZoom->setValue(zoomHoriz);
          horizScrollBar->setValue(static_cast<int>(relative1X * profileWidget->width()));
        }
      }

      // Zoom and postion only vertically if value has changed and current zoom is not closer than 3000 ft for the viewport
      double relVert = std::abs(relative1Y - relative2Y);
      if(relHoriz > 0.)
      {
        int zoomVert = static_cast<int>(1. / relVert);
        if(force || (ui->verticalSliderProfileZoom->value() != zoomVert && zoomVertically && viewportHeightFt > MIN_VIEWPORT_HEIGHT_FT))
        {
          ui->verticalSliderProfileZoom->setValue(zoomVert);
          vertScrollBar->setValue(static_cast<int>(relative1Y * profileWidget->height()));
        }
      }

      changingView = false;
    }
  }
}

void ProfileScrollArea::centerAircraft(const QPoint& aircraftScreenPoint, float verticalSpeed, bool force)
{
  // aircraftScreenPoint is relative to profile widget rect
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

  int x = aircraftScreenPoint.x();
  int y = aircraftScreenPoint.y();

  changingView = true;
  if(force || x - xmarginLeft < horizScrollBar->value() || x > horizScrollBar->value() + viewport->width() - xmarginRight)
    horizScrollBar->setValue(x - xmarginLeft);

  if(force || y - ymarginTop < vertScrollBar->value() || y > vertScrollBar->value() + viewport->height() - ymarginBottom)
    vertScrollBar->setValue(y - vertPos);
  changingView = false;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "x" << x << "y" << y << "centeringAircraft" << changingView;
#endif
}

void ProfileScrollArea::styleChanged()
{
  // Styles cascade to children and mess up UI themes on linux - even if widget is selected by name
#if !defined(Q_OS_LINUX) || defined(DEBUG_INFORMATION)
  // Make the elevation profile splitter handle better visible - update background color
  NavApp::getMainUi()->splitterProfile->setStyleSheet(
    QString("QSplitter::handle { "
            "background: %1;"
            "image: url(:/littlenavmap/resources/icons/splitterhandhoriz.png);"
            " }").
    arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
#endif
  optionsChanged();
}

void ProfileScrollArea::optionsChanged()
{
  hideTooltip();
  profileLabelWidgetHoriz->optionsChanged();
  profileLabelWidgetVert->optionsChanged();
}

void ProfileScrollArea::setMaxVertZoom()
{
  // Max vertical scale for window height 500 ft
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->verticalSliderProfileZoom->setMaximum(std::min(atools::roundToInt(maxWindowAlt / 500.f), 500));
}

void ProfileScrollArea::setMaxHorizZoom()
{
  // Max horizontal scale for window width 4 NM
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->horizontalSliderProfileZoom->setMaximum(atools::roundToInt(std::max(NavApp::getRouteConst().getTotalDistance() / 4.f, 4.f)));
}

#ifdef DEBUG_INFORMATION
void ProfileScrollArea::debugPrintValues()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  qDebug() << Q_FUNC_INFO
           << "VERT min" << ui->verticalSliderProfileZoom->minimum() << "max" << ui->verticalSliderProfileZoom->maximum()
           << "value" << ui->verticalSliderProfileZoom->value()
           << "HORIZ min" << ui->horizontalSliderProfileZoom->minimum() << "max" << ui->horizontalSliderProfileZoom->maximum()
           << "value" << ui->horizontalSliderProfileZoom->value();

  qDebug() << Q_FUNC_INFO << "viewport" << viewport->size() << "profile widget" << profileWidget->size();
}

#endif
