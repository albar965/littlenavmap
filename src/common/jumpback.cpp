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

#include "common/jumpback.h"

#include "app/navapp.h"
#include "options/optiondata.h"

JumpBack::JumpBack(QObject *parent, bool verboseLogging)
  : QObject(parent), verbose(verboseLogging)
{
  parentName = parent->metaObject()->className();
  if(verbose)
    qDebug() << Q_FUNC_INFO << parentName << position;

  timer.setSingleShot(true);
  connect(&timer, &QTimer::timeout, this, &JumpBack::timeout);
}

JumpBack::~JumpBack()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << parentName << position;

  timer.stop();
}

bool JumpBack::isActive() const
{
  return active;
}

void JumpBack::start()
{
  start(atools::geo::EMPTY_POS);
}

void JumpBack::restart()
{
  start(position);
}

void JumpBack::start(const atools::geo::Pos& pos)
{
  if(NavApp::isConnectedAndAircraft() && OptionData::instance().getFlags2().testFlag(opts2::ROUTE_NO_FOLLOW_ON_MOVE))
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << parentName << pos;

    if(!active)
    {
      // Do not update position if already active
      position = pos;
      active = true;
    }

    // Restart timer
    timer.setInterval(OptionData::instance().getSimNoFollowAircraftScrollSeconds() * 1000);
    timer.start();
  }
}

void JumpBack::cancel()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << parentName << position;

  timer.stop();
  active = false;
}

void JumpBack::timeout()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << parentName << position;

  emit jumpBack(position);
}
