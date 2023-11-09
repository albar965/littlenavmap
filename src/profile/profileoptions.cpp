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

#include "profile/profileoptions.h"

#include "gui/treedialog.h"
#include "common/constants.h"
#include "settings/settings.h"

#include <QDebug>

ProfileOptions::ProfileOptions(QWidget *parentWidgetParam)
  : parentWidget(parentWidgetParam)
{

}

bool ProfileOptions::showOptions()
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::TreeDialog treeDialog(parentWidget, QCoreApplication::applicationName() + tr(" - Elevation Profile"),
                                     tr("Select options for Elevation Profile."),
                                     lnm::PROFILE_DISPLAY_OPTIONS_DIALOG, "PROFILE.html#profile-options",
                                     true /* showExpandCollapse */);
  treeDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  treeDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());
  treeDialog.setHeader({tr("Option"), tr("Description")});

  // Profile =====================================================
  /* *INDENT-OFF* */
  QTreeWidgetItem* rootItem = treeDialog.getRootItem();
  treeDialog.addItem2(rootItem, optsp::PROFILE_TOOLTIP, tr("Show Tooltip"), tr("Display a tooltip with elevation and more information when hovering the mouse over the profile."));
  treeDialog.addItem2(rootItem, optsp::PROFILE_HIGHLIGHT, tr("Highlight Position on Map"), tr("Highlight the flight plan position on the map while hovering the mouse over the profile."));

  QTreeWidgetItem *headerItem = treeDialog.addTopItem1(tr("Header Line"));
  treeDialog.addItem2(headerItem, optsp::PROFILE_HEADER_DIST_TIME_TO_DEST, tr("Destination"), tr("Distance and time to destination."));
  treeDialog.addItem2(headerItem, optsp::PROFILE_HEADER_DIST_TIME_TO_TOD, tr("Top of Descent"), tr("Distance and time to top of descent.\n"
                                                                                                   "Not shown if passed."));
  treeDialog.addItem2(headerItem, optsp::PROFILE_HEADER_DESCENT_PATH_DEVIATION, tr("Deviation"), tr("Vertical altitude deviation from descent path.\n"
                                                                                                    "▼ means above (increase sink rate) and ▲ means below (decrease sink rate)."));
  treeDialog.addItem2(headerItem, optsp::PROFILE_HEADER_DESCENT_PATH_ANGLE, tr("Angle and Speed"), tr("Vertical flight path angle needed to keep the vertical path angle.\n"
                                                                                                      "Changes to \"Required angle\" if mandatory in approach procedures."));


  QTreeWidgetItem *aircraftItem = treeDialog.addTopItem1(tr("User Aircraft Labels"));
  treeDialog.addItem2(aircraftItem, optsp::PROFILE_AIRCRAFT_ALTITUDE, tr("Altitude"), tr("Show actual user aircraft altitude at symbol."));
  treeDialog.addItem2(aircraftItem, optsp::PROFILE_AIRCRAFT_VERT_SPEED, tr("Vertical Speed"), tr("Show vertical speed of at user aircraft symbol."));
  treeDialog.addItem2(aircraftItem, optsp::PROFILE_AIRCRAFT_VERT_ANGLE_NEXT, tr("Vertical Speed to Next"),
                      tr("Vertical speed needed to arrive at the calculated altitude at the next waypoint.\n"
                         "Shown on descent only at user aircraft symbol suffixed with \"N\"."));

  QTreeWidgetItem *labelItem = treeDialog.addTopItem1(tr("Labels"));
  treeDialog.addItem2(labelItem, optsp::PROFILE_LABELS_ALT, tr("Show Altitude"), tr("Show or hide the altitude labels at the left side of the elevation profile."));
  treeDialog.addItem2(labelItem, optsp::PROFILE_LABELS_DISTANCE, tr("Distance"), tr("Distance of flight plan leg in the elevation profile header."));
  treeDialog.addItem2(labelItem, optsp::PROFILE_LABELS_MAG_COURSE, tr("Magnetic Course"), tr( "Show magnetic great circle start course for flight plan leg in the header."));
  treeDialog.addItem2(labelItem, optsp::PROFILE_LABELS_TRUE_COURSE, tr("True Course"), tr( "Show true great circle start course for flight plan leg in the header."));
  treeDialog.addItem2(labelItem, optsp::PROFILE_LABELS_RELATED, tr("Related Navaid"), tr( "Related navaid for a procedure fix including bearing and distance in the header."));

  QTreeWidgetItem *profileItem = treeDialog.addTopItem1(tr("Elevation Profile"));
  treeDialog.addItem2(profileItem, optsp::PROFILE_GROUND, tr("Ground"), tr("Green ground display."));
  treeDialog.addItem2(profileItem, optsp::PROFILE_SAFE_ALTITUDE, tr("Safe Altitude Line"), tr("Red safe altitude line for whole flight plan."));
  treeDialog.addItem2(profileItem, optsp::PROFILE_LEG_SAFE_ALTITUDE, tr("Leg Safe Altitude Lines"), tr("Orange safe altitude lines for for each flight plan leg."));

  QTreeWidgetItem *lineItem = treeDialog.addTopItem1(tr("Flight Plan Line"));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_DIST, tr("Distance"), tr("Distance of flight plan leg."));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_MAG_COURSE, tr("Magnetic Course"), tr( "Show magnetic great circle start course."));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_TRUE_COURSE, tr("True Course"), tr("Show true great circle start course."));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_VERTICAL_ANGLE, tr("Descent Flight Path Angle"), tr("Vertical descent path angle only in the descent phase."));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_ALT_RESTRICTION, tr("Altitude Restriction"), tr( "Display procedure altitude restrictions at the navaid label."));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_ALT_RESTRICTION_BLOCK, tr("Altitude Restriction Indicator"), tr("Altitude restrictions shown as blocks in diagram."));
  treeDialog.addItem2(lineItem, optsp::PROFILE_FP_SPEED_RESTRICTION, tr("Speed Restriction"), tr("Show procedure speed restrictions at the navaid label."));
  /* *INDENT-ON* */

  treeDialog.restoreState(false /* restoreCheckState */, true /* restoreExpandState */);

  // Restoring in dialog is disabled - restore options manually
  for(optsp::DisplayOptionProfile opt : optsp::ALL_OPTIONS)
    treeDialog.setItemChecked(opt, displayOptions.testFlag(opt));

  if(treeDialog.exec() == QDialog::Accepted)
  {
    treeDialog.saveState(false /* saveCheckState */, true /* saveExpandState */);

    // Extract options from check items
    for(optsp::DisplayOptionProfile opt : optsp::ALL_OPTIONS)
      displayOptions.setFlag(opt, treeDialog.isItemChecked(opt));

    return true;
  }
  return false;
}

void ProfileOptions::saveState()
{
  atools::settings::Settings::instance().setValue(lnm::PROFILE_DISPLAY_OPTIONS, static_cast<int>(displayOptions));
}

void ProfileOptions::restoreState()
{
  const atools::settings::Settings& settings = atools::settings::Settings::instance();

  if(settings.contains(lnm::PROFILE_DISPLAY_OPTIONS))
    displayOptions = optsp::DisplayOptionsProfile(settings.valueInt(lnm::PROFILE_DISPLAY_OPTIONS));
  // else use default
}
