/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "mappainter.h"
#include "mapscale.h"
#include "common/mapcolors.h"
#include "geo/pos.h"

#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

using namespace Marble;

MapPainter::MapPainter(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale,
                       bool verboseMsg)
  : CoordinateConverter(marbleWidget->viewport()), widget(marbleWidget), query(mapQuery), scale(mapScale),
    verbose(verboseMsg)
{
}

MapPainter::~MapPainter()
{
}

void MapPainter::setRenderHints(GeoPainter *painter)
{
  if(widget->viewContext() == Marble::Still)
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
  }
  else if(widget->viewContext() == Marble::Animation)
  {
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::TextAntialiasing, false);
  }
}

void MapPainter::textBox(GeoPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
                         bool bold, bool italic, bool alignRight, int transparency)
{
  if(texts.isEmpty())
    return;

  painter->save();

  if(transparency != 255)
  {
    if(transparency == 0)
      painter->setBrush(Qt::NoBrush);
    else
    {
      QColor col(mapcolors::textBoxColor);
      col.setAlpha(transparency);
      painter->setBrush(col);
    }
  }
  else
    painter->setBrush(mapcolors::textBoxColor);

  QFontMetrics metrics = painter->fontMetrics();
  int h = metrics.height();

  int yoffset = 0;
  if(transparency != 0)
  {
    painter->setPen(mapcolors::textBackgroundPen);
    for(const QString& t : texts)
    {
      int w = metrics.width(t);
      painter->drawRoundedRect(x - 2, y - h + metrics.descent() + yoffset, w + 4, h, 5, 5);
      yoffset += h;
    }
  }

  if(bold || italic)
  {
    QFont f = painter->font();
    f.setBold(bold);
    f.setItalic(italic);
    painter->setFont(f);
  }

  yoffset = 0;
  painter->setPen(textPen);
  for(const QString& t : texts)
  {
    int newx = x;
    if(alignRight)
      newx -= metrics.width(t);

    painter->drawText(newx, y + yoffset, t);
    yoffset += h;
  }
  painter->restore();
}
