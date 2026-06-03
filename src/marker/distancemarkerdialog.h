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

#ifndef LNM_DISTANCEMARKER_DIALOG_H
#define LNM_DISTANCEMARKER_DIALOG_H

#include "marker/markerdialog.h"

#include <QValidator>

namespace Ui {
class DistanceMarkerDialog;
}

namespace map {
struct DistanceMarker;
}

class QAbstractButton;

/*
 * Shows a dialog where user can set color and label for measurment lines.
 * Can be used for adding or editing marker.
 * Label can be set automaticall from navaid or changed by user.
 *
 * Reads state and defaults on intiantiation and saves it when calling getMarker().
 */
class DistanceMarkerDialog :
  public MarkerDialog<map::DistanceMarker>
{
  Q_OBJECT

public:
  /* Marker is copied to internal. Result is used to assign navaid. editMode false configures dialog for adding. */
  explicit DistanceMarkerDialog(QWidget *parent, const map::DistanceMarker& markerParam, const map::MapResult& result, bool editMode);
  virtual ~DistanceMarkerDialog() override;

  /* Get color from settings before drag and drop action starts */
  static QColor getSavedColor();

private:
  /* A button box button was clicked. Always saves state. */
  void buttonBoxClicked(QAbstractButton *button);

  virtual void restoreState() override;
  virtual void saveState() const override;
  virtual void widgetsToMarker() override;
  virtual void markerToWidgets() override;

  /* Text already given by navaid when editMode is false */
  bool textPreFilled = false;

  /* Color is set when clicking on button */
  const static QString DEFAULT_COLOR_STR;

  Ui::DistanceMarkerDialog *ui;
};

#endif // LNM_DISTANCEMARKER_DIALOG_H
