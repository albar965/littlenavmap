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

#include "connect/xpconnectinstaller.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "gui/dialog.h"
#include "util/fileoperations.h"
#include "gui/messagebox.h"
#include "util/htmlbuilder.h"

#include <QStringBuilder>
#include <QDir>
#include <QUrl>

XpconnectInstaller::XpconnectInstaller(QWidget *parentWidget)
  : parent(parentWidget)
{
  dialog = new atools::gui::Dialog(parent);

}

QString XpconnectInstaller::xpconnectName()
{
#ifdef Q_OS_MACOS

  if(QSysInfo::productVersion().toFloat() >= 10.14)
    // "Little Xpconnect arm64":
    // This is for Apple computers having an Apple Silicon or an Intel CPU.
    // It supports only newer macOS releases from Mojave 10.14 and later.
    return "Little Xpconnect arm64";
  else
    // "Little Xpconnect x86":
    // This is for Apple computers having an Intel CPU. This supports
    // older macOS releases from High Sierra 10.13.
    return "Little Xpconnect x86";

#else
  return "Little Xpconnect";

#endif

}

QString XpconnectInstaller::xpconnectPath()
{
  // Use name in macOS depending on system version
#ifdef Q_OS_MACOS

  QDir bundleDir(QCoreApplication::applicationDirPath()); // littlenavmap
  bundleDir.cdUp(); // MacOS
  bundleDir.cdUp(); // Contents
  bundleDir.cdUp(); // Little Navmap.app

  return bundleDir.path() % atools::SEP % xpconnectName();

#else
  return QCoreApplication::applicationDirPath() % atools::SEP % xpconnectName();

#endif
}

XpconnectInstaller::~XpconnectInstaller()
{
  delete dialog;
}

bool XpconnectInstaller::install()
{
  qDebug() << Q_FUNC_INFO;
  using atools::util::HtmlBuilder;

  bool retval = false;

  // .../X-Plane 11/Resources/plugins
  QString pluginsPath = atools::buildPathNoCase({NavApp::getCurrentSimulatorBasePath(), "resources", "plugins"});

  QString macOsNote;
#ifdef Q_OS_MACOS
  macOsNote = tr("<p>Note to Apple macOS users:<br/>"
                 "You must remove the quarantine flag from the plugin to prevent "
                 "Apple Gatekeeper from blocking the plugin from loading.<br/>Click the help button for more information.</p>");
#endif

  // Ask general question ==========================================
  atools::gui::MessageBox infoBox(parent, QCoreApplication::applicationName(), lnm::ACTIONS_INSTALL_XPCONNECT_INFO,
                                  tr("Do not &show this dialog again and install in the future."), true /* openLinkAuto */);
  infoBox.setHelpUrl(lnm::helpOnlineUrl + "XPCONNECT.html", lnm::helpLanguageOnline());
  infoBox.setMessage(tr("<p>Install or update the Little Xpconnect plugin for %1 in the directory below?</p>"
                          "<p>%2&nbsp;(click to open)</p>"
                            "<p>The X-Plane target installation is as selected in the menu \"Scenery Library\".</p>"
                              "<p><b>Note that you have to close X-Plane while installing the plugin.</b></p>"
                                "%3").
                     arg(NavApp::getCurrentSimulatorName()).
                     arg(HtmlBuilder::aFilePath(pluginsPath, atools::util::html::NOBR_WHITESPACE)).
                     arg(macOsNote));

  infoBox.setIcon(QMessageBox::Question);
  infoBox.setDefaultButton(QDialogButtonBox::Yes);

  if(infoBox.exec() == QMessageBox::Yes)
  {
    // Check if plugins are accessible
    QString errors = atools::checkDirMsg(pluginsPath);
    if(errors.isEmpty())
    {
      QDir pluginsDir(pluginsPath);

      QString strayPluginName;
#if defined(Q_OS_WIN32)
      strayPluginName = "win.xpl";
#elif defined(Q_OS_LINUX)
      strayPluginName = "lin.xpl";
#elif defined(Q_OS_MACOS)
      strayPluginName = "mac.xpl";
#endif

      // Look for stray plugin files and let user know
      QStringList xpl = pluginsDir.entryList({strayPluginName}, QDir::Files | QDir::NoDotAndDotDot);
      if(!xpl.isEmpty())
      {
        atools::gui::MessageBox warnBox(parent, QCoreApplication::applicationName(), lnm::ACTIONS_INSTALL_XPCONNECT_WARN_XPL,
                                        tr("Do not &show this dialog again."));
        warnBox.setShowInFileManager();
        warnBox.setHelpUrl(lnm::helpOnlineUrl + "XPCONNECT.html", lnm::helpLanguageOnline());
        warnBox.setMessage(tr("<p>A stray X-Plane plugin file was found:</p>"
                                "<p>%1&nbsp;(click to show)</p>"
                                  "<p>This is not necessarily from Little Xpconnect but it is usually a "
                                    "result from an incorrect plugin installation which can cause X-Plane to crash.<br/>"
                                    "Removing this file is recommended.</p>").
                           arg(HtmlBuilder::aFilePath(pluginsDir.filePath(xpl.constFirst()), atools::util::html::NOBR_WHITESPACE)));
        warnBox.setIcon(QMessageBox::Warning);
        warnBox.exec();
      }

      // Read previous installations but exclude name of current installation since it will be overwritten anyway
      QStringList xpconnects = pluginsDir.entryList({"*little*xpconnect*"}, QDir::Dirs | QDir::NoDotAndDotDot);
      xpconnects.removeAll(xpconnectName());

      // Log all file operations
      atools::util::FileOperations fileOperations(true /* verbose */);

      bool deleteOk = true;
      if(!xpconnects.isEmpty())
      {
        // Convert paths to display paths using native notation
        QStringList xpconnectsText;
        for(QString& xpc : xpconnects)
        {
          xpc = atools::nativeCleanPath(pluginsDir.absoluteFilePath(xpc));
          xpconnectsText.append(tr("<li>%1</li>").arg(HtmlBuilder::aFilePath(xpc, atools::util::html::NOBR_WHITESPACE)));
        }

        atools::gui::MessageBox questionBox(parent);
        questionBox.setShowInFileManager();
        questionBox.setMessage(tr("<p>Found one or more previous installations of Little Xpconnect using a non-standard name:</p>"
                                    "<p><ul>%1</ul>(click to show)</p>"
                                      "<p>Check these plugins manually if you are not sure what they are.</p>"
                                        "<p>Move these plugins to the system trash now to avoid issues like X-Plane crashes?</p>").
                               arg(xpconnectsText.join(QString())));
        questionBox.setIcon(QMessageBox::Question);

        if(questionBox.exec() == QMessageBox::Yes)
        {
          // Delete all found plugins
          for(const QString& xpc : qAsConst(xpconnects))
          {
            fileOperations.removeDirectoryToTrash(xpc);
            if(fileOperations.hasErrors())
            {
              dialog->warning(tr("Error(s) while moving directory to trash: %1 installation stopped.").
                              arg(fileOperations.getErrors().join(tr(", "))));

              // Stop copying
              deleteOk = false;
            }
          }
        }
      }

      if(deleteOk)
      {
        // Copy from installation folder recursively
        fileOperations.copyDirectory(xpconnectPath(), pluginsPath % atools::SEP % xpconnectName(),
                                     true /* overwrite */, true /* hidden */, true /* system */);

        if(fileOperations.hasErrors())
          dialog->warning(tr("Error(s) while copying directory: %1").arg(fileOperations.getErrors().join(tr(", "))));
        else
          retval = true;
      }
    }
    else
      dialog->warning(tr("X-Plane plugins path \"%1\" not valid.\nReason: %2.").arg(pluginsPath).arg(errors));
  }

  return retval;
}

bool XpconnectInstaller::isXpconnectAvailable()
{
  return QFile::exists(xpconnectPath());
}
