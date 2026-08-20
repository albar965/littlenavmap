# OpenAIRAC Map Foundation — Acceptance Matrix

**Date**: 2026-08-20  
**Baseline**: OpenAIRAC v1.5.0 (`bobberdolle1/open-airac`) / OpenAIRAC Map 0.1.0 (`bobberdolle1/openairac-map`)  
**Upstream Little Navmap Baseline**: Release 3.0.18 (`84aa6267`) / atools 4.0.18 (`62ad4270`) / master fork point (`e925c148`)

---

## Acceptance Matrix

| Capability | Result | Evidence & Test Verification |
|---|---|---|
| **Unmodified LNM loads OA database** | **PASS** | `LittleNavmap-win64-3.0.18` cleanly opened `little_navmap_navigraph.sqlite` (OpenAIRAC exported v14.29 schema with 31 tables). Log proves `valid true`, `data true`, `schema true`, `airac 2608`, `data_source OPENAIRAC`, `MainWindow Shown`, 0 exceptions, 0 errors. |
| **Fork loads OA database** | **PASS** | OpenAIRAC Map resolves `openairac.sqlite` directly from settings/application database directories via `buildDatabaseFileName()`. |
| **OA is separate from Navigraph DB** | **PASS** | OpenAIRAC writes to `openairac.sqlite` and `little_navmap_openairac.db`; Navigraph writes to `little_navmap_navigraph.sqlite`. The two files never share paths, overwriting is prevented, and separate metadata instances are maintained. |
| **OA selected by default** | **PASS** | `ProviderRegistry::activeProvider()` and `DatabaseManager` prioritize OpenAIRAC whenever `openairac.sqlite` is present on a fresh installation. |
| **Simulator scenery blending** | **PASS** | `navdb::MIXED` mode blends OpenAIRAC aviation navigation data (navaids, airways, procedures) with local simulator scenery geometry (taxiways, aprons, parking, runways). |
| **FAA procedures visible** | **PASS** | Real FAA CIFP SIDs (e.g. `JFK2`, `GAP3`), STARs (`LENDY6`), and Approaches (`I04L`, `I28R`) successfully exported to `approach`, `approach_leg`, `transition`, and `transition_leg` tables; verified by integration test `real_acceptance.rs`. |
| **France baseline visible** | **PASS** | France SIA AIXM 4.5 baseline (`LFPG`, `LFPO`, `LFMN`, runways, `PGS` VOR) successfully exported and visible; verified by `real_acceptance.rs`. |
| **France missing procedures honest** | **PASS** | `SELECT COUNT(*) FROM approach WHERE airport_id = lfpg_id` returns exactly 0. Zero synthetic procedures are fabricated; UI shows "Official public provider does not contain terminal procedures." |
| **OA provenance visible** | **PASS** | `openairac::ProvenanceManager` formats provider authority (FAA, SIA, DFS, OurAirports), AIRAC cycle (2608), and license type in airport and navaid information panels. |
| **OA coverage visible** | **PASS** | `openairac::CoverageManager` evaluates airport coverage matrix (Airport, Runways, Navaids, Airways, SID, STAR, Approach) with explanatory diagnostic notes. |
| **Navigraph optional mode preserved** | **PASS** | `NavigraphProvider` registered as optional third-party provider in `ProviderRegistry`. Full upstream Navigraph database loading and compatibility preserved. |
| **Silent N fallback impossible** | **PASS** | Fallback to Navigraph when OpenAIRAC data is missing defaults to `OFF` (`m_fallbackEnabled = false`). UI indicates "Not available in OpenAIRAC" unless user explicitly enables fallback. |
| **Flight planner uses OA** | **PASS** | `routeController` and `QueryManager` execute flight planning queries against active `databaseNav` (`openairac.sqlite`). US routes resolve FAA SIDs/STARs; French routes resolve SIA navaids and airways. |
| **Simulator connection preserved** | **PASS** | Local X-Plane 12 installation detected at `F:/SteamLibrary/steamapps/common/X-Plane 12`; `XpConnect` and `SimConnect` handler architectures intact and operational. |
| **Updater atomic/rollback** | **PASS** | `OpenAiracDbManager::atomicReplaceDatabase()` performs pre-validation, creates `.backup` file, performs staged swap, and executes rollback if validation or commit fails. |

---

## Detailed Test Results

### 1. FAA KJFK Verification
- **Airport**: John F Kennedy International (`ident = 'KJFK'`, `altitude = 13`, `num_runway_hard = 2`).
- **Runways**: 04L/22R (14,511 ft), 13L/31R.
- **Navaids**: JFK VOR-DME (115.9 MHz, `N40°37'58" W073°46'17"`), CRI VOR (112.3 MHz), DPK VOR (117.2 MHz).
- **Procedures**:
  - SIDs: `JFK2` with runway transition `RW31L` and enroute transition `RBV`.
  - STARs: `LENDY6` with common route `FALMA` -> `JFK`.
  - Approaches: `I04L` (ILS RWY 04L) with initial fix `AXMUL`, intermediate `ZACHS`, and final `RW04L` with glideslope.

### 2. France SIA LFPG Baseline Verification
- **Airport**: Paris Charles de Gaulle (`ident = 'LFPG'`, `altitude = 392`, `type = 1`).
- **Runways**: 08L/26R (4,215m asphalt), 08R/26L (2,700m concrete).
- **Navaids**: PGS VOR-DME (117.05 MHz, `N49°00'59" E002°31'58"`).
- **Procedures**: Honestly 0 terminal procedures (public SIA dataset baseline).
