/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_ROUTEEXPORTDATA_H
#define LNM_ROUTEEXPORTDATA_H

#include <QTime>
#include <QString>

/*
 * Contains all online network relevant data that is needed to export flight plans.
 */
class RouteExportData
{
public:
  RouteExportData();

  const QString& getPilotInCommand() const
  {
    return pilotInCommand;
  }

  void setPilotInCommand(const QString& value)
  {
    pilotInCommand = value;
  }

  const QString& getFlightType() const
  {
    return flightType;
  }

  const QString& getFlightRules() const
  {
    return flightRules;
  }

  void setFlightType(const QString& value)
  {
    flightType = value;
  }

  void setFlightRules(const QString& value)
  {
    flightRules = value;
  }

  const QString& getCallsign() const
  {
    return callsign;
  }

  void setCallsign(const QString& value)
  {
    callsign = value;
  }

  const QString& getAirline() const
  {
    return airline;
  }

  void setAirline(const QString& value)
  {
    airline = value;
  }

  const QString& getLivery() const
  {
    return livery;
  }

  void setLivery(const QString& value)
  {
    livery = value;
  }

  int getPassengers() const
  {
    return passengers;
  }

  void setPassengers(int value)
  {
    passengers = value;
  }

  const QString& getAircraftType() const
  {
    return aircraftType;
  }

  void setAircraftType(const QString& value)
  {
    aircraftType = value;
  }

  const QString& getWakeCategory() const
  {
    return wakeCategory;
  }

  void setWakeCategory(const QString& value)
  {
    wakeCategory = value;
  }

  bool isHeavy() const
  {
    return heavy;
  }

  void setHeavy(bool value)
  {
    heavy = value;
  }

  const QString& getEquipmentPrefix() const
  {
    return equipmentPrefix;
  }

  void setEquipmentPrefix(const QString& value)
  {
    equipmentPrefix = value;
  }

  const QString& getEquipmentSuffix() const
  {
    return equipmentSuffix;
  }

  void setEquipmentSuffix(const QString& value)
  {
    equipmentSuffix = value;
  }

  const QString& getEquipment() const
  {
    return equipment;
  }

  void setEquipment(const QString& value)
  {
    equipment = value;
  }

  const QString& getDeparture() const
  {
    return departure;
  }

  void setDeparture(const QString& value)
  {
    departure = value;
  }

  const QString& getDestination() const
  {
    return destination;
  }

  void setDestination(const QString& value)
  {
    destination = value;
  }

  const QString& getAlternate() const
  {
    return alternate;
  }

  void setAlternate(const QString& value)
  {
    alternate = value;
  }

  const QString& getAlternate2() const
  {
    return alternate2;
  }

  void setAlternate2(const QString& value)
  {
    alternate2 = value;
  }

  const QString& getRoute() const
  {
    return route;
  }

  void setRoute(const QString& value)
  {
    route = value;
  }

  int getCruiseAltitude() const
  {
    return cruiseAltitude;
  }

  void setCruiseAltitude(int value)
  {
    cruiseAltitude = value;
  }

  int getSpeed() const
  {
    return speed;
  }

  void setSpeed(int value)
  {
    speed = value;
  }

  const QTime& getDepartureTime() const
  {
    return departureTime;
  }

  void setDepartureTime(const QTime& value)
  {
    departureTime = value;
  }

  const QTime& getDepartureTimeActual() const
  {
    return departureTimeActual;
  }

  void setDepartureTimeActual(const QTime& value)
  {
    departureTimeActual = value;
  }

  int getEnrouteMinutes() const
  {
    return enrouteMinutes;
  }

  void setEnrouteMinutes(int value)
  {
    enrouteMinutes = value;
  }

  int getEnduranceMinutes() const
  {
    return enduranceMinutes;
  }

  void setEnduranceMinutes(int value)
  {
    enduranceMinutes = value;
  }

  const QString& getVoiceType() const
  {
    return voiceType;
  }

  void setVoiceType(const QString& value)
  {
    voiceType = value;
  }

  const QString& getTransponder() const
  {
    return transponder;
  }

  void setTransponder(const QString& value)
  {
    transponder = value;
  }

  const QString& getRemarks() const
  {
    return remarks;
  }

  void setRemarks(const QString& value)
  {
    remarks = value;
  }

private:
  QString pilotInCommand;
  QString flightType;
  QString flightRules;
  QString callsign;
  QString airline;
  QString livery;
  int passengers = 0;

  QString aircraftType;
  QString wakeCategory;
  bool heavy = false;
  QString equipmentPrefix;
  QString equipmentSuffix;
  QString equipment;

  QString departure;
  QString destination;
  QString alternate;
  QString alternate2;
  QString route;
  int cruiseAltitude = 0;
  int speed = 0;

  QTime departureTime;
  QTime departureTimeActual;
  int enrouteMinutes = 0;
  int enduranceMinutes = 0;

  QString voiceType;
  QString transponder;

  QString remarks;

  // [FLIGHTPLAN]
  // CALLSIGN=VPI333
  // PIC=NAME
  // FMCROUTE=
  // LIVERY=
  // AIRLINE=VPI
  // SPEEDTYPE=N
  // POB=83
  // ENDURANCE=0215
  // OTHER=X-IvAp CREW OF 2 PILOT VPI07/COPILOT VPI007
  // ALT2ICAO=
  // ALTICAO=LFMP
  // EET=0115
  // DESTICAO=LFBO
  // ROUTE=TINOT UY268 DIVKO UM731 FJR
  // LEVEL=330
  // LEVELTYPE=F
  // SPEED=300
  // DEPTIME=2110
  // DEPICAO=LFKJ
  // TRANSPONDER=S
  // EQUIPMENT=SDFGW
  // WAKECAT=M
  // ACTYPE=B733
  // NUMBER=1
  // FLIGHTTYPE=N
  // RULES=I

  // <?xml version="1.0" encoding="utf-8"?>
  // <FlightPlan xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  // FlightType="IFR"
  // Equipment="/L"
  // CruiseAltitude="23000"
  // CruiseSpeed="275"
  // DepartureAirport="KBZN"
  // DestinationAirport="KJAC"
  // AlternateAirport="KSLC"
  // Route="DCT 4529N11116W 4527N11114W 4524N11112W 4522N11110W DCT 4520N11110W 4519N11111W/N0276F220 4517N11113W 4516N11115W 4514N11115W/N0275F230 4509N11114W 4505N11113W 4504N11111W 4502N11107W 4500N11105W 4458N11104W 4452N11103W 4450N11105W/N0276F220 4449N11108W 4449N11111W 4449N11113W 4450N11116W 4451N11119W 4450N11119W/N0275F230 4448N11117W 4446N11116W DCT KWYS DCT 4440N11104W 4440N11059W 4439N11055W 4439N11052W 4435N11050W 4430N11050W 4428N11050W 4426N11044W 4427N11041W 4425N11035W 4429N11032W 4428N11031W 4429N11027W 4429N11025W 4432N11024W 4432N11022W 4432N11018W 4428N11017W 4424N11017W 4415N11027W/N0276F220 DCT 4409N11040W 4403N11043W DCT 4352N11039W DCT"
  // Remarks="PBN/D2 DOF/181102 REG/N012SB PER/B RMK/TCAS SIMBRIEF"
  // IsHeavy="false"
  // EquipmentPrefix=""
  // EquipmentSuffix="L"
  // DepartureTime="2035"
  // DepartureTimeAct="0"
  // EnrouteHours="0"
  // EnrouteMinutes="53"
  // FuelHours="2"
  // FuelMinutes="44"
  // VoiceType="Full" />

};

#endif // LNM_ROUTEEXPORTDATA_H
