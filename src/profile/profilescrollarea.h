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

#ifndef LNM_PROFILESCROLLAREA_H
#define LNM_PROFILESCROLLAREA_H

#include <QObject>
#include <QPoint>

class QResizeEvent;
class QWheelEvent;
class QMouseEvent;
class QKeyEvent;
class QScrollArea;
class QScrollBar;
class ProfileWidget;
class ProfileLabelWidget;

namespace atools {
namespace geo {
class Pos;
}
}

/*
 * Takes care of all scrolling operations for the elevation profile. Handles scroll bars, zoom sliders and
 * installs event handlers to catch key and mouse events.
 */
class ProfileScrollArea :
  public QObject
{
  Q_OBJECT

public:
  explicit ProfileScrollArea(ProfileWidget *parent, QScrollArea *scrollAreaParam);
  virtual ~ProfileScrollArea() override;

  /* Top left corner of the viewport inside the widget */
  QPoint getOffset() const;

  /* Update scroll area and label widget */
  void update();

  /* Update label widget */
  void updateLabelWidget();

  void saveState();
  void restoreState();

  /* Check if position is outside margins and position it into left third of the profile if yes */
  void  centerAircraft(const QPoint& screenPoint);

  /* Update split on style change */
  void styleChanged();

  /* Update actions */
  void routeChanged(bool geometryChanged);

  QWidget *getViewport() const
  {
    return viewport;
  }

  QScrollArea *getScrollArea() const
  {
    return scrollArea;
  }

  ProfileLabelWidget *getLabelWidget() const
  {
    return labelWidget;
  }

signals:
  /* Show flight plan waypoint or user position on map. x is widget position. */
  void showPosAlongFlightplan(int x, bool doubleClick);

private:
  /* Show scrollbars on scroll area */
  void showScrollbars(bool show);

  /* Show label widget on the left side */
  void showLabels(bool show);

  /* Show right side of split window from action in menu */
  void showZoom(bool show);

  /* Viewport rectangle inside the widget with offset added */
  QRect getViewPortRect() const;

  /* Is fill mode enabled where zooming and scrolling is disabled */
  bool isFill() const;
  void fillToggled(bool checked);

  /* Expand button clicked */
  void expandClicked();

  /* Event filter on scroll area and viewport -passes events to methods below */
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  /* All event methods return true if event should not be propagated */
  bool wheelEvent(QWheelEvent *event);
  bool mousePressEvent(QMouseEvent *event);
  bool mouseReleaseEvent(QMouseEvent *event);
  bool keyEvent(QKeyEvent *event);
  bool mouseMoveEvent(QMouseEvent *event);
  bool mouseDoubleClickEvent(QMouseEvent *event);
  bool resizeEvent(QResizeEvent *event);

  /* Scroll bar changed - resize widget and adapt scroll bar position */
  void scaleView(QScrollBar *scrollBar);
  void horizontalSliderValueChanged(int value);
  void verticalSliderValueChanged(int value);

  /* Scaling factor for widget */
  float scaleFactorHoriz = 1.f;
  float scaleFactorVert = 1.f;

  ProfileWidget *profileWidget;
  QScrollBar *hScrollBar, *vScrollBar;
  QScrollArea *scrollArea;
  QWidget *viewport;
  ProfileLabelWidget *labelWidget = nullptr;

  /* Mouse draggin position on button down */
  QPoint startDragPos;

};

#endif // LNM_PROFILESCROLLAREA_H
