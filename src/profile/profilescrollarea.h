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

#ifndef LNM_PROFILESCROLLAREA_H
#define LNM_PROFILESCROLLAREA_H

#include <QDateTime>
#include <QObject>
#include <QPoint>

class ProfileLabelWidgetVert;
class ProfileLabelWidgetHoriz;
class ProfileWidget;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QScrollArea;
class QScrollBar;
class QWheelEvent;
class QLabel;

namespace atools {
namespace geo {
class Pos;
}
}

/*
 * Takes care of all scrolling operations for the elevation profile. Handles scroll bars, zoom sliders and
 * installs event handlers to catch key and mouse events for scrolling, dragging and zoomin using key, mouse and wheel.
 */
class ProfileScrollArea :
  public QObject
{
  Q_OBJECT

public:
  explicit ProfileScrollArea(ProfileWidget *parent, QScrollArea *scrollAreaParam);
  virtual ~ProfileScrollArea() override;

  ProfileScrollArea(const ProfileScrollArea& other) = delete;
  ProfileScrollArea& operator=(const ProfileScrollArea& other) = delete;

  /* Top left corner of the viewport inside the widget */
  QPoint getOffset() const;

  /* Update scroll area and label widget */
  void update();

  /* Update label widget */
  void updateLabelWidgets();

  void saveState();
  void restoreState();

  /* Check if position is outside margins and position it into left third of the profile if yes */
  void centerAircraft(const QPoint& aircraftScreenPoint, float verticalSpeed, bool force);

  /* Adjust zoom to have aircraft and destination available */
  void centerRect(const QPoint& leftScreenPoint, const QPoint& rightScreenPoint, bool zoomVertically, bool force);

  bool isPointVisible(const QPoint& point);

  /* Update split on style change */
  void styleChanged();

  void optionsChanged();

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

  ProfileLabelWidgetVert *getLabelWidgetVert() const
  {
    return profileLabelWidgetVert;
  }

  ProfileLabelWidgetHoriz *getLabelWidgetHoriz() const
  {
    return profileLabelWidgetHoriz;
  }

  /* Update zoom slider and labels */
  void routeAltitudeChanged();

  /* Expand button clicked */
  void expandWidget();

  /* Profile widget not scaled left area */
  void setProfileLeftOffset(int value)
  {
    profileLeftOffset = value;
  }

  /* Profile widget not scaled top area */
  void setProfileTopOffset(int value)
  {
    profileTopOffset = value;
  }

  /* Show or hide tooltip. Windows will be positioned on the left or right side depending on global mouse position */
  void showTooltip(const QPoint& globalPos, const QString& text);
  void hideTooltip();
  bool isTooltipVisible() const;

  /* Bring splitter to a resonable size after first start */
  void restoreSplitter();

signals:
  /* Show flight plan waypoint or user position on map. x is widget position. */
  void showPosAlongFlightplan(int x, bool doubleClick);
  void hideRubberBand();
  void jumpBackToAircraftStart();
  void jumpBackToAircraftCancel();

private:
  /* Horizontal or vertical scroll bar of view has changed value or range */
  void vertScrollBarChanged();
  void horizScrollBarChanged();

  /* Show scrollbars on scroll area */
  void showScrollbarsToggled(bool show);

  /* Show right side of split window from action in menu */
  void showZoomToggled(bool show);

  /* Help push button clicked */
  void helpClicked();

  /* Update menu item from splitter position */
  void splitterMoved(int pos, int index);

  /* Event filter on scroll area and viewport -passes events to methods below */
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  /* All event methods return true if event should not be propagated */
  bool wheelEvent(QWheelEvent *event);
  bool mousePressEvent(QMouseEvent *event);
  bool mouseReleaseEvent(QMouseEvent *event);
  bool keyEvent(QKeyEvent *event);
  bool mouseMoveEvent(QMouseEvent *event);
  bool mouseDoubleClickEvent(QMouseEvent *event);
  bool resizeEvent();

  /* Scroll bar changed - resize widget and adapt scroll bar position */
  void scaleView(QScrollBar *scrollBar);
  void scaleViewAll();

  /* Enable and disable actions */
  void updateWidgets();

  /* Scale view after zooming */
  void horizontalZoomSliderValueChanged(int value);
  void verticalZoomSliderValueChanged(int value);

  /* Calculate scroll position center (0.0 to 1.0) value to scroll bar value */
  int toScrollBarValue(const QScrollBar *scrollBar, double scrollPos) const;

  /* Calculate scroll position center (0.0 to 1.0) value from scroll bar */
  double toScrollPos(const QScrollBar *scrollBar);

  void setMaxHorizZoom();
  void setMaxVertZoom();

  void horizScrollBarValueChanged();
  void vertScrollBarValueChanged();

#ifdef DEBUG_INFORMATION
  void debugPrintValues();

#endif

  /* Sum up mouse wheel or trackpad movement before zooming */
  int lastWheelAngle = 0;

  /* Scaling factor for widget - default is minimum as set in ui file */
  int horizScaleFactor = 1;
  int vertScaleFactor = 1;
  float maxWindowAlt = 1.f;

  ProfileWidget *profileWidget;
  QScrollBar *horizScrollBar, *vertScrollBar;
  QScrollArea *scrollArea;
  QWidget *viewport;
  ProfileLabelWidgetVert *profileLabelWidgetVert = nullptr;
  ProfileLabelWidgetHoriz *profileLabelWidgetHoriz = nullptr;

  /* Do not zoom to aircraft and destination too often */
  QDateTime lastCenterAircraftAndDest;

  /* Frameless label widget  */
  QLabel *tooltipLabel = nullptr;

  /* Mouse dragging position on button down */
  QPoint startDragPos;

  /* Remember old values so, that the scroll bar can be adjusted to remain in position.
   *  These are the center positions of the scroll bar  */
  double lastVertScrollPos = 0.5; /* Default center */
  double lastHorizScrollPos = 0.5; /* Default center */

  /* Disable changing the last scroll bar position above when resizing the widget */
  bool noLastScrollPosUpdate = false;

  /* Not scaled areas of the profile to allow correction when zooming */
  int profileLeftOffset = 0, profileTopOffset = 0;

  /* Remember calculated scroll position to compare to resulting scroll bar value.
   * Needed to correct if scrolling is limited at the boundaries. */
  int calculatedHorizScrollPos = 0;
  int calculatedVertScrollPos = 0;

  /* Omit notifications from widgets (jumping back, ...) when updating position */
  bool changingView = false;
};

#endif // LNM_PROFILESCROLLAREA_H
