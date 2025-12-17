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

#ifndef LNM_STYLEHANDLER_H
#define LNM_STYLEHANDLER_H

#include <QPalette>
#include <QList>
#include <QObject>

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

  StyleHandler(const StyleHandler& other) = delete;
  StyleHandler& operator=(const StyleHandler& other) = delete;

  /* Insert menu items for all found styles to submenu. */
  void insertMenuItems(QMenu *menu);

  /* Save and load last used style */
  void saveState() const;
  void restoreState();

  /* Name like "Dark" or "Fusion" */
  QString getCurrentGuiStyleDisplayName() const;

  /* true if style requires darkening the map */
  bool isGuiStyleDark() const;

  const static QLatin1String STYLE_FUSION; /* Fusion */
  const static QLatin1String STYLE_DARK; /* Dark / Fusion */
  const static QLatin1String STYLE_WINDOWSVISTA; /* Windows 10 and 11 */
  const static QLatin1String STYLE_WINDOWS; /* Old Windows */

signals:
  /* Sent on change */
  void preStyleChange(const QString& name, bool dark);
  void styleChanged(const QString& name, bool dark);

private:
  struct Style
  {
    Style(const QString& displayNameParam, const QString& styleNameParam, const QString& stylesheetParam,
          const QPalette& paletteParam, bool darkParam) :
      displayName(displayNameParam), styleName(styleNameParam), stylesheet(stylesheetParam), palette(paletteParam), dark(darkParam)
    {
    }

    QString displayName, styleName, stylesheet;
    QPalette palette;
    bool dark;
  };

  void applyCurrentStyle();
  void createGrayPalette(QPalette& palette);
  void createDarkPalette(QPalette& palette);
  void menuItemTriggered();

  /* All system and custom styles */
  QList<Style> styles;
  /* Currently selected as in menu order */
  int currentStyleIndex = 0;

  /* Menus*/
  QActionGroup *styleActionGroup = nullptr;
  QList<QAction *> menuItems;
  QMainWindow *mainWindow;
};

#endif // LNM_STYLEHANDLER_H
