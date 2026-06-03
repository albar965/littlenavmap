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

#ifndef LNM_TRAFFICPATTERN_H
#define LNM_TRAFFICPATTERN_H

#include "marker/markerdialog.h"

namespace Ui {
class PatternMarkerDialog;
}

namespace map {
struct MapAirport;
struct PatternMarker;
}

class QAbstractButton;
class UnitStringTool;
class RunwayTable;

/*
 * Shows airport and runway information and allows to configure a traffic pattern for a selected runway.
 *
 * Reads state on intiantiation and saves it on destruction
 */
class PatternMarkerDialog :
  public MarkerDialog<map::PatternMarker>
{
  Q_OBJECT

public:
  /* Marker is copied to internal. Result is used to assign navaid. editMode false configures dialog for adding. */
  explicit PatternMarkerDialog(QWidget *parent, const map::PatternMarker& markerParam, const map::MapResult& result, bool editMode);
  virtual ~PatternMarkerDialog() override;

private:
  virtual void restoreState() override;
  virtual void saveState() const override;
  virtual void markerToWidgets() override;
  virtual void widgetsToMarker() override;

  void updateWidgets();
  void updateRunwayLabel();
  void buttonBoxClicked(QAbstractButton *button);
  void runwayTableDoubleClicked();

  UnitStringTool *units = nullptr;
  RunwayTable *runwayTable = nullptr;

  const static QString DEFAULT_COLOR_STR;

  Ui::PatternMarkerDialog *ui;
};

#endif // LNM_TRAFFICPATTERN_H
