/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "marker/distancemarkerdialog.h"

#include "common/constants.h"
#include "common/mapmarkers.h"
#include "common/mapresult.h"
#include "geo/pos.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"
#include "ui_distancemarkerdialog.h"

using atools::geo::Pos;

const QString DistanceMarkerDialog::DEFAULT_COLOR_STR = QColor(Qt::black).name(QColor::HexArgb);

DistanceMarkerDialog::DistanceMarkerDialog(QWidget *parent, const map::DistanceMarker& markerParam, const map::MapResult& result,
                                           bool editMode)
  : MarkerDialog(parent, tr("Measurement Line"), markerParam, result, editMode), ui(new Ui::DistanceMarkerDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  // Assign buttons for base class functions
  setDoNotShowCheckbox(ui->checkBoxDistanceMarkerDoNotShow);
  setPushButtonColor(ui->pushButtonDistanceMarkerColor);
  setLabelHeader(ui->labelDistanceMarkerHeader);

  // Default is OK button
  ui->buttonBoxDistanceMarker->button(QDialogButtonBox::Ok)->setDefault(true);

  // Fill text and focus line input
  ui->lineEditDistanceMarkerLabel->setText(marker->text);
  ui->lineEditDistanceMarkerLabel->setFocus();

  connect(ui->buttonBoxDistanceMarker, &QDialogButtonBox::clicked, this, &DistanceMarkerDialog::buttonBoxClicked);
  connect(ui->pushButtonDistanceMarkerColor, &QPushButton::clicked, this, &MarkerDialog::colorButtonClicked);

  restoreState();
}

DistanceMarkerDialog::~DistanceMarkerDialog()
{
  // Save dialog position and size and checkbox state
  atools::gui::WidgetState(lnm::DISTANCE_MARKER_DIALOG).save(QList<const QObject *>({this, ui->checkBoxDistanceMarkerDoNotShow}));
  delete ui;
}

void DistanceMarkerDialog::buttonBoxClicked(QAbstractButton *button)
{
  atools::gui::WidgetState(lnm::DISTANCE_MARKER_DIALOG).save(this);

  if(button == ui->buttonBoxDistanceMarker->button(QDialogButtonBox::Ok))
  {
    QDialog::accept();
    saveState();
  }
  else if(button == ui->buttonBoxDistanceMarker->button(QDialogButtonBox::Help))
    // Keep dialog open
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "MEASURE.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxDistanceMarker->button(QDialogButtonBox::Cancel))
    QDialog::reject();
  else if(button == ui->buttonBoxDistanceMarker->button(QDialogButtonBox::Reset))
  {
    marker->color = QColor(DEFAULT_COLOR_STR);
    updateButtonColor();
    ui->lineEditDistanceMarkerLabel->clear();
  }
}

void DistanceMarkerDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::DISTANCE_MARKER_DIALOG);

  // Load dialog position and size
  widgetState.restore(this);

  if(isEditMode())
  {
    widgetState.restore(ui->checkBoxDistanceMarkerDoNotShow);
    markerToWidgets();
  }
  else
  {
    textPreFilled = !marker->text.isEmpty();

    if(textPreFilled)
      // Text already given by navaid - do note restore line edit
      widgetState.restore(ui->checkBoxDistanceMarkerDoNotShow);
    else
      // Also restore default text for line edit
      widgetState.restore({ui->checkBoxDistanceMarkerDoNotShow, ui->lineEditDistanceMarkerLabel});

    // Color is save directly in marker
    marker->color = getSavedColor();

    if(textPreFilled)
      // Pre-filled text to widget
      ui->lineEditDistanceMarkerLabel->setText(marker->text);

    widgetsToMarker();
  }

  updateHeader();
  updateButtonColor();
}

void DistanceMarkerDialog::saveState() const
{
  atools::gui::WidgetState widgetState(lnm::DISTANCE_MARKER_DIALOG);

  if(textPreFilled)
    // Do not save pre-filled text from navaid
    widgetState.save({this, ui->checkBoxDistanceMarkerDoNotShow});
  else
    widgetState.save({this, ui->lineEditDistanceMarkerLabel, ui->checkBoxDistanceMarkerDoNotShow});

  atools::settings::Settings::instance().setValue(lnm::DISTANCE_MARKER_DIALOG_COLOR, marker->color.name(QColor::HexArgb));
}

QColor DistanceMarkerDialog::getSavedColor()
{
  return atools::settings::Settings::instance().valueStr(lnm::DISTANCE_MARKER_DIALOG_COLOR, DEFAULT_COLOR_STR);
}

void DistanceMarkerDialog::widgetsToMarker()
{
  if(!ui->lineEditDistanceMarkerLabel->text().isEmpty() && marker->text != ui->lineEditDistanceMarkerLabel->text())
  {
    // Label in widget is not empty and different to marker - set manually
    marker->text = ui->lineEditDistanceMarkerLabel->text();
    marker->manualLabel = true;
  }
  else if(ui->lineEditDistanceMarkerLabel->text().isEmpty())
  {
    // Label is empty - generate label from navaid
    marker->text = getResult()->markerLabel();
    marker->manualLabel = false;
  }
}

void DistanceMarkerDialog::markerToWidgets()
{
  ui->lineEditDistanceMarkerLabel->setText(marker->text);
}
