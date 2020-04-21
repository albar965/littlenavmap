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

#include "choicedialog.h"
#include "ui_choicedialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"

#include <QMimeData>
#include <QPushButton>
#include <QClipboard>
#include <QDebug>
#include <QTextDocumentFragment>
#include <QCheckBox>

#include <settings/settings.h>

const static char ID_PROPERTY[] = "checkboxid";

ChoiceDialog::ChoiceDialog(QWidget *parent, const QString& title, const QString& header,
                           const QString& settingsPrefixParam,
                           const QString& helpBaseUrlParam)
  : QDialog(parent), ui(new Ui::ChoiceDialog), helpBaseUrl(helpBaseUrlParam), settingsPrefix(settingsPrefixParam)
{
  ui->setupUi(this);
  setWindowTitle(title);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->groupBox->setTitle(header);

  if(helpBaseUrl.isEmpty())
    // Remove help button if not requested
    ui->buttonBox->removeButton(ui->buttonBox->button(QDialogButtonBox::Help));

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &ChoiceDialog::buttonBoxClicked);
}

ChoiceDialog::~ChoiceDialog()
{
  atools::gui::WidgetState widgetState(settingsPrefix, false);
  widgetState.save(this);

  delete ui;
}

void ChoiceDialog::add(int id, const QString& text, const QString& tooltip, bool checked)
{
  QCheckBox *checkBox = new QCheckBox(text, this);
  checkBox->setToolTip(tooltip);
  checkBox->setStatusTip(tooltip);
  checkBox->setProperty(ID_PROPERTY, id);
  checkBox->setChecked(checked);
  index.insert(id, checkBox);

  // Add widget before the button box
  ui->verticalLayoutGroupBox->insertWidget(-1, checkBox);
}

QVector<std::pair<int, bool> > ChoiceDialog::getCheckState() const
{
  QVector<std::pair<int, bool> > ids;
  for(int id : index.keys())
  {
    QCheckBox *checkbox = index.value(id);
    ids.append(std::make_pair(checkbox->property(ID_PROPERTY).toInt(), checkbox->isChecked()));
  }
  return ids;
}

QVector<int> ChoiceDialog::getCheckedIds() const
{
  QVector<int> ids;
  for(int id : index.keys())
  {
    if(index.value(id)->isChecked())
      ids.append(id);
  }
  return ids;
}

bool ChoiceDialog::isChecked(int id) const
{
  return index.value(id)->isChecked();
}

void ChoiceDialog::buttonBoxClicked(QAbstractButton *button)
{
  QDialogButtonBox::StandardButton buttonType = ui->buttonBox->standardButton(button);

  if(buttonType == QDialogButtonBox::Ok)
  {
    saveState();
    accept();
  }
  else if(buttonType == QDialogButtonBox::Cancel)
    reject();
  else if(buttonType == QDialogButtonBox::Help)
  {
    if(!helpBaseUrl.isEmpty())
      atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + helpBaseUrl, lnm::helpLanguageOnline());
  }
}

void ChoiceDialog::restoreState()
{
  atools::gui::WidgetState widgetState(settingsPrefix, false);
  widgetState.restore(this);

  // Restore checkboxes
  QStringList ids = atools::settings::Settings::instance().valueStrList(settingsPrefix + "CheckBoxStates");
  for(int i = 0; i < ids.size(); i += 2)
    index.value(ids.at(i).toInt())->setChecked(ids.at(i + 1).toInt());
}

void ChoiceDialog::saveState()
{
  atools::gui::WidgetState widgetState(settingsPrefix, false);
  widgetState.save(this);

  // Save checkboxes in a list of id 1, checked 1, id 2, checked 2, ...
  QStringList ids;
  for(const std::pair<int, bool>& state :getCheckState())
    ids << QString::number(state.first) << QString::number(state.second);

  atools::settings::Settings::instance().setValue(settingsPrefix + "CheckBoxStates", ids);
}
