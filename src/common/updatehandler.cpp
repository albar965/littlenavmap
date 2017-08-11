/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "util/updatecheck.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "navapp.h"
#include "gui/mainwindow.h"
#include "util/htmlbuilder.h"
#include "util/updatecheck.h"
#include "gui/dialog.h"

#include <QMessageBox>

using atools::util::UpdateCheck;
using atools::settings::Settings;
namespace html = atools::util::html;

UpdateHandler::UpdateHandler(MainWindow *parent)
  : QObject(parent)
{
  updateCheck = new UpdateCheck();
  updateCheck->setUrl(lnm::OPTIONS_UPDATE_URL);

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
    qlonglong timestamp = Settings::instance().valueVar(lnm::OPTIONS_UPDATES_LAST_CHECKED).toLongLong();
    if(now - timestamp < diff)
    {
      qInfo() << "Skipping update check. Difference" << now - timestamp;
      return;
    }
  }

  QStringList checked;
  if(!manuallyTriggered)
    // Get skipped updates
    checked = Settings::instance().valueStrList(lnm::OPTIONS_UPDATES_CHECKED);

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
}

/* Called by update checker */
void UpdateHandler::updateFound(atools::util::UpdateList updates)
{
  qDebug() << Q_FUNC_INFO;
  NavApp::deleteSplashScreen();

  Settings::instance().setValueVar(lnm::OPTIONS_UPDATES_LAST_CHECKED, QDateTime::currentDateTime().toSecsSinceEpoch());

  if(!updates.isEmpty())
  {
    // Found updates - fill the HTML text for the dialog =============================
    atools::util::HtmlBuilder html(false);

    html.h3(tr("Updates Found"));
    for(const atools::util::Update& update : updates)
    {
      QString channel;
      switch(update.channel)
      {
        case atools::util::STABLE:
          channel = tr("Stable");
          break;
        case atools::util::BETA:
          channel = tr("Beta");
          break;
        case atools::util::DEVELOP:
          channel = tr("Development");
          break;
      }

      html.b(tr("%1: %2").arg(channel).arg(update.version));

      if(!update.changelog.isEmpty())
        html.text(update.changelog, atools::util::html::NO_ENTITIES);

      if(!update.download.isEmpty())
        html.p().a(tr("<b>&gt;&gt; Direct Download</b>"), update.download, html::NO_ENTITIES | html::LINK_NO_UL).pEnd();


      else
        html.p(tr("No download available for this operating system."));

      if(!update.url.isEmpty())
        html.p().a(tr("<b>&gt;&gt; Release Page</b>"), update.url, html::NO_ENTITIES | html::LINK_NO_UL).pEnd();
      html.hr();
    }

    // Show dialog
    QMessageBox::StandardButtons buttons = manual ? QMessageBox::Ok : QMessageBox::Retry | QMessageBox::Ignore;
    QMessageBox information(QMessageBox::Information, QApplication::applicationName(),
                            html.getHtml(), buttons, mainWindow);
    information.setWindowFlags(information.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    information.setTextInteractionFlags(Qt::TextBrowserInteraction);
    information.setWindowModality(Qt::ApplicationModal);

    if(!manual)
    {
      // Set ignore and remind me buttons
      if(updates.size() > 1)
        information.setButtonText(QMessageBox::Ignore, tr("&Ignore these Updates"));
      else
        information.setButtonText(QMessageBox::Ignore, tr("&Ignore this Update"));

      information.setButtonText(QMessageBox::Retry, tr("&Remind me Later"));
      information.setDefaultButton(QMessageBox::Retry);
      information.setEscapeButton(QMessageBox::Retry);
    }
    // else use ok button only for manually triggered updates

    // Show dialog
    int selected = information.exec();

    if(!manual)
    {
      if(selected == QMessageBox::Ignore)
      {
        // Add all updates shown to the skip list
        QStringList ignore = Settings::instance().valueStrList(lnm::OPTIONS_UPDATES_CHECKED);

        for(const atools::util::Update& update : updates)
          ignore.append(update.version);

        ignore.removeDuplicates();
        Settings::instance().setValue(lnm::OPTIONS_UPDATES_CHECKED, ignore);
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

  atools::gui::Dialog(mainWindow).
  showInfoMsgBox(lnm::ACTIONS_SHOW_UPDATEFAILED,
                 tr("Error while checking for updates to\n\"%1\":\n%2").arg(lnm::OPTIONS_UPDATE_URL).arg(errorString),
                 tr("Do not &show this dialog again."));
}
