/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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
  ptrUp = QStringLiteral("▲");

  // U+25BC Name: BLACK DOWN-POINTING TRIANGLE
  ptrDown = QStringLiteral("▼");

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  // Black and white on macOS and colored on Linux
  ptrRight = QStringLiteral("▶︎");
#else
  // U+25BA Name: BLACK RIGHT-POINTING POINTER
  ptrRight = QStringLiteral("►");
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN32)
  // Black and white on macOS and colored on Linux
  ptrLeft = QStringLiteral("◀︎");
#else
  // U+25C4 Name: BLACK LEFT-POINTING POINTER
  ptrLeft = QStringLiteral("◄");
#endif

  // Wind pointers ==============================================================
  QFontMetrics metrics(font);
#if defined(Q_OS_MACOS)
  windPtrNorth = ptrUp;
#else
  // U+2B9D Name: BLACK UPWARDS EQUILATERAL ARROWHEAD
  windPtrNorth = atools::inFont(metrics, QStringLiteral("⮝")) ? QStringLiteral("⮝") : ptrUp;
#endif

#if defined(Q_OS_MACOS)
  windPtrSouth = ptrDown;
#else
  // U+2B9F Name: BLACK DOWNWARDS EQUILATERAL ARROWHEAD
  windPtrSouth = atools::inFont(metrics, QStringLiteral("⮟")) ? QStringLiteral("⮟") : ptrDown;
#endif

#if defined(Q_OS_MACOS)
  windPtrEast = ptrRight;
#else
  // U+2B9E Name: BLACK RIGHTWARDS EQUILATERAL ARROWHEAD
  windPtrEast = atools::inFont(metrics, QStringLiteral("⮞")) ? QStringLiteral("⮞") : ptrRight;
#endif

#if defined(Q_OS_MACOS)
  windPtrWest = ptrLeft;
#else
  // U+2B9C Name: BLACK LEFTWARDS EQUILATERAL ARROWHEAD
  windPtrWest = atools::inFont(metrics, QStringLiteral("⮜")) ? QStringLiteral("⮜") : ptrLeft;
#endif
}

void TextPointer::printDebug()
{
  qDebug() << Q_FUNC_INFO
           << "windPtrNorth" << windPtrNorth
           << "windPtrSouth" << windPtrSouth
           << "windPtrEast" << windPtrEast
           << "windPtrWest" << windPtrWest;
  qDebug() << Q_FUNC_INFO
           << "ptrUp" << ptrUp
           << "ptrDown" << ptrDown
           << "ptrRight" << ptrRight
           << "ptrLeft" << ptrLeft;
}
