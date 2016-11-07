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

#include "route/routemapobjectlist.h"
#include "route/routestring.h"

// EDDF SID RID Y163 ODAVU Z114 RIDSU UL607 UTABA UM738 TIRUL Y740 NATAG UM738 ADOSA UP131 LUMAV UM726 GAVRA UY345 RITEB STAR LIRF
// EDDF DCT RIDSU UL607 UTABA UM738 TIRUL Y740 NATAG UM738 AMTEL UM727 TAQ DCT LIRF

RouteString::RouteString()
{

}

RouteString::~RouteString()
{

}

// Ident;Region;Name;Airway;Type;Freq. MHz/kHz;Range nm;Course °M;Direct °M;Distance nm;Remaining nm;Leg Time hh:mm;ETA hh:mm UTC
// LOWI;;Innsbruck;;;;;;;;558,9;;0:00
// NORIN;LO;;;;;;12;12;7,8;551,0;0:04;0:04
// ALGOI;LO;;UT23;;;;275;274;36,7;514,3;0:22;0:26
// BAMUR;LS;;UN871;;;;278;278;38,8;475,5;0:23;0:49
// DINAR;LS;;Z2;;;;267;266;20,5;455,0;0:12;1:02
// KUDES;LS;;Z2;;;;266;266;7,3;447,7;0:04;1:06
// DITON;LS;;UN871;;;;238;238;25,0;422,7;0:15;1:21
// SUREP;LS;;UN871;;;;237;237;15,5;407,2;0:09;1:31
// BERSU;LS;;UN871;;;;237;237;3,4;403,8;0:02;1:33
// ROTOS;LS;;Z55;;;;290;290;9,4;394,4;0:05;1:38
// BADEP;LS;;UZ669;;;;231;231;15,7;378,7;0:09;1:48
// ULMES;LS;;UZ669;;;;231;231;6,9;371,8;0:04;1:52
// VADAR;LS;;UZ669;;;;231;231;28,5;343,3;0:17;2:09
// MILPA;LF;;UZ669;;;;239;239;41,9;301,4;0:25;2:34
// MOKIP;LF;;UL612;;;;286;285;33,7;267,7;0:20;2:54
// PODEP;LF;;UL612;;;;286;285;49,9;217,8;0:29;3:24
// MOU;LF;Moulins;UL612;VORDME (H);116,70;195;285;285;12,4;205,4;0:07;3:32
// KUKOR;LF;;UM129;;;;245;245;23,0;182,4;0:13;3:45
// LARON;LF;;UM129;;;;245;245;42,9;139,5;0:25;4:11
// GUERE;LF;;UM129;;;;245;245;5,1;134,4;0:03;4:14
// BEBIX;LF;;UM129;;;;245;245;32,2;102,3;0:19;4:33
// LMG;LF;Limoges;UM129;VORDME (H);114,50;195;245;245;17,6;84,7;0:10;4:44
// DIBAG;LF;;UN460;;;;262;262;10,1;74,6;0:06;4:50
// FOUCO;LF;;UN460;;;;262;262;13,0;61,6;0:07;4:58
// CNA;LF;Cognac;UN460;VORDME (H);114,65;195;263;262;33,7;27,9;0:20;5:18
// LFCY;;Medis;;;;;270;269;27,9;0,0;0:16;5:35
// LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
QString RouteString::createStringForRoute(const RouteMapObjectList& route)
{
  QStringList retval;
  QString lastAw, lastId;
  for(int i = 0; i < route.size(); i++)
  {
    const RouteMapObject& rmo = route.at(i);
    const QString& airway = rmo.getAirway();
    const QString& ident = rmo.getIdent();

    if(airway.isEmpty())
    {
      if(!lastId.isEmpty())
        retval.append(lastId);
      if(i > 0)
        // Add direct if not start airport
        retval.append("DCT");
    }
    else
    {
      if(airway != lastAw)
      {
        // Airways change add last ident of the last airway
        retval.append(lastId);
        retval.append(airway);
      }
      // Airway is the same skip waypoint
    }

    lastId = ident;
    lastAw = airway;
  }

  // Add destination
  retval.append(lastId);

  return retval.join(" ");
}

void RouteString::createRouteFromString(const QString& str, RouteMapObjectList& route)
{

}
