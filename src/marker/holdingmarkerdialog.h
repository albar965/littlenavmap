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

#ifndef LNM_HOLDDIALOG_H
#define LNM_HOLDDIALOG_H

#include "marker/markerdialog.h"

namespace Ui {
class HoldingMarkerDialog;
}

namespace atools {
namespace geo {
class Pos;
}
}

namespace map {
struct MapResult;
struct HoldingMarker;
}

class UnitStringTool;
class QAbstractButton;

/*
 * Allows user to customize hold parameters before adding it to the map. Similar to traffic pattern.
 * Can be used for adding or editing marker.
 * Order of usage from result is (same as in map context menu) airport, vor, ndb, waypoint, pos
 */
class HoldingMarkerDialog :
  public MarkerDialog<map::HoldingMarker>
{
  Q_OBJECT

public:
  /* Marker is copied to internal. Result is used to assign navaid. editMode false configures dialog for adding. */
  explicit HoldingMarkerDialog(QWidget *parent, const map::HoldingMarker& markerParam, const map::MapResult& result, bool editMode);
  virtual ~HoldingMarkerDialog() override;

private:
  virtual void restoreState() override;
  virtual void saveState() const override;
  virtual void markerToWidgets() override;
  virtual void widgetsToMarker() override;

  void fillMarkerFromResultAndWidgets();
  void buttonBoxClicked(QAbstractButton *button);
  void updateLengthLabel();

  UnitStringTool *units = nullptr;
  const static QString DEFAULT_COLOR_STR;

  Ui::HoldingMarkerDialog *ui;
};

#endif // LNM_HOLDDIALOG_H
