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

#include "gui/stylehandler.h"
#include "gui/palettesettings.h"
#include "settings/settings.h"
#include "common/constants.h"

#include <QStyle>
#include <QMenu>
#include <QStyleFactory>
#include <QApplication>
#include <QPixmapCache>
#include <QStyleFactory>
#include <QActionGroup>
#include <QDebug>
#include <QMainWindow>
#include <QWindow>

using atools::settings::Settings;

StyleHandler::StyleHandler(QMainWindow *mainWindowParam)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{

  // Override goofy fusion tab close buttons on Windows and macOS
#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)

  QString fusionStyleSheet(
    QLatin1Literal("QTabBar::close-button "
                   "{ image: url(:/littlenavmap/resources/icons/tab_close_button.png); }") +
    QLatin1Literal("QTabBar::close-button:hover "
                   "{ image: url(:/littlenavmap/resources/icons/tab_close_button_hover.png); }"));
#else
  QString fusionStyleSheet;
#endif

  // Collect names and palettes from all system styles
  for(const QString& styleName : QStyleFactory::keys())
  {
    QStyle *style = QStyleFactory::create(styleName);

    QPalette palette = style->standardPalette();
    QString stylesheet;
    if(styleName == "Fusion")
    {
      // Store fusion palette settings a in a separate ini file
      QString filename = Settings::instance().getConfigFilename("_fusionstyle.ini");
      atools::gui::PaletteSettings paletteSettings(filename, "StyleColors");
      paletteSettings.syncPalette(palette);
      stylesheet = fusionStyleSheet;
    }

    styles.append({styleName, styleName, stylesheet, palette, false /* night */});
    delete style;
  }

  QPalette darkPalette;
  createDarkPalette(darkPalette);

  // Add stylesheet for better checkbox radio button and toolbutton visibility
  QString nightStyleSheet(
    QLatin1Literal("QCheckBox::indicator:checked "
                   "{ image: url(:/littlenavmap/resources/icons/checkbox_dark_checked.png); }") +
    QLatin1Literal("QCheckBox::indicator:unchecked "
                   "{ image: url(:/littlenavmap/resources/icons/checkbox_dark_unchecked.png); }") +
    QLatin1Literal("QRadioButton::indicator:checked "
                   "{ image: url(:/littlenavmap/resources/icons/radiobutton_dark_checked.png); }") +
    QLatin1Literal("QRadioButton::indicator:unchecked "
                   "{ image: url(:/littlenavmap/resources/icons/radiobutton_dark_unchecked.png); }") +
    // Night mode shows bright tab bars with this change in macOS
#if !defined(Q_OS_MACOS)
    QLatin1Literal("QTabBar::close-button "
                   "{ image: url(:/littlenavmap/resources/icons/tab_close_button_night.png); }") +
    QLatin1Literal("QTabBar::close-button:hover "
                   "{ image: url(:/littlenavmap/resources/icons/tab_close_button_hover_night.png); }") +
#endif
    QString("QToolButton:checked { background-color: %1;}").arg(darkPalette.color(QPalette::Window).lighter(600).name())
    );

  // Store dark palette settings a in a separate ini file
  QString filename = Settings::instance().getConfigFilename("_nightstyle.ini");
  atools::gui::PaletteSettings paletteSettings(filename, "StyleColors");
  paletteSettings.syncPalette(darkPalette);

  styles.append({"Night", "Fusion", nightStyleSheet, darkPalette, true /* night */});
}

StyleHandler::~StyleHandler()
{
  delete styleActionGroup;
}

QString StyleHandler::getCurrentGuiStyleDisplayName()
{
  return styles.at(currentStyleIndex).displayName;
}

bool StyleHandler::isCurrentGuiStyleNight()
{
  return styles.at(currentStyleIndex).night;
}

void StyleHandler::insertMenuItems(QMenu *menu)
{
  delete styleActionGroup;
  styleActionGroup = new QActionGroup(menu);

  int index = 0;
  for(const Style& style : styles)
  {
    QAction *action = new QAction(style.displayName);
    action->setData(index);
    action->setCheckable(true);
    action->setChecked(index == currentStyleIndex);
    action->setStatusTip(tr("Switch user interface style to \"%1\"").arg(style.displayName));
    action->setActionGroup(styleActionGroup);

    if(style.displayName == "Fusion")
    {
      action->setShortcut(QKeySequence(tr("Shift+F2")));
      action->setShortcutContext(Qt::ApplicationShortcut);
    }
    else if(style.displayName == "Night")
    {
      action->setShortcut(QKeySequence(tr("Shift+F3")));
      action->setShortcutContext(Qt::ApplicationShortcut);
    }

    menu->addAction(action);
    menuItems.append(action);

    connect(action, &QAction::triggered, this, &StyleHandler::menuItemTriggered);
    index++;
  }
}

void StyleHandler::menuItemTriggered()
{
  QAction *action = dynamic_cast<QAction *>(sender());

  if(action != nullptr)
  {
    currentStyleIndex = action->data().toInt();
    applyCurrentStyle();
  }
}

void StyleHandler::applyCurrentStyle()
{
  qDebug() << Q_FUNC_INFO << "index" << currentStyleIndex;

  const Style& style = styles.at(currentStyleIndex);
  emit preStyleChange(style.displayName, style.night);

  QApplication::setStyle(QStyleFactory::create(style.styleName));

  qApp->setPalette(style.palette);
  qApp->setStyleSheet(style.stylesheet);

  // Need to clear due to Qt bug
  QPixmapCache::clear();

  emit styleChanged(style.displayName, style.night);
}

void StyleHandler::saveState()
{
  Settings& settings = Settings::instance();
  settings.setValue(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX, currentStyleIndex);
}

void StyleHandler::restoreState()
{
  Settings& settings = Settings::instance();

  if(settings.contains(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX))
  {
    // Style already selected
    currentStyleIndex = settings.valueInt(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX);
    if(currentStyleIndex >= 0 && currentStyleIndex < styles.size())
      menuItems[currentStyleIndex]->setChecked(true);
    else
    {
      qWarning() << Q_FUNC_INFO << "Invalid style index" << currentStyleIndex;
      currentStyleIndex = 0;
      menuItems[0]->setChecked(true);
    }
  }
  else
  {
    // Select default style - first startup ==================================

    // Look for default style in the list
    int index = 0;
    QString currentStyleName = QApplication::style()->objectName();
    for(const QString& styleName : QStyleFactory::keys())
    {
      if(styleName.compare(currentStyleName, Qt::CaseInsensitive) == 0)
      {
        if(index >= menuItems.size())
          index = 0;

        currentStyleIndex = index;
        menuItems[currentStyleIndex]->setChecked(true);
        break;
      }
      index++;
    }
  }

  applyCurrentStyle();
}

void StyleHandler::createDarkPalette(QPalette& palette)
{
  // Create dark palette colors for all groups
  palette = QGuiApplication::palette();
  palette.setColor(QPalette::Window, QColor(15, 15, 15));
  palette.setColor(QPalette::WindowText, QColor(200, 200, 200));
  palette.setColor(QPalette::Shadow, QColor(0, 0, 0));
  palette.setColor(QPalette::Base, QColor(20, 20, 20));
  palette.setColor(QPalette::AlternateBase, QColor(35, 35, 35));
  palette.setColor(QPalette::ToolTipBase, QColor(35, 35, 35));
  palette.setColor(QPalette::ToolTipText, QColor(200, 200, 200));
  palette.setColor(QPalette::Text, QColor(200, 200, 200));
  palette.setColor(QPalette::Button, QColor(35, 35, 35));
  palette.setColor(QPalette::ButtonText, QColor(200, 200, 200));
  palette.setColor(QPalette::BrightText, QColor(250, 250, 250));
  palette.setColor(QPalette::Link, QColor(42, 130, 218));
  palette.setColor(QPalette::LinkVisited, QColor(62, 160, 248));

  palette.setColor(QPalette::Light, QColor(85, 85, 85));
  palette.setColor(QPalette::Midlight, QColor(65, 65, 65));
  palette.setColor(QPalette::Mid, QColor(20, 20, 20));
  palette.setColor(QPalette::Dark, QColor(5, 5, 5));

  palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  palette.setColor(QPalette::HighlightedText, Qt::black);

  // Create dark palette colors for disabled group
  // Active, //Disabled, //Inactive,
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
  palette.setColor(QPalette::Disabled, QPalette::Button, QColor(65, 65, 65));
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
  palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 100, 100));

  // darkPalette.setColor(QPalette::Active, QPalette::Text, QColor(100, 100, 100));
  // darkPalette.setColor(QPalette::Active, QPalette::ButtonText, QColor(100, 100, 100));
  // darkPalette.setColor(QPalette::Inactive, QPalette::Text, QColor(100, 100, 100));
  // darkPalette.setColor(QPalette::Inactive, QPalette::ButtonText, QColor(100, 100, 100));
}
