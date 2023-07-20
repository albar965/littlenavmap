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

#ifndef LNM_DIRTOOL_H
#define LNM_DIRTOOL_H

#include <QCoreApplication>

class QDir;

/*
 * Creates recommended folder structure for saving and loading files in documents folder.
 */
class DirTool
{
  Q_DECLARE_TR_FUNCTIONS(DirTool)

public:
  /*
   * @param base Documents folder like C:\Users\ME\Documents
   * @param appName Application name used to create folders
   * @param settingsKeyPrefix Used to save dialog confirmation (do not show again)
   */
  explicit DirTool(QWidget *parent, const QString& base, const QString& appName, const QString& settingsKeyPrefix);

  /* Checks for existence and shows dialog if folders are incomplete */
  void runIfMissing(bool manual, bool& complete, bool& created);

  /* Get base folder like "C:\Users\ME\Documents\Little Navmap Files" with native separators. */
  QString getApplicationDir() const;

private:
  /* Asks user to create directory structure. Creates directories and changes file dialog defaults if user confirms.*/
  void run(bool manual, bool& created);
  bool hasAllDirs();

  /* Fills errors if any */
  bool createAllDirs();

  /* true if all folders exist */
  bool hasDir(const QString& dir);

  /* Update file dialog and other defaults in settings file
   * Some options have to be updated if file dialog settingsPrefix or line edit widget name changes*/
  void updateOptions();

  /* Create based on documentsDir/applicationDir and fills errors if any */
  void mkdir(const QString& dir);
  void mkdirBase();

  /* Concatenated folder name*/
  QString d(const QString& dir);

  QWidget *parentWidget;
  QString documentsDir /* E.g. "C:\Users\ME\Documents" */,
          applicationDir /* Localized, e.g. "Little Navmap Files" */,
          flightPlanDir, perfDir, layoutDir, airspaceDir, globeDir, airportsDir, mapThemesDir;
  QString settingsPrefix;
  QStringList errors;

};

#endif // LNM_DIRTOOL_H
