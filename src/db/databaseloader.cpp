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

#include "databaseloader.h"
#include "databasemeta.h"
#include "db/databasedialog.h"
#include "logging/loggingdefs.h"
#include "settings/settings.h"
#include <fs/bglreaderoptions.h>
#include <fs/bglreaderprogressinfo.h>
#include "common/formatter.h"
#include "fs/fspaths.h"
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressDialog>
#include <QSettings>
#include <QApplication>

#include <fs/navdatabase.h>

#include <sql/sqlutil.h>

#include <gui/errorhandler.h>

const QString meta("<p><b>Last Update: %1. Database Version: %2.%3. Program Version: %4.%5.</b></p>");

const QString table(QObject::tr("<table>"
                                  "<tbody>"
                                    "<tr> "
                                      "<td width=\"60\"><b>Files:</b>"
                                      "</td>    "
                                      "<td width=\"60\">%L5"
                                      "</td> "
                                      "<td width=\"60\"><b>VOR:</b>"
                                      "</td> "
                                      "<td width=\"60\">%L7"
                                      "</td> "
                                      "<td width=\"60\"><b>Marker:</b>"
                                      "</td>     "
                                      "<td width=\"60\">%L10"
                                      "</td>"
                                    "</tr>"
                                    "<tr> "
                                      "<td width=\"60\"><b>Airports:</b>"
                                      "</td> "
                                      "<td width=\"60\">%L6"
                                      "</td> "
                                      "<td width=\"60\"><b>ILS:</b>"
                                      "</td> "
                                      "<td width=\"60\">%L8"
                                      "</td> "
                                      "<td width=\"60\"><b>Boundaries:</b>"
                                      "</td> <td width=\"60\">%L11"
                                    "</td>"
                                  "</tr>"
                                  "<tr> "
                                    "<td width=\"60\">"
                                    "</td>"
                                    "<td width=\"60\">"
                                    "</td>"
                                    "<td width=\"60\"><b>NDB:</b>"
                                    "</td> "
                                    "<td width=\"60\">%L9"
                                    "</td> "
                                    "<td width=\"60\"><b>Waypoints:"
                                    "</b>"
                                  "</td>  "
                                  "<td width=\"60\">%L12"
                                  "</td>"
                                "</tr>"
                              "</tbody>"
                            "</table>"
                                ));

const QString text(QObject::tr(
                     "<b>%1</b><br/><br/><br/>"
                     "<b>Time:</b> %2<br/>%3%4"
                     ) + table);

const QString textWithFile(QObject::tr(
                             "<b>Scenery:</b> %1 (%2)<br/>"
                             "<b>File:</b> %3<br/><br/>"
                             "<b>Time:</b> %4<br/>"
                             ) + table);

using namespace atools::settings;

DatabaseLoader::DatabaseLoader(QWidget *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), parentWidget(parent)
{

}

DatabaseLoader::~DatabaseLoader()
{
  delete progressDialog;
}

void DatabaseLoader::run()
{
  DatabaseDialog dlg(parentWidget);
  dlg.setBasePath(basePath);
  dlg.setSceneryConfigFile(sceneryCfg);

  runInternal(dlg);
}

void DatabaseLoader::runInternal(DatabaseDialog& dlg)
{
  try
  {
    QString metaText;
    DatabaseMeta dbmeta(db);
    if(!dbmeta.isValid())
      metaText = meta.arg(tr("Invalid")).arg(tr("Invalid")).arg(tr("Invalid"));
    else
      metaText = meta.arg(dbmeta.getLastLoadTime().toString()).
                 arg(dbmeta.getMajorVersion()).arg(dbmeta.getMinorVersion()).
                 arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR);

    atools::sql::SqlUtil util(db);

    // Get row counts for the dialog
    QString tableText = table.arg(util.rowCount("bgl_file")).
                        arg(util.rowCount("airport")).
                        arg(util.rowCount("vor")).
                        arg(util.rowCount("ils")).
                        arg(util.rowCount("ndb")).
                        arg(util.rowCount("marker")).
                        arg(util.rowCount("boundary")).
                        arg(util.rowCount("waypoint"));

    dlg.setHeader(metaText + "<p><b>Currently Loaded:</b></p><p>" + tableText + "</p>");

    int retval = dlg.exec();
    dlg.hide();

    if(retval == QDialog::Accepted)
    {
      sceneryCfg = dlg.getSceneryConfigFile();
      basePath = dlg.getBasePath();
      if(loadScenery(&dlg))
        // Successfully loaded
        dbmeta.update(DB_VERSION_MAJOR, DB_VERSION_MINOR);
      else
        // Try again until the user cancels the dialog
        runInternal(dlg);
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parentWidget).handleException(e);
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parentWidget).handleUnknownException();
  }
}

bool DatabaseLoader::loadScenery(QWidget *parent)
{
  emit preDatabaseLoad();
  using atools::fs::BglReaderOptions;

  bool success = true;
  QString config = Settings::getOverloadedPath(":/littlenavmap/resources/config/navdatareader.cfg");
  qInfo() << "loadScenery: Config file" << config;

  QSettings settings(config, QSettings::IniFormat);

  BglReaderOptions opts;
  opts.loadFromSettings(settings);

  delete progressDialog;
  progressDialog = new QProgressDialog(parent);

  QLabel *label = new QLabel(progressDialog);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setIndent(10);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setMinimumWidth(600);

  progressDialog->setWindowModality(Qt::WindowModal);
  progressDialog->setLabel(label);
  progressDialog->setAutoClose(false);
  progressDialog->setAutoReset(false);
  progressDialog->setMinimumDuration(0);
  progressDialog->show();

  opts.setSceneryFile(sceneryCfg);
  opts.setBasepath(basePath);

  QElapsedTimer timer;
  opts.setProgressCallback(std::bind(&DatabaseLoader::progressCallback, this, std::placeholders::_1, timer));

  // Let the dialog close and show the busy pointer
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  try
  {
    atools::fs::Navdatabase nd(&opts, db);
    nd.create();
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(parentWidget).handleException(e);
    success = false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(parentWidget).handleUnknownException();
    success = false;
  }

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  if(!progressDialog->wasCanceled() && success)
  {
    // Show results and wait until user selects ok
    progressDialog->setCancelButtonText(tr("&OK"));
    progressDialog->exec();
  }
  else
    // Loading was cancelled
    success = false;

  delete progressDialog;
  progressDialog = nullptr;

  emit postDatabaseLoad();
  return success;
}

bool DatabaseLoader::progressCallback(const atools::fs::BglReaderProgressInfo& progress, QElapsedTimer& timer)
{
  if(progress.isFirstCall())
  {
    timer.start();
    progressDialog->setMinimum(0);
    progressDialog->setMaximum(progress.getTotal());
  }
  progressDialog->setValue(progress.getCurrent());

  if(progress.isNewOther())
    progressDialog->setLabelText(
      text.arg(progress.getOtherAction()).
      arg(formatter::formatElapsed(timer)).
      arg(QString()).
      arg(QString()).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumBoundaries()).
      arg(progress.getNumWaypoints()));
  else if(progress.isNewSceneryArea() || progress.isNewFile())
    progressDialog->setLabelText(
      textWithFile.arg(progress.getSceneryTitle()).
      arg(progress.getSceneryPath()).
      arg(progress.getBglFilename()).
      arg(formatter::formatElapsed(timer)).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumBoundaries()).
      arg(progress.getNumWaypoints()));
  else if(progress.isLastCall())
    progressDialog->setLabelText(
      text.arg(tr("Done")).
      arg(formatter::formatElapsed(timer)).
      arg(QString()).
      arg(QString()).
      arg(progress.getNumFiles()).
      arg(progress.getNumAirports()).
      arg(progress.getNumVors()).
      arg(progress.getNumIls()).
      arg(progress.getNumNdbs()).
      arg(progress.getNumMarker()).
      arg(progress.getNumBoundaries()).
      arg(progress.getNumWaypoints()));

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  return progressDialog->wasCanceled();
}

void DatabaseLoader::saveState()
{
  Settings& s = Settings::instance();
  s->setValue("Database/BasePath", basePath);
  s->setValue("Database/SceneryConfig", sceneryCfg);
}

void DatabaseLoader::restoreState()
{
  Settings& s = Settings::instance();
  atools::fs::fstype::SimulatorType type = atools::fs::fstype::FSX;

  basePath = s->value("Database/BasePath", atools::fs::FsPaths::getBasePath(type)).toString();
  sceneryCfg = s->value("Database/SceneryConfig", atools::fs::FsPaths::getSceneryLibraryPath(type)).toString();

  qDebug() << "Base Path" << basePath;
  qDebug() << "Scenery Configuration Path" << sceneryCfg;
}
