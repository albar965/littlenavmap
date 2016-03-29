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

#include "morsecode.h"

#include <QHash>

/* *INDENT-OFF* */
const QHash<QChar, QString> CODES {
  { 'A', "·−" },
  { 'B', "−···" },
  { 'C', "−·−·" },
  { 'D', "−··" },
  { 'E', "·" },
  { 'F', "··−·" },
  { 'G', "−−·" },
  { 'H', "····" },
  { 'I', "··" },
  { 'J', "·−−−" },
  { 'K', "−·−" },
  { 'L', "·−··" },
  { 'M', "−−" },
  { 'N', "−·" },
  { 'O', "−−−" },
  { 'P', "·−−·" },
  { 'Q', "−−·−" },
  { 'R', "·−·" },
  { 'S', "···" },
  { 'T', "−" },
  { 'U', "··−" },
  { 'V', "···−" },
  { 'W', "·−−" },
  { 'X', "−··−" },
  { 'Y', "−·−−" },
  { 'Z', "−−··" },
  { '1', "·−−−−" },
  { '2', "··−−−" },
  { '3', "···−−" },
  { '4', "····−" },
  { '5', "·····" },
  { '6', "−····" },
  { '7', "−−···" },
  { '8', "−−−··" },
  { '9', "−−−−·" },
  { '0', "−−−−−" }
};
/* *INDENT-ON* */

MorseCode::MorseCode(const QString& signSeparator, const QString& charSeparator)
  : signSep(signSeparator), charSep(charSeparator)
{
}

MorseCode::~MorseCode()
{
}

QString MorseCode::getCode(const QString& text)
{
  QString txt = text.toUpper();
  QString retval;

  for(QChar c : txt)
  {
    QString codeStr;
    QString code = CODES.value(c);
    for(QChar cc : code)
    {
      if(!codeStr.isEmpty())
        codeStr.append(signSep);
      codeStr.append(cc);
    }
    if(!retval.isEmpty())
      retval.append(charSep);
    retval.append(codeStr);
  }
  return retval;
}
