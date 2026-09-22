# RNAV reference feature (private build)

## Native map use

Right-click any valid Little Navmap map coordinate and select **RNAV Reference ...**. The dialog displays up to three usable VOR/DME bases:

```text
VOR/DME | Frequency | Radial | Distance | Range
SFD     | 113.85 MHz| R-143° |22.4 NM|80 NM
```

The displayed **Distance** is the ground/geodesic reference distance from the base VOR/DME to the selected target coordinate. It is the value used to define the virtual RNAV point; it is not the aircraft receiver's live DME slant range.

Candidate eligibility: calibrated VOR, DME available, valid VOR frequency, and target point within the published station range. The magnetic radial uses the VOR's calibrated magnetic variation, matching Little Navmap's measurement-line convention.

## Local API

Start Little Navmap's Web Server, then request:

```text
GET http://127.0.0.1:8965/api/rnav/reference?lat=48.1234&lon=11.5678
```

The response contains `target` and ranked `candidates`. Browser clients such as an MSFS 2024 EFB can use this endpoint because LNM's existing API emits CORS response headers.

The server should remain localhost-only unless a trusted home LAN client requires access. Do not expose it to the public internet.
