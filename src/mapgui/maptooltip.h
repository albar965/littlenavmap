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

#ifndef LITTLENAVMAP_MAPTOOLTIP_H
#define LITTLENAVMAP_MAPTOOLTIP_H

#include <QColor>
#include <QCoreApplication>

namespace map {

class AircraftTrailSegment;
struct MapResult;

}

class WeatherReporter;
class Route;
class MainWindow;
class HtmlInfoBuilder;

namespace atools {
namespace geo {
class Pos;
}
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
  explicit MapTooltip(MainWindow *parentWindow);
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
  QString buildTooltip(const map::MapResult& mapSearchResult, const atools::geo::Pos& pos,
                       const Route& route, bool airportDiagram);

private:
  bool checkText(atools::util::HtmlBuilder& html) const;

  template<typename TYPE>
  void buildOneTooltip(atools::util::HtmlBuilder& html, bool& overflow, int& numEntries, const QList<TYPE>& list,
                       const HtmlInfoBuilder& info, void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&) const) const;
  template<typename TYPE>
  void buildOneTooltipRt(atools::util::HtmlBuilder& html, bool& overflow, bool& distance, int& numEntries, const QList<TYPE>& list,
                         const HtmlInfoBuilder& info, void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&) const) const;

  static Q_DECL_CONSTEXPR int MAX_LINES = 20;

  MainWindow *mainWindow = nullptr;
  WeatherReporter *weather;

};

#endif // LITTLENAVMAP_MAPTOOLTIP_H
