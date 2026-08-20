# OpenAIRAC Map — Upstream Synchronization Guide

This document defines the synchronization strategy and architectural boundaries between upstream **Little Navmap** (`albar965/littlenavmap`) and **OpenAIRAC Map** (`bobberdolle1/openairac-map`).

---

## 1. Repositories & Remotes

| Role | Repository URL | Purpose |
|---|---|---|
| **Origin** | `https://github.com/bobberdolle1/openairac-map.git` | Official OpenAIRAC Map repository |
| **Upstream** | `https://github.com/albar965/littlenavmap.git` | Upstream Little Navmap master |

### Recommended Remote Setup

```bash
git remote -v
# origin   https://github.com/bobberdolle1/openairac-map.git (fetch & push)
# upstream https://github.com/albar965/littlenavmap.git (fetch & push)
```

---

## 2. Fork Baseline

- **Initial Fork Point**: Commit `e925c1486f7657a4623dfca23c215d3c07f49201` (`Split mapflags include file`) on `master`.
- **Upstream Version Baseline**: Little Navmap 3.0.x / 3.1.x develop series.
- **OpenAIRAC Map Version**: 0.1.0.

---

## 3. Merge & Rebase Strategy

To keep upstream merges fast and conflict-free:

1. **Rebase/Merge Cadence**: Sync regularly from `upstream/master` on every upstream minor release or bi-weekly.
2. **Preference**: Use `git merge upstream/master` with explicit merge commits to preserve clear upstream history.
3. **Conflict Resolution Policy**:
   - Upstream flight planning, Marble map widget, SimConnect, and routing algorithm improvements MUST be accepted.
   - OpenAIRAC provider abstraction (`src/openairac/`), OpenAIRAC database slot, provenance formatting, and branding layers MUST be retained.

### Command Workflow

```bash
# Fetch latest upstream commits
git fetch upstream master

# Ensure local master is clean
git checkout master

# Merge upstream changes
git merge upstream/master -m "merge: sync with upstream littlenavmap $(date +%Y-%m-%d)"

# Verify build and tests
qmake littlenavmap.pro
make -j$(nproc)
```

---

## 4. Architectural Boundaries

To minimize merge friction:

### Kept Strictly Close to Upstream (Avoid Invasive Changes)
- `src/route/` (Flight planning, calculation, route commands)
- `src/query/` (Map and spatial queries)
- `src/connect/` (SimConnect, XpConnect, Little Navconnect clients)
- `src/perf/` (Aircraft performance models)
- `src/weather/` (NOAA, METAR, GRIB weather engines)
- `src/marble/` (Marble mapping widget interfaces)

### OpenAIRAC Extension & Customization Modules (Isolated)
- `src/openairac/` (NavigationProvider abstraction, ProvenanceManager, CoverageManager, OpenAiracDbManager)
- `src/db/databasemanager.cpp` (Additive: OpenAIRAC database file resolution & Navigation Data menu)
- `src/common/htmlinfobuilder.cpp` (Additive: Provenance & Coverage display blocks)
- `docs/` (OpenAIRAC documentation and integration contracts)

---

## 5. Summary Table

| Category | Policy |
|---|---|
| License | `GPL-3.0-or-later` preserved across all files |
| Upstream Credits | 100% preserved in About dialog, headers, and docs |
| Database Files | `openairac.sqlite` strictly isolated from `little_navmap_navigraph.sqlite` |
| Navigraph | Preserved as optional integration; fallback is OFF by default |
