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

#ifndef LNM_TRAFFICPATTERN_H
#define LNM_TRAFFICPATTERN_H

#include <QDialog>

namespace Ui {
class TrafficPatternDialog;
}

namespace map {
struct MapAirport;
struct TrafficPattern;
}

class QAbstractButton;
class UnitStringTool;
class RunwaySelection;

/*
 * Shows airport and runway information and allows to configure a traffic pattern for a selected runway.
 *
 * Reads state on intiantiation and saves it on destruction
 */
class TrafficPatternDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit TrafficPatternDialog(QWidget *parent, const map::MapAirport& mapAirport);
  virtual ~TrafficPatternDialog();

  void fillTrafficPattern(map::TrafficPattern& pattern);

private:
  void restoreState();
  void saveState();

  void updateWidgets();
  void updateRunwayLabel();
  void buttonBoxClicked(QAbstractButton *button);
  void colorButtonClicked();
  void updateButtonColor();
  void doubleClicked();

  Ui::TrafficPatternDialog *ui;

  QColor color;

  UnitStringTool *units = nullptr;

  RunwaySelection *runwaySelection = nullptr;

};

#endif // LNM_TRAFFICPATTERN_H
