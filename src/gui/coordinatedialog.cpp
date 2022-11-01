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

#include "gui/coordinatedialog.h"
#include "ui_coordinatedialog.h"

#include "geo/pos.h"
#include "common/unit.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "gui/helphandler.h"
#include "fs/util/coordinates.h"
#include "gui/widgetstate.h"
#include "common/unitstringtool.h"

#include <QPushButton>

CoordinateDialog::CoordinateDialog(QWidget *parent, const atools::geo::Pos& pos)
  : QDialog(parent), ui(new Ui::CoordinateDialog)
{
  position = new atools::geo::Pos(pos);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->lineEditMapCoordinateLatLon->setText(Unit::coords(*position));

  // Change label depending on order
  if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY)
    ui->labelMapCoordinateLatLon->setText(tr("&Longitude and Latitude:"));
  else
    ui->labelMapCoordinateLatLon->setText(tr("&Latitude and Longitude:"));

  coordsEdited(QString());

  connect(ui->lineEditMapCoordinateLatLon, &QLineEdit::textChanged, this, &CoordinateDialog::coordsEdited);
  connect(ui->buttonBoxMapCoordinate, &QDialogButtonBox::clicked, this, &CoordinateDialog::buttonBoxClicked);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->doubleSpinBoxMapCoordinateZoom});

  atools::gui::WidgetState(lnm::MAP_COORDINATE_DIALOG).restore({this, ui->doubleSpinBoxMapCoordinateZoom});
}

CoordinateDialog::~CoordinateDialog()
{
  atools::gui::WidgetState(lnm::MAP_COORDINATE_DIALOG).save({this, ui->doubleSpinBoxMapCoordinateZoom});

  delete position;
  delete units;
  delete ui;
}

void CoordinateDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxMapCoordinate->button(QDialogButtonBox::Ok))
  {
    bool hemisphere = false;
    atools::geo::Pos pos = atools::fs::util::fromAnyFormat(ui->lineEditMapCoordinateLatLon->text(), &hemisphere);

    if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY && !hemisphere)
      // Parsing uses lat/lon - swap for lon/lat
      // Swap coordinates for lat lon formats if no hemisphere (N, S, E, W) is given
      atools::fs::util::maybeSwapOrdinates(pos, ui->lineEditMapCoordinateLatLon->text());

    if(pos.isValid())
      *position = pos;

    QDialog::accept();
  }
  else if(button == ui->buttonBoxMapCoordinate->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "JUMPCOORDINATE.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxMapCoordinate->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void CoordinateDialog::coordsEdited(const QString&)
{
  QString message;
  bool valid = formatter::checkCoordinates(message, ui->lineEditMapCoordinateLatLon->text());
  ui->buttonBoxMapCoordinate->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelMapCoordinateCoordStatus->setText(message);
}

float CoordinateDialog::getZoomDistanceKm() const
{
  return Unit::rev(static_cast<float>(ui->doubleSpinBoxMapCoordinateZoom->value()), &Unit::distMeterF) / 1000.f;
}
