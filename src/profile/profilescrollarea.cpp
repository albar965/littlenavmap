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

#include "profile/profilescrollarea.h"
#include "profile/profilewidget.h"
#include "profile/profilelabelwidget.h"
#include "gui/widgetstate.h"
#include "common/constants.h"

#include "navapp.h"
#include "atools.h"

#include <QDebug>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QWidget>
#include "ui_mainwindow.h"

ProfileScrollArea::ProfileScrollArea(ProfileWidget *parent, QScrollArea *scrollAreaParam)
  : QObject(parent), profileWidget(parent),
  hScrollBar(scrollAreaParam->horizontalScrollBar()), vScrollBar(scrollAreaParam->verticalScrollBar()),
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

  // Connect zoom sliders
  connect(ui->horizontalSliderProfileZoom, &QSlider::valueChanged,
          this, &ProfileScrollArea::horizontalZoomSliderValueChanged);
  connect(ui->verticalSliderProfileZoom, &QSlider::valueChanged,
          this, &ProfileScrollArea::verticalZoomSliderValueChanged);

  // Update label on scroll bar changes and remember position
  connect(hScrollBar, &QScrollBar::rangeChanged, this, &ProfileScrollArea::horizScrollBarsChanged);
  connect(vScrollBar, &QScrollBar::rangeChanged, this, &ProfileScrollArea::vertScrollBarsChanged);
  connect(hScrollBar, &QScrollBar::valueChanged, this, &ProfileScrollArea::horizScrollBarsChanged);
  connect(vScrollBar, &QScrollBar::valueChanged, this, &ProfileScrollArea::vertScrollBarsChanged);

  connect(ui->pushButtonProfileExpand, &QPushButton::clicked, this, &ProfileScrollArea::expandClicked);
  connect(ui->actionProfileExpand, &QAction::triggered, this, &ProfileScrollArea::expandClicked);

  connect(ui->pushButtonProfileFit, &QCheckBox::toggled, this, &ProfileScrollArea::fillToggled);
  connect(ui->actionProfileFit, &QAction::toggled, this, &ProfileScrollArea::fillToggled);

  connect(ui->actionProfileShowLabels, &QAction::toggled, this, &ProfileScrollArea::showLabels);
  connect(ui->actionProfileShowScrollbars, &QAction::toggled, this, &ProfileScrollArea::showScrollbars);
  connect(ui->actionProfileShowZoom, &QAction::toggled, this, &ProfileScrollArea::showZoom);

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

QRect ProfileScrollArea::getViewPortRect() const
{
  return QRect(scrollArea->widget()->pos() * -1, scrollArea->viewport()->size());
}

void ProfileScrollArea::expandClicked()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  scaleFactorVert = scaleFactorHoriz = 1.0;

  // Resize widget temporarily and the switch it off again
  scrollArea->setWidgetResizable(true);
  ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->minimum());
  ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->minimum());
  scrollArea->setWidgetResizable(false);
}

void ProfileScrollArea::vertScrollBarsChanged()
{
  if(!changingScrollBars && vScrollBar->maximum() > 0)
    lastVertScrollPos = static_cast<double>(vScrollBar->value()) / (vScrollBar->maximum() - vScrollBar->minimum());

  labelWidget->update();
}

void ProfileScrollArea::splitterMoved(int pos, int index)
{
  Q_UNUSED(pos);
  Q_UNUSED(index);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionProfileShowZoom->blockSignals(true);
  ui->actionProfileShowZoom->setChecked(ui->splitterProfile->sizes().at(1) > 0);
  ui->actionProfileShowZoom->blockSignals(false);
}

void ProfileScrollArea::horizScrollBarsChanged()
{
  if(!changingScrollBars && hScrollBar->maximum() > 0)
    lastHorizScrollPos = static_cast<double>(hScrollBar->value()) / (hScrollBar->maximum() - hScrollBar->minimum());
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
  Q_UNUSED(geometryChanged);

  Ui::MainWindow *ui = NavApp::getMainUi();
  // Max horizontal scale for window width 3 NM
  ui->horizontalSliderProfileZoom->setMaximum(std::max(atools::roundToInt(NavApp::getRoute().getTotalDistance() / 3.f),
                                                       3));

  // Max vertical scale for window height 500 ft
  ui->verticalSliderProfileZoom->setMaximum(std::max(atools::roundToInt(maxWindowAlt / 500.f), 500));

  updateWidgets();
}

void ProfileScrollArea::setMaxWindowAlt(float value)
{
  if(atools::almostNotEqual(maxWindowAlt, value, 10.f) && value >= 500.f && NavApp::getRoute().size() > 1)
  {
    maxWindowAlt = value;

    // Max vertical scale for window height 500 ft
    NavApp::getMainUi()->verticalSliderProfileZoom->setMaximum(atools::roundToInt(maxWindowAlt / 500.f));

    updateWidgets();
  }
}

void ProfileScrollArea::fillToggled(bool checked)
{
  Q_UNUSED(checked);
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Adjust action and push button each other
  if(sender() == ui->actionProfileFit)
    ui->pushButtonProfileFit->setChecked(ui->actionProfileFit->isChecked());
  else if(sender() == ui->pushButtonProfileFit)
    ui->actionProfileFit->setChecked(ui->pushButtonProfileFit->isChecked());

  scaleFactorVert = scaleFactorHoriz = 1.0;

  updateWidgets();
}

void ProfileScrollArea::updateWidgets()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool routeEmpty = NavApp::getRoute().size() < 2;
  bool checked = ui->actionProfileFit->isChecked();

  scrollArea->setWidgetResizable(checked);

  ui->pushButtonProfileFit->setDisabled(routeEmpty);
  ui->actionProfileFit->setDisabled(routeEmpty);

  if(checked || routeEmpty)
  {
    // Reset zoom sliders
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->minimum());
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->minimum());
    lastVertScrollPos = 1.;
    lastHorizScrollPos = 0.;
  }

  // Disable scrolling and zooming
  ui->labelProfileHorizontalSlider->setDisabled(checked || routeEmpty);
  ui->labelProfileVerticalSlider->setDisabled(checked || routeEmpty);
  ui->pushButtonProfileExpand->setDisabled(checked || routeEmpty);
  ui->actionProfileExpand->setDisabled(checked || routeEmpty);
  ui->horizontalSliderProfileZoom->setDisabled(checked || routeEmpty);
  ui->verticalSliderProfileZoom->setDisabled(checked || routeEmpty);
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
      consumed = resizeEvent(dynamic_cast<QResizeEvent *>(event));
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
  if(isFill() || NavApp::getRoute().size() < 2)
    return false;

  Ui::MainWindow *ui = NavApp::getMainUi();
  if(event->key() == Qt::Key_Home)
  {
    // Scroll to left beginning
    hScrollBar->setValue(hScrollBar->minimum());
    return true;
  }
  else if(event->key() == Qt::Key_End)
  {
    // Scroll to right end
    hScrollBar->setValue(hScrollBar->maximum());
    return true;
  }
  if(event->key() == Qt::Key_PageUp)
  {
    hScrollBar->setValue(hScrollBar->value() - hScrollBar->pageStep());
    return true;
  }
  else if(event->key() == Qt::Key_PageDown)
  {
    hScrollBar->setValue(hScrollBar->value() + hScrollBar->pageStep());
    return true;
  }
  else if(event->key() == Qt::Key_Plus)
  {
    // Zoom in vertically
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() + 1);
    return true;
  }
  else if(event->key() == Qt::Key_Minus)
  {
    // Zoom out vertically
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() - 1);
    return true;
  }
  else if(event->key() == Qt::Key_Asterisk)
  {
    // Zoom in horizontally
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() + 1);
    return true;
  }
  else if(event->key() == Qt::Key_Slash)
  {
    // Zoom out horizontally
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() - 1);
    return true;
  }
  else if(event->key() == Qt::Key_0 || event->key() == Qt::Key_Insert)
  {
    // Reset using 0 or 0/Ins on numpad
    ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->minimum());
    ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->minimum());
    return true;
  }
  return false;
}

bool ProfileScrollArea::mouseDoubleClickEvent(QMouseEvent *event)
{
  qDebug() << Q_FUNC_INFO << (event->pos() + getOffset());
  emit showPosAlongFlightplan(event->pos().x() + getOffset().x(), true);
  return true;
}

bool ProfileScrollArea::mouseMoveEvent(QMouseEvent *event)
{
  if(isFill() || NavApp::getRoute().size() < 2)
    return false;

  if(!startDragPos.isNull())
  {
    // Mouse down and dragging
    QPoint diff = event->pos() - startDragPos;

    hScrollBar->setValue(hScrollBar->value() - diff.x());
    vScrollBar->setValue(vScrollBar->value() - diff.y());
    startDragPos = event->pos();

    // Event consumed - do not propagate
    return true;
  }
  return false;
}

bool ProfileScrollArea::mousePressEvent(QMouseEvent *event)
{
  if(isFill() || NavApp::getRoute().size() < 2 || event->button() != Qt::LeftButton)
    return false;

  // Initiate mouse dragging
  startDragPos = event->pos();
  return false;
}

bool ProfileScrollArea::mouseReleaseEvent(QMouseEvent *event)
{
  if(isFill() || NavApp::getRoute().size() < 2 || event->button() != Qt::LeftButton)
    return false;

  // End mouse dragging
  startDragPos = QPoint();
  return false;
}

bool ProfileScrollArea::wheelEvent(QWheelEvent *event)
{
  if(!isFill() && NavApp::getRoute().size() > 1)
  {
    Ui::MainWindow *ui = NavApp::getMainUi();
    QPoint numPixels = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;

    if(event->modifiers() == Qt::ShiftModifier)
    {
      // Zoom in vertically
      if(!numPixels.isNull())
        ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() + numPixels.y());
      else if(!numDegrees.isNull())
        ui->verticalSliderProfileZoom->setValue(ui->verticalSliderProfileZoom->value() + numDegrees.y() / 15);
    }
    else if(event->modifiers() == Qt::NoModifier)
    {
      // Zoom in horizontally
      if(!numPixels.isNull())
        ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() + numPixels.y());
      else if(!numDegrees.isNull())
        ui->horizontalSliderProfileZoom->setValue(ui->horizontalSliderProfileZoom->value() + numDegrees.y() / 15);
    }
  }

  // Consume all events
  return true;
}

bool ProfileScrollArea::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);

  horizScrollBarsChanged();
  vertScrollBarsChanged();

  // Event not consumed
  return false;
}

void ProfileScrollArea::scaleView(QScrollBar *scrollBar)
{
  QSize size(atools::roundToInt(scaleFactorHoriz *scrollArea->viewport()->width()),
             atools::roundToInt(scaleFactorVert *scrollArea->viewport()->height()));

  changingScrollBars = true;
  profileWidget->resize(size);
  changingScrollBars = false;

  int min = scrollBar->minimum();
  int max = scrollBar->maximum();

  // Keep scroll bar positions
  if(scrollBar->orientation() == Qt::Vertical)
    scrollBar->setValue(atools::roundToInt(lastVertScrollPos * (max - min)));
  else if(scrollBar->orientation() == Qt::Horizontal)
    scrollBar->setValue(atools::roundToInt(lastHorizScrollPos * (max - min)));
}

void ProfileScrollArea::horizontalZoomSliderValueChanged(int value)
{
  // 1 = out, 40 = in
  scaleFactorHoriz = value;
  scaleView(hScrollBar);
}

void ProfileScrollArea::verticalZoomSliderValueChanged(int value)
{
  // 1 = out, 40 = in
  scaleFactorVert = value;
  scaleView(vScrollBar);
}

bool ProfileScrollArea::isFill() const
{
  return NavApp::getMainUi()->pushButtonProfileFit->isChecked();
}

QPoint ProfileScrollArea::getOffset() const
{
  return scrollArea->widget()->pos() * -1;
}

void ProfileScrollArea::showZoom(bool show)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(show)
    // Use arbitrary large value for the drawing part - the widget will redistribute it
    ui->splitterProfile->setSizes({10000, ui->gridLayoutProfileButtons->minimumSize().width()});
  else
    ui->splitterProfile->setSizes({10000, 0});
}

void ProfileScrollArea::showScrollbars(bool show)
{
  scrollArea->setVerticalScrollBarPolicy(show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
  scrollArea->setHorizontalScrollBarPolicy(show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
}

void ProfileScrollArea::showLabels(bool show)
{
  labelWidget->setVisible(show);
}

void ProfileScrollArea::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::PROFILE_WINDOW_OPTIONS).save({
    ui->splitterProfile,
    ui->actionProfileFit,
    ui->actionProfileCenterAircraft,
    ui->actionProfileFollow,
    ui->actionProfileShowLabels,
    ui->actionProfileShowScrollbars,
    ui->actionProfileShowZoom
  });

}

void ProfileScrollArea::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::PROFILE_WINDOW_OPTIONS).restore({
    ui->splitterProfile,
    ui->actionProfileFit,
    ui->actionProfileCenterAircraft,
    ui->actionProfileFollow,
    ui->actionProfileShowLabels,
    ui->actionProfileShowScrollbars,
    ui->actionProfileShowZoom
  });
  fillToggled(ui->actionProfileFit->isChecked());
}

void ProfileScrollArea::centerAircraft(const QPoint& screenPoint)
{
  // Use rectangle on the left side of the view
  int xmarginLeft = viewport->width() / 10;
  int ymarginTop = viewport->height() / 4;
  int xmarginRight = viewport->width() * 2 / 3;
  int ymarginBottom = viewport->height() / 4;
  int x = screenPoint.x();
  int y = screenPoint.y();

  if(x - xmarginLeft < hScrollBar->value() || x > hScrollBar->value() + viewport->width() - xmarginRight)
    hScrollBar->setValue(std::min(std::max(hScrollBar->minimum(), x - xmarginLeft), hScrollBar->maximum()));

  if(y - ymarginTop < vScrollBar->value() || y > vScrollBar->value() + viewport->height() - ymarginBottom)
    vScrollBar->setValue(std::min(std::max(vScrollBar->minimum(), y - viewport->height() / 2), vScrollBar->maximum()));
}

void ProfileScrollArea::styleChanged()
{
  // Make the elevation profile splitter handle better visible
  NavApp::getMainUi()->splitterProfile->setStyleSheet(
    QString("QSplitter::handle { "
            "background: %1;"
            "image: url(:/littlenavmap/resources/icons/splitterhandhoriz.png); }").
    arg(QApplication::palette().color(QPalette::Window).darker(120).name()));
}
