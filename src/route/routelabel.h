/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_ROUTELABEL_H
#define LNM_ROUTELABEL_H

#include "route/routelabelflags.h"

#include <QObject>

class QString;
class Route;
class RouteLeg;

namespace map {
struct MapRunwayEnd;
struct MapRunway;
}
namespace atools {
namespace util {
class HtmlBuilder;
}
}

/*
 * Generates the flight plan table header text and provides methods to get header text for printing and HTML export.
 */
class RouteLabel
  : public QObject
{
  Q_OBJECT

public:
  explicit RouteLabel(QWidget *parent, const Route& routeParam);
  virtual ~RouteLabel() override;

  /* Update the header label according to current configuration options */
  void updateHeaderLabel();

  /* Update the footer showing distance, time and fuel for selected flight plan legs */
  void updateFooterSelectionLabel();

  /* Update red messages at bottom of route dock window and profile dock window if altitude calculation has errors*/
  void updateFooterErrorLabel();

  /* Build header text formatted for printing */
  void buildPrintText(atools::util::HtmlBuilder& html, bool titleOnly);

  /* Build header text formatted for HTML export */
  void buildHtmlText(atools::util::HtmlBuilder& html);

  /* Save and load configuration */
  void saveState() const;
  void restoreState();

  void styleChanged();

  void optionsChanged();

  /* Set and read display configuration */
  void setFlag(routelabel::LabelFlag flag, bool on = true)
  {
    flags.setFlag(flag, on);
  }

  bool isFlag(routelabel::LabelFlag flag) const
  {
    return flags.testFlag(flag);
  }

signals:
  /* Departure or destination link in the header clicked */
  void flightplanLabelLinkActivated(const QString& link);

private:
  void buildHeaderAirports(atools::util::HtmlBuilder& html, bool widget);
  void buildHeaderDistTime(atools::util::HtmlBuilder& html, bool widget);

  void buildHeaderRunwayTakeoff(atools::util::HtmlBuilder& html, const map::MapRunway& runway, const map::MapRunwayEnd& runwayEnd);
  void buildHeaderRunwayTakeoffWind(atools::util::HtmlBuilder& html, const map::MapRunwayEnd& runwayEnd);
  void buildHeaderDepart(atools::util::HtmlBuilder& html, bool widget);

  void buildHeaderArrival(atools::util::HtmlBuilder& html, bool widget);

  void buildHeaderRunwayLand(atools::util::HtmlBuilder& html, const map::MapRunway& runway, const map::MapRunwayEnd& runwayEnd);
  void buildHeaderRunwayLandWind(atools::util::HtmlBuilder& html, const map::MapRunwayEnd& runwayEnd);

  void buildHeaderRunwayWind(atools::util::HtmlBuilder& html, const map::MapRunwayEnd& runwayEnd, const RouteLeg& leg);

  void buildHeaderTocTod(atools::util::HtmlBuilder& html); /* Only for print and HTML export */
  void buildErrorLabel(QString& toolTipText, QStringList errors, const QString& header);

  void updateAll();
  void updateFont();

  void fetchTakeoffRunway(map::MapRunway& runway, map::MapRunwayEnd& runwayEnd);
  void fetchLandingRunway(map::MapRunway& runway, map::MapRunwayEnd& runwayEnd);

  const Route& route;
  routelabel::LabelFlags flags = routelabel::LABEL_ALL;
};

#endif // LNM_ROUTELABEL_H
