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

#include "mapgui/imageexportdialog.h"
#include "ui_imageexportdialog.h"

#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"

#include <QPushButton>

/* Combo box indexes */
enum ResolutionIndex
{
  CURRENT_MAP_VIEW,
  CUSTOM_RESOLUTION,
  RES_720P_1280_720,
  RES_1080P_1920_1080,
  RES_1440P_2560_1440,
  RES_2160P_3840_2160,
  RES_4320P_7680_4320
};

/* Map index to resolution */
const static QMap<ResolutionIndex, std::pair<int, int> > resolutions({
  {CURRENT_MAP_VIEW, std::make_pair(-1, -1)},
  {CUSTOM_RESOLUTION, std::make_pair(-1, -1)},
  {RES_720P_1280_720, std::make_pair(1280, 720)},
  {RES_1080P_1920_1080, std::make_pair(1920, 1080)},
  {RES_1440P_2560_1440, std::make_pair(2560, 1440)},
  {RES_2160P_3840_2160, std::make_pair(3840, 2160)},
  {RES_4320P_7680_4320, std::make_pair(7680, 4320)}
});

ImageExportDialog::ImageExportDialog(QWidget *parent, const QString& titleParam, const QString& optionPrefixParam,
                                     int currentWidth, int currentHeight)
  : QDialog(parent), ui(new Ui::ImageExportDialog), optionPrefix(optionPrefixParam),
  curWidth(currentWidth), curHeight(currentHeight)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);
  setWindowTitle(QApplication::applicationName() + titleParam);

  // Put current map size into current map view option
  ui->comboBoxResolution->setItemText(CURRENT_MAP_VIEW, ui->comboBoxResolution->itemText(CURRENT_MAP_VIEW).
                                      arg(curWidth).arg(curHeight));

  connect(ui->comboBoxResolution, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &ImageExportDialog::currentResolutionIndexChanged);
  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &ImageExportDialog::buttonBoxClicked);

  // Reload widget states
  restoreState();
}

ImageExportDialog::~ImageExportDialog()
{
  atools::gui::WidgetState(optionPrefix).save(this);

  delete ui;
}

QSize ImageExportDialog::getSize() const
{
  ResolutionIndex index = static_cast<ResolutionIndex>(ui->comboBoxResolution->currentIndex());
  if(index == CURRENT_MAP_VIEW)
    return QSize(curWidth, curHeight);
  else if(index == CUSTOM_RESOLUTION)
    return QSize(ui->spinBoxWidth->value(), ui->spinBoxHeight->value());
  else
  {
    std::pair<int, int> size = resolutions.value(index);
    return QSize(size.first, size.second);
  }
}

bool ImageExportDialog::isCurrentView() const
{
  return ui->comboBoxResolution->currentIndex() == CURRENT_MAP_VIEW;
}

bool ImageExportDialog::isAvoidBlurredMap() const
{
  return ui->checkBoxAvoidBlurred->isChecked();
}

void ImageExportDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(
      parentWidget(), lnm::helpOnlineUrl + "IMAGEEXPORT.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void ImageExportDialog::saveState()
{
  atools::gui::WidgetState widgetState(optionPrefix, false);
  widgetState.save({
    this,
    ui->comboBoxResolution,
    ui->spinBoxWidth,
    ui->spinBoxHeight,
    ui->checkBoxAvoidBlurred
  });
}

void ImageExportDialog::restoreState()
{
  atools::gui::WidgetState widgetState(optionPrefix, false);
  widgetState.restore({
    this,
    ui->comboBoxResolution,
    ui->spinBoxWidth,
    ui->spinBoxHeight,
    ui->checkBoxAvoidBlurred
  });
  currentResolutionIndexChanged();
}

void ImageExportDialog::currentResolutionIndexChanged()
{
  ResolutionIndex index = static_cast<ResolutionIndex>(ui->comboBoxResolution->currentIndex());
  ui->spinBoxWidth->setEnabled(index == CUSTOM_RESOLUTION);
  ui->spinBoxHeight->setEnabled(index == CUSTOM_RESOLUTION);
}
