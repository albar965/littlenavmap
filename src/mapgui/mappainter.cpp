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

#include "geo/pos.h"

#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

using namespace Marble;

MapPainter::MapPainter(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale)
  : CoordinateConverter(marbleWidget->viewport()), widget(marbleWidget), query(mapQuery), scale(mapScale)
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
