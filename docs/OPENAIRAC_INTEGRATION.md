# OpenAIRAC Integration Contract & Architecture

This document defines the interface boundary, schema contract, and database lifecycle between the **OpenAIRAC core compiler/platform** (`bobberdolle1/open-airac`) and **OpenAIRAC Map** (`bobberdolle1/openairac-map`).

---

## 1. Architectural Boundary

```text
OpenAIRAC Core (Rust)
  ├── Open / Government Ingestion (FAA, SIA, DFS, OurAirports, OFM)
  ├── Temporal Database & Reconciler
  ├── Procedure Validator & LPV FAS Geometer
  └── SQLite Exporter (`openairac-export-lnm`)
            │
            ▼ Output: `openairac.sqlite` (Schema v14.29)
┌─────────────────────────────────────────────────────────────┐
│ OpenAIRAC Map (C++ / Qt)                                    │
│   ├── NavigationProvider: OpenAIRAC (Default)               │
│   ├── NavigationProvider: Simulator Scenery                 │
│   ├── NavigationProvider: Navigraph (Optional, Fallback OFF)│
│   ├── Provenance & Coverage UI Engine                       │
│   └── Flight Planning & Moving Map Engine                   │
└─────────────────────────────────────────────────────────────┘
```

- **Separation of Concerns**: OpenAIRAC core owns data ingestion, reconciliation, temporal validity, and cryptographic bundle verification. OpenAIRAC Map consumes the exported SQLite database for GUI flight planning, search, and moving map display.
- **Physical Database Separation**: OpenAIRAC database is stored in `openairac.sqlite` (and `little_navmap_openairac.sqlite`). Navigraph data is stored in `little_navmap_navigraph.sqlite`. The two databases are NEVER mixed, merged, or overwritten into the same file.

---

## 2. SQLite Database Contract

OpenAIRAC Map requires SQLite schema version **14.29** (`db_version_major = 14`, `db_version_minor = 29`).

### Core Tables & Structure

| Table | Required Columns / Semantics |
|---|---|
| `metadata` | `db_version_major` (14), `db_version_minor` (29), `data_source` ('OPENAIRAC'), `airac_cycle` (e.g. '2608'), `has_sid_star` (1/0) |
| `airport` | 70 standard columns including `ident`, `name`, `type`, `rating`, `altitude`, `lonx`, `laty`, runway summary flags |
| `runway` | Runway pairs with primary/secondary runway ends, true headings, dimensions, and surface types |
| `runway_end` | Physical threshold coordinates (`lonx`, `laty`), threshold elevation (`altitude`), and magnetic heading |
| `waypoint` | Named 5-letter enroute and terminal navigation fixes |
| `vor` | VOR, VOR-DME, VORTAC facilities with station declination, elevation, and frequency |
| `ndb` | NDB facilities with frequency in kHz*10 |
| `ils` | ILS localizer bearing and glideslope pitch angle |
| `airway` | Airway route fragments with start/end fixes, min/max altitude bounds, and spatial bounding boxes |
| `approach` | Approaches, SIDs (suffix='D', type='GPS'), and STARs (suffix='A', type='GPS') |
| `approach_leg` | Ordered procedure legs with ARINC path terminators (IF, TF, CF, DF, FA, CA, VA, etc.), speed limits, and altitude constraints |
| `transition` | Enroute and runway transitions for SIDs, STARs, and Approaches |
| `transition_leg` | Waypoint legs comprising transitions |
| `bgl_file` | Source file metadata referencing `scenery_area` for provenance lookup |
| `scenery_area` | Primary scenery area record for OpenAIRAC |
| `magdecl` | WMM magnetic declination epoch and reference point |

---

## 3. Database Update & Rollback Mechanism

1. **Detection**: OpenAiracDbManager continuously checks for `openairac.sqlite` in the configured settings/application database directory.
2. **Staged Validation**: When updating navdata, the new database is checked with `PRAGMA integrity_check`, `PRAGMA foreign_key_check`, and metadata version compatibility BEFORE activating.
3. **Atomic Swap**:
   - `openairac.sqlite` -> `openairac.sqlite.backup` (created for rollback).
   - Candidate database atomically renamed to `openairac.sqlite`.
4. **Automatic Rollback**: If the new database fails post-swap verification or encounters an error during startup, the `.backup` file is automatically restored.

---

## 4. Provider & Provenance Integration

- **OpenAIRAC First**: On a fresh install, OpenAIRAC is the preferred and default navigation provider whenever `openairac.sqlite` is present.
- **Scenery Blending**: OpenAIRAC provides the authoritative aviation navigation data (fixes, navaids, airways, procedures), while simulator scenery provides local airport geometry (taxiways, aprons, parking, runways).
- **Truthful Absence**: If terminal procedures are not provided by an open public authority (e.g. France SIA baseline), OpenAIRAC Map honestly displays zero procedures and indicates the source reason. No synthetic procedures are invented.
