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

#ifndef LNM_HOLDDIALOG_H
#define LNM_HOLDDIALOG_H

#include <QDialog>

namespace Ui {
class HoldDialog;
}

namespace atools {
namespace geo {
class Pos;
}
}

namespace map {
struct MapSearchResult;
struct Hold;
}

class UnitStringTool;
class QAbstractButton;

/*
 * Allows user to customize hold parameters before adding it to the map. Similar to traffic pattern.
 *
 * Order of usage from result is (same as in map context menu) airport, vor, ndb, waypoint, pos
 */
class HoldDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit HoldDialog(QWidget *parent, const map::MapSearchResult& resultParam, const atools::geo::Pos& positionParam);
  virtual ~HoldDialog() override;

  void fillHold(map::Hold& hold);

private:
  void restoreState();
  void saveState();

  void buttonBoxClicked(QAbstractButton *button);
  void colorButtonClicked();
  void updateButtonColor();
  void updateWidgets();
  void updateLength();

  Ui::HoldDialog *ui;
  map::MapSearchResult *result;
  atools::geo::Pos *position;

  QColor color;
  UnitStringTool *units = nullptr;
};

#endif // LNM_HOLDDIALOG_H
