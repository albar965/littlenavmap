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

#ifndef LNM_COMMON_JUMPBACK_H
#define LNM_COMMON_JUMPBACK_H

#include "geo/pos.h"

#include <QObject>
#include <QTimer>

/*
 * Provides functionality for jumping back to aircraft i.e. disable aircraft centering temporarily when
 * user zooms or scrolls around.
 *
 * Uses actionMapAircraftCenter action, getSimNoFollowAircraftOnScrollSeconds and opts::ROUTE_NO_FOLLOW_ON_MOVE options.
 */
class JumpBack :
  public QObject
{
  Q_OBJECT

public:
  explicit JumpBack(QObject *parent);
  virtual ~JumpBack() override;

  /* Stop timer and cancel any jumping back */
  void cancel();

  /* Currently waiting since user changed position manually */
  bool isActive() const;

  /* Start timer providing coordindates or zoom distances optionally.**/
  void start();
  void start(const QVariantList& jumpBackValues);

  /* Start timer and do not reset values */
  void restart();

  void updateValues(const QVariantList& jumpBackValues);

  const QVariantList& getValues() const
  {
    return values;
  }

signals:
  /* Timer triggered. Go back to aircraft centering. */
  void jumpBack(const QVariantList& values);

private:
  void timeout();

  QTimer timer;
  QVariantList values;
  bool active = false;
};

#endif // LNM_COMMON_JUMPBACK_H
