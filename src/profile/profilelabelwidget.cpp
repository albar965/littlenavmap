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

#include "profile/profilelabelwidget.h"

#include "profile/profilewidget.h"
#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "profile/profilescrollarea.h"
#include "navapp.h"
#include "common/unit.h"

#include <QContextMenuEvent>
#include <QPainter>

ProfileLabelWidget::ProfileLabelWidget(ProfileWidget *parent, ProfileScrollArea *profileScrollArea)
  : QWidget(parent), profileWidget(parent), scrollArea(profileScrollArea)
{
  setContextMenuPolicy(Qt::DefaultContextMenu);
  setFocusPolicy(Qt::StrongFocus);
}

ProfileLabelWidget::~ProfileLabelWidget()
{

}

/* Pass context menu to profile widget */
void ProfileLabelWidget::contextMenuEvent(QContextMenuEvent *event)
{
  qDebug() << Q_FUNC_INFO;

  QPoint globalpoint;
  if(event->reason() == QContextMenuEvent::Keyboard)
    // Event does not contain position is triggered by keyboard
    globalpoint = QCursor::pos();
  else
    globalpoint = event->globalPos();

  profileWidget->showContextMenu(globalpoint);
}

void ProfileLabelWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  // qDebug() << Q_FUNC_INFO;
  int w = rect().width(), h = rect().height();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Fill background white
  painter.fillRect(rect(), QApplication::palette().color(QPalette::Base));

  if(profileWidget->hasValidRouteForDisplay(NavApp::getRoute()))
  {
    // Calculate coordinates for local system from scroll widget
    QPoint offset = scrollArea->getOffset();
    int safeAltY = profileWidget->getMinSafeAltitudeY() - offset.y();
    int flightplanY = profileWidget->getFlightplanAltY() - offset.y();

    // Draw labels on left side widget ========================================================

    // Draw altitude labels ================================
    SymbolPainter symPainter;
    for(const std::pair<int, int>& scale : profileWidget->calcScaleValues())
    {
      int y = scale.first - offset.y();
      if(y > -5 && y < h + 5)
        symPainter.textBox(&painter, {QString::number(scale.second, 'f', 0)},
                           mapcolors::profileElevationScalePen,
                           w - 2, y, textatt::BOLD | textatt::RIGHT, 0);
    }

    // Draw text labels ========================================================
    // Safe altitude label
    if(safeAltY > -5 && safeAltY < h + 5)
      symPainter.textBox(&painter, {Unit::altFeet(profileWidget->getMinSafeAltitudeFt())},
                         mapcolors::profileSafeAltLinePen,
                         w - 2, safeAltY, textatt::BOLD | textatt::RIGHT, 255,
                         QApplication::palette().color(QPalette::Base));

    if(flightplanY > -5 && flightplanY < h + 5)
    {
      // Route cruise altitude
      float routeAlt = NavApp::getRoute().getCruisingAltitudeFeet();
      symPainter.textBox(&painter, {Unit::altFeet(routeAlt)},
                         QApplication::palette().color(QPalette::Text),
                         w - 2, flightplanY, textatt::BOLD | textatt::RIGHT, 255,
                         QApplication::palette().color(QPalette::Base));
    }
  }

  // Dim the whole map for night mode by drawing a half transparent black rectangle
  mapcolors::darkenPainterRect(painter);
}
