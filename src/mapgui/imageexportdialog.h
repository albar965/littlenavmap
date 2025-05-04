/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_IMAGEEXPORTDIALOG_H
#define LNM_IMAGEEXPORTDIALOG_H

#include <QDialog>

namespace Ui {
class ImageExportDialog;
}

class QAbstractButton;

/*
 * Dialog that allows to select the resolution when exporting map images
 */
class ImageExportDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* Pass current map size for original view option */
  explicit ImageExportDialog(QWidget *parent, const QString& titleParam, const QString& optionPrefixParam, const QSize& size);
  virtual ~ImageExportDialog() override;

  ImageExportDialog(const ImageExportDialog& other) = delete;
  ImageExportDialog& operator=(const ImageExportDialog& other) = delete;

  /* Get selected size */
  QSize getSize() const;

  /* Checkbox: Zoom out once to avoid blurry map */
  bool isAvoidBlurredMap() const;

  /* true if image should be saved as is in map view */
  bool isCurrentView() const;

private:
  void currentResolutionIndexChanged();
  void buttonBoxClicked(QAbstractButton *button);
  void saveState() const;
  void restoreState();

  /* Prefix for save widget states */
  Ui::ImageExportDialog *ui;
  QString optionPrefix;
  QSize currentSize;
};

#endif // LNM_IMAGEEXPORTDIALOG_H
