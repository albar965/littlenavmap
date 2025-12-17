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

#ifndef LNM_TEXTPOINTER_H
#define LNM_TEXTPOINTER_H

class QFont;
class QString;

/*
 * Manages special pointer Unicode characters depending on system, font and availability.
 * Call initWindPtr() before use.
 *
 * All used pointers are [▲▼▶︎►◀︎◄⮝⮟⮞⮜]
 */
class TextPointer
{
public:
  /* Initialize wind pointers depending on font */
  static void initPointerCharacters(const QFont& font);

  static const QString& getWindPointerNorth()
  {
    return windPtrNorth;
  }

  static const QString& getWindPointerSouth()
  {
    return windPtrSouth;
  }

  static const QString& getWindPointerEast()
  {
    return windPtrEast;
  }

  static const QString& getWindPointerWest()
  {
    return windPtrWest;
  }

  static const QString& getPointerUp()
  {
    return ptrUp;
  }

  static const QString& getPointerDown()
  {
    return ptrDown;
  }

  static const QString& getPointerRight()
  {
    return ptrRight;
  }

  static const QString& getPointerLeft()
  {
    return ptrLeft;
  }

private:
  static QString windPtrNorth, windPtrSouth, windPtrEast, windPtrWest, ptrUp, ptrDown, ptrRight, ptrLeft;
};

#endif // LNM_TEXTPOINTER_H
