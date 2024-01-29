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

#include <QStringBuilder>
#include <QDir>
#include <QUrl>

XpconnectInstaller::XpconnectInstaller(QWidget *parentWidget)
  : parent(parentWidget)
{
  dialog = new atools::gui::Dialog(parent);

  // Use name in macOS depending on system version
#ifdef Q_OS_MACOS
  if(QSysInfo::productVersion().toFloat() >= 10.14)
    // "Little Xpconnect arm64":
    // This is for Apple computers having an Apple Silicon or an Intel CPU.
    // It supports only newer macOS releases from Mojave 10.14 and later.
    xpconnectName = "Little Xpconnect arm64";
  else
    // "Little Xpconnect x86":
    // This is for Apple computers having an Intel CPU. This supports
    // older macOS releases from High Sierra 10.13.
    xpconnectName = "Little Xpconnect x86";
#else
  xpconnectName = "Little Xpconnect";
#endif
}

XpconnectInstaller::~XpconnectInstaller()
{
  delete dialog;
}

bool XpconnectInstaller::install()
{
  qDebug() << Q_FUNC_INFO;

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
  QUrl fileUrl = QUrl::fromLocalFile(pluginsPath);
  atools::gui::MessageBox infoBox(parent, QCoreApplication::applicationName(), lnm::ACTIONS_INSTALL_XPCONNECT_INFO,
                                  tr("Do not &show this dialog again and install in the future."), true /* openLinkAuto */);
  infoBox.setHelpUrl(lnm::helpOnlineUrl + "XPCONNECT.html", lnm::helpLanguageOnline());
  infoBox.setMessage(tr("<p>Install or update the Little Xpconnect plugin for %1 in the directory below?</p>"
                          "<p><a href=\"%2\">%3</a>&nbsp;(click to open)</p>"
                            "<p>The X-Plane target installation is as selected in the menu \"Scenery Library\".</p>"
                              "%4").arg(NavApp::getCurrentSimulatorName()).arg(fileUrl.toString()).
                     arg(atools::nativeCleanPath(pluginsPath)).arg(macOsNote));

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
        fileUrl = QUrl::fromLocalFile(pluginsDir.filePath(xpl.constFirst()));
        atools::gui::MessageBox warnBox(parent, QCoreApplication::applicationName(), lnm::ACTIONS_INSTALL_XPCONNECT_WARN_XPL,
                                        tr("Do not &show this dialog again."));
        warnBox.setShowInFileManager();
        warnBox.setHelpUrl(lnm::helpOnlineUrl + "XPCONNECT.html", lnm::helpLanguageOnline());
        warnBox.setMessage(tr("<p>A stray X-Plane plugin file was found:</p>"
                                "<p><a href=\"%1\">%2</a>&nbsp;(click to show)</p>"
                                  "<p>This is not neccessarily from Little Xpconnect but it is usually a "
                                    "result from an incorrect plugin installation which can cause problems.<br/>"
                                    "Removing this file is recommended.</p>").
                           arg(fileUrl.toString()).arg(atools::nativeCleanPath(pluginsDir.filePath(xpl.constFirst()))));
        warnBox.setIcon(QMessageBox::Warning);
        warnBox.exec();
      }

      // Read previous installations but exclude name of current installation since it will be overwritten anyway
      QStringList xpconnects = pluginsDir.entryList({"*Xpconnect*"}, QDir::Dirs | QDir::NoDotAndDotDot);
      xpconnects.removeAll(xpconnectName);

      // Log all file operations
      atools::util::FileOperations fileOperations(true /* verbose */);

      bool deleteOk = true;
      if(!xpconnects.isEmpty())
      {
        // Convert paths to display paths using native notation
        QStringList xpconnectsText;
        for(QString& xpconnect : xpconnects)
        {
          xpconnect = atools::nativeCleanPath(pluginsDir.absoluteFilePath(xpconnect));
          xpconnectsText.append(tr("<li><a href=\"%1\">%2</a></li>").
                                arg(QUrl::fromLocalFile(xpconnect).toString()).arg(atools::nativeCleanPath(xpconnect)));
        }

        atools::gui::MessageBox questionBox(parent);
        questionBox.setShowInFileManager();
        questionBox.setMessage(tr("<p>Found one or more previous installations of Little Xpconnect using a non-standard name:</p>"
                                    "<p><ul>%1</ul>(click to show)</p>"
                                      "<p>Check these plugins manually if you are not sure what they are.</p>"
                                        "<p>Move these plugins to the system trash now to avoid issues?</p>").
                               arg(xpconnectsText.join(QString())));
        questionBox.setIcon(QMessageBox::Question);

        if(questionBox.exec() == QMessageBox::Yes)
        {
          // Delete all found plugins
          for(const QString& xpconnect : qAsConst(xpconnects))
          {
            fileOperations.removeDirectoryToTrash(xpconnect);
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
#ifdef Q_OS_MACOS
        QDir bundleDir(QCoreApplication::applicationDirPath());
        bundleDir.cdUp(); // MacOS
        bundleDir.cdUp(); // Contents
        bundleDir.cdUp(); // Little Navmap.app
        QString sourcePath = bundleDir.path() % QDir::separator() % xpconnectName;
#else
        QString sourcePath = QCoreApplication::applicationDirPath() % QDir::separator() % xpconnectName;
#endif
        // Copy from installation folder recursively
        fileOperations.copyDirectory(sourcePath, pluginsPath % QDir::separator() % xpconnectName,
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
