/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "route/routestringdialog.h"

#include "route/routestring.h"
#include "route/routecontroller.h"
#include "fs/pln/flightplan.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "common/unit.h"

#include "ui_routestringdialog.h"

#include <QClipboard>

using atools::gui::HelpHandler;

RouteStringDialog::RouteStringDialog(QWidget *parent, RouteController *routeController)
  : QDialog(parent), ui(new Ui::RouteStringDialog), controller(routeController)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->setupUi(this);

  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setText(tr("Create Flight &Plan"));

  flightplan = new atools::fs::pln::Flightplan;
  routeString = new RouteString(routeController->getFlightplanEntryBuilder());

  ui->plainTextEditRouteString->setPlainText(
    routeString->createStringForRoute(routeController->getRouteMapObjects(), routeController->getSpeedKts()));

  connect(ui->pushButtonRouteStringRead, &QPushButton::clicked,
          this, &RouteStringDialog::readClicked);
  connect(ui->pushButtonRouteStringFromClipboard, &QPushButton::clicked,
          this, &RouteStringDialog::fromClipboardClicked);
  connect(ui->pushButtonRouteStringToClipboard, &QPushButton::clicked,
          this, &RouteStringDialog::toClipboardClicked);

  connect(ui->plainTextEditRouteString, &QPlainTextEdit::textChanged,
          this, &RouteStringDialog::updateButtonState);

  connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
          this, &RouteStringDialog::updateButtonState);

  connect(ui->buttonBoxRouteString, &QDialogButtonBox::clicked,
          this, &RouteStringDialog::buttonBoxClicked);

  updateButtonState();
}

RouteStringDialog::~RouteStringDialog()
{
  delete routeString;
  delete ui;
  delete flightplan;
}

const atools::fs::pln::Flightplan& RouteStringDialog::getFlightplan() const
{
  return *flightplan;
}

void RouteStringDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER);
  widgetState.save({this, ui->splitterRouteString});
}

void RouteStringDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER).
  restore({this, ui->splitterRouteString});
  updateButtonState();
}

void RouteStringDialog::readClicked()
{
  qDebug() << "RouteStringDialog::readClicked()";

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  flightplan->clear();
  bool success = routeString->createRouteFromString(
    ui->plainTextEditRouteString->toPlainText(), *flightplan, speedKts);

  ui->textEditRouteStringErrors->clear();

  QGuiApplication::restoreOverrideCursor();

  QString msg;

  if(success)
  {
    msg =
      tr("<b>Found %1 waypoints. Flight plan from %3 (%4) to %5 (%6). Distance is %2.</b><br/>").
      arg(flightplan->getEntries().size()).
      arg(Unit::distNm(flightplan->getDistanceNm())).
      arg(flightplan->getDepartureAiportName()).
      arg(flightplan->getDepartureIdent()).
      arg(flightplan->getDestinationAiportName()).
      arg(flightplan->getDestinationIdent());
    ui->textEditRouteStringErrors->setHtml(msg);
  }

  if(!routeString->getMessages().isEmpty())
  {
    for(const QString& err : routeString->getMessages())
      ui->textEditRouteStringErrors->append(err + "<br/>");
  }

  ui->plainTextEditRouteString->setPlainText(
    RouteString::cleanRouteString(ui->plainTextEditRouteString->toPlainText()).join(" "));
  updateButtonState();
}

void RouteStringDialog::fromClipboardClicked()
{
  ui->plainTextEditRouteString->setPlainText(
    RouteString::cleanRouteString(QGuiApplication::clipboard()->text()).join(" "));
}

void RouteStringDialog::toClipboardClicked()
{
  QGuiApplication::clipboard()->setText(ui->plainTextEditRouteString->toPlainText());
}

/* A button box button was clicked */
void RouteStringDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Ok))
    QDialog::accept();
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrl(parentWidget(), lnm::HELP_ONLINE_URL + "ROUTEDESCR.html", lnm::helpLanguages());
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void RouteStringDialog::updateButtonState()
{
  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setDisabled(flightplan->getEntries().isEmpty());

  ui->pushButtonRouteStringToClipboard->setDisabled(
    RouteString::cleanRouteString(ui->plainTextEditRouteString->toPlainText()).isEmpty());

  ui->pushButtonRouteStringFromClipboard->setDisabled(
    QGuiApplication::clipboard()->text().simplified().isEmpty());
}
