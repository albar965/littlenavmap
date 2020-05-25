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

#ifndef LNM_ROUTECALCWIN_H
#define LNM_ROUTECALCWIN_H

#include <QObject>

class UnitStringTool;
class QAbstractButton;

namespace rd {

/* Routing type as selected in the radio buttons in the window. */
enum RoutingType
{
  AIRWAY,
  RADIONNAV
};

/* Airway types as selected in the radio buttons in the window. */
enum AirwayRoutingType
{
  BOTH,
  VICTOR,
  JET
};

}

/*
 * Manages the flight plan calculation dock window and its widgets.
 */
class RouteCalcWindow :
  public QObject
{
  Q_OBJECT

public:
  explicit RouteCalcWindow(QWidget *parent);
  virtual ~RouteCalcWindow();

  /* Open and set up the dialog for a full flight plan calculation */
  void showForFullCalculation();

  /* Open and set up the dialog for a flight plan calculation between the selected flight plan legs.*/
  void showForSelectionCalculation(const QList<int>& selectedRows, bool canCalc);

  /* Update dialog contents when flight plan table selection changes */
  void selectionChanged(const QList<int>& selectedRows, bool canCalc);

  /* Get and set cruise altitude in spin box widget */
  float getCruisingAltitudeFt() const;
  void setCruisingAltitudeFt(float altitudeFeet);

  /* Update messages if route has changed. */
  void updateWidgets();

  /* Load and save widget status */
  void restoreState();
  void saveState();

  /* Clear routing network cache and disconnect all queries */
  void preDatabaseLoad();
  void postDatabaseLoad();

  void optionsChanged();

  /* Airway or radionav */
  rd::RoutingType getRoutingType() const;

  /* VIctor, Jet or both */
  rd::AirwayRoutingType getAirwayRoutingType() const;

  /* true if RNAV airways should be avoided */
  bool isAirwayNoRnav() const;

  /* 0= waypoints only, 10 = airways only */
  int getAirwayWaypointPreference() const;

  /* true if NDB should be included in radionav calculation */
  bool isRadionavNdb() const;

  /* Use tracks (NAT, PACOTS and AUSOTS) in airway calculation */
  bool isUseTracks() const;

  /* Full route or selection. Status of combo box in window. */
  bool isCalculateSelection() const;

  /* From flight plan index for calculation. -1 if not valid. */
  int getRouteRangeFromIndex() const
  {
    return fromIndex;
  }

  /* To flight plan index for calculation. -1 if not valid. */
  int getRouteRangeToIndex() const
  {
    return toIndex;
  }

  float getAirwayPreferenceCostFactor() const;

  static constexpr int AIRWAY_WAYPOINT_PREF_MIN = 0;
  static constexpr int AIRWAY_WAYPOINT_PREF_MAX = 10;

signals:
  /* Use clicked calculate flight plan button */
  void calculateClicked();
  void calculateDirectClicked();
  void calculateReverseClicked();

private:
  /* Fill header message with departure, destination or error messages. */
  void updateHeader();
  void updatePreferenceLabel();

  /* Adjust flight plan altitude spin box */
  void adjustAltitudePressed();

  void helpClicked();

  void dockVisibilityChanged(bool visible);

  /* Range/selection */
  int fromIndex = -1, toIndex = -1;
  bool canCalculateSelection = false;
  UnitStringTool *units = nullptr;

  QList<QObject *> widgets;
  QStringList preferenceTexts;
};

#endif // LNM_ROUTECALCWIN_H
