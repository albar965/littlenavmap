# OpenAIRAC Integrated Preflight Flight Briefing

This document describes the design and operation of the integrated preflight flight briefing system in OpenAIRAC.

---

## 1. Overview

The **OpenAIRAC Flight Briefing** integrates three independent aviation layers into a unified preflight screen:

1. **Aeronautical Navigation Infrastructure** (`OpenAIRAC Navdata`): Authoritative procedures, navaids, airways, and runways.
2. **Published Chart Documents** (`OpenAIRAC Charts`): Official departure diagrams, SIDs, STARs, and approach plates (FAA d-TPP / France SIA eAIP).
3. **Live Aviation Meteorology** (`OpenAIRAC Weather`): Live METAR observations, TAF forecasts at destination ETA, and active route corridor SIGMET hazards.

---

## 2. Briefing Sections

### 1. Departure Airport
- Current METAR observation, flight category badge (`VFR`/`MVFR`/`IFR`/`LIFR`), decoded surface conditions, and report age.
- Published TAF terminal forecast.
- Departure charts count and SID procedure status.

### 2. Destination Airport (with TAF-at-ETA)
- Current METAR observation.
- **Forecast at ETA**: Automatically selects the specific TAF forecast period matching the aircraft's estimated arrival time.
- Destination charts count and procedure status (honestly reporting public navdata availability).

### 3. Enroute Hazards & Route Corridor Analysis
- Evaluates a 50 NM lateral buffer along the active flight plan route.
- Intersecting international SIGMET polygons (convective, severe turbulence, severe icing, volcanic ash).
- In-flight PIREP pilot reports within the corridor.

### 4. Data Provenance & Cycle Status
- Navdata AIRAC cycle (e.g. `2608`).
- Charts cycle (e.g. `2608`).
- Live weather fetch timestamp.
- Full provenance transparency: `FAA` / `SIA` through OpenAIRAC, `NOAA AviationWeather.gov`.
