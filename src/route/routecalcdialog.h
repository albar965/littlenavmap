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

#ifndef LNM_ROUTECALCWIN_H
#define LNM_ROUTECALCWIN_H

#include <QDialog>
#include <QObject>

class UnitStringTool;
class QAbstractButton;

namespace Ui {
class RouteCalcDialog;
}

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
class RouteCalcDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RouteCalcDialog(QWidget *parent);
  virtual ~RouteCalcDialog() override;

  RouteCalcDialog(const RouteCalcDialog& other) = delete;
  RouteCalcDialog& operator=(const RouteCalcDialog& other) = delete;

  /* Open and set up the dialog for a full flight plan calculation */
  void showForFullCalculation();

  /* Open and set up the dialog for a flight plan calculation between the selected flight plan legs.*/
  void showForSelectionCalculation();

  /* Update dialog contents when flight plan table selection changes */
  void selectionChanged();

  /* Update dialog contents when flight plan changes */
  void routeChanged();

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

  /* Min and max values including for ui->horizontalSliderRouteCalcAirwayPreference.
   * Sync with DIRECT_COST_FACTORS */
  static constexpr int AIRWAY_WAYPOINT_PREF_MIN = 0;
  static constexpr int AIRWAY_WAYPOINT_PREF_MAX = 14;
  static constexpr int AIRWAY_WAYPOINT_PREF_CENTER = (AIRWAY_WAYPOINT_PREF_MAX - AIRWAY_WAYPOINT_PREF_MIN) / 2;

signals:
  /* Use clicked calculate flight plan button */
  void downloadTrackClicked();
  void calculateClicked();
  void calculateDirectClicked();
  void calculateReverseClicked();

private:
  /* Fill header message with departure, destination or error messages. */
  void updateHeader();
  void updatePreferenceLabel();

  /* Adjust flight plan altitude spin box */
  void adjustAltitudePressed();

  void buttonBoxClicked(QAbstractButton *button);

  /* Catch events to allow repositioning */
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;

  /* Range/selection */
  int fromIndex = -1, toIndex = -1;
  bool canCalculateSelection = false;
  UnitStringTool *units = nullptr;

  QList<QObject *> widgets;
  QStringList preferenceTexts;

  Ui::RouteCalcDialog *ui;

  /* Remember dialog position when reopening */
  QPoint position;

  /* Set to true before emitting calculateClicked() to avoid user clicking button twice. */
  bool calculating = false;
};

#endif // LNM_ROUTECALCWIN_H
