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

#ifndef AIRSPACETOOLBARHANDLER_H
#define AIRSPACETOOLBARHANDLER_H

#include "common/mapflags.h"

#include <QCoreApplication>
#include <QVector>
#include <QWidgetAction>

class QAction;
class QActionGroup;
class QToolButton;
class QSlider;
class MainWindow;

namespace asinternal {
class AirspaceAltSliderAction;
class AirspaceLabelAction;
}

/* Manages the airspace toolbar, its tool buttons and all the actions in the drop down menus */
class AirspaceToolBarHandler :
  public QObject
{
  Q_OBJECT

public:
  AirspaceToolBarHandler(MainWindow *parent);
  virtual ~AirspaceToolBarHandler() override;

  void createToolButtons();

  /* Updates all */
  void updateAll();

  const QVector<QToolButton *>& getAirspaceToolButtons() const
  {
    return airspaceToolButtons;
  }

signals:
  void updateAirspaceTypes(map::MapAirspaceFilter filter);

private:
  void createAirspaceToolButton(const QString& icon, const QString& buttonHelp,
                                const std::initializer_list<map::MapAirspaceTypes>& types,
                                const std::initializer_list<map::MapAirspaceFlags>& flags,
                                bool groupActions = false, bool minMaxAltitude = false);

  /* Update button depressed state or not */
  void updateToolButtons();

  /* Check or uncheck menu actions with blocked signal based on NavApp::getShownMapAirspaces() */
  void updateToolActions();

  /* Extract flags from not grouped actions all/none/type and emit updateAirspaceTypes() */
  void actionAllNoneOrTypeTriggered();

  /* Radio group button clicked. emit updateAirspaceTypes() */
  void actionRadioGroupTriggered(QAction *action);

  /* Sent from main airspace button */
  void allAirspacesToggled();

  /* Altitude sliders moved or clicked */
  void altSliderChanged();

  /* Enable or disable sliders based on NavApp::getShownMapAirspaces() */
  void updateSliders();

  /* Update altitude label from slider values */
  void updateSliderLabel();

  /* List of all actions */
  QVector<QAction *> airspaceActions;

  /* List of actions that are not grouped */
  QVector<QToolButton *> airspaceToolButtons;

  /* List of grouped actions or null if not grouped */
  QVector<QActionGroup *> airspaceToolGroups;

  /* Flags and types for each button */
  QVector<map::MapAirspaceFilter> airspaceToolButtonFilters;
  MainWindow *mainWindow;

  /* Widget wrapper allowing to put an arbitrary widget into a menu */
  asinternal::AirspaceAltSliderAction *sliderActionAltMin = nullptr, *sliderActionAltMax = nullptr;
  asinternal::AirspaceLabelAction *labelActionAirspace = nullptr;
};

namespace asinternal {
/*
 * Wraps a slider into an action allowing to add it to a menu.
 * This is a single slider which can be used for minimum and maximum value
 */
class AirspaceAltSliderAction
  : public QWidgetAction
{
  Q_OBJECT

public:
  AirspaceAltSliderAction(QObject *parent, bool maxSliderParam);

  int getAltitudeFt() const;

  /* Adjusts sliders but does not send signals */
  void setAltitudeFt(int altitude);

  /* Minimum step in feet */
  const static int SLIDER_STEP_ALT_FT = 500;

  /* true if used for maximum which means reversed display and values */
  bool isMaxSlider() const
  {
    return maxSlider;
  }

signals:
  void valueChanged(int value);
  void sliderReleased();

protected:
  /* Create and delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* minmum and maximum values in ft */
  int minValue() const;
  int maxValue() const; /* Unlimited */
  void setSliderValue(int value);

  /* List of created/registered slider widgets */
  QVector<QSlider *> sliders;

  /* Altitude in feet / WIND_SLIDER_STEP_ALT_FT */
  int sliderValue = 0;

  /* true if reversed for maximum slider */
  bool maxSlider;
};
}

#endif // AIRSPACETOOLBARHANDLER_H
