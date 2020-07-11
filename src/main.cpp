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

#include "gui/mainwindow.h"
#include "common/constants.h"
#include "logging/logginghandler.h"
#include "logging/loggingutil.h"
#include "options/optionsdialog.h"
#include "settings/settings.h"
#include "gui/translator.h"
#include "gui/mapposhistory.h"
#include "common/maptypes.h"
#include "gui/application.h"
#include "common/formatter.h"
#include "exception.h"
#include "navapp.h"
#include "atools.h"
#include "gui/errorhandler.h"
#include "db/databasemanager.h"
#include "common/settingsmigrate.h"
#include "common/aircrafttrack.h"
#include "fs/sc/simconnectdata.h"
#include "fs/sc/simconnectreply.h"
#include "common/maptypes.h"
#include "common/proctypes.h"
#include "common/unit.h"
#include "fs/weather/metarparser.h"
#include "userdata/userdataicons.h"
#include "routeexport/routeexportformat.h"
#include "gui/dockwidgethandler.h"

#include <QCommandLineParser>
#include <QDebug>
#include <QSplashScreen>
#include <QSslSocket>
#include <QStyleFactory>
#include <QSharedMemory>
#include <QMessageBox>
#include <QLibrary>
#include <QPixmapCache>
#include <QSettings>
#include <QScreen>

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
  // Initialize the resources from atools static library
  Q_INIT_RESOURCE(atools);

  // Register all types to allow conversion from/to QVariant and thus reading/writing into settings
  atools::geo::registerMetaTypes();
  atools::fs::sc::registerMetaTypes();
  atools::gui::MapPosHistory::registerMetaTypes();
  atools::gui::DockWidgetHandler::registerMetaTypes();

  qRegisterMetaTypeStreamOperators<FsPathType>();
  qRegisterMetaTypeStreamOperators<SimulatorTypeMap>();

  qRegisterMetaTypeStreamOperators<map::DistanceMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::DistanceMarker> >();

  qRegisterMetaTypeStreamOperators<map::TrafficPattern>();
  qRegisterMetaTypeStreamOperators<QList<map::TrafficPattern> >();

  qRegisterMetaTypeStreamOperators<map::Hold>();
  qRegisterMetaTypeStreamOperators<QList<map::Hold> >();

  qRegisterMetaTypeStreamOperators<map::RangeMarker>();
  qRegisterMetaTypeStreamOperators<QList<map::RangeMarker> >();

  qRegisterMetaTypeStreamOperators<at::AircraftTrackPos>();
  qRegisterMetaTypeStreamOperators<QList<at::AircraftTrackPos> >();

  qRegisterMetaTypeStreamOperators<RouteExportFormat>();
  qRegisterMetaTypeStreamOperators<RouteExportFormatMap>();

  qRegisterMetaTypeStreamOperators<map::MapAirspaceFilter>();
  qRegisterMetaTypeStreamOperators<map::MapObjectRef>();

  atools::fs::FsPaths::intitialize();

  // Tasks that have to be done before creating the application object and logging system =================
  QStringList messages;
  QSettings earlySettings(QSettings::IniFormat, QSettings::UserScope, "ABarthel", "little_navmap");
  // The loading mechanism can be configured through the QT_OPENGL environment variable and the following application attributes:
  // Qt::AA_UseDesktopOpenGL Equivalent to setting QT_OPENGL to desktop.
  // Qt::AA_UseOpenGLES Equivalent to setting QT_OPENGL to angle.
  // Qt::AA_UseSoftwareOpenGL Equivalent to setting QT_OPENGL to software.
  QString renderOpt = earlySettings.value("Options/RenderOpt", "none").toString();
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
      messages.append("Wrong renderer " + renderOpt);
  }
  messages.append("RenderOpt " + renderOpt);

  int checkState = earlySettings.value("OptionsDialog/Widget_checkBoxOptionsGuiHighDpi", 2).toInt();
  if(checkState == 2)
  {
    messages.append("High DPI scaling enabled");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
  }
  else
  {
    messages.append("High DPI scaling disabled");
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling, false);
    // QGuiApplication::setAttribute(Qt::AA_DisableHighDpiScaling, true); // Freezes with QT_SCALE_FACTOR=2 on Linux
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, false);
    QGuiApplication::setAttribute(Qt::AA_Use96Dpi, true);
  }

  NavApp::setShowExceptionDialog(earlySettings.value("Options/ExceptionDialog", true).toBool());

  // Create application object ===========================================================
  int retval = 0;
  NavApp app(argc, argv);

#ifndef DEBUG_DISABLE_SPLASH
  // Start splash screen
  NavApp::initSplashScreen();
#endif

  DatabaseManager *dbManager = nullptr;

#if defined(Q_OS_WIN32)
  QApplication::addLibraryPath(QApplication::applicationDirPath() + QDir::separator() + "plugins");
#endif

  try
  {
    app.processEvents();

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption settingsDirOpt({"s", "settings-directory"},
                                      QObject::tr("Use <settings-directory> instead of \"%1\".").
                                      arg(NavApp::organizationName()),
                                      QObject::tr("settings-directory"));
    parser.addOption(settingsDirOpt);

    // Process the actual command line arguments given by the user
    parser.process(*QCoreApplication::instance());

    if(parser.isSet(settingsDirOpt) && !parser.value(settingsDirOpt).isEmpty())
      Settings::setOverrideOrganisation(parser.value(settingsDirOpt));

    // Initialize logging and force logfiles into the system or user temp directory
    // This will prefix all log files with orgranization and application name and append ".log"
    LoggingHandler::initializeForTemp(atools::settings::Settings::getOverloadedPath(
                                        ":/littlenavmap/resources/config/logging.cfg"));

    // Print some information which can be useful for debugging
    LoggingUtil::logSystemInformation();
    for(const QString& message : messages)
      qInfo() << message;

    qInfo().noquote().nospace() << "atools revision " << atools::gitRevision() << " "
                                << Application::applicationName() << " revision " << GIT_REVISION;

    LoggingUtil::logStandardPaths();
    Settings::logSettingsInformation();

    qInfo() << "SSL supported" << QSslSocket::supportsSsl()
            << "build library" << QSslSocket::sslLibraryBuildVersionString()
            << "library" << QSslSocket::sslLibraryVersionString();

    qInfo() << "Available styles" << QStyleFactory::keys();

    qInfo() << "SimConnectData Version" << atools::fs::sc::SimConnectData::getDataVersion()
            << "SimConnectReply Version" << atools::fs::sc::SimConnectReply::getReplyVersion();

    atools::fs::FsPaths::logAllPaths();

    qInfo() << "QT_OPENGL" << QProcessEnvironment::systemEnvironment().value("QT_OPENGL");
    qInfo() << "QT_SCALE_FACTOR" << QProcessEnvironment::systemEnvironment().value("QT_SCALE_FACTOR");
    if(app.testAttribute(Qt::AA_UseDesktopOpenGL))
      qInfo() << "Using Qt desktop renderer";
    if(app.testAttribute(Qt::AA_UseOpenGLES))
      qInfo() << "Using Qt angle renderer";
    if(app.testAttribute(Qt::AA_UseSoftwareOpenGL))
      qInfo() << "Using Qt software renderer";

    qInfo() << "UI default font" << app.font();
    for(const QScreen *screen: QGuiApplication::screens())
      qInfo() << "Screen" << screen->name()
              << "size" << screen->size()
              << "physical size" << screen->physicalSize()
              << "DPI ratio" << screen->devicePixelRatio()
              << "DPI x" << screen->logicalDotsPerInchX()
              << "y" << screen->logicalDotsPerInchX();

    migrate::checkAndMigrateSettings();

    Settings& settings = Settings::instance();

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

      if(font != QApplication::font())
        app.setFont(font);
    }
    qInfo() << "Loaded font" << font.toString() << "from options. Stored font info" << fontStr;

    // Load available translations ============================================
    qInfo() << "Loading translations for" << OptionsDialog::getLocale();
    Translator::load(OptionsDialog::getLocale());

    // Load region override ============================================
    // Forcing the English locale if the user has chosen it this way
    if(OptionsDialog::isOverrideRegion())
    {
      qInfo() << "Overriding region settings";
      QLocale::setDefault(QLocale("en"));
    }

    // Add paths here to allow translation =================================
    Application::addReportPath(QObject::tr("Log files:"), LoggingHandler::getLogFiles());

    Application::addReportPath(QObject::tr("Database directory:"),
                               {Settings::getPath() + QDir::separator() + lnm::DATABASE_DIR});
    Application::addReportPath(QObject::tr("Configuration:"), {Settings::getFilename()});
    Application::setEmailAddresses({"alex@littlenavmap.org"});

    // Load help URLs from urls.cfg =================================
    lnm::loadHelpUrls();

    // Avoid static translations and load these dynamically now  =================================
    Unit::initTranslateableTexts();
    UserdataIcons::initTranslateableTexts();
    map::initTranslateableTexts();
    proc::initTranslateableTexts();
    atools::fs::weather::initTranslateableTexts();
    formatter::initTranslateableTexts();

#if defined(Q_OS_MACOS)
    // Check for minimum macOS version 10.10
    if(QSysInfo::macVersion() != QSysInfo::MV_None && QSysInfo::macVersion() < QSysInfo::MV_10_10)
    {
      NavApp::deleteSplashScreen();
      QMessageBox::critical(nullptr, QObject::tr("%1 - Error").arg(QApplication::applicationName()),
                            QObject::tr("%1 needs at least macOS Yosemite version 10.10 or newer.").
                            arg(QApplication::applicationName()));
      return 1;
    }
#endif

#if defined(Q_OS_WIN32)
    // Detect other running application instance - this is unsafe on Unix since shm can remain after crashes
    QSharedMemory shared("203abd54-8a6a-4308-a654-6771efec62cd"); // generated GUID
    if(!shared.create(512, QSharedMemory::ReadWrite))
    {
      NavApp::deleteSplashScreen();
      QMessageBox::critical(nullptr, QObject::tr("%1 - Error").arg(QApplication::applicationName()),
                            QObject::tr("%1 is already running.").arg(QApplication::applicationName()));
      return 1;
    }
#endif

    // Set up Marble widget and print debugging informations
    MarbleGlobal::Profiles profiles = MarbleGlobal::detectProfiles();
    MarbleGlobal::getInstance()->setProfiles(profiles);

    qDebug() << "Marble Local Path:" << MarbleDirs::localPath();
    qDebug() << "Marble Plugin Local Path:" << MarbleDirs::pluginLocalPath();
    qDebug() << "Marble Data Path (Run Time) :" << MarbleDirs::marbleDataPath();
    qDebug() << "Marble Plugin Path (Run Time) :" << MarbleDirs::marblePluginPath();
    qDebug() << "Marble System Path:" << MarbleDirs::systemPath();
    qDebug() << "Marble Plugin System Path:" << MarbleDirs::pluginSystemPath();

    MarbleDirs::setMarbleDataPath(QApplication::applicationDirPath() + QDir::separator() + "data");

#if defined(Q_OS_MACOS)
    QDir pluginsDir(QApplication::applicationDirPath());
    pluginsDir.cdUp();
    pluginsDir.cd("PlugIns");
    MarbleDirs::setMarblePluginPath(pluginsDir.absolutePath());
#else
    MarbleDirs::setMarblePluginPath(QApplication::applicationDirPath() + QDir::separator() + "plugins");
#endif

    MarbleDebug::setEnabled(settings.getAndStoreValue(lnm::OPTIONS_MARBLE_DEBUG, false).toBool());

    qDebug() << "New Marble Local Path:" << MarbleDirs::localPath();
    qDebug() << "New Marble Plugin Local Path:" << MarbleDirs::pluginLocalPath();
    qDebug() << "New Marble Data Path (Run Time) :" << MarbleDirs::marbleDataPath();
    qDebug() << "New Marble Plugin Path (Run Time) :" << MarbleDirs::marblePluginPath();
    qDebug() << "New Marble System Path:" << MarbleDirs::systemPath();
    qDebug() << "New Marble Plugin System Path:" << MarbleDirs::pluginSystemPath();

    // Disable tooltip effects since these do not work well with tooltip updates while displaying
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);

    // Check if database is compatible and ask the user to erase all incompatible ones
    // If erasing databases is refused exit application
    bool databasesErased = false;
    dbManager = new DatabaseManager(nullptr);

    /* Copy from application directory to settings directory if newer and create indexes if missing */
    dbManager->checkCopyAndPrepareDatabases();

    if(dbManager->checkIncompatibleDatabases(&databasesErased))
    {
      delete dbManager;
      dbManager = nullptr;

      MainWindow mainWindow;

      // Show database dialog if something was removed
      mainWindow.setDatabaseErased(databasesErased);

      mainWindow.show();

      // Hide splash once main window is shown
      NavApp::finishSplashScreen();

      qDebug() << "Before app.exec()";
      retval = app.exec();
    }

    qInfo() << "app.exec() done, retval is" << retval << (retval == 0 ? "(ok)" : "(error)");
  }
  catch(atools::Exception& e)
  {
    ATOOLS_HANDLE_EXCEPTION(e);
    // Does not return in case of fatal error
  }
  catch(...)
  {
    ATOOLS_HANDLE_UNKNOWN_EXCEPTION;
    // Does not return in case of fatal error
  }

  delete dbManager;
  dbManager = nullptr;

  return retval;
}
