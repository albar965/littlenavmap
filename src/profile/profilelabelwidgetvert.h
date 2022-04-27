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

#ifndef LNM_PROFILELABELWIDGETVERT_H
#define LNM_PROFILELABELWIDGETVERT_H

#include <QWidget>

class ProfileWidget;
class ProfileScrollArea;

/*
 * Used to draw the left vertical label area besides the scrollable elevation profile widget.
 */
class ProfileLabelWidgetVert :
  public QWidget
{
  Q_OBJECT

public:
  explicit ProfileLabelWidgetVert(ProfileWidget *parent, ProfileScrollArea *profileScrollArea);
  virtual ~ProfileLabelWidgetVert() override;

  void routeChanged();
  void optionsChanged();

private:
  virtual void paintEvent(QPaintEvent *) override;

  /* Pass context menu to the profile widget */
  virtual void contextMenuEvent(QContextMenuEvent *event) override;

  ProfileWidget *profileWidget;
  ProfileScrollArea *scrollArea;
};

#endif // LNM_PROFILELABELWIDGETVERT_H
