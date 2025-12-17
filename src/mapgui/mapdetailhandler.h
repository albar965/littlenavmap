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

#ifndef LNM_MAPDETAILHANDLER_H
#define LNM_MAPDETAILHANDLER_H

#include <QObject>
#include <QWidgetAction>

class QAction;
class QToolButton;
class QSlider;

namespace mdinternal {
class DetailSliderAction;
class DetailLabelAction;
}

/*
 * Adds a toolbutton with a reset action, a slider and a label to change the map detail level.
 * Saves and loads detail level. Updates menu actions.
 */
class MapDetailHandler :
  public QObject
{
  Q_OBJECT

public:
  explicit MapDetailHandler(QWidget *parent);
  virtual ~MapDetailHandler() override;

  MapDetailHandler(const MapDetailHandler& other) = delete;
  MapDetailHandler& operator=(const MapDetailHandler& other) = delete;

  /* Initialize, create actions and add button to toolbar. */
  void insertToolbarButton();

  /* Save and load flags if requested in options */
  void saveState() const;
  void restoreState();

  /* Increase or decrease map detail resulting in more and bigger symbols
   * emits updateDetailLevel() */
  void increaseMapDetail();
  void decreaseMapDetail();

  /* Increase or decrease number of labels
   * emits updateDetailLevel() */
  void increaseMapDetailText();
  void decreaseMapDetailText();

  /* Reset detail level and label density to MAP_DEFAULT_DETAIL_LEVEL 0.
   * emits updateDetailLevel() */
  void defaultMapDetail();

  /* Detail level in range MAP_MIN_DETAIL_LEVEL(_TEXT) -> MAP_DEFAULT_DETAIL_LEVEL -> MAP_MAX_DETAIL_LEVEL(_TEXT) */
  int getDetailLevel() const;
  int getDetailLevelText() const;

signals:
  /* Redraw map on slider change or reset. */
  void updateDetailLevel(int level, int levelText);

private:
  void setDetailLevel(int level);
  void setDetailLevelText(int level);

  void detailSliderChanged();
  void updateActions();

  /* Actions for toolbar button and menu */
  QAction *actionReset = nullptr;

  /* Widget wrapper allowing to put an arbitrary widget into a menu */
  mdinternal::DetailSliderAction *sliderActionDetailLevel = nullptr;
  mdinternal::DetailLabelAction *labelActionDetailLevel = nullptr;

  mdinternal::DetailSliderAction *sliderActionDetailLevelText = nullptr;
  mdinternal::DetailLabelAction *labelActionDetailLevelText = nullptr;

  /* Toolbutton getting all actions for dropdown menu */
  QToolButton *toolButton = nullptr;
};

namespace mdinternal {
/*
 * Wraps a slider into an action allowing to add it to a menu.
 */
class DetailSliderAction
  : public QWidgetAction
{
  Q_OBJECT

public:
  explicit DetailSliderAction(QObject *parent, const QString& settingsKeyParam, int minimumValue, int maximumValue);
  virtual ~DetailSliderAction() override;

  /* MAP_MIN_DETAIL_LEVEL = 8 -> MAP_DEFAULT_DETAIL_LEVEL = 10 -> MAP_MAX_DETAIL_LEVEL = 15 */
  int getSliderValue() const;

  void saveState() const;
  void restoreState();

  void setSliderValue(int value);
  void reset();

signals:
  void valueChanged(int value);
  void sliderReleased();

protected:
  /* Create and delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* List of created/registered slider widgets */
  QList<QSlider *> sliders;
  int sliderValue = 0, minValue, maxValue;
  QString settingsKey;
};
}

#endif // LNM_MAPDETAILHANDLER_H
