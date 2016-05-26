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

#ifndef HTMLBUILDER_H
#define HTMLBUILDER_H

#include <QLocale>
#include <QStringList>
#include <QCoreApplication>
#include <QColor>
#include <QSize>

namespace html {
enum Flag
{
  NONE = 0x0000,
  BOLD = 0x0001,
  ITALIC = 0x0002,
  UNDERLINE = 0x0004,
  STRIKEOUT = 0x0008,
  SUBSCRIPT = 0x0010,
  SUPERSCRIPT = 0x0020,
  SMALL = 0x0040,
  BIG = 0x0080,
  ALIGN_RIGHT = 0x1000 // Only for table data
};

Q_DECLARE_FLAGS(Flags, Flag);
Q_DECLARE_OPERATORS_FOR_FLAGS(html::Flags);
}

class HtmlBuilder
{
  Q_DECLARE_TR_FUNCTIONS(HtmlBuilder)

public:
  HtmlBuilder(bool hasBackColor);
  ~HtmlBuilder();

  HtmlBuilder& b(const QString& str);
  HtmlBuilder& i(const QString& str);
  HtmlBuilder& u(const QString& str);
  HtmlBuilder& sub(const QString& str);
  HtmlBuilder& sup(const QString& str);
  HtmlBuilder& small(const QString& str);
  HtmlBuilder& big(const QString& str);
  HtmlBuilder& hr(int size = 1, int widthPercent = 100);
  HtmlBuilder& a(const QString& text, const QString& href,
                 html::Flags flags = html::NONE, QColor color = QColor());
  HtmlBuilder& img(const QString& src, const QString& alt = QString(),
                   const QString& style = QString(), QSize size = QSize());
  HtmlBuilder& img(const QIcon& icon, const QString& alt = QString(),
                   const QString& style = QString(), QSize size = QSize());

  HtmlBuilder& ol();
  HtmlBuilder& olEnd();
  HtmlBuilder& ul();
  HtmlBuilder& ulEnd();
  HtmlBuilder& li(const QString& str, html::Flags flags = html::NONE, QColor color = QColor());

  HtmlBuilder& text(const QString& str, html::Flags flags = html::NONE, QColor color = QColor());

  /* Add string enclosed in a paragraph */
  HtmlBuilder& p(const QString& str, html::Flags flags = html::NONE, QColor color = QColor());
  HtmlBuilder& p();
  HtmlBuilder& pEnd();

  HtmlBuilder& pre(const QString& str, html::Flags flags = html::NONE, QColor color = QColor());

  HtmlBuilder& br();
  HtmlBuilder& textBr(const QString& str);
  HtmlBuilder& brText(const QString& str);
  HtmlBuilder& nbsp();

  /* Add HTML header */
  HtmlBuilder& h(int level, const QString& str, html::Flags flags = html::NONE,
                 QColor color = QColor(), const QString& id = QString());
  HtmlBuilder& h1(const QString& str, html::Flags flags = html::NONE,
                  QColor color = QColor(), const QString& id = QString());
  HtmlBuilder& h2(const QString& str, html::Flags flags = html::NONE,
                  QColor color = QColor(), const QString& id = QString());
  HtmlBuilder& h3(const QString& str, html::Flags flags = html::NONE,
                  QColor color = QColor(), const QString& id = QString());
  HtmlBuilder& h4(const QString& str, html::Flags flags = html::NONE,
                  QColor color = QColor(), const QString& id = QString());
  HtmlBuilder& h5(const QString& str, html::Flags flags = html::NONE,
                  QColor color = QColor(), const QString& id = QString());

  HtmlBuilder& table();
  HtmlBuilder& tableEnd();
  HtmlBuilder& row(const QString& name, const QString& value,
                   html::Flags flags = html::BOLD, QColor color = QColor());
  HtmlBuilder& row(const QString& name, float value, int precision = -1,
                   html::Flags flags = html::BOLD, QColor color = QColor());
  HtmlBuilder& row(const QString& name, double value, int precision = -1,
                   html::Flags flags = html::BOLD, QColor color = QColor());
  HtmlBuilder& row(const QString& name, int value,
                   html::Flags flags = html::BOLD, QColor color = QColor());
  HtmlBuilder& rowVar(const QString& name, const QVariant& value,
                      html::Flags flags = html::BOLD, QColor color = QColor());

  HtmlBuilder& tr(QColor backgroundColor = QColor());
  HtmlBuilder& trEnd();
  HtmlBuilder& td(const QString& str, html::Flags flags = html::NONE, QColor color = QColor());

  HtmlBuilder& doc();
  HtmlBuilder& docEnd();

  bool checklength(int maxLines, const QString& msg);

  bool isEmpty() const
  {
    return htmlText.isEmpty();
  }

  void clear();

  const QString& getHtml() const
  {
    return htmlText;
  }

  QLocale::FormatType getDateFormat() const
  {
    return dateFormat;
  }

  void setDateFormat(QLocale::FormatType value)
  {
    dateFormat = value;
  }

  int getPrecision() const
  {
    return defaultPrecision;
  }

  void setPrecision(int value)
  {
    defaultPrecision = value;
  }

  int getNumLines() const
  {
    return numLines;
  }

  bool isTruncated() const
  {
    return truncated;
  }

  void setTruncated(bool value)
  {
    truncated = value;
  }

private:
  /* Select alternating entries based on the index from the string list */
  const QString& alt(const QStringList& list) const;
  QString asText(const QString& str, html::Flags flags, QColor color);

  QString rowBackColor, rowBackColorAlt, tableRowHeader;
  QStringList tableRow, tableRowAlignRight, tableRowBegin;

  int tableIndex = 0, defaultPrecision = 0, numLines = 0;
  QString htmlText;

  QLocale locale;
  QLocale::FormatType dateFormat = QLocale::ShortFormat;
  bool truncated = false;
};

#endif // HTMLBUILDER_H
