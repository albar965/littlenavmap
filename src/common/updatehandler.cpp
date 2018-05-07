/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "common/updatehandler.h"

#include "gui/updatedialog.h"
#include "util/updatecheck.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "navapp.h"
#include "gui/mainwindow.h"
#include "util/htmlbuilder.h"
#include "util/updatecheck.h"
#include "gui/dialog.h"

#include <QDialogButtonBox>

using atools::util::UpdateCheck;
using atools::settings::Settings;
namespace html = atools::util::html;

UpdateHandler::UpdateHandler(MainWindow *parent)
  : QObject(parent)
{
#ifdef DEBUG_UPDATE
  updateCheck = new UpdateCheck(true);
#else
  updateCheck = new UpdateCheck(false);
#endif

  // Get URL from options for debugging otherwise use default
  updateCheck->setUrl(Settings::instance().valueStr(lnm::OPTIONS_UPDATE_URL, lnm::OPTIONS_UPDATE_DEFAULT_URL));

  connect(updateCheck, &UpdateCheck::updateFound, this, &UpdateHandler::updateFound);
  connect(updateCheck, &UpdateCheck::updateFailed, this, &UpdateHandler::updateFailed);
}

UpdateHandler::~UpdateHandler()
{
  delete updateCheck;
}

void UpdateHandler::checkForUpdates(opts::UpdateChannels channelOpts, bool manuallyTriggered)
{
  qDebug() << Q_FUNC_INFO << "channels" << channelOpts << "manual" << manuallyTriggered;

  manual = manuallyTriggered;
#ifndef DEBUG_UPDATE
  if(!manuallyTriggered)
  {
    // Check timestamp if automatically triggered on starup
    qlonglong diff = 0;
    switch(OptionData::instance().getUpdateRate())
    {
      case opts::DAILY:
        diff = 24 * 60 * 60;
        break;
      case opts::WEEKLY:
        diff = 24 * 60 * 60 * 7;
        break;
      case opts::NEVER:
        // Let  use check manually
        return;
    }

    qlonglong now = QDateTime::currentDateTime().toSecsSinceEpoch();
    qlonglong timestamp = Settings::instance().valueVar(lnm::OPTIONS_UPDATE_LAST_CHECKED).toLongLong();
    if(now - timestamp < diff)
    {
      qInfo() << "Skipping update check. Difference" << now - timestamp;
      return;
    }
  }
#endif

  QString checked;
  if(!manuallyTriggered)
    // Get skipped update
    checked = Settings::instance().valueStr(lnm::OPTIONS_UPDATE_CHECKED);

  // Copy combo box selection to channel enum
  atools::util::UpdateChannels channels = atools::util::STABLE;
  switch(channelOpts)
  {
    case opts::STABLE:
      channels = atools::util::STABLE;
      break;
    case opts::STABLE_BETA:
      channels = atools::util::STABLE | atools::util::BETA;
      break;
    case opts::STABLE_BETA_DEVELOP:
      channels = atools::util::STABLE | atools::util::BETA | atools::util::DEVELOP;
      break;
  }

  // Async
  qInfo() << "Checking for updates. Channels" << channels << "already checked" << checked;
  updateCheck->checkForUpdates(checked, manuallyTriggered /* notifyForEmptyUpdates */, channels);
  Settings::instance().setValueVar(lnm::OPTIONS_UPDATE_LAST_CHECKED, QDateTime::currentDateTime().toSecsSinceEpoch());
}

/* Called by update checker */
void UpdateHandler::updateFound(atools::util::UpdateList updates)
{
  qDebug() << Q_FUNC_INFO;
  NavApp::deleteSplashScreen();

  if(!updates.isEmpty())
  {
    // Found updates - fill the HTML text for the dialog =============================
    atools::util::HtmlBuilder html(false);

    html.h3(tr("Update Available"));

    // Show only the newest one
    const atools::util::Update& update = updates.first();
    QString channel;
    switch(update.channel)
    {
      case atools::util::STABLE:
        channel = tr("Stable Version");
        break;
      case atools::util::BETA:
        channel = tr("Beta/Test Version");
        break;
      case atools::util::DEVELOP:
        channel = tr("Development Version");
        break;
    }

    html.b(tr("%1: %2").arg(channel).arg(update.version));

    if(!update.url.isEmpty())
      html.a(tr("&nbsp;&nbsp;&nbsp;&nbsp;<b>&gt;&gt; Release Information &lt;&lt;</b>"),
             update.url, html::NO_ENTITIES | html::LINK_NO_UL);

    bool hasDownload = false;
    if(!update.download.isEmpty())
    {
      hasDownload = true;
      html.a(tr("&nbsp;&nbsp;&nbsp;&nbsp;<b>&gt;&gt; Download &lt;&lt;</b>"),
             update.download, html::NO_ENTITIES | html::LINK_NO_UL);
    }
    else
      html.p().b(tr("No download available for this operating system.")).pEnd();

    if(!update.changelog.isEmpty())
      html.text(update.changelog, atools::util::html::NO_ENTITIES);

    NavApp::deleteSplashScreen();

    // Show dialog
    UpdateDialog information(mainWindow, manual, hasDownload);
    information.setMessage(html.getHtml(), update.download);

    // Show dialog
    information.exec();

    if(!manual)
    {
      // DestructiveRole = ignore
      // NoRole = later
      // RejectRole = Close (manual only)
      // YesRole = download (has download only)
      QDialogButtonBox::ButtonRole role = information.getButtonClickedRole();

      if(role == QDialogButtonBox::DestructiveRole)
      {
        // Add latest update - do not report anything earlier or equal again
        QString ignore = updates.first().version;
        qDebug() << Q_FUNC_INFO << "Ignoring updates now" << ignore;
        Settings::instance().setValue(lnm::OPTIONS_UPDATE_CHECKED, ignore);
      }
    }
  }
  else
    // Nothing found but notification requested for manual trigger
    QMessageBox::information(mainWindow, QApplication::applicationName(), QObject::tr("No Updates available."));
}

/* Called by update checker */
void UpdateHandler::updateFailed(QString errorString)
{
  qDebug() << Q_FUNC_INFO;

  NavApp::deleteSplashScreen();

  QString message = tr("Error while checking for updates at\n\"%1\":\n%2").
                    arg(updateCheck->getUrl().toDisplayString()).arg(errorString);

  if(manual)
    QMessageBox::information(mainWindow, QApplication::applicationName(), message);
  else
    atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOW_UPDATEFAILED, message,
                                                   tr("Do not &show this dialog again."));
}
