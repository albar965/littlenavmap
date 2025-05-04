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

#include "textpointer.h"

#include "atools.h"

#include <QFontMetrics>
#include <QString>

/* Wind pointers with Latin1 fallback characters */
QString TextPointer::windPtrNorth("^");
QString TextPointer::windPtrSouth("v");
QString TextPointer::windPtrEast(">");
QString TextPointer::windPtrWest("<");
QString TextPointer::ptrUp("^");
QString TextPointer::ptrDown("v");
QString TextPointer::ptrRight(">");
QString TextPointer::ptrLeft("<");

void TextPointer::initPointerCharacters(const QFont& font)
{
  // General pointers ==============================================================
  // U+25B2 Name: BLACK UP-POINTING TRIANGLE
  ptrUp = "▲";

  // U+25BC Name: BLACK DOWN-POINTING TRIANGLE
  ptrDown = "▼";

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  // Black and white on macOS and colored on Linux
  ptrRight = "▶︎";
#else
  // U+25BA Name: BLACK RIGHT-POINTING POINTER
  ptrRight = "►";
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  // Black and white on macOS and colored on Linux
  ptrLeft = "◀︎";
#else
  // U+25C4 Name: BLACK LEFT-POINTING POINTER
  ptrLeft = "◄";
#endif

  // Wind pointers ==============================================================
  QFontMetrics metrics(font);
#if defined(Q_OS_MACOS)
  windPtrNorth = ptrUp;
#else
  // U+2B9D Name: BLACK UPWARDS EQUILATERAL ARROWHEAD
  windPtrNorth = atools::inFont(metrics, "⮝") ? "⮝" : ptrUp;
#endif

#if defined(Q_OS_MACOS)
  windPtrSouth = ptrDown;
#else
  // U+2B9F Name: BLACK DOWNWARDS EQUILATERAL ARROWHEAD
  windPtrSouth = atools::inFont(metrics, "⮟") ? "⮟" : ptrDown;
#endif

#if defined(Q_OS_MACOS)
  windPtrEast = ptrRight;
#else
  // U+2B9E Name: BLACK RIGHTWARDS EQUILATERAL ARROWHEAD
  windPtrEast = atools::inFont(metrics, "⮞") ? "⮞" : ptrRight;
#endif

#if defined(Q_OS_MACOS)
  windPtrWest = ptrLeft;
#else
  // U+2B9C Name: BLACK LEFTWARDS EQUILATERAL ARROWHEAD
  windPtrWest = atools::inFont(metrics, "⮜") ? "⮜" : ptrLeft;
#endif
}
