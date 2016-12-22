/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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
#include "settings/settings.h"
#include "gui/translator.h"
#include "gui/mapposhistory.h"
#include "gui/application.h"
#include "exception.h"
#include "gui/errorhandler.h"
#include "db/databasemanager.h"
#include "common/settingsmigrate.h"
#include "common/aircrafttrack.h"
#include "fs/sc/simconnectdata.h"
#include "fs/sc/simconnectreply.h"

#include <QDebug>
#include <QSplashScreen>
#include <QSslSocket>
#include <QStyleFactory>

#if defined(Q_OS_WIN32)
#include <QSharedMemory>
#include <QMessageBox>
#endif

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
  qRegisterMetaTypeStreamOperators<atools::geo::Pos>();
  qRegisterMetaTypeStreamOperators<FsPathType>();

  qRegisterMetaTypeStreamOperators<atools::fs::FsPaths::SimulatorType>();
  qRegisterMetaTypeStreamOperators<SimulatorTypeMap>();

  qRegisterMetaTypeStreamOperators<atools::gui::MapPosHistoryEntry>();
  qRegisterMetaTypeStreamOperators<QList<atools::gui::MapPosHistoryEntry> >();

  qRegisterMetaTypeStreamOperators<maptypes::DistanceMarker>();
  qRegisterMetaTypeStreamOperators<QList<maptypes::DistanceMarker> >();

  qRegisterMetaTypeStreamOperators<maptypes::RangeMarker>();
  qRegisterMetaTypeStreamOperators<QList<maptypes::RangeMarker> >();

  qRegisterMetaTypeStreamOperators<at::AircraftTrackPos>();
  qRegisterMetaTypeStreamOperators<QList<at::AircraftTrackPos> >();

  // Needed to send SimConnectData through queued connections
  qRegisterMetaType<atools::fs::sc::SimConnectData>();
  qRegisterMetaType<atools::fs::sc::SimConnectReply>();
  qRegisterMetaType<atools::fs::sc::WeatherRequest>();

  // Set application information
  int retval = 0;
  Application app(argc, argv);
  Application::setWindowIcon(QIcon(":/littlenavmap/resources/icons/littlenavmap.svg"));
  Application::setApplicationName("Little Navmap");
  Application::setOrganizationName("ABarthel");
  Application::setOrganizationDomain("abarthel.org");
  Application::setApplicationVersion("1.2.1.beta");

  // Start splash screen
  QPixmap pixmap(":/littlenavmap/resources/icons/splash.png");
  QSplashScreen splash(pixmap);
  splash.show();
  app.processEvents();

  splash.showMessage(QObject::tr("Version %5 (revision %6)").
                     arg(Application::applicationVersion()).arg(GIT_REVISION),
                     Qt::AlignRight | Qt::AlignBottom, Qt::white);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  DatabaseManager *dbManager = nullptr;

#if defined(Q_OS_WIN32)
  QApplication::addLibraryPath(QApplication::applicationDirPath() + QDir::separator() + "plugins");
#endif

  try
  {
    app.processEvents();

    // Initialize logging and force logfiles into the system or user temp directory
    // This will prefix all log files with orgranization and application name and append ".log"
    LoggingHandler::initializeForTemp(atools::settings::Settings::getOverloadedPath(
                                        ":/littlenavmap/resources/config/logging.cfg"));
    Application::addReportPath(QObject::tr("Log files:"), LoggingHandler::getLogFiles());

    Application::addReportPath(QObject::tr("Database directory:"),
                               {Settings::getPath() + QDir::separator() + lnm::DATABASE_DIR});
    Application::addReportPath(QObject::tr("Configuration:"), {Settings::getFilename()});
    Application::setEmailAddresses({"albar965@mailbox.org", "albar965@t-online.de"});

    // Print some information which can be useful for debugging
    LoggingUtil::logSystemInformation();
    LoggingUtil::logStandardPaths();
    Settings::logSettingsInformation();

    qInfo() << "SimConnectData Version" << atools::fs::sc::SimConnectData::getDataVersion()
            << "SimConnectReply Version" << atools::fs::sc::SimConnectReply::getReplyVersion();

    atools::fs::FsPaths::logAllPaths();

    qInfo() << "SSL supported" << QSslSocket::supportsSsl();
    qInfo() << "Available styles" << QStyleFactory::keys();

    migrate::checkAndMigrateSettings();

    Settings& settings = Settings::instance();

    // Load local and Qt system translations from various places
    Translator::load(settings.valueStr(lnm::OPTIONS_LANGUAGE, QString()));

#if defined(Q_OS_WIN32)
    // Detect other running application instance - this is unsafe on Unix since shm can remain after crashes
    QSharedMemory shared("203abd54-8a6a-4308-a654-6771efec62cd"); // generated GUID
    if(!shared.create(512, QSharedMemory::ReadWrite))
    {
      splash.close();
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
    MarbleDirs::setMarblePluginPath(QApplication::applicationDirPath() + QDir::separator() + "plugins");
    MarbleDebug::setEnabled(settings.getAndStoreValue(lnm::OPTIONS_MARBLEDEBUG, false).toBool());

    qDebug() << "New Marble Local Path:" << MarbleDirs::localPath();
    qDebug() << "New Marble Plugin Local Path:" << MarbleDirs::pluginLocalPath();
    qDebug() << "New Marble Data Path (Run Time) :" << MarbleDirs::marbleDataPath();
    qDebug() << "New Marble Plugin Path (Run Time) :" << MarbleDirs::marblePluginPath();
    qDebug() << "New Marble System Path:" << MarbleDirs::systemPath();
    qDebug() << "New Marble Plugin System Path:" << MarbleDirs::pluginSystemPath();

    // Check if database is compatible and ask the user to erase all incompatible ones
    // If erasing databases is refused exit application
    dbManager = new DatabaseManager(nullptr);
    if(dbManager->checkIncompatibleDatabases(&splash))
    {
      delete dbManager;
      dbManager = nullptr;

      MainWindow mainWindow;
      mainWindow.show();

      // Hide splash once main window is shown
      splash.finish(&mainWindow);

      qDebug() << "Before app.exec()";
      retval = app.exec();
    }

    qDebug() << "app.exec() done, retval is" << retval << (retval == 0 ? "(ok)" : "(error)");
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
