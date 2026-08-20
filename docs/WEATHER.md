# OpenAIRAC Map — Live Aviation Weather Guide

This document describes how live aviation weather from NOAA AviationWeather.gov is integrated into **OpenAIRAC Map**.

---

## 1. Airport Weather Tab

When an airport is selected, the **Information** -> **Weather** tab displays:
- **Live METAR Observation**: Flight category badge (`VFR`, `MVFR`, `IFR`, `LIFR`), decoded wind speed and gusts, visibility, temperature, dewpoint, altimeter setting, and report age.
- **Terminal Aerodrome Forecast (TAF)**: Multi-period forecast periods with validity intervals and change groups (`FM`, `TEMPO`, `BECMG`).
- **Data Source**: Explicit provider badge `[AWC]` indicating live data from NOAA Aviation Weather Center.

---

## 2. Preflight Flight Briefing

To generate an integrated preflight briefing for your active route:
1. Load or plan a route in OpenAIRAC Map.
2. Select menu: **Flight Plan** -> **OpenAIRAC Preflight Briefing...** (or shortcut `Ctrl+Shift+B`).
3. The briefing displays:
   - Departure weather and available departure charts.
   - Destination weather and **Forecast at ETA** based on your planned flight time.
   - **Enroute Hazards**: Active international SIGMET polygons intersecting the route corridor (50 NM half-width).
   - Provenance and AIRAC cycle status for navdata, charts, and weather.
