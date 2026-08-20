# OpenAIRAC Map EFB Roadmap

This document outlines the architectural evolution of **OpenAIRAC Map** from a flight planner / moving map foundation into a comprehensive open-source **Electronic Flight Bag (EFB)**.

---

## 1. Core Architectural Extension Points

OpenAIRAC Map introduces modular provider interfaces to decouple external aviation services from the core moving map and routing engine:

```text
OpenAIRAC Map EFB Platform
  ├── NavigationProvider      (OpenAIRAC, Simulator, Navigraph)
  ├── ChartProvider           (Government eAIP, Georeferenced PDF/PNG, OpenCharts)
  ├── WeatherProvider         (NOAA GFS/METAR/TAF, DWD, ECMWF, SIGMET)
  ├── OnlineNetworkProvider   (VATSIM, IVAO, POSCON, PilotEdge)
  ├── NotamProvider           (FAA NOTAM, Eurocontrol EAD, ICAO)
  └── TelemetryBridge         (X-Plane, MSFS, P3D, Mobile/Tablet Companion)
```

---

## 2. EFB Phased Roadmap

### Phase 1: Open & Government Aviation Charts (v2.0)
- Ingestion and rendering of open government charts (FAA Digital Terminal Procedures Publication / d-TPP).
- European AIP / eAIP chart integration (France SIA, DFS Germany, ENAIRE Spain).
- Georeferenced chart calibration metadata (clipping bounds, projection parameters, airport reference points).

### Phase 2: Georeferenced Chart Overlay & Automatic Switching (v2.1)
- Live aircraft position overlaid onto georeferenced airport diagram and instrument approach plates.
- Automatic chart switching based on active flight plan phase (Departure -> Enroute -> Arrival -> Approach -> Taxi).

### Phase 3: Comprehensive Weather Engine (v2.2)
- High-resolution live METAR, TAF, and SIGMET/AIRMET decoded layers.
- Route weather profile (cross-section showing winds aloft, icing probability, turbulence, and cloud tops).
- Real-time radar / precipitation tile overlays.

### Phase 4: Online Network Integration (v2.3)
- Real-time VATSIM / IVAO ATC airspace sector visualization.
- Controller frequency auto-tuning assistance and ATIS text feeds.
- Live traffic rendering with wake turbulence and TCAS situational awareness.

### Phase 5: Worldwide NOTAM Aggregator (v2.4)
- Geocoded NOTAM filtering along the planned route and destination runway closures.
- Visual airspace restriction overlays with temporal activation timers.

### Phase 6: Web & Mobile / Tablet Companion (v3.0)
- Browser-based and local network tablet companion display (iPad / Android EFB mode).
- Bidirectional flight plan synchronization and scratchpad integration.

### Phase 7: Performance, Weight & Balance, Fuel Planning (v3.1)
- Advanced take-off and landing performance calculation (runway condition, obstacle clearance, climb gradients).
- Detailed fuel burn predictions based on specific engine model curves.

---

## 3. Guiding Constitution

1. **Open-AIRAC First**: All baseline capabilities function completely free of any commercial subscription.
2. **Deterministic Provenance**: Every chart, weather report, and NOTAM carries explicit source authority and publication timestamp.
3. **Upstream Compatibility**: Additive modular extensions that keep upstream Little Navmap codebase clean and maintainable.
