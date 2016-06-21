/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "parkingdialog.h"
#include "ui_parkingdialog.h"
#include <QPushButton>
#include "common/maptypes.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"

ParkingDialog::ParkingDialog(QWidget *parent, MapQuery *mapQuery,
                             const maptypes::MapAirport& departureAirport)
  : QDialog(parent), ui(new Ui::ParkingDialog)
{
  ui->setupUi(this);

  ui->labelSelectParking->setText(ui->labelSelectParking->text().arg(maptypes::airportText(departureAirport)));

  const QList<maptypes::MapParking> *parkingCache = mapQuery->getParkingsForAirport(departureAirport.id);

  // Create a copy from the cached objects and exclude fuel
  for(const maptypes::MapParking& p : *parkingCache)
    if(p.type != "FUEL")
      parkings.append(p);

  // Sort by name and numbers
  std::sort(parkings.begin(), parkings.end(),
            [ = ](const maptypes::MapParking & p1, const maptypes::MapParking & p2)->bool
            {
              if(p1.name == p2.name)
                return p1.number < p2.number;
              else
                return p1.name < p2.name;
            });

  // Add to list widget
  for(const maptypes::MapParking& p : parkings)
  {
    QString text = maptypes::parkingName(p.name) + " " + QLocale().toString(p.number) +
                   ", " + maptypes::parkingTypeName(p.type) +
                   ", " + QLocale().toString(p.radius * 2) + " ft" +
                   (p.jetway ? ", Has Jetway" : "");

    new QListWidgetItem(mapcolors::iconForParkingType(p.type), text, ui->listWidgetSelectParking);
  }

  updateButtons();
  connect(ui->listWidgetSelectParking, &QListWidget::itemSelectionChanged, this,
          &ParkingDialog::updateButtons);

  connect(ui->listWidgetSelectParking, &QListWidget::itemActivated, this,
          &QDialog::accept);

  connect(ui->buttonBoxSelectParking, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxSelectParking, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ParkingDialog::~ParkingDialog()
{
  delete ui;
}

bool ParkingDialog::getSelectedParking(maptypes::MapParking& parking)
{
  if(ui->listWidgetSelectParking->currentItem() != nullptr)
  {
    parking = parkings.at(ui->listWidgetSelectParking->currentRow());
    return true;
  }
  return false;
}

void ParkingDialog::updateButtons()
{
  ui->buttonBoxSelectParking->button(QDialogButtonBox::Ok)->setEnabled(
    ui->listWidgetSelectParking->currentItem() != nullptr);
}
