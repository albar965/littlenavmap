/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef USER_ICONMANAGER_H
#define USER_ICONMANAGER_H

#include <QCache>
#include <QMap>
#include <QObject>

class MainWindow;
class QFileInfo;

/*
 * Maintains and caches the icons for user defined data types.
 * Icons with pattern userpoint_*.svg are taken from the resources and can be overloaded from the configuration directory.
 * New types can be added by adding new icons with above pattern where * is recognized as the new type.
 */
class UserdataIcons :
  public QObject
{
  Q_OBJECT

public:
  UserdataIcons(QObject *parent);
  virtual ~UserdataIcons();

  QPixmap *getIconPixmap(const QString& type, int size);

  /* Resolve icon names, overloads and extract types*/
  void loadIcons();

  QStringList getAllTypes() const
  {
    return typeMap.keys();
  }

  bool hasType(const QString& type) const
  {
    return typeMap.contains(type);
  }

  QString getIconPath(const QString& type) const;

  QString getDefaultType(const QString& type);

  /* Inititialize the translations for types. */
  static void initTranslateableTexts();

  /* Convert type names to translated version and back. Returns type if nothing was found */
  static QString typeToTranslated(const QString& type);
  static QString translatedToType(const QString& type);

private:
  void loadIcon(const QFileInfo& entry);

  MainWindow *mainWindow;

  /* Maps type to filepath */
  QMap<QString, QString> typeMap;

  /* Maps type name and size to pixmap */
  typedef  std::pair<QString, int> PixmapCacheKey;
  QCache<PixmapCacheKey, QPixmap> pixmapCache;

};

#endif // USER_ICONMANAGER_H
