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

#ifndef LNM_MAPAIRPORTHANDLER_H
#define LNM_MAPAIRPORTHANDLER_H

#include "common/mapflags.h"
#include "options/optionchangeflags.h"
#include "options/optionflags.h"

#include <QObject>
#include <QWidgetAction>

class QAction;
class QToolButton;
class QSlider;

class QLabel;
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
  explicit MapAirportHandler(QObject * parent);
  virtual ~MapAirportHandler() override;

  MapAirportHandler(const MapAirportHandler& other) = delete;
  MapAirportHandler& operator=(const MapAirportHandler& other) = delete;

  /* Initialize, create actions and add button to toolbar. */
  void insertToolbarButton();

  /* Save and load flags if requested in options */
  void saveState();
  void restoreState();

  /* Get enabled airport types and filters. One or more flags out of AIRPORT_ALL_AND_ADDON */
  map::MapTypes getAirportTypes() const
  {
    return airportTypes;
  }

  /* Get minimum or maximum runway to show. */
  int getMinimumRunwayFt() const;
  int getMaximumRunwayFt() const;

  /* true if a value is in its default position i.e. not set. */
  int isMinimumRunwaySet() const;
  int isMaximumRunwaySet() const;

  /* Reset map display settings */
  void resetSettingsToDefault();

  /* Update units, labels and more */
  void optionsChanged(const optc::OptionChangeFlags& changeFlags);

  /* Get text like "Runway length between %1 and %2." or "No runway length limit." */
  QString getRunwayText() const;

signals:
  /* Redraw map */
  void updateAirportTypes();

private:
  /* Action from toolbar toggled */
  void toolbarActionTriggered();
  void actionResetTriggered();
  void actionOnlyAddonTriggered();

  /* Set actions (blocking signals) */
  void flagsToActions();
  void actionsToFlags();

  /* Add action to toolbar button */
  QAction *addAction(const QString& icon, const QString& text, const QString& tooltip, const QKeySequence& shortcut);

  void runwaySliderValueChanged();
  void runwaySliderReleased();
  void updateRunwayLabel();
  void updateButtons();

  /* Actions for toolbar button and menu */
  QAction *actionReset = nullptr, *actionAddonOnly = nullptr, *actionHard = nullptr, *actionSoft = nullptr, *actionEmpty = nullptr,
          *actionAddonNone = nullptr, *actionAddonZoom = nullptr, *actionAddonZoomFilter = nullptr,
          *actionUnlighted = nullptr, *actionNoProcedures = nullptr, *actionClosed = nullptr, *actionMil = nullptr, *actionWater = nullptr,
          *actionHelipad = nullptr;
  QList<QAction *> allActions;

  QActionGroup *actionGroupAddon = nullptr;

  /* Widget wrapper allowing to put an arbitrary widget into a menu */
  apinternal::AirportSliderAction *sliderActionRunwayLengthMin = nullptr, *sliderActionRunwayLengthMax = nullptr;
  apinternal::AirportLabelAction *labelActionRunwayLength = nullptr;

  /* Toolbutton getting all actions for dropdown menu */
  QToolButton *toolButton = nullptr;

  /* Selection */
  map::MapTypes airportTypes = map::AIRPORT_DEFAULT;
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
  /* typeMinSlider = true: minimum length, typeMinSlider = false: maximum length */
  explicit AirportSliderAction(QObject *parent, bool typeMinSlider);

  /* Real value or -1 for leftmost in local short distance units like ft or meter / 100. */
  int getSliderValueReal() const
  {
    return sliderValueReal;
  }

  void saveState() const;
  void restoreState();

  void optionsChanged(const optc::OptionChangeFlags& changeFlags);

  /* Set limitation to avoid overlapping ranges between min and max slider */
  void setLimit(int limitReal);

  /* Reset slider and values to default */
  void reset();

  /* minmum and maximum values in local unit (ft or meter) */
  int getMinValueReal() const; /* Unlimited */
  int getMaxValueReal() const;

signals:
  void valueChanged(int value);
  void sliderReleased();

private:
  /* Create and delete widgets for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* Set value - real and not raw slider value */
  void setValuesReal(int realValue);
  void setMinMaxValues();

  void sliderValueChanged(int valueRaw);

  void setMinMaxValue(QSlider *slider);
  void setValueReal(QSlider *slider, int valueReal);

  /* typeMinSlider = true: Raw is 0 to 140 from left to right. Real is 0 to 140 left to right.
   * typeMinSlider = false: Raw is from -140 from right to left. Real is 0 to 140 left to right.*/
  int rawToReal(int rawValue)
  {
    return minimumSlider ? rawValue : -rawValue;
  }

  int realToRaw(int realValue)
  {
    return minimumSlider ? realValue : -realValue;
  }

  /* List of created/registered slider widgets in menu and tear-off */
  QList<QSlider *> sliders;
  opts::UnitShortDist sliderDistUnit = opts::DIST_SHORT_FT;

  // Real values in local units / 100
  int sliderValueReal = 0, sliderLimitMinReal = 0, sliderLimitMaxReal = 0;

  /* Minimum slider if true. Otherwise maximum with shifted values. */
  bool minimumSlider = true;
};

/*
 * Internal Wrapper for label action.
 */
class AirportLabelAction
  : public QWidgetAction
{
  Q_OBJECT

public:
  explicit AirportLabelAction(QObject *parent)
    : QWidgetAction(parent)
  {
  }

  void setText(const QString& textParam);

private:
  /* Create a delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* List of created/registered labels */
  QList<QLabel *> labels;
  QString text;
};

}

#endif // LNM_MAPAIRPORTHANDLER_H
