# OpenAIRAC Map — Offline Chart Pack & Cache Management

This document describes how chart assets are cached, validated, and managed for offline flight planning and in-flight cockpit usage.

---

## 1. Local Chart Cache Storage

OpenAIRAC Map maintains a content-addressed, SHA-256 verified local asset cache in:

| Platform | Cache Location |
|---|---|
| Windows | `%APPDATA%\ABarthel\charts_cache\` |
| Linux | `~/.config/ABarthel/charts_cache/` |
| macOS | `~/Library/Application Support/ABarthel/charts_cache/` |

- When a chart is opened for the first time, it downloads on-demand from the official government portal (FAA d-TPP / France SIA).
- All subsequent opens load directly from local cache with zero latency and zero internet requirement.

---

## 2. Airport Offline Packs

To prepare for offline flying:
1. Open the **OpenAIRAC Charts** dock.
2. Search for the desired departure or arrival airport (e.g. `KJFK`).
3. Click **Download All (Offline Pack)**.
4. All published plates for that airport are downloaded and verified into your local cache.

---

## 3. Security & Validation Safeguards

- **Content Hashing**: Every asset is verified against its SHA-256 digest.
- **Magic Byte Validation**: Ensures valid `%PDF-` file signature, protecting against corrupt downloads or malicious files.
- **Path Traversal Protection**: Rejects file names with traversal sequences (`..`, `/`, `\`).
