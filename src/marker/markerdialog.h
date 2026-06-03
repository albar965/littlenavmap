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

#ifndef LNM_MARKER_DIALOG_H
#define LNM_MARKER_DIALOG_H

#include "common/mapflags.h"

#include <QDialog>
#include <QCheckBox>
#include <QApplication>

class QLabel;
class QWidget;
class QPushButton;

namespace atools {
namespace geo {
class Pos;
}
}
namespace map {
struct MapResult;
}

/* Marker dialog private parts not depending on template parameters */
class MarkerDialogPrivate
{
  Q_DECLARE_TR_FUNCTIONS(MarkerDialogInternal)

public:
  MarkerDialogPrivate(QWidget *parent, const QString& titleType, const map::MapResult& resultParam, bool editModeParam);
  ~MarkerDialogPrivate();

  void updateHeader(map::MapType navType, atools::geo::Pos& pos);
  void colorButtonClicked(QColor& markerColorRef);
  void updateButtonColor(QColor& markerColorRef);

  map::MapResult *result;
  QPushButton *pushButtonColor = nullptr;
  QLabel *labelHeader = nullptr;
  QCheckBox *doNotShowCheckbox = nullptr;
  QWidget *parentWidget;
  bool editMode = false, dialogOpened = false;
  QString title;
};

/*
 * Template base class for all marker edit dialog windows
 */
template<typename TYPE>
class MarkerDialog :
  public QDialog
{
public:
  explicit MarkerDialog(QWidget *parent, const QString& titleType, const TYPE& markerParam, const map::MapResult& result,
                        bool editModeParam)
    : QDialog(parent)
  {
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(QApplication::applicationName() % (editModeParam ? tr(" - Edit %1") : tr(" - Add %1")).arg(titleType));

    p = new MarkerDialogPrivate(parent, titleType, result, editModeParam);
    marker = new TYPE;
    *marker = markerParam;
  }

  virtual ~MarkerDialog() override
  {
    delete p;
    delete marker;
  }

  /* Do not copy */
  MarkerDialog(const MarkerDialog& other) = delete;
  MarkerDialog& operator=(const MarkerDialog& other) = delete;

  /* forceShow shows the dialog even if the "do not show again..." box is checked */
  int execMarkerDialog(bool forceShow, bool *disableUpdates)
  {
    // Show if forced or if "do not show box" is not clicked
    if(forceShow || !isDoNoShowEnabled())
    {
      p->dialogOpened = true;

      // Disable background updates to avoid focus loss events
      if(disableUpdates != nullptr)
        *disableUpdates = true;
      int retval = QDialog::exec();
      if(disableUpdates != nullptr)
        *disableUpdates = false;
      return retval;
    }
    else
      return QDialog::Accepted;
  }

  /* Copy widget contents to marker and return it */
  const TYPE& getMarker()
  {
    widgetsToMarker();
    return *marker;
  }

  void colorButtonClicked()
  {
    p->colorButtonClicked(marker->getColor());
  }

protected:
  /* Copy marker to widgets */
  virtual void markerToWidgets() = 0;

  /* Restore from settings and partially copy to marker and widgets depending on edit mode*/
  virtual void restoreState() = 0;

  /* Save widget state to settings */
  virtual void saveState() const = 0;

  /* Copy widget state to marker */
  virtual void widgetsToMarker() = 0;

  /* Edit or add mode */
  bool isEditMode() const
  {
    return p->editMode;
  }

  const map::MapResult *getResult()
  {
    return p->result;
  }

  void updateButtonColor()
  {
    p->updateButtonColor(marker->getColor());
  }

  void setPushButtonColor(QPushButton *newPushButtonColor)
  {
    p->pushButtonColor = newPushButtonColor;
  }

  void setLabelHeader(QLabel *newLabelHeader)
  {
    p->labelHeader = newLabelHeader;
  }

  void setDoNotShowCheckbox(QCheckBox *newDoNotShowCheckbox)
  {
    p->doNotShowCheckbox = newDoNotShowCheckbox;
  }

  void updateHeader()
  {
    p->updateHeader(marker->getNav().type, marker->position);
  }

  bool isDoNoShowEnabled()
  {
    return p->doNotShowCheckbox != nullptr ? p->doNotShowCheckbox->isChecked() : false;
  }

  TYPE *marker;

private:
  MarkerDialogPrivate *p;

  /* Make private to disable call from outside */
  using QDialog::exec;
};

#endif // LNM_MARKER_DIALOG_H
