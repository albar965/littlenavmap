/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "navapp.h"
#include "settings/settings.h"
#include "query/procedurequery.h"
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
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#if defined(Q_OS_MACOS)
  fixedFont.setPointSizeF(fixedFont.pointSizeF() * 1.2);
#endif
  ui->plainTextEditRouteString->setFont(fixedFont);
  ui->plainTextEditRouteString->setWordWrapMode(QTextOption::WrapAnywhere);

  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setText(tr("Create Flight &Plan"));

  flightplan = new atools::fs::pln::Flightplan;
  routeString = new RouteString(routeController->getFlightplanEntryBuilder());

  // Build options dropdown menu
  QAction *action;
  action = new QAction(tr("Add departure and destination airport"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::START_AND_DEST));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Add DCT (direct) instructions"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::DCT));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Add cruise speed and altitude instruction"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::ALT_AND_SPEED));
  ui->toolButtonRouteStringOptions->addAction(action);

  if(NavApp::hasSidStarInDatabase())
  {
    action = new QAction(tr("Add SID and STAR"), ui->toolButtonRouteStringOptions);
    action->setCheckable(true);
    action->setData(static_cast<int>(rs::SID_STAR));
    ui->toolButtonRouteStringOptions->addAction(action);
  }

  action = new QAction(tr("Add generic SID and STAR"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::SID_STAR_GENERIC));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Add Waypoints instead of Airways"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::NO_AIRWAYS));
  ui->toolButtonRouteStringOptions->addAction(action);

  connect(ui->pushButtonRouteStringRead, &QPushButton::clicked, this, &RouteStringDialog::readButtonClicked);
  connect(ui->pushButtonRouteStringFromClipboard, &QPushButton::clicked, this,
          &RouteStringDialog::fromClipboardClicked);
  connect(ui->pushButtonRouteStringToClipboard, &QPushButton::clicked, this, &RouteStringDialog::toClipboardClicked);

  connect(ui->plainTextEditRouteString, &QPlainTextEdit::textChanged, this, &RouteStringDialog::updateButtonState);

  connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &RouteStringDialog::updateButtonState);

  connect(ui->buttonBoxRouteString, &QDialogButtonBox::clicked, this, &RouteStringDialog::buttonBoxClicked);

  connect(ui->toolButtonRouteStringOptions, &QToolButton::triggered, this,
          &RouteStringDialog::toolButtonOptionTriggered);

  connect(ui->pushButtonRouteStringUpdate, &QPushButton::clicked, this, &RouteStringDialog::updateButtonClicked);
}

RouteStringDialog::~RouteStringDialog()
{
  delete routeString;
  delete ui;
  delete flightplan;
}

void RouteStringDialog::updateButtonClicked()
{
  ui->plainTextEditRouteString->setPlainText(RouteString::createStringForRoute(NavApp::getRouteConst(),
                                                                               NavApp::getSpeedKts(), options));
}

void RouteStringDialog::toolButtonOptionTriggered(QAction *action)
{
  Q_UNUSED(action);

  // Copy menu state for options bitfield
  for(const QAction *act : ui->toolButtonRouteStringOptions->actions())
  {
    rs::RouteStringOptions opts(act->data().toInt());
    if(act->isChecked())
      options |= opts;
    else
      options &= ~opts;
  }
}

const atools::fs::pln::Flightplan& RouteStringDialog::getFlightplan() const
{
  return *flightplan;
}

void RouteStringDialog::saveState()
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER).save(
    {this, ui->splitterRouteString, ui->comboBoxRouteStringFlightplanType});
  atools::settings::Settings::instance().setValue(lnm::ROUTE_STRING_DIALOG_OPTIONS, static_cast<int>(options));
}

void RouteStringDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_STRING_DIALOG_SPLITTER).restore(
    {this, ui->splitterRouteString, ui->comboBoxRouteStringFlightplanType});
  options = getOptionsFromSettings();
  updateButtonState();

  ui->plainTextEditRouteString->setPlainText(RouteString::createStringForRoute(NavApp::getRouteConst(),
                                                                               NavApp::getSpeedKts(), options));
}

rs::RouteStringOptions RouteStringDialog::getOptionsFromSettings()
{
  return rs::RouteStringOptions(atools::settings::Settings::instance().
                                valueInt(lnm::ROUTE_STRING_DIALOG_OPTIONS, rs::DEFAULT_OPTIONS));
}

void RouteStringDialog::readButtonClicked()
{
  qDebug() << Q_FUNC_INFO;

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  flightplan->clear();
  flightplan->getProperties().clear();

  bool success = routeString->createRouteFromString(
    ui->plainTextEditRouteString->toPlainText(), *flightplan, speedKts, altitudeIncluded);

  ui->textEditRouteStringErrors->clear();

  QGuiApplication::restoreOverrideCursor();

  QString msg;

  if(success)
  {
    msg =
      tr("Found %1 waypoints. Flight plan from <b>%3 (%4)</b> to <b>%5 (%6)</b>.<br/>Distance is %2.<br/>").
      arg(flightplan->getEntries().size()).
      arg(Unit::distNm(flightplan->getDistanceNm())).
      arg(flightplan->getDepartureAiportName()).
      arg(flightplan->getDepartureIdent()).
      arg(flightplan->getDestinationAiportName()).
      arg(flightplan->getDestinationIdent());

    QString sid = ProcedureQuery::getSidAndTransition(flightplan->getProperties());
    if(!sid.isEmpty())
      msg += tr("Found SID <b>%1</b>.<br/>").arg(sid);

    QString star = ProcedureQuery::getStarAndTransition(flightplan->getProperties());
    if(!star.isEmpty())
      msg += tr("Found STAR <b>%1</b>.<br/>").arg(star);

    ui->textEditRouteStringErrors->setHtml(msg);
  }

  if(!routeString->getMessages().isEmpty())
  {
    for(const QString& err : routeString->getMessages())
      ui->textEditRouteStringErrors->append(err + "<br/>");
  }

  ui->plainTextEditRouteString->setPlainText(RouteString::cleanRouteString(
                                               ui->plainTextEditRouteString->toPlainText()).join(" "));
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

void RouteStringDialog::updateFlightplan()
{
  // Update type from current combox box setting
  // Low / high altitude is set later when resolving the airways

  if(ui->comboBoxRouteStringFlightplanType->currentIndex() == 0)
    flightplan->setFlightplanType(atools::fs::pln::IFR);
  else
    flightplan->setFlightplanType(atools::fs::pln::VFR);

  if(flightplan->getEntries().size() == 2)
    flightplan->setRouteType(atools::fs::pln::DIRECT);
  else if(flightplan->getEntries().size() > 2)
  {
    // Check if this is a VOR / NDB only route
    bool foundOtherThanVorOrNdb = false;

    for(int i = 1; i < flightplan->getEntries().size() - 1; i++)
    {
      const atools::fs::pln::FlightplanEntry& entry = flightplan->getEntries().at(i);
      if(!entry.getAirway().isEmpty() ||
         (entry.getWaypointType() != atools::fs::pln::entry::VOR &&
          entry.getWaypointType() != atools::fs::pln::entry::NDB))
      {
        foundOtherThanVorOrNdb = true;
        break;
      }
    }

    if(!foundOtherThanVorOrNdb)
      flightplan->setRouteType(atools::fs::pln::VOR);
  }
}

/* A button box button was clicked */
void RouteStringDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Ok))
  {
    updateFlightplan();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrl(parentWidget(), lnm::HELP_ONLINE_URL + "ROUTEDESCR.html", lnm::helpLanguagesOnline());
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void RouteStringDialog::updateButtonState()
{
  QStringList cleanString = RouteString::cleanRouteString(ui->plainTextEditRouteString->toPlainText());

  ui->pushButtonRouteStringRead->setEnabled(!cleanString.isEmpty());
  ui->pushButtonRouteStringUpdate->setEnabled(!NavApp::getRouteConst().isEmpty());

  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setDisabled(flightplan->getEntries().isEmpty());

  ui->pushButtonRouteStringToClipboard->setDisabled(cleanString.isEmpty());

  ui->pushButtonRouteStringFromClipboard->setDisabled(QGuiApplication::clipboard()->text().simplified().isEmpty());

  // Copy option flags to dropdown menu items
  for(QAction *act : ui->toolButtonRouteStringOptions->actions())
  {
    act->blockSignals(true);
    act->setChecked(rs::RouteStringOptions(act->data().toInt()) & options);
    act->blockSignals(false);
  }
}
