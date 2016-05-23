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

#include "htmlbuilder.h"
#include "logging/loggingdefs.h"

#include <QApplication>
#include <QPalette>
#include <QDateTime>

HtmlBuilder::HtmlBuilder()
{
  // Create darker colors dynamically from default palette
  rowBackColor = QApplication::palette().color(QPalette::Active, QPalette::Base).
                 darker(105).name(QColor::HexRgb);
  rowBackColorAlt = QApplication::palette().color(QPalette::Active, QPalette::AlternateBase).
                    darker(105).name(QColor::HexRgb);

  tableRow.append("<tr bgcolor=\"" + rowBackColor + "\"><td>%1</td><td>%2</td></tr>");
  tableRow.append("<tr bgcolor=\"" + rowBackColorAlt + "\"><td>%1</td><td>%2</td></tr>");

  tableRowAlignRight.append(
    "<tr bgcolor=\"" + rowBackColor + "\"><td>%1</td><td align=\"right\">%2</td></tr>");
  tableRowAlignRight.append(
    "<tr bgcolor=\"" + rowBackColorAlt + "\"><td>%1</td><td align=\"right\">%2</td></tr>");

  tableRowHeader = "<tr><td>%1</td></tr>";

}

HtmlBuilder::~HtmlBuilder()
{

}

HtmlBuilder& HtmlBuilder::row(const QString& name, const QVariant& value, html::Flags flags, QColor color)
{
  QString valueStr;
  switch(value.type())
  {
    case QVariant::Invalid:
      valueStr = "Error: Invalid Variant";
      qWarning() << "Invalid Variant in HtmlBuilder. Name" << name;
      break;
    case QVariant::Bool:
      valueStr = value.toBool() ? tr("Yes") : tr("No");
      break;
    case QVariant::Int:
      valueStr = locale.toString(value.toInt());
      break;
    case QVariant::UInt:
      valueStr = locale.toString(value.toUInt());
      break;
    case QVariant::LongLong:
      valueStr = locale.toString(value.toLongLong());
      break;
    case QVariant::ULongLong:
      valueStr = locale.toString(value.toULongLong());
      break;
    case QVariant::Double:
      valueStr = locale.toString(value.toDouble(), 'f', defaultPrecision);
      break;
    case QVariant::Char:
    case QVariant::String:
      valueStr = value.toString();
      break;
    case QVariant::Map:
      break;
    case QVariant::List:
      break;
    case QVariant::StringList:
      valueStr = value.toStringList().join(", ");
      break;
    case QVariant::Date:
      valueStr = locale.toString(value.toDate(), dateFormat);
      break;
    case QVariant::Time:
      valueStr = locale.toString(value.toTime(), dateFormat);
      break;
    case QVariant::DateTime:
      valueStr = locale.toString(value.toDateTime(), dateFormat);
      break;
    case QVariant::Url:
    // TODO add href
    case QVariant::Icon:
    // TODO add image
    default:
      qWarning() << "Invalid variant type" << value.typeName() << "in HtmlBuilder. Name" << name;
      valueStr = QString("Error: Invalid variant type \"%1\"").arg(value.typeName());

  }
  html += alt(flags & html::ALIGN_RIGHT ? tableRowAlignRight : tableRow).
          arg(asText(name, flags, color), value.toString());
  index++;
  return *this;
}

HtmlBuilder& HtmlBuilder::row(const QString& name, const QString& value, html::Flags flags, QColor color)
{
  html += alt(flags & html::ALIGN_RIGHT ? tableRowAlignRight : tableRow).
          arg(asText(name, flags, color), value);
  index++;
  return *this;
}

HtmlBuilder& HtmlBuilder::row(const QString& name, float value, int precision, html::Flags flags,
                              QColor color)
{
  html += alt(flags & html::ALIGN_RIGHT ? tableRowAlignRight : tableRow).
          arg(asText(name, flags, color),
              locale.toString(value, 'f', precision != -1 ? precision : defaultPrecision));
  index++;
  return *this;
}

HtmlBuilder& HtmlBuilder::row(const QString& name, double value, int precision, html::Flags flags,
                              QColor color)
{
  html += alt(flags & html::ALIGN_RIGHT ? tableRowAlignRight : tableRow).
          arg(asText(name, flags, color),
              locale.toString(value, 'f', precision != -1 ? precision : defaultPrecision));
  index++;
  return *this;
}

HtmlBuilder& HtmlBuilder::row(const QString& name, int value, html::Flags flags, QColor color)
{
  html += alt(flags & html::ALIGN_RIGHT ? tableRowAlignRight : tableRow).
          arg(asText(name, flags, color), locale.toString(value));
  index++;
  return *this;
}

HtmlBuilder& HtmlBuilder::table()
{
  html += "<table border=\"0\" cellpadding=\"2\" cellspacing=\"0\"><tbody>";
  return *this;
}

HtmlBuilder& HtmlBuilder::tableEnd()
{
  html += "</tbody></table>";
  return *this;
}

HtmlBuilder& HtmlBuilder::h(int level, const QString& str, html::Flags flags, QColor color)
{
  QString num = QString::number(level);
  html += "<h" + num + ">" + asText(str, flags, color) + "</h" + num + ">";
  index = 0;
  return *this;
}

HtmlBuilder& HtmlBuilder::h1(const QString& str, html::Flags flags, QColor color)
{
  h(1, str, flags, color);
  return *this;
}

HtmlBuilder& HtmlBuilder::h2(const QString& str, html::Flags flags, QColor color)
{
  h(2, str, flags, color);
  return *this;
}

HtmlBuilder& HtmlBuilder::h3(const QString& str, html::Flags flags, QColor color)
{
  h(3, str, flags, color);
  return *this;
}

HtmlBuilder& HtmlBuilder::h4(const QString& str, html::Flags flags, QColor color)
{
  h(4, str, flags, color);
  return *this;
}

HtmlBuilder& HtmlBuilder::b(const QString& str)
{
  text(str, html::BOLD);
  return *this;
}

HtmlBuilder& HtmlBuilder::u(const QString& str)
{
  text(str, html::UNDERLINE);
  return *this;
}

HtmlBuilder& HtmlBuilder::sub(const QString& str)
{
  text(str, html::SUBSCRIPT);
  return *this;
}

HtmlBuilder& HtmlBuilder::sup(const QString& str)
{
  text(str, html::SUPERSCRIPT);
  return *this;
}

HtmlBuilder& HtmlBuilder::small(const QString& str)
{
  text(str, html::SMALL);
  return *this;
}

HtmlBuilder& HtmlBuilder::big(const QString& str)
{
  text(str, html::BIG);
  return *this;
}

HtmlBuilder& HtmlBuilder::br()
{
  html += "<br/>";
  return *this;
}

HtmlBuilder& HtmlBuilder::hr(int size, int widthPercent)
{
  html += "<hr size=\"" + QString::number(size) + "\" width=\"" + QString::number(widthPercent) + "%\"/>";
  return *this;
}

HtmlBuilder& HtmlBuilder::ol()
{
  html += "<ol>";
  return *this;
}

HtmlBuilder& HtmlBuilder::olEnd()
{
  html += "</ol>";
  return *this;
}

HtmlBuilder& HtmlBuilder::ul()
{
  html += "<ul>";
  return *this;
}

HtmlBuilder& HtmlBuilder::ulEnd()
{
  html += "</ul>";
  return *this;
}

HtmlBuilder& HtmlBuilder::li(const QString& str, html::Flags flags, QColor color)
{
  html += "<li>" + asText(str, flags, color) + "</li>";
  return *this;
}

QString HtmlBuilder::asText(const QString& str, html::Flags flags, QColor color)
{
  QString prefix, suffix;
  if(flags & html::BOLD)
  {
    prefix.append("<b>");
    suffix.prepend("</b>");
  }

  if(flags & html::ITALIC)
  {
    prefix.append("<i>");
    suffix.prepend("</i>");
  }

  if(flags & html::UNDERLINE)
  {
    prefix.append("<u>");
    suffix.prepend("</u>");
  }

  if(flags & html::SUBSCRIPT)
  {
    prefix.append("<sub>");
    suffix.prepend("</sub>");
  }

  if(flags & html::SUPERSCRIPT)
  {
    prefix.append("<sup>");
    suffix.prepend("</sup>");
  }

  if(flags & html::SMALL)
  {
    prefix.append("<small>");
    suffix.prepend("</small>");
  }

  if(flags & html::BIG)
  {
    prefix.append("<big>");
    suffix.prepend("</big>");
  }

  if(color.isValid())
  {
    prefix.append("<span style=\"color:" + color.name(QColor::HexRgb) + "\">");
    suffix.prepend("</span>");
  }

  return prefix + str + suffix;
}

HtmlBuilder& HtmlBuilder::text(const QString& str, html::Flags flags, QColor color)
{
  html += asText(str, flags, color);
  return *this;
}

HtmlBuilder& HtmlBuilder::p(const QString& str, html::Flags flags, QColor color)
{
  html += "<p>";
  text(str, flags, color);
  html += "</p>";
  index = 0;
  return *this;
}

HtmlBuilder& HtmlBuilder::doc()
{
  html +=
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
      "<html>"
        "<head>"
        "</head>"
        "<body style=\"font-family:'sans'; font-size:8pt; font-weight:400; font-style:normal;\">";
  index = 0;
  return *this;
}

HtmlBuilder& HtmlBuilder::docEnd()
{
  html += "</body></html>";
  return *this;
}

const QString& HtmlBuilder::alt(const QStringList& list) const
{
  return list.at(index % list.size());
}
