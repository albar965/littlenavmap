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

#ifndef LITTLENAVMAP_MAPFLAGSTEXT_H
#define LITTLENAVMAP_MAPFLAGSTEXT_H

#include "util/flags.h"

namespace text {

/* Flags that determine what information is added to an icon */
enum Flag : quint32
{
  NO_FLAG = 0,
  IDENT = 1 << 0, /* Draw airport or navaid ICAO ident */
  TYPE = 1 << 1, /* Draw navaid type (HIGH, MEDIUM, TERMINAL, HH, H, etc.) */
  FREQ = 1 << 2, /* Draw navaid frequency */
  NAME = 1 << 3,
  MORSE = 1 << 4, /* Draw navaid morse code */
  INFO = 1 << 5, /* Additional airport information like tower frequency, etc. */
  ELLIPSE_IDENT = 1 << 6 /* Add allipse to first text (ident) and ignore additional texts if additonal are not empty */
};

ATOOLS_DECLARE_FLAGS_32(Flags, text::Flag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(text::Flags)

/* Low level text attributes like font attributes, text placement or colors for custom text boxes */
enum Attribute : quint32
{
  NO_ATTRIBUTE = 0,

  /* Font attributes */
  BOLD = 1 << 0,
  ITALIC = 1 << 1,
  UNDERLINE = 1 << 2,
  OVERLINE = 1 << 3,
  STRIKEOUT = 1 << 4,

  /* Text placement */
  LEFT = 1 << 5, /* Reference point is at the right of the text (right-aligned) to place text at the LEFT of an icon */
  RIGHT = 1 << 6, /* Reference point is at the left of the text (left-aligned) to place text at the RIGHT of an icon */
  CENTER = 1 << 7,

  /* Vertical alignment */
  BELOW = 1 << 8, /* Reference point at top to place text BELOW an icon */
  ABOVE = 1 << 9, /* Reference point at bottom to place text ABOVE an icon */

  /* Absolute text placement */
  ABS = 1 << 17,

  /* Color attributes */
  ROUTE_BG_COLOR = 1 << 10, /* Use light yellow background for route objects */
  LOG_BG_COLOR = 1 << 11, /* Use light blue text background for log */
  PREVIEW_BG_COLOR = 1 << 12, /* Use text background for procedure preview */
  NO_BACKGROUND = 1 << 13, /* No text background as selected in options */
  WARNING_COLOR = 1 << 14, /* Orange warning text */
  ERROR_COLOR = 1 << 15, /* White on red error text */

  NO_ROUND_RECT = 1 << 16, /* No rounded background rect */

  /* Automatic text placement to octants for flight plan labels */
  PLACE_ABOVE_CENTER = ABOVE | CENTER,
  PLACE_ABOVE_RIGHT = ABOVE | RIGHT,
  PLACE_RIGHT = RIGHT,
  PLACE_BELOW_RIGHT = BELOW | RIGHT,
  PLACE_BELOW_CENTER = BELOW | CENTER,
  PLACE_BELOW_LEFT = BELOW | LEFT,
  PLACE_LEFT = LEFT,
  PLACE_ABOVE_LEFT = ABOVE | LEFT,

  /* Horizontal placement flags */
  PLACE_ALL_HORIZ = LEFT | RIGHT,
  /* Vertical placement flags */
  PLACE_ALL_VERT = ABOVE | BELOW,

  /* All placement flags */
  PLACE_ALL = LEFT | RIGHT | CENTER | BELOW | ABOVE,
};

ATOOLS_DECLARE_FLAGS_32(Attributes, text::Attribute)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(text::Attributes)
} // namespace text

Q_DECLARE_TYPEINFO(text::Flags, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(text::Attribute, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_MAPFLAGSTEXT_H
