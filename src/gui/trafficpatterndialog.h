/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel albar965@mailbox.org
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

#include "common/maptypes.h"

#include <QDialog>

namespace Ui {
class TrafficPatternDialog;
}

class QAbstractButton;
class UnitStringTool;

class TrafficPatternDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit TrafficPatternDialog(QWidget *parent, const map::MapAirport& mapAirport);
  virtual ~TrafficPatternDialog();

  void restoreState();
  void saveState();

  void fillTrafficPattern(map::TrafficPattern& pattern);

private:
  Ui::TrafficPatternDialog *ui;
  void updateWidgets();
  void updateRunwayLabel(int index);
  void fillRunwayComboBox();
  void fillAirportLabel();
  void buttonBoxClicked(QAbstractButton *button);

  QList<map::MapRunway> runways;
  map::MapAirport airport;
  QColor color;

  UnitStringTool *units = nullptr;

  void colorButtonClicked();
  void updateButtonColor();
};

#endif // LNM_TRAFFICPATTERN_H
