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

#include "route/routestringdialog.h"

#include "route/routestring.h"
#include "fs/pln/flightplan.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "common/constants.h"

#include "ui_routestringdialog.h"

#include <QClipboard>

RouteStringDialog::RouteStringDialog(QWidget *parent, MapQuery *mapQuery, const QString& initialString)
  : QDialog(parent), ui(new Ui::RouteStringDialog), query(mapQuery)
{
  ui->setupUi(this);

  flightplan = new atools::fs::pln::Flightplan;

  ui->plainTextEditRouteString->setPlainText(initialString);
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

  RouteString routeString(query);

  flightplan->clear();
  bool success = routeString.createRouteFromString(ui->plainTextEditRouteString->toPlainText(), *flightplan);

  ui->textEditRouteStringErrors->clear();

  QString msg;

  if(success)
    msg = tr("Read string and found %1 waypoints. Flight plan distance %2 nm.<br/>").
          arg(flightplan->getEntries().size()).
          arg(QLocale().toString(flightplan->getDistanceNm(), 'f', 0));

  if(routeString.getErrors().isEmpty())
    ui->textEditRouteStringErrors->setHtml(tr("%1No errors.").arg(msg));
  else
  {
    ui->textEditRouteStringErrors->setHtml(msg);
    for(const QString& err : routeString.getErrors())
      ui->textEditRouteStringErrors->append(err + "<br/>");
  }

  ui->plainTextEditRouteString->setPlainText(
    RouteString::cleanRouteString(ui->plainTextEditRouteString->toPlainText()));
  updateButtonState();
}

void RouteStringDialog::fromClipboardClicked()
{
  ui->plainTextEditRouteString->setPlainText(
    RouteString::cleanRouteString(QGuiApplication::clipboard()->text()));
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
    atools::gui::HelpHandler::openHelpUrl(
      this, atools::gui::HelpHandler::getHelpUrl(this, "help", "index.html", "h2routestring"));
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
