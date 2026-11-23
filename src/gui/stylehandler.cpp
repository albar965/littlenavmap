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

#include "gui/stylehandler.h"

#include "atools.h"
#include "common/constants.h"
#include "gui/application.h"
#include "gui/palettesettings.h"
#include "gui/simplewaitdialog.h"
#include "settings/settings.h"

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
#include <QStringBuilder>
#include <QStyleHints>
#include <QToolTip>
#include <QToolBar>
#include <QAbstractButton>
#include <QPushButton>
#include <QToolButton>

const QLatin1String StyleHandler::STYLE_FUSION("Fusion");
const QLatin1String StyleHandler::STYLE_DARK("Dark");
const QLatin1String StyleHandler::STYLE_WINDOWSVISTA("windowsvista");
const QLatin1String StyleHandler::STYLE_WINDOWS("Windows");
const QLatin1String StyleHandler::STYLE_MACOS("macOS");

const static QLatin1String STYLE_COLOR_GROUP("StyleColors");

using atools::settings::Settings;
using atools::gui::PaletteSettings;

// ============================================
/* Immutable internal class describing a style */
class StyleDescription
{
public:
  StyleDescription(const QString& displayNameParam, const QString& styleNameParam, const QString& stylesheetParam,
                   const QPalette& paletteParam, bool darkParam)
    : displayName(displayNameParam), styleName(styleNameParam), stylesheet(stylesheetParam), palette(paletteParam),
    dark(darkParam), paletteValid(true)
  {
  }

  StyleDescription(const QString& displayNameParam, const QString& styleNameParam, const QString& stylesheetParam,
                   bool darkParam)
    : displayName(displayNameParam), styleName(styleNameParam), stylesheet(stylesheetParam),
    dark(darkParam), paletteValid(false)
  {
  }

  const QString& getDisplayName() const
  {
    return displayName;
  }

  const QString& getStyleName() const
  {
    return styleName;
  }

  const QString& getStylesheet() const
  {
    return stylesheet;
  }

  const QPalette& getPalette() const
  {
    return palette;
  }

  bool isDark() const
  {
    return dark;
  }

  bool isPaletteValid() const
  {
    return paletteValid;
  }

private:
  QString displayName, styleName, stylesheet;
  QPalette palette;
  bool dark, paletteValid;
};

// =============================================================================
StyleHandler::StyleHandler(QMainWindow *mainWindowParam)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{
  // Owned by QGuiApplication
  styleHints = QGuiApplication::styleHints();

  // Get unchanged system palette
  systemPalette = QGuiApplication::palette();

  qDebug() << Q_FUNC_INFO << "Default style name" << QApplication::style()->name();

  // Catch dark/light mode changes
  connect(styleHints, &QStyleHints::colorSchemeChanged, this, &StyleHandler::colorSchemeChanged);

  // Send void styleChanged() on palette changes
  connect(atools::gui::Application::applicationInstance(), &atools::gui::Application::paletteChanged, this, &StyleHandler::paletteChanged);

  // Collect names and palettes from all system styles ===================================
  for(const QString& styleName : QStyleFactory::keys())
  {
    qDebug() << Q_FUNC_INFO << "Found style" << styleName;
    const QStyle *style = QStyleFactory::create(styleName);

    if(style != nullptr)
    {
      if(styleName == STYLE_WINDOWS || styleName == STYLE_WINDOWSVISTA || styleName == STYLE_MACOS)
        continue;

      if(styleName == STYLE_FUSION)
      {
        // Try to load fusion palette settings from a ini file in settings
        const QString paletteFile = Settings::getConfigFilename(lnm::FUSIONSTYLE_INI_SUFFIX);
        QPalette stylePalette;

        if(atools::checkFile(Q_FUNC_INFO, paletteFile, false /* warn */))
        {
          qDebug() << Q_FUNC_INFO << "Loading Fusion palette from" << paletteFile;

          // Palette file already exists - sync and assign it to Fusion
          PaletteSettings paletteSettings(paletteFile, STYLE_COLOR_GROUP);
          stylePalette = style->standardPalette();
          paletteSettings.syncPalette(stylePalette);
        }
        else
        {
          qDebug() << Q_FUNC_INFO << "Loading Fusion palette from resources";

          // Palette missing - load bright default from resources
          // Load fusion palette which is always bright, also with dark system settings
          PaletteSettings(lnm::FUSIONSTYLE_INI, STYLE_COLOR_GROUP).
          loadPalette(brightFusionPalette);
          stylePalette = brightFusionPalette;
        }

        styleDescriptions.append(StyleDescription(styleName, styleName, QStringLiteral(), stylePalette, false /* dark */));
        fusionStyleIndex = styleDescriptions.size() - 1;
      }
      else
        styleDescriptions.append(StyleDescription(styleName, styleName, QStringLiteral(), false /* dark */));

      delete style;
    }
    else
      qWarning() << Q_FUNC_INFO << "Style is null" << styleName;
  }

  // Create dark style ==================================================================
  // This is not a Qt style but a copy of Fusion

  // Try to load fusion palette settings from a ini file in settings
  const QString paletteFile = Settings::getConfigFilename(lnm::DARKSTYLE_INI_SUFFIX);
  QPalette darkPalette;

  if(atools::checkFile(Q_FUNC_INFO, paletteFile, false /* warn */))
  {
    qDebug() << Q_FUNC_INFO << "Loading Dark palette from" << paletteFile;

    // Palette file already exists - sync and assign it to dark
    PaletteSettings(paletteFile, STYLE_COLOR_GROUP).syncPalette(darkPalette);
  }
  else
  {
    qDebug() << Q_FUNC_INFO << "Loading Dark palette from resources";

    // Palette missing - load dark default from resources
    PaletteSettings(lnm::DARKSTYLE_INI, STYLE_COLOR_GROUP).loadPalette(darkPalette);
  }

  // Add stylesheet for better checkbox radio button and toolbutton visibility
  QString darkStyleSheet(
    // Menu item separator ===============
    QStringLiteral("QMenu::separator { background-color: %1; }").arg(darkPalette.color(QPalette::Light).name()) %

    // Toolbutton checked background ===============
    QStringLiteral("QToolButton:checked { background-color: %1; }").arg(darkPalette.color(QPalette::Light).name()) %
    QStringLiteral("QPushButton:checked { background-color: %1; }").arg(darkPalette.color(QPalette::Light).name()) %
    QStringLiteral("QMenu::item:selected { border-color: lightgray; background: %1; }").arg(darkPalette.color(QPalette::Highlight).name()) %
    QStringLiteral("QGroupBox {background-color: %2; }").arg(darkPalette.color(QPalette::Mid).name()) %

    // Separator lines =================
    // QStringLiteral("QFrame[frameShape=\"4\"], QFrame[frameShape=\"5\"] { color: #555555; }") %

    // Menu padding ====================
    "QMenu::item { padding: 2px 2px; }"

    // Thicker frame for selected menu items with icons ====================
    "QMenu::icon:checked:enabled { background: #808080; border: 2px solid darkgray; border-radius: 2px; }"
    "QMenu::icon:checked:disabled { background: #808080; border: 2px solid dimgray; border-radius: 2px; }"

    // Checkbox images ====================
    // "QCheckBox::indicator:checked { image: url(:/littlenavmap/resources/icons/checkbox_dark_checked.png); }"
    // "QCheckBox::indicator:checked:!enabled { image: url(:/littlenavmap/resources/icons/checkbox_dark_checked_disabled.png); }"
    // "QCheckBox::indicator:unchecked { image: url(:/littlenavmap/resources/icons/checkbox_dark_unchecked.png); }"
    // "QCheckBox::indicator:unchecked:!enabled { image: url(:/littlenavmap/resources/icons/checkbox_dark_unchecked_disabled.png); }"

    // Tree view checkboxes ====================
    // "QTreeView::indicator:checked {image: url(:/littlenavmap/resources/icons/checkbox_dark_checked.png);}"
    // "QTreeView::indicator:checked:!enabled {image: url(:/littlenavmap/resources/icons/checkbox_dark_checked_disabled.png);}"
    // "QTreeView::indicator:unchecked {image: url(:/littlenavmap/resources/icons/checkbox_dark_unchecked.png);}"
    // "QTreeView::indicator:unchecked:!enabled {image: url(:/littlenavmap/resources/icons/checkbox_dark_unchecked_disabled.png);}"

    // Radio button images ====================
    // "QRadioButton::indicator:checked { image: url(:/littlenavmap/resources/icons/radiobutton_dark_checked.png); }"
    // "QRadioButton::indicator:checked:!enabled { image: url(:/littlenavmap/resources/icons/radiobutton_dark_checked_disabled.png); }"
    // "QRadioButton::indicator:unchecked { image: url(:/littlenavmap/resources/icons/radiobutton_dark_unchecked.png); }"
    // "QRadioButton::indicator:unchecked:!enabled { image: url(:/littlenavmap/resources/icons/radiobutton_dark_unchecked_disabled.png); }"

    // #if !defined(Q_OS_MACOS)
    //// Night mode shows bright tab bars with this change in macOS
    // "QTabBar::close-button { image: url(:/littlenavmap/resources/icons/close_dark.svg); }"
    // "QTabBar::close-button:hover { image: url(:/littlenavmap/resources/icons/close_dark_hover.svg); }"
    // #endif
    );

  styleDescriptions.append(StyleDescription(STYLE_DARK, STYLE_FUSION, QStringLiteral(), darkPalette, true /* dark */));
  darkStyleIndex = styleDescriptions.size() - 1;
}

StyleHandler::~StyleHandler()
{
  ATOOLS_DELETE(styleActionGroup);
  ATOOLS_DELETE_LOG(currentStyle);
}

void StyleHandler::logPalette(const QPalette& palette)
{
  qDebug() << "Palette =========================================";
  qDebug() << "Active ====================";
  qDebug() << Q_FUNC_INFO << "Window" << palette.color(QPalette::Active, QPalette::Window).name(QColor::HexRgb)
           << "WindowText" << palette.color(QPalette::Active, QPalette::WindowText).name(QColor::HexRgb)
           << "Base" << palette.color(QPalette::Active, QPalette::Base).name(QColor::HexRgb)
           << "AlternateBase" << palette.color(QPalette::Active, QPalette::AlternateBase).name(QColor::HexRgb);

  qDebug() << Q_FUNC_INFO << "ToolTipBase" << palette.color(QPalette::Active, QPalette::ToolTipBase).name(QColor::HexRgb)
           << "ToolTipText" << palette.color(QPalette::Active, QPalette::ToolTipText).name(QColor::HexRgb)
           << "PlaceholderText" << palette.color(QPalette::Active, QPalette::PlaceholderText).name(QColor::HexRgb);

  qDebug() << Q_FUNC_INFO << "Text" << palette.color(QPalette::Active, QPalette::Text).name(QColor::HexRgb)
           << "Button" << palette.color(QPalette::Active, QPalette::Button).name(QColor::HexRgb)
           << "ButtonText" << palette.color(QPalette::Active, QPalette::ButtonText).name(QColor::HexRgb)
           << "BrightText" << palette.color(QPalette::Active, QPalette::BrightText).name(QColor::HexRgb);

  qDebug() << "Inactive ====================";
  qDebug() << Q_FUNC_INFO << "Window" << palette.color(QPalette::Inactive, QPalette::Window).name(QColor::HexRgb)
           << "WindowText" << palette.color(QPalette::Inactive, QPalette::WindowText).name(QColor::HexRgb)
           << "Base" << palette.color(QPalette::Inactive, QPalette::Base).name(QColor::HexRgb)
           << "AlternateBase" << palette.color(QPalette::Inactive, QPalette::AlternateBase).name(QColor::HexRgb);

  qDebug() << Q_FUNC_INFO << "ToolTipBase" << palette.color(QPalette::Inactive, QPalette::ToolTipBase).name(QColor::HexRgb)
           << "ToolTipText" << palette.color(QPalette::Inactive, QPalette::ToolTipText).name(QColor::HexRgb)
           << "PlaceholderText" << palette.color(QPalette::Inactive, QPalette::PlaceholderText).name(QColor::HexRgb);

  qDebug() << Q_FUNC_INFO << "Text" << palette.color(QPalette::Inactive, QPalette::Text).name(QColor::HexRgb)
           << "Button" << palette.color(QPalette::Inactive, QPalette::Button).name(QColor::HexRgb)
           << "ButtonText" << palette.color(QPalette::Inactive, QPalette::ButtonText).name(QColor::HexRgb)
           << "BrightText" << palette.color(QPalette::Inactive, QPalette::BrightText).name(QColor::HexRgb);
}

void StyleHandler::applyCurrentStyleInitial()
{
  applyCurrentStyle(true /* initial */);
  updateMenuStatus();
}

void StyleHandler::initStyle()
{
  // Application takes ownership
  QApplication::setStyle(QStyleFactory::create(STYLE_FUSION));
}

QString StyleHandler::getCurrentGuiStyleDisplayName() const
{
  return styleDescriptions.at(styleIndex()).getDisplayName();
}

bool StyleHandler::isGuiStyleDark() const
{
  if(automaticStyle)
  {
    Qt::ColorScheme colorScheme = QGuiApplication::styleHints()->colorScheme();
    if(colorScheme == Qt::ColorScheme::Unknown)
    {
      // Default constructor builds system palette which is unchanged by styles
      QPalette systemPalette;

      // Dark if text is brighter than background
      return systemPalette.color(QPalette::WindowText).lightness() > systemPalette.color(QPalette::Window).lightness();
    }
    else
      return colorScheme == Qt::ColorScheme::Dark;
  }
  else
    return styleDescriptions.at(styleIndex()).isDark();
}

bool StyleHandler::isGuiStyleAnyFusion() const
{
  const QString& name = styleDescriptions.at(styleIndex()).getStyleName();
  return name == STYLE_FUSION || name == STYLE_DARK;
}

void StyleHandler::applyStyleSheet(QMenu *toolButtonMenu, QString buttonStylesheet, QString menuStylesheet) const
{
  if(toolButtonMenu != nullptr)
  {
    if(buttonStylesheet.isEmpty() || menuStylesheet.isEmpty())
      styleSheets(buttonStylesheet, menuStylesheet);

    // Tool bar menu button =====================================
    toolButtonMenu->setStyleSheet(menuStylesheet);
    for(QAction *toolButtonMenuAction: toolButtonMenu->actions())
    {
      // Action for tool bar menu button =====================================
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "......" << toolButtonMenuAction->objectName() << toolButtonMenuAction->text();
#endif

      QAbstractButton *abstractButton = dynamic_cast<QAbstractButton *>(toolButtonMenuAction);
      if(abstractButton != nullptr)
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "........" << abstractButton->objectName() << abstractButton->text();
#endif

        abstractButton->setStyleSheet(buttonStylesheet);

        QPushButton *button = dynamic_cast<QPushButton *>(abstractButton);
        if(button != nullptr && !button->icon().isNull())
          button->setFlat(true);
      }
    }
  }
}

void StyleHandler::applyStyleSheet(QWidget *parentWidget, const QList<QWidget *>& buttonWidgets) const
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << isGuiStyleAnyFusion();
#endif

  QString buttonStylesheet, menuStylesheet;
  styleSheets(buttonStylesheet, menuStylesheet);

  // Get all toolbars and widgets to the respective actions
  if(parentWidget != nullptr)
  {
    for(QToolBar *toolBar : parentWidget->findChildren<QToolBar *>())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << toolBar->objectName();
#endif

      // All toolbar actions =====================================
      for(QAction *action : toolBar->actions())
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << ".." << action->objectName() << action->text();
#endif
        QWidget *widgetForAction = toolBar->widgetForAction(action);

        QToolButton *toolButton = dynamic_cast<QToolButton *>(widgetForAction);
        if(toolButton != nullptr)
        {
#ifdef DEBUG_INFORMATION
          qDebug() << Q_FUNC_INFO << "...." << toolButton->objectName() << toolButton->text();
#endif
          applyStyleSheet(toolButton->menu(), buttonStylesheet, menuStylesheet);
        }
        else
        {
          // Normal push or other button =====================================
          QAbstractButton *abstractButton = dynamic_cast<QAbstractButton *>(widgetForAction);
          if(abstractButton != nullptr)
          {
#ifdef DEBUG_INFORMATION
            qDebug() << Q_FUNC_INFO << "...." << abstractButton->objectName() << abstractButton->text();
#endif

            abstractButton->setStyleSheet(buttonStylesheet);

            QPushButton *pushButton = dynamic_cast<QPushButton *>(abstractButton);
            if(pushButton != nullptr && !pushButton->icon().isNull())
              pushButton->setFlat(true);
          }
        }
      }
    }

    // Set style to all menus =================================================
    for(QMenu *menu : parentWidget->findChildren<QMenu *>())
      applyStyleSheet(menu, buttonStylesheet, menuStylesheet);
  }

  // Get all other toolbar-like buttons ========================================
  for(QWidget *widget : std::as_const(buttonWidgets))
  {
    QPushButton *pushButton = dynamic_cast<QPushButton *>(widget);
    if(pushButton != nullptr && !pushButton->icon().isNull())
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "...." << pushButton->objectName() << pushButton->text();
#endif

      pushButton->setStyleSheet(buttonStylesheet);
      pushButton->setFlat(true);
    }
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "done";
#endif
}

void StyleHandler::styleSheets(QString& buttonStylesheet, QString& menuStyleSheet) const
{
  if(isGuiStyleAnyFusion())
  {
    // Build stylesheet with colors
    // Limit to QAbstractButton to avoid changing the menu items
    QPalette palette = QGuiApplication::palette();

    buttonStylesheet = QStringLiteral(
      "QAbstractButton { border: 1px solid transparent; padding: 1px; margin: 1px; }"
      "QAbstractButton:checked { border: 1px solid %1; padding: 1px; background: %2; margin: 1px; }"
      "QAbstractButton:hover { border: 1px solid %3; padding: 1px; margin: 1px; }").
                       arg(palette.color(QPalette::Active, QPalette::Mid).name(),
                           palette.color(QPalette::Active, QPalette::Midlight).name(),
                           palette.color(QPalette::Active, QPalette::Highlight).name());

    if(isGuiStyleDark())
    {
      menuStyleSheet =
        //// Menu item separator ===============
        QStringLiteral("QMenu::separator { background-color: %1; }").arg(palette.color(QPalette::Light).name()) %
        QStringLiteral("QMenu::item:selected { border-color: lightgray; background: %1; }").
        arg(palette.color(QPalette::Highlight).name()) %
        // Menu padding (internal) ====================
        "QMenu::item { padding: 3px 3px; margin-left: 3px; }"

        // Thicker frame for selected menu items with icons ====================
        "QMenu::icon:checked:enabled { background: #808080; border: 1px solid darkgray; border-radius: none; }"
        "QMenu::icon:checked:disabled { background: #808080; border: 1px solid dimgray; border-radius: none; }";
    }
  }
}

int StyleHandler::styleIndex() const
{
  int index = -1;
  if(automaticStyle)
    index = /*isSystemStyleDark() ? darkStyleIndex : */ fusionStyleIndex; // Fusion takes the system palette which covers dark too
  else
    index = currentStyleIndex;
  return index;
}

void StyleHandler::insertMenuItems(QMenu *menu)
{
  menuItemAuto = new QAction(tr("&Select automatically"), menu);
  menuItemAuto->setCheckable(true);
  menuItemAuto->setChecked(automaticStyle);
  menuItemAuto->setStatusTip(tr("Switch user interface style based on system settings"));
  menu->addAction(menuItemAuto);
  menu->addSeparator();
  connect(menuItemAuto, &QAction::triggered, this, &StyleHandler::menuItemAutoTriggered);

  delete styleActionGroup;
  styleActionGroup = new QActionGroup(menu);
  int index = 0;
  for(const StyleDescription& styleDescription : std::as_const(styleDescriptions))
  {
    QAction *action = new QAction('&' % styleDescription.getDisplayName(), menu);
    action->setData(index);
    action->setCheckable(true);
    action->setChecked(index == currentStyleIndex);
    action->setStatusTip(tr("Switch user interface style to \"%1\"").arg(styleDescription.getDisplayName()));
    action->setActionGroup(styleActionGroup);

    if(styleDescription.getDisplayName() == STYLE_FUSION)
    {
      action->setShortcut(QKeySequence(tr("Shift+F2")));
      action->setShortcutContext(Qt::ApplicationShortcut);
    }
    else if(styleDescription.getDisplayName() == STYLE_DARK)
    {
      action->setShortcut(QKeySequence(tr("Shift+F3")));
      action->setShortcutContext(Qt::ApplicationShortcut);
    }

    menu->addAction(action);
    menuItems.append(action);

    connect(action, &QAction::triggered, this, &StyleHandler::menuItemTriggered);
    index++;
  }

  updateMenuStatus();
}

void StyleHandler::updateMenuStatus()
{
  for(QAction *action : std::as_const(menuItems))
    action->setDisabled(automaticStyle);
}

void StyleHandler::menuItemAutoTriggered()
{
  automaticStyle = menuItemAuto->isChecked();
  updateMenuStatus();
  applyStyleInternal();
}

void StyleHandler::menuItemTriggered()
{
  QAction *action = dynamic_cast<QAction *>(sender());

  if(action != nullptr && currentStyleIndex != action->data().toInt())
  {
    currentStyleIndex = action->data().toInt();
    applyStyleInternal();
  }
}

void StyleHandler::colorSchemeChanged(Qt::ColorScheme colorScheme)
{
  qDebug() << Q_FUNC_INFO << colorScheme;
  applyStyleInternal();
}

void StyleHandler::applyStyleInternal()
{
  atools::gui::SimpleWaitDialog simpleWaitDialog(mainWindow, tr("Applying style ..."));
  applyCurrentStyle(false /* initial */);
}

void StyleHandler::paletteChanged(const QPalette&)
{
  if(!applyingStyle)
    emit styleChanged();
}

void StyleHandler::applyCurrentStyle(bool initial)
{
  qDebug() << Q_FUNC_INFO << "index" << currentStyleIndex << "automaticStyle" << automaticStyle;

  // Avoid recursion through palette signal when applying style
  applyingStyle = true;

  const StyleDescription& styleDescription = styleDescriptions.at(styleIndex());
  emit preStyleChange();

  if(initial)
    atools::gui::Application::showSplashScreenMessage(tr("Applying style ... "));

  // Ownership of the style object is transferred to QApplication
  QStyle *style = QStyleFactory::create(styleDescription.getStyleName());

  if(style != nullptr && (!automaticStyle || !initial))
  {
    qDebug() << Q_FUNC_INFO << "setStyle()" << "automaticStyle" << automaticStyle << "initial" << initial
             << "name" << style->name() << "display name" << styleDescription.getDisplayName();

    // Sets the widget's GUI style to style. The ownership of the style object is not transferred
    mainWindow->setStyle(style);
    ATOOLS_DELETE_LOG(currentStyle);
    currentStyle = style;

    // Set palette sends a signal which is blocked above in paletteChanged()
    QPalette palette;
    if(automaticStyle)
      // Unchanged system palette
      palette = systemPalette;
    else
    {
      if(styleDescription.isPaletteValid())
        palette = styleDescription.getPalette();
      else
        palette = style->standardPalette();
    }

    qDebug() << Q_FUNC_INFO << "setPalette()";
    QApplication::setPalette(palette);
    QToolTip::setPalette(QApplication::palette());
    logPalette(palette);
  }
  else
    qWarning() << Q_FUNC_INFO << "Style is null" << styleDescription.getStyleName();

  // Need to clear due to Qt bug
  QPixmapCache::clear();

#if defined(Q_OS_WIN32)
  // Workaround for Windows using automatic style and dark. Avoid dark links and blue alternate base.
  if(QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark && automaticStyle)
  {
    qDebug() << Q_FUNC_INFO << "Is Dark";
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::AlternateBase, palette.color(QPalette::Base).lighter(150));
    palette.setColor(QPalette::Link, palette.color(QPalette::Link).lighter(150));
    palette.setColor(QPalette::LinkVisited, palette.color(QPalette::LinkVisited).lighter(150));
    QApplication::setPalette(palette);
  }
#elif defined(Q_OS_MACOS)
  if(QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark && automaticStyle)
  {
    qDebug() << Q_FUNC_INFO << "Is Dark";
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::AlternateBase, palette.color(QPalette::Base).lighter(150));
    palette.setColor(QPalette::ToolTipText, palette.color(QPalette::WindowText));
    QApplication::setPalette(palette);
  }
#endif

  applyingStyle = false;

  emit styleChanged();
}

void StyleHandler::saveState() const
{
  Settings& settings = Settings::instance();
  settings.setValue(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX, currentStyleIndex);
  settings.setValue(lnm::OPTIONS_DIALOG_GUI_STYLE_AUTO, automaticStyle);
}

void StyleHandler::restoreState()
{
  Settings& settings = Settings::instance();

  if(menuItems.isEmpty())
  {
    qWarning() << Q_FUNC_INFO << "No styles found";
    return;
  }

  automaticStyle = settings.valueBool(lnm::OPTIONS_DIALOG_GUI_STYLE_AUTO, true);
  menuItemAuto->setChecked(automaticStyle);

  if(settings.contains(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX))
  {
    // Style already selected
    currentStyleIndex = settings.valueInt(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX);
    if(currentStyleIndex >= 0 && currentStyleIndex < styleDescriptions.size())
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
    currentStyleIndex = -1;

    // Use Fusion on macOS since the Qt apple style is a mess
    QString currentStyleName = STYLE_FUSION;
    const QStringList keys = QStyleFactory::keys();
    for(const QString& styleName : keys)
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "styleName" << styleName;
#endif

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

    if(currentStyleIndex == -1)
    {
      qWarning() << Q_FUNC_INFO << "No default style" << currentStyleName << " found";
      currentStyleIndex = 0;
      menuItems[currentStyleIndex]->setChecked(true);
    }
    else
      qDebug() << Q_FUNC_INFO << "Default style" << currentStyleName;
  }
}
