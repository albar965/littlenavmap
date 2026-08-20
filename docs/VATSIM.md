# VATSIM Integration Specification — OpenAIRAC Map

## 1. Official Data Endpoints

OpenAIRAC Map interfaces exclusively with official VATSIM public APIs:
* **Live Network Feed**: `https://data.vatsim.net/v3/vatsim-data.json` (Polling cadence: 15 seconds)
* **Events Feed**: `https://events.vatsim.net/api/v2/events` (Polling cadence: 10 minutes)

---

## 2. Privacy and Security Guardrails

1. **No Account / Credentials Required**: The application operates completely anonymously as a public data consumer.
2. **Never Collect Passwords**: OpenAIRAC Map never displays password prompts or collects VATSIM user credentials.
3. **No Network Transmissions**: OpenAIRAC Map does not transmit user aircraft telemetry or voice to the network.
4. **Primary Map Labeling**: Aircraft are labeled by callsign, type, and flight level (e.g. `BAW123 B789 FL350`).
5. **Fail-Closed Sanitization**: All incoming remarks, ATIS text, and event descriptions are escaped to prevent XSS / markup injection.

---

## 3. UI Components

* **`OpenAIRACEventsDock`**: View active and upcoming VATSIM events, participating airports, and event descriptions.
* **`FlightBriefingDialog`**: Section 4 details active departure, enroute, and arrival ATC plus active corridor traffic.
* **Status Bar Indicator**: Displays network health (e.g. `VATSIM LIVE 1,822 users (6s ago)`).
