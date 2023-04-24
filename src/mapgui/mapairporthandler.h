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

#ifndef LNM_MAPAIRPORTHANDLER_H
#define LNM_MAPAIRPORTHANDLER_H

#include "common/mapflags.h"
#include "options/optiondata.h"

#include <QObject>
#include <QWidgetAction>

class QAction;
class QToolButton;
class QSlider;

namespace apinternal {
class AirportSliderAction;
class AirportLabelAction;
}

/*
 * Adds a toolbutton with more actions, a slider and a label to configure airport display on the map.
 * Saves and loads airport visibility flags.
 */
struct MapAirportHandler :
  public QObject
{
  Q_OBJECT

public:
  explicit MapAirportHandler(QWidget *parent);
  virtual ~MapAirportHandler() override;

  MapAirportHandler(const MapAirportHandler& other) = delete;
  MapAirportHandler& operator=(const MapAirportHandler& other) = delete;

  /* Initialize, create actions and add button to toolbar. */
  void addToolbarButton();

  /* Save and load flags if requested in options */
  void saveState();
  void restoreState();

  /* Get enabled airport types and filters. One or more flags out of AIRPORT_ALL_AND_ADDON */
  map::MapTypes getAirportTypes() const
  {
    return airportTypes;
  }

  /* Get minimum runway to show. -1 if not applicable or not set */
  int getMinimumRunwayFt() const;

  /* Reset map display settings */
  void resetSettingsToDefault();

  /* Update units, labels and more */
  void optionsChanged();

signals:
  /* Redraw map */
  void updateAirportTypes();

private:
  /* Action from toolbar toggled */
  void toolbarActionTriggered();
  void actionResetTriggered();

  /* Set actions (blocking signals) */
  void flagsToActions();
  void actionsToFlags();

  /* Add action to toolbar button */
  QAction *addAction(const QString& icon, const QString& text, const QString& tooltip, const QKeySequence& shortcut);

  void runwaySliderValueChanged();
  void runwaySliderReleased();
  void updateRunwayLabel();
  void updateToolbutton();

  /* Actions for toolbar button and menu */
  QAction *actionReset = nullptr, *actionHard = nullptr, *actionSoft = nullptr, *actionEmpty = nullptr, *actionAddon = nullptr,
          *actionUnlighted = nullptr, *actionNoProcedures = nullptr, *actionClosed = nullptr, *actionMil = nullptr, *actionWater = nullptr,
          *actionHelipad = nullptr;

  /* Widget wrapper allowing to put an arbitrary widget into a menu */
  apinternal::AirportSliderAction *sliderActionRunwayLength = nullptr;
  apinternal::AirportLabelAction *labelActionRunwayLength = nullptr;

  /* Toolbutton getting all actions for dropdown menu */
  QToolButton *toolButton = nullptr;

  /* Selection */
  map::MapTypes airportTypes = map::AIRPORT_ALL_AND_ADDON;
};

namespace apinternal {
/*
 * Wraps a slider into an action allowing to add it to a menu.
 */
class AirportSliderAction
  : public QWidgetAction
{
  Q_OBJECT

public:
  explicit AirportSliderAction(QObject *parent);

  /* value or -1 for leftmost in local units */
  int getSliderValue() const;

  void saveState();
  void restoreState();

  void optionsChanged();

  void setValue(int value);
  void reset();

signals:
  void valueChanged(int value);
  void sliderReleased();

protected:
  /* Create and delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  void sliderValueChanged(int value);

  /* minmum and maximum values in local unit (ft or meter) */
  int minValue() const; /* Unlimited */
  int maxValue() const;

  /* List of created/registered slider widgets */
  QVector<QSlider *> sliders;
  int sliderValue = 0;
  opts::UnitShortDist sliderDistUnit = opts::DIST_SHORT_FT;
};
}

#endif // LNM_MAPAIRPORTHANDLER_H
