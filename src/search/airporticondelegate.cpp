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

#include "search/airporticondelegate.h"

#include "app/navapp.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "common/symbolpainter.h"
#include "search/sqlmodel.h"
#include "search/sqlproxymodel.h"
#include "sql/sqlrecord.h"

#include <QPainter>

AirportIconDelegate::AirportIconDelegate(const ColumnList *columns)
  : cols(columns)
{
  symbolPainter = new SymbolPainter();
  mapTypesFactory = new MapTypesFactory();
}

AirportIconDelegate::~AirportIconDelegate()
{
  delete symbolPainter;
  delete mapTypesFactory;
}

void AirportIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  QModelIndex idx(index);
  const SqlModel *sqlModel = dynamic_cast<const SqlModel *>(index.model());
  if(sqlModel == nullptr)
  {
    // Convert index to source if distance search proxy is used
    const SqlProxyModel *sqlProxyModel = dynamic_cast<const SqlProxyModel *>(index.model());
    Q_ASSERT(sqlProxyModel != nullptr);
    sqlModel = dynamic_cast<const SqlModel *>(sqlProxyModel->sourceModel());
    idx = sqlProxyModel->mapToSource(index);
  }
  Q_ASSERT(sqlModel != nullptr);

  // Get airport from the SQL model
  map::MapAirport ap;
  mapTypesFactory->fillAirport(sqlModel->getSqlRecord(idx.row()), ap, true /* complete */, false /* nav */,
                               NavApp::isAirportDatabaseXPlane(false /* navdata */));

  // Create a style copy
  QStyleOptionViewItem opt(option);
  opt.displayAlignment = Qt::AlignRight;

  // Check for empty airport and empty selected
  if(!ap.emptyDraw())
    opt.font.setBold(true);

  // Add-on italic underlined
  if(ap.addon())
  {
    opt.font.setItalic(true);
    opt.font.setUnderline(true);
  }

  // Closed - strike out
  if(ap.closed())
    opt.font.setStrikeOut(true);

  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);

  // Draw the text
  QStyledItemDelegate::paint(painter, opt, index);

  // Draw the symbol
  int symbolSize = option.rect.height() - 6;
  symbolPainter->drawAirportSymbol(painter, ap, option.rect.x() + symbolSize,
                                   option.rect.y() + symbolSize / 2.f + 2.f, symbolSize, false, false, false);
}
