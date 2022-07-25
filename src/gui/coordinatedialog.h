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

#ifndef LITTLENAVMAP_JUMPCOORDDIALOG_H
#define LITTLENAVMAP_JUMPCOORDDIALOG_H

#include <QDialog>

namespace Ui {
class CoordinateDialog;
}

namespace atools {
namespace geo {
class Pos;
}
}

class QAbstractButton;
class UnitStringTool;

/*
 * Dialog showing coordinates and zoom distance. User can jump to a defined map position with this one.
 */
class CoordinateDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit CoordinateDialog(QWidget *parent, const atools::geo::Pos& pos);
  virtual ~CoordinateDialog() override;

  CoordinateDialog(const CoordinateDialog& other) = delete;
  CoordinateDialog& operator=(const CoordinateDialog& other) = delete;

  const atools::geo::Pos& getPosition() const
  {
    return *position;
  }

  float getZoomDistanceKm() const;

private:
  void coordsEdited(const QString&);
  void buttonBoxClicked(QAbstractButton *button);

  Ui::CoordinateDialog *ui;
  atools::geo::Pos *position;
  UnitStringTool *units = nullptr;
};

#endif // LITTLENAVMAP_JUMPCOORDDIALOG_H
