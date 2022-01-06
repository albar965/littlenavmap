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

#include "profile/profilelabelwidgethoriz.h"

#include "profile/profilewidget.h"
#include "common/mapcolors.h"
#include "profile/profilescrollarea.h"
#include "navapp.h"
#include "route/route.h"
#include "common/unit.h"

#include <QContextMenuEvent>
#include <QPainter>

ProfileLabelWidgetHoriz::ProfileLabelWidgetHoriz(ProfileWidget *parent, ProfileScrollArea *profileScrollArea)
  : QWidget(parent), profileWidget(parent), scrollArea(profileScrollArea)
{
  setContextMenuPolicy(Qt::DefaultContextMenu);
  setFocusPolicy(Qt::StrongFocus);
}

ProfileLabelWidgetHoriz::~ProfileLabelWidgetHoriz()
{

}

void ProfileLabelWidgetHoriz::optionsChanged()
{
  setVisible(OptionData::instance().getDisplayOptionsProfile() & optsd::PROFILE_TOP_ANY);
  update();
}

/* Pass context menu to profile widget */
void ProfileLabelWidgetHoriz::contextMenuEvent(QContextMenuEvent *event)
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

void ProfileLabelWidgetHoriz::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  const QVector<int>& waypointX = profileWidget->getWaypointX();

  // Fill background white
  painter.fillRect(rect(), QApplication::palette().color(QPalette::Base));
  optsd::DisplayOptionsProfile opts = OptionData::instance().getDisplayOptionsProfile();

  if(profileWidget->hasValidRouteForDisplay() && opts & optsd::PROFILE_TOP_ANY)
  {
    setVisible(true);

    // Prepare bold font sligthly smaller ============
    const QString sep = tr(" / ");
    QFont fontBold = QApplication::font();
    fontBold.setPointSizeF(fontBold.pointSizeF() * 0.85);
    fontBold.setBold(true);
    QFontMetrics metricsBold(fontBold);

    // Prepare normal font sligthly smaller ============
    QFont fontNormal = QApplication::font();
    fontNormal.setPointSizeF(fontNormal.pointSizeF() * 0.85);
    fontNormal.setBold(false);
    QFontMetrics metricsNormal(fontNormal);

    QPoint offset = scrollArea->getOffset();
    const Route& route = profileWidget->getRoute();

    int vpWidth = scrollArea->getViewport()->width();

    int lines = 0;

    // Get display options
    bool distOpt = opts.testFlag(optsd::PROFILE_TOP_DISTANCE);
    bool magCrsOpt = opts.testFlag(optsd::PROFILE_TOP_MAG_COURSE);
    bool trueCrsOpt = opts.testFlag(optsd::PROFILE_TOP_TRUE_COURSE);
    bool relatedOpt = opts.testFlag(optsd::PROFILE_TOP_RELATED);

    // Iterate through all waypoints =============================================================
    for(int i = 0; i < waypointX.size(); i++)
    {
      int x = waypointX.value(i);

      int p = x - offset.x();

      // Draw vertical separator line ============================
      painter.setPen(mapcolors::profileWaypointLinePen);
      if(p >= 0 && p <= vpWidth)
        painter.drawLine(x - offset.x() + 1, 0, x - offset.x() + 1, height());

      if(i > 0)
      {
        int x0 = waypointX.value(i - 1);
        int p0 = x0 - offset.x();

        if(!((p < 0 && p0 < 0) || (p > vpWidth && p0 > vpWidth))) // Check if leg is visible on profile
        {
          if(NavApp::isCurrentGuiStyleNight())
            // Use normal text for better visibility
            painter.setPen(QApplication::palette().color(QPalette::Text));
          else
            painter.setPen(mapcolors::profileElevationScalePen);

          const RouteLeg& leg = route.getLegAt(i);
          int legWidth = x - x0;
          int pos = x0 + legWidth / 2;

          // Build up text depending on space ==============================================
          QString text = leg.buildLegText(distOpt, magCrsOpt, trueCrsOpt, false).join(sep);
          if(metricsBold.horizontalAdvance(text) >= legWidth)
            text = leg.buildLegText(distOpt, magCrsOpt, false, false).join(sep);

          if(metricsBold.horizontalAdvance(text) >= legWidth)
            text = leg.buildLegText(distOpt, false, false, false).join(sep);

          if(metricsBold.horizontalAdvance(text) >= legWidth)
            text = Unit::distNm(leg.getDistanceTo(), false, 20, false);

          if(!text.isEmpty())
          {
            int textWidth = metricsBold.horizontalAdvance(text);
            if(textWidth < legWidth)
            {
              painter.setFont(fontBold);
              painter.drawText(pos - offset.x() - textWidth / 2 + 1, metricsBold.ascent(), text);

              // Adjust lines for widget size
              lines = std::max(lines, 1);
            }
          }

          if(relatedOpt)
          {
            // Build second line for related text =====================================================
            QStringList related = proc::procedureLegRecommended(leg.getProcedureLeg());

            if(metricsBold.horizontalAdvance(related.join(sep)) >= legWidth && !related.isEmpty())
              related.removeLast();

            if(metricsBold.horizontalAdvance(related.join(sep)) >= legWidth && !related.isEmpty())
              related.removeLast();

            if(!related.isEmpty())
            {
              painter.setFont(fontNormal);

              QString relTxt = related.join(sep);
              int textWidthRelated = metricsNormal.horizontalAdvance(relTxt);

              if(textWidthRelated < legWidth)
              {
                painter.setFont(fontNormal);
                painter.drawText(pos - offset.x() - textWidthRelated / 2 + 1, metricsNormal.ascent() + metricsNormal.height(), relTxt);

                // Adjust lines for widget size
                lines = std::max(lines, 2);
              }
            }
          }
        }
      }
    }

    // Set widget size depending on content
    setMinimumHeight(metricsBold.height() * lines + 1);
  }
  else
    setMinimumWidth(1); // Setting to 0 hides the widget

  // Dim the whole map for night mode by drawing a half transparent black rectangle
  mapcolors::darkenPainterRect(painter);
}
