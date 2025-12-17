/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_SEARCHEVENTFILTER_H
#define LNM_SEARCHEVENTFILTER_H

#include <QObject>

class QEvent;
class AbstractSearch;

/*
 * Catches the return key in lists and calls AbstractSearch::showSelectedEntry()
 */
class SearchViewEventFilter :
  public QObject
{
public:
  SearchViewEventFilter(AbstractSearch *parent);

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  AbstractSearch *searchBase;
};

/*
 * Event filter for input lines. Cursor down calls AbstractSearch::activateView() and Return AbstractSearch::showFirstEntry()
 */
class SearchWidgetEventFilter :
  public QObject
{
public:
  SearchWidgetEventFilter(AbstractSearch *parent);

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  AbstractSearch *searchBase;
};

#endif // LNM_SEARCHEVENTFILTER_H
