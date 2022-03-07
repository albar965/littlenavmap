/*****************************************************************************
* Copyright 2015-2021 Alexander Barthel alex@littlenavmap.org
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

#include <QObject>

class QString;
class Route;

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
  void updateWindowLabel();

  /* Build header text formatted for printing */
  void buildPrintText(atools::util::HtmlBuilder& html, bool titleOnly);

  /* Build header text formatted for HTML export */
  void buildHtmlText(atools::util::HtmlBuilder& html);

  /* Save and load configuration */
  void saveState();
  void restoreState();

  bool isHeaderAirports() const
  {
    return headerAirports;
  }

  void setHeaderAirports(bool value)
  {
    headerAirports = value;
  }

  bool isHeaderDeparture() const
  {
    return headerDeparture;
  }

  void setHeaderDeparture(bool value)
  {
    headerDeparture = value;
  }

  bool isHeaderArrival() const
  {
    return headerArrival;
  }

  void setHeaderArrival(bool value)
  {
    headerArrival = value;
  }

  bool isHeaderRunwayTakeoff() const
  {
    return headerRunwayTakeoff;
  }

  void setHeaderRunwayTakeoff(bool value)
  {
    headerRunwayTakeoff = value;
  }

  bool isHeaderRunwayLand() const
  {
    return headerRunwayLand;
  }

  void setHeaderRunwayLand(bool value)
  {
    headerRunwayLand = value;
  }

  bool isHeaderDistTime() const
  {
    return headerDistTime;
  }

  void setHeaderDistTime(bool value)
  {
    headerDistTime = value;
  }

signals:
  /* Departure or destination link in the header clicked */
  void flightplanLabelLinkActivated(const QString& link);

private:
  void buildHeaderAirports(atools::util::HtmlBuilder& html, bool widget);
  void buildHeaderDepart(atools::util::HtmlBuilder& html, bool widget);
  void buildHeaderArrival(atools::util::HtmlBuilder& html, bool widget);
  void buildHeaderRunwayTakeoff(atools::util::HtmlBuilder& html);
  void buildHeaderRunwayLand(atools::util::HtmlBuilder& html);
  void buildHeaderTocTod(atools::util::HtmlBuilder& html); /* Only for print and HTML export */
  void buildHeaderDistTime(atools::util::HtmlBuilder& html);

  bool headerAirports = true, headerDeparture = true, headerArrival = true, headerRunwayTakeoff = true, headerRunwayLand = true,
       headerDistTime = true;

  const Route& route;
};

#endif // LNM_ROUTELABEL_H
