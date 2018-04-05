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

#ifndef LITTLENAVMAP_MAPTOOLTIP_H
#define LITTLENAVMAP_MAPTOOLTIP_H

#include <QColor>
#include <QApplication>

namespace map {
struct MapSearchResult;

}

class MapQuery;
class WeatherReporter;
class Route;
class MainWindow;

namespace atools {
namespace util {
class HtmlBuilder;
}
}

namespace proc {
struct MapProcedurePoint;

}

/*
 * Builds a HTML tooltip for map display with a maximum length of 20 lines.
 */
class MapTooltip
{
  Q_DECLARE_TR_FUNCTIONS(MapTooltip)

public:
  MapTooltip(MainWindow *parentWindow);
  virtual ~MapTooltip();

  /*
   * Build a HTML map tooltip also containing icons. Content is similar to the information panel.
   * @param mapSearchResult filled from get nearest methods. All objects will be added until the
   * maximum number of lines is reached.
   * @param route Needed to access route objects
   * @param airportDiagram set to true if the tooltip should also cover objects that are
   * displayed in airport diagrams.
   * @return HTML code of the tooltip
   */
  QString buildTooltip(const map::MapSearchResult& mapSearchResult,
                       const QList<proc::MapProcedurePoint>& procPoints, const Route& route,
                       bool airportDiagram);

private:
  bool checkText(atools::util::HtmlBuilder& html, int numEntries);

  static Q_DECL_CONSTEXPR int MAX_LINES = 20;
  static Q_DECL_CONSTEXPR int MAX_ENTRIES = 3;

  MainWindow *mainWindow = nullptr;
  MapQuery *mapQuery;
  WeatherReporter *weather;
};

#endif // LITTLENAVMAP_MAPTOOLTIP_H
