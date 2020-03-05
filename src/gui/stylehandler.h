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

#ifndef LNM_STYLEHANDLER_H
#define LNM_STYLEHANDLER_H

#include <QPalette>
#include <QVector>

class QMenu;
class QPalette;
class QActionGroup;
class QAction;
class QMainWindow;

/*
 * Creates additonal GUI styles and provides function and menu items to switch between them.
 *
 * Also maintains the style configuration files.
 */
class StyleHandler :
  public QObject
{
  Q_OBJECT

public:
  StyleHandler(QMainWindow *mainWindowParam);
  virtual ~StyleHandler() override;

  /* Insert menu items for all found styles to submenu. */
  void insertMenuItems(QMenu *menu);

  /* Save and load last used style */
  void saveState();
  void restoreState();

  /* Name like "Night" or "Fusion" */
  QString getCurrentGuiStyleDisplayName();

  /* true if style requires darkening the map */
  bool isCurrentGuiStyleNight();

signals:
  /* Sent on change */
  void preStyleChange(const QString& name, bool night);
  void styleChanged(const QString& name, bool night);

private:
  struct Style
  {
    QString displayName, styleName, stylesheet;
    QPalette palette;
    bool night;
  };

  void applyCurrentStyle();
  void createGrayPalette(QPalette& palette);
  void createDarkPalette(QPalette& palette);
  void menuItemTriggered();

  /* All system and custom styles */
  QVector<Style> styles;
  /* Currently selected as in menu order */
  int currentStyleIndex = 0;

  /* Menus*/
  QActionGroup *styleActionGroup = nullptr;
  QVector<QAction *> menuItems;
  QMainWindow *mainWindow;
};

#endif // LNM_STYLEHANDLER_H
