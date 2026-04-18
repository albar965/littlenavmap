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

#include "commandline.h"

#include "common/constants.h"
#include "settings/settings.h"
#include "gui/application.h"

#include <QDebug>
#include <QCommandLineParser>
#include <QStringBuilder>

using atools::gui::Application;

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

  buildOption(settingsPathOpt, "p", "settings-path",
              tr("Use <settings-path> to store options and databases into the given directory. "
                 "<settings-path> can be relative or absolute. "
                   "Missing directories are created. Path can be on any drive."),
              tr("settings-path"));

  buildOption(logPathOpt, "l", "log-path",
              tr("Use <log-path> to store log files into the given directory. "
                 "<log-path> can be relative or absolute. "
                   "Missing directories are created. Path can be on any drive."),
              tr("log-path"));

  buildOption(cachePathOpt, "c", "cache-path",
              tr("Use <cache-path> to store tiles from online maps. "
                 "Missing directories are created. Path can be on any drive."),
              tr("cache-path"));

  buildOption(flightplanOpt, "f", lnm::STARTUP_FLIGHTPLAN,
              tr("Load the given <%1> file on startup. Can be one of the supported formats like "
                 "\".lnmpln\", \".pln\", \".fms\", "
                 "\".fgfp\", \".fpl\", \".gfp\" or others.").arg(lnm::STARTUP_FLIGHTPLAN),
              lnm::STARTUP_FLIGHTPLAN);

  buildOption(flightplanDescrOpt, "d", lnm::STARTUP_FLIGHTPLAN_DESCR,
              tr("Parse and load the given <%1> flight plan route description on startup. "
                 "Example \"EDDF BOMBI LIRF\".").arg(lnm::STARTUP_FLIGHTPLAN_DESCR),
              lnm::STARTUP_FLIGHTPLAN_DESCR);

  // Load aircraft performance =============================
  buildOption(performanceOpt, "a", lnm::STARTUP_AIRCRAFT_PERF,
              tr("Load the given <%1> aircraft performance file "
                 "\".lnmperf\" on startup.").arg(lnm::STARTUP_AIRCRAFT_PERF),
              lnm::STARTUP_AIRCRAFT_PERF);

  // Load layout =============================
  buildOption(layoutOpt, "y", lnm::STARTUP_LAYOUT,
              tr("Load the given <%1> window layout file"
                 "\".lnmlayout\" on startup.").arg(lnm::STARTUP_LAYOUT),
              lnm::STARTUP_LAYOUT);

  // Load GPX =============================
  buildOption(gpxOpt, "x", lnm::STARTUP_GPX,
              tr("Load the given <%1> GPX file \".gpx\" on startup.").arg(lnm::STARTUP_GPX),
              lnm::STARTUP_GPX);

  // Load marker =============================
  buildOption(markerOpt, "u", lnm::STARTUP_MARKER,
              tr("Load the given <%1> user feature file \".lnmuserfeat\" on startup.").arg(lnm::STARTUP_MARKER),
              lnm::STARTUP_MARKER);

  // Force =============================
  buildOption(forceOpt, QStringLiteral(), lnm::STARTUP_FORCE_LOADING,
              tr("Force overwriting of files without asking. Be careful with this option."));

  // Quit =============================
  buildOption(quitOpt, "q", lnm::STARTUP_COMMAND_QUIT,
              tr("Quit an already running instance. "
                 "The running instance might still ask about exiting or saving files."));

  // Disable data exchange - do not send a message to a running application =============================
  buildOption(noDataExchangeOpt, QStringLiteral(), lnm::STARTUP_NO_DATA_EXCHANGE,
              tr("Do not activate another instance. "
                 "This is used only internally. Using this the wrong way might result in data loss."), QStringLiteral(), QStringLiteral(),
              true /* hiddenFromHelp */);

  // Reset window layout after restart =============================
  buildOption(resetLayoutOpt, QStringLiteral(), lnm::STARTUP_RESET_LAYOUT,
              tr("Reset window layout back to default. "
                 "This is used only internally. Using this the wrong way might result in data loss."), QStringLiteral(), QStringLiteral(),
              true /* hiddenFromHelp */);

  // Language =============================
  buildOption(languageOpt, "g", "language",
              tr("Use language code <language> like \"de\" or \"en_US\" for the user interface. "
                 "The code is not checked for existence or validity and "
                 "is saved for the next startup."), "language");
}

CommandLine::~CommandLine()
{
  delete parser;
  delete settingsPathOpt;
  delete logPathOpt;
  delete cachePathOpt;
  delete flightplanOpt;
  delete flightplanDescrOpt;
  delete performanceOpt;
  delete layoutOpt;
  delete gpxOpt;
  delete markerOpt;
  delete languageOpt;
  delete forceOpt;
  delete quitOpt;
  delete noDataExchangeOpt;
}

void CommandLine::buildOption(QCommandLineOption *& option, const QString& shortOption, const QString& longOption,
                              const QString& description, const QString& valueName, const QString& defaultValue, bool hiddenFromHelp)
{
  QStringList opts({shortOption, longOption});
  opts.removeAll(QStringLiteral());
  option = new QCommandLineOption(opts, description, valueName, defaultValue);
  if(hiddenFromHelp)
    option->setFlags(QCommandLineOption::HiddenFromHelp);

  parser->addOption(*option);
}

QString CommandLine::getOption(int argc, char *argv[], const QString& name, const QString& longname)
{
  for(int i = 0; i < argc; i++)
  {
    QString arg(argv[i]);
    arg = arg.trimmed();

    if((arg == '-' % name || arg == "--" % longname) && i < argc - 1)
      // Next argument
      return QString(argv[i + 1]).trimmed();
    else if(arg.startsWith("--" % longname % '='))
    {
      // Argument after equals
      QString argstr = arg.section('=', 1, 1).trimmed();
      if(argstr.startsWith('"'))
        argstr.remove(0, 1);

      if(argstr.endsWith('"'))
        argstr.chop(1);
      argstr = argstr.trimmed();

      return argstr;
    }
  }

  return QStringLiteral();
}

void CommandLine::process()
{
  // ==============================================
  // Process the actual command line arguments given by the user
  parser->process(*QCoreApplication::instance());

  // Settings directory
  if(parser->isSet(*settingsPathOpt) && !parser->value(*settingsPathOpt).isEmpty())
    atools::settings::Settings::setOverridePath(parser->value(*settingsPathOpt));

  if(parser->isSet(*logPathOpt) && !parser->value(*logPathOpt).isEmpty())
    logPath = parser->value(*logPathOpt);

  // File loading
  if(parser->isSet(*flightplanOpt) && parser->isSet(*flightplanDescrOpt))
    qWarning() << tr("Only one of options -f and -d can be used");

  if(parser->isSet(*flightplanOpt) && !parser->value(*flightplanOpt).isEmpty())
    Application::addStartupOptionStr(lnm::STARTUP_FLIGHTPLAN, parser->value(*flightplanOpt));

  if(parser->isSet(*flightplanDescrOpt) && !parser->value(*flightplanDescrOpt).isEmpty())
    Application::addStartupOptionStr(lnm::STARTUP_FLIGHTPLAN_DESCR, parser->value(*flightplanDescrOpt));

  if(parser->isSet(*performanceOpt) && !parser->value(*performanceOpt).isEmpty())
    Application::addStartupOptionStr(lnm::STARTUP_AIRCRAFT_PERF, parser->value(*performanceOpt));

  if(parser->isSet(*layoutOpt) && !parser->value(*layoutOpt).isEmpty())
    Application::addStartupOptionStr(lnm::STARTUP_LAYOUT, parser->value(*layoutOpt));

  if(parser->isSet(*gpxOpt) && !parser->value(*gpxOpt).isEmpty())
    Application::addStartupOptionStr(lnm::STARTUP_GPX, parser->value(*gpxOpt));

  if(parser->isSet(*markerOpt) && !parser->value(*markerOpt).isEmpty())
    Application::addStartupOptionStr(lnm::STARTUP_MARKER, parser->value(*markerOpt));

  if(parser->isSet(*forceOpt))
    Application::addStartupOptionStr(lnm::STARTUP_FORCE_LOADING, QStringLiteral());

  if(parser->isSet(*quitOpt))
    Application::addStartupOptionStr(lnm::STARTUP_COMMAND_QUIT, QStringLiteral());

  if(parser->isSet(*noDataExchangeOpt))
    Application::addStartupOptionStr(lnm::STARTUP_NO_DATA_EXCHANGE, QStringLiteral());

  if(parser->isSet(*resetLayoutOpt))
    Application::addStartupOptionStr(lnm::STARTUP_RESET_LAYOUT, QStringLiteral());

  // Other arguments without option
  if(!parser->positionalArguments().isEmpty())
    Application::addStartupOptionStrList(lnm::STARTUP_OTHER_ARGUMENTS, parser->positionalArguments());

  if(parser->isSet(*languageOpt) && !parser->value(*languageOpt).isEmpty())
    language = parser->value(*languageOpt);

  // "/home/USER/.local/share" ("/home/USER/.local/share/marble/maps/earth/openstreetmap")
  // "C:/Users/USER/AppData/Local" ("C:\Users\USER\AppData\Local\.marble\data\maps\earth\openstreetmap")
  if(parser->isSet(*cachePathOpt) && !parser->value(*cachePathOpt).isEmpty())
    // . /home/USER/.local/share/marble
    cachePath = parser->value(*cachePathOpt);
}
