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

#include "optiondata.h"

#include "exception.h"

#include <QDebug>

OptionData *OptionData::optionData = nullptr;

OptionData::OptionData()
{

}

OptionData::~OptionData()
{

}

const OptionData& OptionData::instance()
{
  OptionData& od = instanceInternal();

  if(!od.valid)
  {
    qCritical() << "OptionData not initialized yet";
    throw new atools::Exception("OptionData not initialized yet");
  }

  return od;
}

OptionData& OptionData::instanceInternal()
{
  if(optionData == nullptr)
  {
    qDebug() << "Creating new OptionData";
    optionData = new OptionData();
  }

  return *optionData;

}




