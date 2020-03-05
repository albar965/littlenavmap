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

#include "gui/timedialog.h"
#include "ui_timedialog.h"
#include "ui_mainwindow.h"

#include "common/constants.h"
#include "gui/helphandler.h"
#include "mapgui/mapwidget.h"
#include "gui/widgetstate.h"

#include <QAbstractButton>
#include <QPushButton>
#include <navapp.h>

TimeDialog::TimeDialog(QWidget *parent, const QDateTime& datetime) :
  QDialog(parent), ui(new Ui::TimeDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  ui->calendarWidget->setSelectedDate(datetime.date());
  ui->timeEdit->setTime(datetime.time());

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &TimeDialog::buttonBoxClicked);
  atools::gui::WidgetState(lnm::TIME_DIALOG).restore(this);
}

TimeDialog::~TimeDialog()
{
  atools::gui::WidgetState(lnm::TIME_DIALOG).save(this);
  delete ui;
}

QDateTime TimeDialog::getDateTime() const
{
  return QDateTime(ui->calendarWidget->selectedDate(), ui->timeEdit->time());
}

/* A button box button was clicked */
void TimeDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok) || button == ui->buttonBox->button(QDialogButtonBox::Apply))
  {
    qDebug() << Q_FUNC_INFO << getDateTime();

    // Select user option and update
    NavApp::getMainUi()->actionMapShowSunShadingUserTime->setChecked(true);
    MapWidget *mapWidget = NavApp::getMapWidget();
    mapWidget->setSunShadingDateTime(getDateTime());
    mapWidget->update();
    mapWidget->updateSunShadingOption();

    if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
      QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(
      parentWidget(), lnm::helpOnlineUrl + "SUNSHADOW.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}
