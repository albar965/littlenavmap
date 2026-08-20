# OpenAIRAC Map — Online Flying & Live Network Awareness (v0.4.0)

## 1. Product Concept

OpenAIRAC Map integrates official online simulation network telemetry (e.g. VATSIM) to provide complete situational awareness for virtual aviators while preserving strict data provenance and independence.

```
+-----------------------------------------------------------------------------+
|                           OPENAIRAC MAP PLATFORM                            |
+-----------------------------------------------------------------------------+
|  NAVDATA     | OpenAIRAC Canonical Navdata (FAA / SIA / Europe / World)     |
|  CHARTS      | FAA d-TPP / France SIA eAIP Terminal Charts                  |
|  WEATHER     | NOAA AviationWeather.gov (METAR / TAF / SIGMET / Hazards)    |
|  ONLINE      | VATSIM Data API v3 & Events API v2 (Pilots, ATC, ATIS)       |
|  SIMULATOR   | X-Plane / MSFS Live Telemetry & Scenery                      |
+-----------------------------------------------------------------------------+
```

---

## 2. Key Capabilities in v0.4.0

### 2.1 Live Network Layers
* **VATSIM Pilots**: Real-time traffic with callsign, aircraft type, and altitude display (e.g. `BAW123 B789 FL350`).
* **Display Interpolation**: Smooth aircraft movement between 15-second data pulses without position jumping or infinite extrapolation on delayed feeds.
* **VATSIM ATC Controllers**: Range indicators and frequencies for active clearance delivery, ground, tower, approach, and enroute center controllers.
* **VATSIM ATIS**: Live airport ATIS indicator letters (e.g. `INFO B`) and decoded weather/runway broadcast text.

### 2.2 Airport Online Workspace
When selecting an airport (e.g. `KJFK`, `EGLL`, `LFPG`):
* **ATC Online**: List of active delivery, ground, tower, and radar approach stations.
* **Current ATIS**: Full decoded text, active phonetic letter, and frequency.
* **Traffic Operations**: Real-time count of filed arrivals, filed departures, and ground aircraft.

### 2.3 Route ATC & Online Briefing
For any active flight plan (e.g. `KJFK -> LFPG`):
* **ATC Along Route**: Automatically groups active controllers by phase (`Departure`, `Enroute`, `Arrival`).
* **Corridor Traffic**: Counts active aircraft flying within the 50 NM route corridor.
* **Events Match**: Highlights active and upcoming VATSIM events matching departure/destination airports.
* **Integrated Briefing**: Section 4 in OpenAIRAC Preflight Briefing provides full online operational intelligence.
