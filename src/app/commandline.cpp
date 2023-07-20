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

#include "commandline.h"

#include "common/constants.h"
#include "app/navapp.h"
#include "settings/settings.h"

#include <QDebug>
#include <QCommandLineParser>

CommandLine::CommandLine()
{
  parser = new QCommandLineParser;
  parser->setApplicationDescription(
    tr("\n%1 version %2 command line options.\n"
       "Note that all arguments without \"-\" or \"--\" options are checked for\n"
       "compatible file types and are loaded if possible.").arg(QCoreApplication::applicationName(),
                                                                QCoreApplication::applicationVersion()));
  parser->addHelpOption();
  parser->addVersionOption();

  settingsDirOpt = new QCommandLineOption({"s", "settings-directory"},
                                          QObject::tr("Use <settings-directory> instead of \"%1\". This does *not* override the full path. "
                                                      "Spaces are replaced with underscores.").arg(NavApp::organizationName()),
                                          QObject::tr("settings-directory"));
  parser->addOption(*settingsDirOpt);

  settingsPathOpt = new QCommandLineOption({"p", "settings-path"},
                                           QObject::tr("Use <settings-path> to store options and databases into the given directory. "
                                                       "<settings-path> can be relative or absolute. "
                                                         "Missing directories are created. Path can be on any drive."),
                                           QObject::tr("settings-path"));
  parser->addOption(*settingsPathOpt);

  logPathOpt = new QCommandLineOption({"l", "log-path"},
                                      QObject::tr("Use <log-path> to store log files into the given directory. "
                                                  "<log-path> can be relative or absolute. "
                                                    "Missing directories are created. Path can be on any drive."),
                                      QObject::tr("settings-path"));
  parser->addOption(*logPathOpt);

  cachePathOpt = new QCommandLineOption({"c", "cache-path"},
                                        QObject::tr("Use <cache-path> to store tiles from online maps. "
                                                    "Missing directories are created. Path can be on any drive."),
                                        QObject::tr("cache-path"));
  parser->addOption(*cachePathOpt);

  flightplanOpt = new QCommandLineOption({"f", lnm::STARTUP_FLIGHTPLAN},
                                         QObject::tr("Load the given <%1> file on startup. Can be one of the supported formats like "
                                                     "\".lnmpln\", \".pln\", \".fms\", "
                                                     "\".fgfp\", \".fpl\", \".gfp\" or others.").arg(lnm::STARTUP_FLIGHTPLAN),
                                         lnm::STARTUP_FLIGHTPLAN);
  parser->addOption(*flightplanOpt);

  flightplanDescrOpt = new QCommandLineOption({"d", lnm::STARTUP_FLIGHTPLAN_DESCR},
                                              QObject::tr("Parse and load the given <%1> flight plan route description on startup. "
                                                          "Example \"EDDF BOMBI LIRF\".").arg(lnm::STARTUP_FLIGHTPLAN_DESCR),
                                              lnm::STARTUP_FLIGHTPLAN_DESCR);
  parser->addOption(*flightplanDescrOpt);

  performanceOpt = new QCommandLineOption({"a", lnm::STARTUP_AIRCRAFT_PERF},
                                          QObject::tr("Load the given <%1> aircraft performance file "
                                                      "\".lnmperf\" on startup.").arg(lnm::STARTUP_AIRCRAFT_PERF),
                                          lnm::STARTUP_AIRCRAFT_PERF);
  parser->addOption(*performanceOpt);

  layoutOpt = new QCommandLineOption({"y", lnm::STARTUP_LAYOUT},
                                     QObject::tr("Load the given <%1> window layout file"
                                                 "\".lnmlayout\" on startup.").arg(lnm::STARTUP_LAYOUT),
                                     lnm::STARTUP_LAYOUT);
  parser->addOption(*layoutOpt);

  languageOpt = new QCommandLineOption({"g", "language"},
                                       QObject::tr("Use language code <language> like \"de\" or \"en_US\" for the user interface. "
                                                   "The code is not checked for existence or validity and "
                                                   "is saved for the next startup."), "language");
  parser->addOption(*languageOpt);
}

CommandLine::~CommandLine()
{
  delete parser;
  delete settingsDirOpt;
  delete settingsPathOpt;
  delete logPathOpt;
  delete cachePathOpt;
  delete flightplanOpt;
  delete flightplanDescrOpt;
  delete performanceOpt;
  delete layoutOpt;
  delete languageOpt;
}

void CommandLine::process()
{
  // ==============================================
  // Process the actual command line arguments given by the user
  parser->process(*QCoreApplication::instance());

  // Settings directory
  if(parser->isSet(*settingsDirOpt) && parser->isSet(*settingsPathOpt))
    qWarning() << QObject::tr("Only one of options -s and -p can be used");

  if(parser->isSet(*settingsDirOpt) && !parser->value(*settingsDirOpt).isEmpty())
    atools::settings::Settings::setOverrideOrganisation(parser->value(*settingsDirOpt));

  if(parser->isSet(*settingsPathOpt) && !parser->value(*settingsPathOpt).isEmpty())
    atools::settings::Settings::setOverridePath(parser->value(*settingsPathOpt));

  if(parser->isSet(*logPathOpt) && !parser->value(*logPathOpt).isEmpty())
    logPath = parser->value(*logPathOpt);

  // File loading
  if(parser->isSet(*flightplanOpt) && parser->isSet(*flightplanDescrOpt))
    qWarning() << QObject::tr("Only one of options -f and -d can be used");

  if(parser->isSet(*flightplanOpt) && !parser->value(*flightplanOpt).isEmpty())
    NavApp::addStartupOptionStr(lnm::STARTUP_FLIGHTPLAN, parser->value(*flightplanOpt));

  if(parser->isSet(*flightplanDescrOpt) && !parser->value(*flightplanDescrOpt).isEmpty())
    NavApp::addStartupOptionStr(lnm::STARTUP_FLIGHTPLAN_DESCR, parser->value(*flightplanDescrOpt));

  if(parser->isSet(*performanceOpt) && !parser->value(*performanceOpt).isEmpty())
    NavApp::addStartupOptionStr(lnm::STARTUP_AIRCRAFT_PERF, parser->value(*performanceOpt));

  if(parser->isSet(*layoutOpt) && !parser->value(*layoutOpt).isEmpty())
    NavApp::addStartupOptionStr(lnm::STARTUP_LAYOUT, parser->value(*layoutOpt));

  // Other arguments without option
  if(!parser->positionalArguments().isEmpty())
    NavApp::addStartupOptionStrList(lnm::STARTUP_OTHER_ARGUMENTS, parser->positionalArguments());

  if(parser->isSet(*languageOpt) && !parser->value(*languageOpt).isEmpty())
    language = parser->value(*languageOpt);

  // "/home/USER/.local/share" ("/home/USER/.local/share/marble/maps/earth/openstreetmap")
  // "C:/Users/USER/AppData/Local" ("C:\Users\USER\AppData\Local\.marble\data\maps\earth\openstreetmap")
  if(parser->isSet(*cachePathOpt) && !parser->value(*cachePathOpt).isEmpty())
    ///home/USER/.local/share/marble
    cachePath = parser->value(*cachePathOpt);
}
