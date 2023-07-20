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

#include "common/updatehandler.h"

#include "common/constants.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "gui/updatedialog.h"
#include "app/navapp.h"
#include "settings/settings.h"
#include "util/htmlbuilder.h"
#include "util/updatecheck.h"
#include "util/updatecheck.h"

#include <QDialogButtonBox>

using atools::util::UpdateCheck;
using atools::settings::Settings;
namespace html = atools::util::html;

UpdateHandler::UpdateHandler(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  updateCheck = new UpdateCheck();

  // Get URL from options for debugging otherwise use default
  updateCheck->setUrl(Settings::instance().valueStr(lnm::OPTIONS_UPDATE_URL, lnm::updateDefaultUrl));

  connect(updateCheck, &UpdateCheck::updateFound, this, &UpdateHandler::updateFound);
  connect(updateCheck, &UpdateCheck::updateFailed, this, &UpdateHandler::updateFailed);

  // Set timer for regular update check if timeout is expired since last download
  updateTimer.setInterval(Settings::instance().getAndStoreValue(lnm::OPTIONS_UPDATE_TIMER_SECONDS, 900).toInt() * 1000);
  updateTimer.setSingleShot(true);
  connect(&updateTimer, &QTimer::timeout, this, &UpdateHandler::updateCheckTimeout);
}

UpdateHandler::~UpdateHandler()
{
  delete updateCheck;
}

void UpdateHandler::updateCheckTimeout()
{
  checkForUpdates(UPDATE_REASON_TIMER);
}

void UpdateHandler::checkForUpdates(UpdateReason reason)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "channels" << channelOpts << "reason" << reason << "URL" << updateCheck->getUrl();
#endif

  // Don't do anything if already downloading
  if(updateCheck->isDownloading())
    return;

  updateReason = reason;

  bool check = true;
  // Do not react to timer events if not enabled (connected to sim) or a modal window is showing
  if(reason == UPDATE_REASON_TIMER && (!enabled || QApplication::modalWindow() != nullptr))
    check = false;

  // Debug option to test updates
  updateCheck->setForceDebug(reason == UPDATE_REASON_FORCE);

  // Check timestamp for timer and startup events
  if(check && updateReason != UPDATE_REASON_MANUAL && updateReason != UPDATE_REASON_FORCE)
  {
    // Check timestamp if automatically triggered on starup
    qint64 diffSeconds = 0;
    switch(OptionData::instance().getUpdateRate())
    {
      case opts::DAILY:
        diffSeconds = 24 * 60 * 60;
        break;
      case opts::WEEKLY:
        diffSeconds = 24 * 60 * 60 * 7;
        break;
      case opts::NEVER:
        // User checks manually
        check = false;
    }

    QDateTime lastChecked = QDateTime::fromSecsSinceEpoch(Settings::instance().valueVar(lnm::OPTIONS_UPDATE_LAST_CHECKED).toLongLong());
    qint64 lastDiffSeconds = lastChecked.secsTo(QDateTime::currentDateTime());
    if(lastDiffSeconds < diffSeconds)
      check = false;

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "diffSeconds" << diffSeconds << "lastDiffSeconds" << lastDiffSeconds
             << "check" << check << "lastChecked" << lastChecked;
#endif
  }

  if(check)
  {
    QString checked;
    if(updateReason != UPDATE_REASON_MANUAL && updateReason != UPDATE_REASON_FORCE)
      // Get last skipped update version and skip this and all earlier
      checked = Settings::instance().valueStr(lnm::OPTIONS_UPDATE_ALREADY_CHECKED);

    // Copy combo box selection to channel enum
    atools::util::UpdateChannels channels = atools::util::NONE;
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

    qInfo() << Q_FUNC_INFO << "Checking for updates now. Channels" << channels << "already checked" << checked;

    // Async
    updateCheck->checkForUpdates(checked, updateReason == UPDATE_REASON_MANUAL /* notifyForEmptyUpdates */, channels);

    // Set timestamp for last check
    Settings::instance().setValueVar(lnm::OPTIONS_UPDATE_LAST_CHECKED, QDateTime::currentDateTime().toSecsSinceEpoch());
  }

  if(reason == UPDATE_REASON_TIMER || reason == UPDATE_REASON_STARTUP)
    updateTimer.start();
}

/* Called by update checker */
void UpdateHandler::updateFound(atools::util::UpdateList updates)
{
  qDebug() << Q_FUNC_INFO;

  for(const atools::util::Update& update:updates)
    qDebug() << Q_FUNC_INFO << update.version;

  NavApp::closeSplashScreen();

  if(!updates.isEmpty())
  {
    // Found updates - fill the HTML text for the dialog =============================
    atools::util::HtmlBuilder html(false);

    // Show only the newest one
    const atools::util::Update& update = updates.constFirst();

    if(!update.changelog.isEmpty())
      html.text(update.changelog, atools::util::html::NO_ENTITIES);

    NavApp::closeSplashScreen();

    // Show dialog
    UpdateDialog updateDialog(mainWindow, updateReason == UPDATE_REASON_MANUAL);
    updateDialog.setMessage(html.getHtml());

    // Show dialog
    updateDialog.exec();

    if(updateReason != UPDATE_REASON_MANUAL)
    {
      if(updateDialog.isIgnoreThisUpdate())
      {
        // Add latest update - do not report anything earlier or equal again
        QString ignore = updates.constFirst().version;
        qDebug() << Q_FUNC_INFO << "Ignoring updates now" << ignore;
        Settings::instance().setValue(lnm::OPTIONS_UPDATE_ALREADY_CHECKED, ignore);
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

  NavApp::closeSplashScreen();

  QString message = tr("Error while checking for updates at\n\"%1\":\n%2").
                    arg(updateCheck->getUrl().toDisplayString()).arg(errorString);

  if(updateReason == UPDATE_REASON_MANUAL)
    QMessageBox::warning(mainWindow, QApplication::applicationName(), message);
  else
    atools::gui::Dialog(mainWindow).showWarnMsgBox(lnm::ACTIONS_SHOW_UPDATE_FAILED, message, tr("Do not &show this dialog again."));
}
