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

#ifndef LNM_COMMANDLINE_H
#define LNM_COMMANDLINE_H

#include <QCoreApplication>

class QCommandLineParser;
class QCommandLineOption;

/*
 * Reads command line arguments and store them in NavApp startup options or settings
 */
class CommandLine
{
  Q_DECLARE_TR_FUNCTIONS(CommandLine)

public:
  /* Builds command line parser in constructor */
  CommandLine();
  ~CommandLine();

  /* Read options and store values in settings or startup options or member variables */
  void process();

  /* "l", "log-path" */
  const QString& getLogPath() const
  {
    return logPath;
  }

  /* "c", "cache-path" */
  const QString& getCachePath() const
  {
    return cachePath;
  }

  /* "g", "language" */
  const QString& getLanguage() const
  {
    return language;
  }

private:
  QCommandLineParser *parser = nullptr;
  QString logPath, cachePath, language;

  QCommandLineOption *settingsDirOpt = nullptr, *settingsPathOpt = nullptr, *logPathOpt = nullptr, *cachePathOpt = nullptr,
                     *flightplanOpt = nullptr, *flightplanDescrOpt = nullptr, *performanceOpt,
                     *layoutOpt = nullptr, *languageOpt = nullptr;
};

#endif // LNM_COMMANDLINE_H
