/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "app/commandline.h"
#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/maptypes.h"
#include "common/maptypes.h"
#include "common/proctypes.h"
#include "common/settingsmigrate.h"
#include "common/textpointer.h"
#include "common/unit.h"
#include "db/databasemanager.h"
#include "exception.h"
#include "fs/sc/simconnectdata.h"
#include "fs/sc/simconnectreply.h"
#include "fs/weather/metarparser.h"
#include "geo/aircrafttrail.h"
#include "geo/calculations.h"
#include "gui/dockwidgethandler.h"
#include "gui/mainwindow.h"
#include "gui/mapposhistory.h"
#include "gui/translator.h"
#include "logging/logginghandler.h"
#include "logging/loggingutil.h"
#include "options/optionsdialog.h"
#include "routeexport/routeexportformat.h"
#include "settings/settings.h"
#include "userdata/userdataicons.h"
#include "util/crashhandler.h"
#include "util/properties.h"

#include <QDebug>
#include <QSplashScreen>
#include <QSslSocket>
#include <QStyleFactory>
#include <QPixmapCache>
#include <QScreen>
#include <QProcessEnvironment>

#include <marble/MarbleGlobal.h>
#include <marble/MarbleDirs.h>
#include <marble/MarbleDebug.h>

using namespace Marble;
using atools::gui::Application;
using atools::logging::LoggingHandler;
using atools::logging::LoggingUtil;
using atools::settings::Settings;
using atools::gui::Translator;

int main(int argc, char *argv[])
{
  // Start timer to measure startup time
  Application::startup();

  // Initialize the resources from atools static library
  Q_INIT_RESOURCE(atools);

  // Register all types to allow conversion from/to QVariant and thus reading/writing into settings
  atools::geo::registerMetaTypes();
  atools::fs::sc::registerMetaTypes();
  atools::util::Properties::registerMetaTypes();
  atools::gui::MapPosHistory::registerMetaTypes();
  atools::gui::DockWidgetHandler::registerMetaTypes();

  qRegisterMetaTypeStreamOperators<FsPathType>();
  qRegisterMetaTypeStreamOperators<SimulatorTypeMap>();

  qRegisterMetaTypeStreamOperators<map::DistanceMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::DistanceMarker> >();

  qRegisterMetaTypeStreamOperators<map::PatternMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::PatternMarker> >();

  qRegisterMetaTypeStreamOperators<map::HoldingMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::HoldingMarker> >();

  qRegisterMetaTypeStreamOperators<map::MsaMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::MsaMarker> >();

  qRegisterMetaTypeStreamOperators<map::RangeMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::RangeMarker> >();

  qRegisterMetaTypeStreamOperators<AircraftTrailPos>();
  qRegisterMetaTypeStreamOperators<QList<AircraftTrailPos> >();

  qRegisterMetaTypeStreamOperators<RouteExportFormat>();
  qRegisterMetaTypeStreamOperators<RouteExportFormatMap>();

  qRegisterMetaTypeStreamOperators<map::MapAirspaceFilter>();
  qRegisterMetaTypeStreamOperators<map::MapTypes>();

  // Register types and load process environment
  atools::fs::FsPaths::intitialize();

  // Tasks that have to be done before creating the application object and logging system =================
  // Application name is not available yet

  // Create settings instance - no QCoreApplication needed so far =========================================
  QString settingsPath = CommandLine::getOption(argc, argv, "p", "settings-path");
  Settings::setOverridePath(settingsPath);
  Settings::setOrganizationName(lnm::OPTIONS_APPLICATION_ORGANIZATION);
  Settings::setApplicationName(lnm::OPTIONS_APPLICATION);

  Settings& settings = Settings::instance();

  QStringList logMessages;

  // Render options =================================================
  // The loading mechanism can be configured through the QT_OPENGL environment variable and the following application attributes:
  // Qt::AA_UseDesktopOpenGL Equivalent to setting QT_OPENGL to desktop.
  // Qt::AA_UseOpenGLES Equivalent to setting QT_OPENGL to angle.
  // Qt::AA_UseSoftwareOpenGL Equivalent to setting QT_OPENGL to software.
  QString renderOpt = settings.valueStr("Options/RenderOpt", "none");
  if(!renderOpt.isEmpty())
  {
    // QT_OPENGL does not work - so do this ourselves
    if(renderOpt == "desktop")
    {
      QApplication::setAttribute(Qt::AA_UseDesktopOpenGL, true);
      QApplication::setAttribute(Qt::AA_UseOpenGLES, false);
      QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);
    }
    else if(renderOpt == "angle")
    {
      QApplication::setAttribute(Qt::AA_UseDesktopOpenGL, false);
      QApplication::setAttribute(Qt::AA_UseOpenGLES, true);
      QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);
    }
    else if(renderOpt == "software")
    {
      QApplication::setAttribute(Qt::AA_UseDesktopOpenGL, false);
      QApplication::setAttribute(Qt::AA_UseOpenGLES, false);
      QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);
    }
    else if(renderOpt != "none")
      logMessages.append("Wrong renderer " + renderOpt);
  }
  logMessages.append("RenderOpt " + renderOpt);

  // High DPI option =================================================
  if(settings.valueInt("OptionsDialog/Widget_checkBoxOptionsGuiHighDpi", 2) == 2)
  {
    logMessages.append("High DPI scaling enabled");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true); // Set before QGuiApplication is constructed.
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
  }
  else
  {
    logMessages.append("High DPI scaling disabled");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling, false);
    // QGuiApplication::setAttribute(Qt::AA_DisableHighDpiScaling, true); // Freezes with QT_SCALE_FACTOR=2 on Linux
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, false);
#if !defined(Q_OS_WIN32)
    // Causes weird font effects on Windows
    QGuiApplication::setAttribute(Qt::AA_Use96Dpi, true);
#endif
  }

  // Show dialog on exception in main event queue - can be disabled for debugging purposes
  NavApp::setShowExceptionDialog(settings.valueBool("Options/ExceptionDialog", true));

  // Add freetype font engine arguments for Windows ===========================================================
#if defined(Q_OS_WIN32)
  // Get checkbox value
  bool freetype = settings.valueInt("OptionsDialog/Widget_checkBoxOptionsFreetype", 2) == 2;
#else
  bool freetype = false;
#endif

  // Byte arrays have to remain for the whole runtime
  QByteArray argument = QString("-platform").toLatin1(), value = QString("windows:fontengine=freetype").toLatin1();

  // Copy pointers to regular arguments - add two for freetype options if needed
  int appArgc = freetype ? argc + 2 : argc;
  QVarLengthArray<char *> appArgv(appArgc);

  for(int i = 0; i < argc; i++)
    appArgv[i] = argv[i];

  if(freetype)
  {
    appArgv[appArgc - 2] = argument.data();
    appArgv[appArgc - 1] = value.data();
  }

  // Create application object ===========================================================
  int retval = 0;
  NavApp app(appArgc, appArgv.data());

  DatabaseManager *dbManager = nullptr;

#if defined(Q_OS_WIN32)
  QApplication::addLibraryPath(QApplication::applicationDirPath() + atools::SEP + "plugins");
#endif

  try
  {
    // ==============================================
    // Read command line arguments and store them in NavApp startup options or settings
    CommandLine commandLine;

    // Process the actual command line arguments given by the user
    commandLine.process();

    // ==============================================
    // Check if LNM is already running - send message across shared memory and exit if yes, otherwise continue normally
    if(!NavApp::initDataExchange())
    {
      Application::setEmailAddresses({"alex@littlenavmap.org"});
      Application::setContactUrl("https://www.littlenavmap.org/contact.html");

      // ==============================================
      // Initialize logging and force logfiles into the system or user temp directory
      // This will prefix all log files with orgranization and application name and append ".log"
      QString logCfg = Settings::getOverloadedPath(":/littlenavmap/resources/config/logging.cfg");
      if(commandLine.getLogPath().isEmpty())
        LoggingHandler::initializeForTemp(logCfg);
      else
      {
        QDir().mkpath(commandLine.getLogPath());
        LoggingHandler::initialize(logCfg, commandLine.getLogPath());
      }

      // ==============================================
      // Print some information which can be useful for debugging
      Settings::logMessages();
      LoggingUtil::logSystemInformation();

      for(const QString& message : logMessages)
        qInfo() << message;

      // Initialize crashhandler - disable on Linux to get core files
      if(settings.valueBool("Options/PrintStackTrace", true))
      {
        atools::util::crashhandler::init();
        atools::util::crashhandler::setStackTraceLog(Settings::getConfigFilename(lnm::STACKTRACE_SUFFIX, lnm::CRASHREPORTS_DIR));
      }

      // ==============================================
      // Set language from command line into options - will be saved
      if(!commandLine.getLanguage().isEmpty())
        OptionData::saveLanguageToConfigFile(commandLine.getLanguage());

      // Load available translations early ============================================
      QString language = OptionData::getLanguageFromConfigFile();
      if(language.isEmpty())
        // Use system default for now if not given in settings yet
        language = QLocale().name();

      qInfo() << "Loading translations for" << language;
      Translator::load(language);

      // Load help URLs from urls.cfg =================================
      lnm::loadHelpUrls();

      // Check lock file for previously crashed instances ===================================
      NavApp::recordStartNavApp();

      // ==============================================
      // Start splash screen
      if(settings.valueBool(lnm::OPTIONS_DIALOG_SHOW_SPLASH, true))
        Application::initSplashScreen(":/littlenavmap/resources/icons/splash.png", GIT_REVISION_LITTLENAVMAP);

      // Log system information ========================================
      qInfo().noquote().nospace() << "atools revision " << atools::gitRevision() << " "
                                  << Application::applicationName() << " revision " << GIT_REVISION_LITTLENAVMAP;

      LoggingUtil::logStandardPaths();

      qInfo() << "SSL supported" << QSslSocket::supportsSsl()
              << "build library" << QSslSocket::sslLibraryBuildVersionString()
              << "library" << QSslSocket::sslLibraryVersionString();

      qInfo() << "Available styles" << QStyleFactory::keys();

      qInfo() << "SimConnectData Version" << atools::fs::sc::SimConnectData::getDataVersion()
              << "SimConnectReply Version" << atools::fs::sc::SimConnectReply::getReplyVersion();

      qInfo() << "QT_OPENGL" << QProcessEnvironment::systemEnvironment().value("QT_OPENGL");
      qInfo() << "QT_SCALE_FACTOR" << QProcessEnvironment::systemEnvironment().value("QT_SCALE_FACTOR");
      if(QApplication::testAttribute(Qt::AA_UseDesktopOpenGL))
        qInfo() << "Using Qt desktop renderer";
      if(QApplication::testAttribute(Qt::AA_UseOpenGLES))
        qInfo() << "Using Qt angle renderer";
      if(QApplication::testAttribute(Qt::AA_UseSoftwareOpenGL))
        qInfo() << "Using Qt software renderer";

      qInfo() << "UI default font" << QApplication::font();
      const QList<QScreen *> screens = QGuiApplication::screens();
      for(const QScreen *screen: screens)
        qInfo() << "Screen" << screen->name() << "size" << screen->size() << "physical size" << screen->physicalSize()
                << "DPI ratio" << screen->devicePixelRatio() << "DPI x" << screen->logicalDotsPerInchX()
                << "y" << screen->logicalDotsPerInchX();

      // Start settings and file migration
      migrate::checkAndMigrateSettings();

      qInfo() << "Settings dir name" << Settings::getDirName();

      int pixmapCache = settings.valueInt(lnm::OPTIONS_PIXMAP_CACHE, -1);
      qInfo() << "QPixmapCache cacheLimit" << QPixmapCache::cacheLimit() << "KB";
      if(pixmapCache != -1)
      {
        qInfo() << "Overriding pixmap cache" << pixmapCache << "KB";
        QPixmapCache::setCacheLimit(pixmapCache);
      }

      // Load font from options settings ========================================
      QString fontStr = settings.valueStr(lnm::OPTIONS_DIALOG_FONT, QString());
      QFont font;
      if(!fontStr.isEmpty())
      {
        font.fromString(fontStr);
        QApplication::setFont(font);
      }

      TextPointer::initPointerCharacters(QApplication::font());
      qInfo() << "Loaded font" << font.toString() << "from options. Stored font info" << fontStr;

      // Load region override ============================================
      // Forcing the English locale if the user has chosen it this way
      if(OptionsDialog::isOverrideRegion())
      {
        qInfo() << "Overriding region settings";
        QLocale::setDefault(QLocale("en"));
      }

      qDebug() << "Locale after setting to" << OptionData::getLanguageFromConfigFile() << QLocale()
               << "decimal point" << QString(QLocale().decimalPoint())
               << "group separator" << QString(QLocale().groupSeparator());

      // Add paths here to allow translation =================================
      Application::addReportPath(QObject::tr("Log files:"), LoggingHandler::getLogFiles(false /* includeBackups */));
      Application::addReportPath(QObject::tr("Database directory:"), {Settings::getPath() + atools::SEP + lnm::DATABASE_DIR});
      Application::addReportPath(QObject::tr("Configuration:"), {Settings::getFilename()});

      // Load simulator paths =================================
      atools::fs::FsPaths::loadAllPaths();
      atools::fs::FsPaths::logAllPaths();

      // Avoid static translations and load these dynamically now  =================================
      Unit::initTranslateableTexts();
      UserdataIcons::initTranslateableTexts();
      map::initTranslateableTexts();
      proc::initTranslateableTexts();
      atools::fs::weather::initTranslateableTexts();
      formatter::initTranslateableTexts();

      // =============================================================================================
      // Set up Marble widget and print debugging information
      MarbleGlobal::Profiles profiles = MarbleGlobal::detectProfiles();
      MarbleGlobal::getInstance()->setProfiles(profiles);

      qDebug() << "Marble Local Path:" << MarbleDirs::localPath();
      qDebug() << "Marble Plugin Local Path:" << MarbleDirs::pluginLocalPath();
      qDebug() << "Marble Data Path (Run Time) :" << MarbleDirs::marbleDataPath();
      qDebug() << "Marble Plugin Path (Run Time) :" << MarbleDirs::marblePluginPath();
      qDebug() << "Marble System Path:" << MarbleDirs::systemPath();
      qDebug() << "Marble Plugin System Path:" << MarbleDirs::pluginSystemPath();

      MarbleDirs::setMarbleDataPath(QApplication::applicationDirPath() + atools::SEP + "data");

      if(!commandLine.getCachePath().isEmpty())
      {
        // "/home/USER/.local/share" ("/home/USER/.local/share/marble/maps/earth/openstreetmap")
        // "C:/Users/USER/AppData/Local" ("C:\Users\USER\AppData\Local\.marble\data\maps\earth\openstreetmap")

        /// home/USER/.local/share/marble
        QFileInfo cacheFileinfo(commandLine.getCachePath());

        QString marbleCache;

        if(cacheFileinfo.isRelative())
          marbleCache = QApplication::applicationDirPath() + atools::SEP + cacheFileinfo.filePath();
        else
          marbleCache = cacheFileinfo.absoluteFilePath();

        marbleCache = marbleCache + atools::SEP + "marble";

        // Have to create full path to avoid Marble showing a migration dialog
        QString marbleCachePath = marbleCache + atools::SEP + "maps" + atools::SEP + "earth";
        qDebug() << Q_FUNC_INFO << "Creating" << marbleCachePath;
        QDir().mkpath(marbleCachePath);

        qDebug() << Q_FUNC_INFO << "Setting Marble cache to" << marbleCache;
        MarbleDirs::setMarbleLocalPath(marbleCache);
      }

#if defined(Q_OS_MACOS)
      QDir pluginsDir(QApplication::applicationDirPath());
      pluginsDir.cdUp();
      pluginsDir.cd("PlugIns");
      MarbleDirs::setMarblePluginPath(pluginsDir.absolutePath());
#else
      MarbleDirs::setMarblePluginPath(QApplication::applicationDirPath() + atools::SEP + "plugins");
#endif

      MarbleDebug::setEnabled(settings.getAndStoreValue(lnm::OPTIONS_MARBLE_DEBUG, false).toBool());

      qDebug() << "New Marble Local Path:" << MarbleDirs::localPath();
      qDebug() << "New Marble Plugin Local Path:" << MarbleDirs::pluginLocalPath();
      qDebug() << "New Marble Data Path (Run Time) :" << MarbleDirs::marbleDataPath();
      qDebug() << "New Marble Plugin Path (Run Time) :" << MarbleDirs::marblePluginPath();
      qDebug() << "New Marble System Path:" << MarbleDirs::systemPath();
      qDebug() << "New Marble Plugin System Path:" << MarbleDirs::pluginSystemPath();

      // =============================================================================================
      // Disable tooltip effects since these do not work well with tooltip updates while displaying
      QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
      QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);

      QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

      // =============================================================================================
      // Check if database is compatible and ask the user to erase all incompatible ones
      // If erasing databases is refused exit application
      bool databasesErased = false;
      dbManager = new DatabaseManager(nullptr);

      /* Copy from application directory to settings directory if newer and create indexes if missing */
      dbManager->checkCopyAndPrepareDatabases();

      if(dbManager->checkIncompatibleDatabases(&databasesErased))
      {
        ATOOLS_DELETE_LOG(dbManager);

        QApplication::setQuitOnLastWindowClosed(true);

        MainWindow mainWindow;

        // Show database dialog if something was removed
        mainWindow.setDatabaseErased(databasesErased);

        mainWindow.show();

        // Hide splash once main window is shown
        NavApp::finishSplashScreen(&mainWindow);

        // =============================================================================================
        // Run application
        qDebug() << Q_FUNC_INFO << "Before QApplication::exec()";
        retval = QApplication::exec();
        qDebug() << Q_FUNC_INFO << "After QApplication::exec()";
      }
      else
      {
        ATOOLS_DELETE_LOG(dbManager);
        atools::gui::Application::recordExit();
      }
    } // if(!NavApp::initSharedMemory())
    else
      retval = 0;

    qInfo() << "QApplication::exec() done, retval is" << retval << (retval == 0 ? "(ok)" : "(error)");
  }
  catch(atools::Exception& e)
  {
    NavApp::deInitDataExchange();
    ATOOLS_PRINT_STACK_CRITICAL("Caught exception in main");
    ATOOLS_HANDLE_EXCEPTION(e);
    // Does not return in case of fatal error
  }
  catch(...)
  {
    NavApp::deInitDataExchange();
    ATOOLS_PRINT_STACK_CRITICAL("Caught exception in main");
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
    // Does not return in case of fatal error
  }

  atools::util::crashhandler::clearStackTrace(Settings::getConfigFilename(lnm::STACKTRACE_SUFFIX, lnm::CRASHREPORTS_DIR));
  NavApp::deInitDataExchange();

  ATOOLS_DELETE_LOG(dbManager);

  qInfo() << "About to shut down logging";
  atools::logging::LoggingHandler::shutdown();

  return retval;
}
