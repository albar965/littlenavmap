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

#include "routestring/routestringdialog.h"

#include "routestring/routestringwriter.h"
#include "routestring/routestringreader.h"
#include "navapp.h"
#include "settings/settings.h"
#include "query/procedurequery.h"
#include "route/routecontroller.h"
#include "fs/pln/flightplan.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "common/unit.h"
#include "atools.h"

#include "ui_routestringdialog.h"

#include <QClipboard>
#include <QAction>

using atools::gui::HelpHandler;
namespace apln = atools::fs::pln;

RouteStringDialog::RouteStringDialog(QWidget *parent, RouteController *routeController)
  : QDialog(parent), ui(new Ui::RouteStringDialog), controller(routeController)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Make the splitter handle better visible
  ui->splitterRouteString->setStyleSheet(QString("QSplitter::handle { "
                                                 "background: %1;"
                                                 "image: url(:/littlenavmap/resources/icons/splitterhandvert.png); "
                                                 "}").
                                         arg(QApplication::palette().color(QPalette::Window).darker(120).name()));

  // Disallow collapsing of the upper view
  ui->splitterRouteString->setCollapsible(0, false);
  if(ui->splitterRouteString->handle(1) != nullptr)
  {
    ui->splitterRouteString->handle(1)->
    setToolTip(tr("Resize upper and lower part or open and close lower message area of the dialog."));
    ui->splitterRouteString->handle(1)->setStatusTip(ui->splitterRouteString->handle(1)->toolTip());
  }

  QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#if defined(Q_OS_MACOS)
  fixedFont.setPointSizeF(fixedFont.pointSizeF() * 1.2);
#endif
  ui->plainTextEditRouteString->setFont(fixedFont);
  ui->plainTextEditRouteString->setWordWrapMode(QTextOption::WrapAnywhere);

  ui->buttonBoxRouteString->button(QDialogButtonBox::Ok)->setText(tr("Create Flight &Plan"));

  flightplan = new apln::Flightplan;
  routeStringWriter = new RouteStringWriter();
  routeStringReader = new RouteStringReader(routeController->getFlightplanEntryBuilder());

  // Build options dropdown menu
  QAction *action;
  action = new QAction(tr("Write departure and destination airport"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::START_AND_DEST));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Write DCT (direct) instructions"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::DCT));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Write cruise speed and altitude instruction"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::ALT_AND_SPEED));
  ui->toolButtonRouteStringOptions->addAction(action);

  if(NavApp::hasSidStarInDatabase())
  {
    action = new QAction(tr("Write SID and STAR"), ui->toolButtonRouteStringOptions);
    action->setCheckable(true);
    action->setData(static_cast<int>(rs::SID_STAR));
    ui->toolButtonRouteStringOptions->addAction(action);
  }

  action = new QAction(tr("Write generic SID and STAR"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::SID_STAR_GENERIC));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Write Waypoints instead of Airways"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::NO_AIRWAYS));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Write Alternates"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::ALTERNATES));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Read trailing Airports as Alternates"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::READ_ALTERNATES));
  ui->toolButtonRouteStringOptions->addAction(action);

#ifdef DEBUG_INFORMATION
  action = new QAction(tr("Read first and last item as Navaid"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::READ_NO_AIRPORTS));
  ui->toolButtonRouteStringOptions->addAction(action);

  action = new QAction(tr("Read: Match coordinates to Waypoints"), ui->toolButtonRouteStringOptions);
  action->setCheckable(true);
  action->setData(static_cast<int>(rs::READ_MATCH_WAYPOINTS));
  ui->toolButtonRouteStringOptions->addAction(action);
#endif

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
  delete routeStringWriter;
  delete routeStringReader;
  delete ui;
  delete flightplan;
}

void RouteStringDialog::updateButtonClicked()
{
  ui->plainTextEditRouteString->setPlainText(routeStringWriter->createStringForRoute(NavApp::getRouteConst(),
                                                                                     NavApp::getRouteCruiseSpeedKts(),
                                                                                     options));
}

void RouteStringDialog::toolButtonOptionTriggered(QAction *action)
{
  Q_UNUSED(action)

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
  ui->splitterRouteString->setHandleWidth(6);
  options = getOptionsFromSettings();
  updateButtonState();

  ui->plainTextEditRouteString->setPlainText(routeStringWriter->createStringForRoute(NavApp::getRouteConst(),
                                                                                     NavApp::getRouteCruiseSpeedKts(),
                                                                                     options));
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

  bool success = routeStringReader->createRouteFromString(
    ui->plainTextEditRouteString->toPlainText(), options, flightplan, nullptr, &speedKts, &altitudeIncluded);

  ui->textEditRouteStringErrors->clear();

  QGuiApplication::restoreOverrideCursor();

  QString msg;

  if(success)
  {
    QString from = (!flightplan->getDepartureName().isEmpty() &&
                    flightplan->getDepartureName() != flightplan->getDepartureIdent()) ?
                   tr("%1 (%2)").arg(flightplan->getDepartureName()).arg(flightplan->getDepartureIdent()) :
                   tr("%1").arg(flightplan->getDepartureIdent());

    QString to = (!flightplan->getDestinationName().isEmpty() &&
                  flightplan->getDestinationName() != flightplan->getDestinationIdent()) ?
                 tr("%1 (%2)").arg(flightplan->getDestinationName()).arg(flightplan->getDestinationIdent()) :
                 tr("%1").arg(flightplan->getDestinationIdent());

    msg = tr("Flight plan from <b>%1</b> to <b>%2</b>.<br/>").arg(from).arg(to);

    msg.append(tr("Distance without procedures: <b>%1</b>.<br/>").arg(Unit::distNm(flightplan->getDistanceNm())));

    QStringList idents;
    for(apln::FlightplanEntry& entry : flightplan->getEntries())
      idents.append(entry.getIdent());

    if(!idents.isEmpty())
      msg.append(tr("Found %1 %2: <b>%3</b>.<br/>").
                 arg(idents.size()).
                 arg(idents.size() == 1 ? tr("waypoint") : tr("waypoints")).
                 arg(atools::elideTextShortMiddle(idents.join(tr(" ")), 150)));

    QString sid = ProcedureQuery::getSidAndTransition(flightplan->getProperties());
    if(!sid.isEmpty())
      msg += tr("Found SID: <b>%1</b>.<br/>").arg(sid);

    QString star = ProcedureQuery::getStarAndTransition(flightplan->getProperties());
    if(!star.isEmpty())
      msg += tr("Found STAR: <b>%1</b>.<br/>").arg(star);

    ui->textEditRouteStringErrors->setHtml(msg);
  }

  if(!routeStringReader->getMessages().isEmpty())
  {
    for(const QString& err : routeStringReader->getMessages())
      ui->textEditRouteStringErrors->append(err + "<br/>");
  }

  ui->plainTextEditRouteString->setPlainText(rs::cleanRouteString(ui->plainTextEditRouteString->toPlainText()).join(" "));
  updateButtonState();
}

void RouteStringDialog::fromClipboardClicked()
{
  ui->plainTextEditRouteString->setPlainText(
    rs::cleanRouteString(QGuiApplication::clipboard()->text()).join(" "));
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
    flightplan->setFlightplanType(apln::IFR);
  else
    flightplan->setFlightplanType(apln::VFR);
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
    HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "ROUTEDESCR.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxRouteString->button(QDialogButtonBox::Close))
    QDialog::reject();
}

void RouteStringDialog::updateButtonState()
{
  QStringList cleanString = rs::cleanRouteString(ui->plainTextEditRouteString->toPlainText());

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
