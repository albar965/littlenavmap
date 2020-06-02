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

#include "route/userwaypointdialog.h"
#include "ui_userwaypointdialog.h"

#include "geo/pos.h"
#include "common/unit.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/constants.h"
#include "gui/helphandler.h"
#include "fs/util/coordinates.h"
#include "gui/widgetstate.h"
#include "fs/pln/flightplanentry.h"

#include <QPushButton>

UserWaypointDialog::UserWaypointDialog(QWidget *parent, const atools::fs::pln::FlightplanEntry& entryParam)
  : QDialog(parent), ui(new Ui::UserWaypointDialog)
{
  entry = new atools::fs::pln::FlightplanEntry;
  *entry = entryParam;

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  bool waypointEdit = entry->getWaypointType() != atools::fs::pln::entry::USER;
  ui->lineEditRouteUserWaypointIdent->setHidden(waypointEdit);
  ui->labelRouteUserWaypointIdent->setHidden(waypointEdit);
  ui->lineEditRouteUserWaypointRegion->setHidden(waypointEdit);
  ui->labelRouteUserWaypointRegion->setHidden(waypointEdit);
  ui->lineEditRouteUserWaypointName->setHidden(waypointEdit);
  ui->labelRouteUserWaypointName->setHidden(waypointEdit);
  ui->lineEditRouteUserWaypointLatLon->setHidden(waypointEdit);
  ui->labelRouteUserWaypointLatLon->setHidden(waypointEdit);
  ui->labelRouteUserWaypointCoordStatus->setHidden(waypointEdit);
  ui->labelRouteUserWaypointHeader->setHidden(!waypointEdit);
  ui->labelRouteUserWaypointHeader2->setHidden(true /*!waypointEdit*/);

  if(waypointEdit)
  {
    setWindowTitle(QApplication::applicationName() + tr(" - Edit Flight Plan Position Remarks"));
    ui->labelRouteUserWaypointHeader->setText(tr("<b>%1 %2 region %3</b>").
                                              arg(entry->getWaypointTypeAsDisplayString()).
                                              arg(entry->getIdent()).
                                              arg(entry->getRegion()));

    // ui->labelRouteUserWaypointHeader2->setText(tr("Note that remarks are a part of the flight plan and are "
    // "removed when calculating "
    // "flight plans or deleting waypoints."));
  }
  else
    setWindowTitle(QApplication::applicationName() + tr(" - Edit Flight Plan Position"));

  ui->plainTextEditRouteUserWaypointComment->setPlainText(entry->getComment());
  if(!waypointEdit)
  {
    ui->lineEditRouteUserWaypointIdent->setText(entry->getIdent());
    ui->lineEditRouteUserWaypointRegion->setText(entry->getRegion());
    ui->lineEditRouteUserWaypointName->setText(entry->getName());
    ui->lineEditRouteUserWaypointLatLon->setText(Unit::coords(entry->getPosition()));
    coordsEdited(QString());
  }

  connect(ui->lineEditRouteUserWaypointLatLon, &QLineEdit::textChanged, this, &UserWaypointDialog::coordsEdited);
  connect(ui->buttonBoxRouteUserWaypoint, &QDialogButtonBox::clicked, this, &UserWaypointDialog::buttonBoxClicked);

  atools::gui::WidgetState(lnm::ROUTE_USERWAYPOINT_DIALOG).restore(this);
}

UserWaypointDialog::~UserWaypointDialog()
{
  atools::gui::WidgetState(lnm::ROUTE_USERWAYPOINT_DIALOG).save(this);

  delete entry;
  delete ui;
}

void UserWaypointDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteUserWaypoint->button(QDialogButtonBox::Ok))
  {
    if(entry->getWaypointType() == atools::fs::pln::entry::USER)
    {
      entry->setIdent(ui->lineEditRouteUserWaypointIdent->text());
      entry->setRegion(ui->lineEditRouteUserWaypointRegion->text());
      entry->setName(ui->lineEditRouteUserWaypointName->text());

      atools::geo::Pos pos = atools::fs::util::fromAnyFormat(ui->lineEditRouteUserWaypointLatLon->text());
      if(OptionData::instance().getUnitCoords() == opts::COORDS_LONX_LATY)
        // Parsing uses lat/lon - swap for lon/lat
        pos.swapLonXLatY();

      if(pos.isValid())
        entry->setPosition(pos);
    }
    entry->setComment(ui->plainTextEditRouteUserWaypointComment->toPlainText());

    QDialog::accept();
  }
  else if(button == ui->buttonBoxRouteUserWaypoint->button(QDialogButtonBox::Help))
  {
    if(entry->getWaypointType() == atools::fs::pln::entry::USER)
      atools::gui::HelpHandler::openHelpUrlWeb(
        parentWidget(), lnm::helpOnlineUrl + "EDITFPPOSITION.html", lnm::helpLanguageOnline());
    else
      atools::gui::HelpHandler::openHelpUrlWeb(
        parentWidget(), lnm::helpOnlineUrl + "EDITFPREMARKS.html", lnm::helpLanguageOnline());
  }
  else if(button == ui->buttonBoxRouteUserWaypoint->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void UserWaypointDialog::coordsEdited(const QString& text)
{
  Q_UNUSED(text);

  QString message;
  bool valid = formatter::checkCoordinates(message, ui->lineEditRouteUserWaypointLatLon->text());
  ui->buttonBoxRouteUserWaypoint->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelRouteUserWaypointCoordStatus->setText(message);
}
