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

  /* Enable first line. Example: "Ronchi Dei Legionari (LIPQ) 20 to Almeria (LEAM)" */
  void setHeaderAirports(bool value)
  {
    headerAirports = value;
  }

  /* Enable second line for departure and arrival procedures.
   * Example: "Depart runway 09 using SID CHI8B. Arrive using STAR DOSE2R (07) via ELVEX and RNAV-ZAM07W (R07-Z) at runway 07. " */
  void setHeaderDepartDest(bool value)
  {
    headerDepartDest = value;
  }

  /* Enable runway information. Example: "Takeoff from 09, 86°M, 90°T, 9.810 ft. Land at 07, 73°M, 9.982 ft, PAPI4." */
  void setHeaderRunway(bool value)
  {
    headerRunway = value;
  }

  /* Distance and time information like "Distance 965 nm, time 2 h 37 m." */
  void setHeaderDistTime(bool value)
  {
    headerDistTime = value;
  }

  bool getHeaderAirports() const
  {
    return headerAirports;
  }

  bool getHeaderDepartDest() const
  {
    return headerDepartDest;
  }

  bool getHeaderRunway() const
  {
    return headerRunway;
  }

  bool getHeaderDistTime() const
  {
    return headerDistTime;
  }

signals:
  /* Departure or destination link in the header clicked */
  void flightplanLabelLinkActivated(const QString& link);

private:
  void buildHeaderAirports(atools::util::HtmlBuilder& html, bool widget);
  void buildHeaderDepartArrival(atools::util::HtmlBuilder& html, bool widget);
  void buildHeaderRunway(atools::util::HtmlBuilder& html);
  void buildHeaderTocTod(atools::util::HtmlBuilder& html); /* Only for print and HTML export */
  void buildHeaderDistTime(atools::util::HtmlBuilder& html);

  bool headerAirports = true, headerDepartDest = true, headerRunway = true, headerDistTime = true;

  const Route& route;
};

#endif // LNM_ROUTELABEL_H
