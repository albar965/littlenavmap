/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_MAPTHEMEHANDLER_H
#define LNM_MAPTHEMEHANDLER_H

#include <QStringList>
#include <QMap>
#include <QCoreApplication>
#include <QDir>

class QFileInfo;
class QToolButton;
class QActionGroup;

/*
 * Contains all information about a theme briefly extracted from a DGML file.
 */
class MapTheme
{
public:
  /* true if not default constructed and has a valid id */
  bool isValid() const
  {
    return !dgmlFilepath.isEmpty() && index >= 0;
  }

  /* Use bright colors for MORA and other texts on map */
  bool isDarkTheme() const
  {
    return theme == "cartodark";
  }

  /* Enable show city and other POIs button if true */
  bool hasPlacemarks() const
  {
    return geodataLayer && !online;
  }

  /* Offline themes cannot do sun shading */
  bool canSunShading() const
  {
    return geodataLayer;
  }

  /* Zoom in discrete steps if true */
  bool hasDiscreteZoom() const
  {
    return discrete;
  }

  /* Index of this one in MapThemeHandler vector of themes */
  int getIndex() const
  {
    return index;
  }

  /* Full absolute path to DGML file */
  const QString& getDgmlFilepath() const
  {
    return dgmlFilepath;
  }

  /* Theme name as found in the DGML file for tag "name" in "header". */
  const QString& getName() const
  {
    return name;
  }

  /* Short copyright from attribute "short" in element "license". */
  const QString& getCopyright() const
  {
    return copyright;
  }

  /* Theme id/short name as found in the DGML file for tag "theme" in "header". */
  const QString& getThemeId() const
  {
    return theme;
  }

  /* Always earth */
  const QString& getTarget() const
  {
    return target;
  }

  /* Name as found in the DGML file for tag "url" in "header". */
  const QString& getUrlName() const
  {
    return urlName;
  }

  /* Name as found in the DGML file for tag "url", attribute "href" in "header". */
  const QString& getUrlRef() const
  {
    return urlRef;
  }

  /* Get a list of all API keys, usernames, tokens or whatever was given in curly brackes in the element "downloadUrl" */
  const QStringList& getKeys() const
  {
    return keys;
  }

  bool hasKeys() const
  {
    return !keys.isEmpty();
  }

  /* True if it has texture layers which means it is an online map. */
  bool isTextureLayer() const
  {
    return textureLayer;
  }

  /* True if it has geodata layers which means it is an offline map. */
  bool isGeodataLayer() const
  {
    return geodataLayer;
  }

  /* Zooming can be done in discrete (for tiled online maps) steps or smooth */
  bool isDiscrete() const
  {
    return discrete;
  }

  /* Not visible themes are excluded from the list */
  bool isVisible() const
  {
    return visible;
  }

  /* Online theme which has an element "downloadUrl" */
  bool isOnline() const
  {
    return online;
  }

private:
  friend class MapThemeHandler;
  friend QDebug operator<<(QDebug out, const MapTheme& theme);

  int index = -1;
  QString dgmlFilepath, name, copyright, theme, target, urlName, urlRef;
  QStringList sourceDirs;
  QStringList keys;
  bool textureLayer = false, geodataLayer = false, discrete = false, visible = false, online = false;
};

/*
 * Extracts all relevant information from map themes in folder data/maps/earth and keeps a sorted list of theme objects.
 * Also loads and saves API key, username or token values for maps.
 *
 * Themes are referenced by theme id which is element <theme> in the DGML file.
 *
 * Also takes care of the two actions for map projection.
 */
class MapThemeHandler :
  public QObject
{
  Q_OBJECT

public:
  MapThemeHandler(QWidget *mainWindowParam);
  virtual ~MapThemeHandler() override;

  /* Load all theme files into Theme objects. Not visible themes will be excluded */
  void loadThemes();

  /* Get theme by theme id (element <theme> in DGML. */
  const MapTheme& getTheme(const QString& themeId) const;

  /* Get default theme which is the one with short name "openstreetmap" */
  const MapTheme& getDefaultTheme() const
  {
    return defaultTheme;
  }

  /* Currently selected theme id from actions */
  QString getCurrentThemeId() const;

  /* Sort order is always online/offline and then alphabetical */
  const QVector<MapTheme>& getThemes() const
  {
    return themes;
  }

  /* See related methods in MapTheme */
  bool isDarkTheme(const QString& themeId) const
  {
    return getTheme(themeId).isDarkTheme();
  }

  bool hasPlacemarks(const QString& themeId) const
  {
    return getTheme(themeId).hasPlacemarks();
  }

  bool canSunShading(const QString& themeId) const
  {
    return !getTheme(themeId).canSunShading();
  }

  bool hasDiscreteZoom(const QString& themeId) const
  {
    return !getTheme(themeId).hasDiscreteZoom();
  }

  /* Get a map of API key, username or token key/value pairs as loaded from all DGML configuration files. */
  const QMap<QString, QString>& getMapThemeKeys() const
  {
    return mapThemeKeys;
  }

  QHash<QString, QString> getMapThemeKeysHash() const;

  /* Set theme keys to save */
  void setMapThemeKeys(const QMap<QString, QString>& keys);

  /* Load and save key values from separate binary file to avoid users accidentally
   * sharing their keys with the .ini configuration file.
   * Adds only keys which are available from map configuration when loading. Others are ignored */
  void restoreState();
  void saveState();

  /* Sets up the toolbutton, menu and actions for the toolbar and the main menu */
  void setupMapThemesUi();

  /* Called by the actions after key updates */
  void changeMapTheme();
  void changeMapProjection();

  /* Reload themes and rebuild menu */
  void optionsChanged();

  /* Check path if it is a directory and counts map themes in it */
  static QString getStatusTextForDir(const QString& path);

  /* Checks default and user folder and shows an error dialog if any is invalid */
  static void validateMapThemeDirectories();

private:
  static QString getMapThemeDefaultDir();
  static QString getMapThemeUserDir();

  /* Get theme by internal index */
  const MapTheme& themeByIndex(int themeIndex) const;

  /* Look for directories with a valid DGML file in the earth dir */
  const QList<QFileInfo> findMapThemes(const QStringList& paths);

  /* Briefly read the most important data from a DGML file needed to build a MapTheme object */
  MapTheme loadTheme(const QFileInfo& dgml);

  void changeMapThemeActions(const QString& themeId);

  void restoreKeyfile();
  void saveKeyfile();

  /* Sorted list of all loaded themes */
  QVector<MapTheme> themes;
  QHash<QString, int> themeIdToIndexMap;

  /* Default OSM theme used as fallback */
  MapTheme defaultTheme;

  /* All keys for all maps */
  QMap<QString, QString> mapThemeKeys;

  QToolButton *toolButtonMapTheme = nullptr;
  QActionGroup *actionGroupMapTheme = nullptr;
  QWidget *mainWindow;

  QActionGroup *mapProjectionActionGroup = nullptr;
};

QDebug operator<<(QDebug out, const MapTheme& theme);

Q_DECLARE_TYPEINFO(MapTheme, Q_MOVABLE_TYPE);

#endif // LNM_MAPTHEMEHANDLER_H
