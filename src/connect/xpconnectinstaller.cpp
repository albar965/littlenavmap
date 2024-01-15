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
#include "gui/helphandler.h"
#include "util/fileoperations.h"

#include <QStringBuilder>

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

  // Ask general question ==========================================
  int cont = dialog->showQuestionMsgBox(lnm::ACTIONS_INSTALL_XPCONNECT_INFO,
                                        tr("<p>Install or update the Little Xpconnect plugin for %1 in the directory below?</p>"
                                             "<p>\"%2\"</p>"
                                               "<p>The X-Plane target installation is as selected in the menu \"Scenery Library\".</p>").
                                        arg(NavApp::getCurrentSimulatorName()).arg(pluginsPath),
                                        tr("Do not &show this dialog again and install in the future."),
                                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Help, QMessageBox::No, QMessageBox::Yes);

  if(cont == QMessageBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(parent, lnm::helpOnlineUrl + "XPCONNECT.html", lnm::helpLanguageOnline());
  else if(cont == QMessageBox::Yes)
  {
    // Check if plugins are accessible
    QString errors = atools::checkDirMsg(pluginsPath);
    if(errors.isEmpty())
    {
      QDir pluginsDir(pluginsPath);

      // Look for stray plugin files and let user know
      QStringList xpl = pluginsDir.entryList({"*.xpl"}, QDir::Files | QDir::NoDotAndDotDot);
      if(!xpl.isEmpty())
        dialog->showWarnMsgBox(lnm::ACTIONS_INSTALL_XPCONNECT_WARN_XPL,
                               tr("<p>A stray X-Plane plugin file was found:</p>"
                                    "<p>\"%1\"</p>"
                                      "<p>This is not neccessarily from Little Xpconnect but it is usually a "
                                        "result from an incorrect plugin installation which can cause problems.<br/>"
                                        "Removing this file is recommended.</p>").arg(pluginsDir.filePath(xpl.constFirst())),
                               tr("Do not &show this dialog again."));

      // Read previous installations but exclude name of current installation since it will be overwritten anyway
      QStringList xpconnects = pluginsDir.entryList({"*Xpconnect*"}, QDir::Dirs | QDir::NoDotAndDotDot);
      xpconnects.removeAll(xpconnectName);

      // Log all file operations
      atools::util::FileOperations fileOperations(true /* verbose */);

      bool deleteOk = true;
      if(!xpconnects.isEmpty())
      {
        // Convert paths to display paths using native notation
        for(QString& xpconnect : xpconnects)
          xpconnect = atools::nativeCleanPath(pluginsDir.absoluteFilePath(xpconnect));

        int del = dialog->question(tr("<p>Found one or more previous installations of Little Xpconnect using a non-standard name:</p>"
                                        "<p>%1</p>"
                                          "<p>Check these plugins manually if you are not sure what they are.</p>"
                                            "<p>Delete these plugins now to avoid issues?</p>").
                                   arg(atools::strJoin(tr("\""), xpconnects, tr("\"<br/>\""), tr("\"<br/>\""), tr("\""))),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if(del == QMessageBox::Yes)
        {
          // Delete all found plugins
          for(const QString& xpconnect : qAsConst(xpconnects))
          {
            if(!fileOperations.removeDirectory(xpconnect))
            {
              dialog->warning(tr("Error(s) while deleting directory: %1 installation stopped.").
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
        if(!fileOperations.copyDirectory(QCoreApplication::applicationDirPath() % QDir::separator() % xpconnectName,
                                         pluginsPath % QDir::separator() % xpconnectName, true /* overwrite */))
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
