# v8 Config Precedence

## Order of Truth
1. Compile-time defaults.
2. SPIFFS persisted config.
3. Runtime cloud settings from Firebase (`/ttgo_tcall/settings/runtime`).
4. Firestore gateway data (`sim_modules`, `allowed_numbers`, `sms_jobs`, `call_jobs`) for control-plane decisions.
5. Runtime changes from dashboard or cloud sync.
6. Persist the last known good configuration.

## ThingSpeak Credentials
- ThingSpeak upload credentials are treated as device configuration, not runtime cloud state.
- The channel ID and write API key should be supplied from local secrets or persisted config.
- Missing ThingSpeak credentials disable the upload path and should be reported clearly at startup.

## Runtime Cloud Settings (v8.1)
- `intervalOfDhtSeconds` controls telemetry push interval.
- `showFirebasePushLogs` controls periodic success-log visibility.
- On startup, firmware reads Firebase values first and keeps them when they already exist.
- If a key is missing or invalid, firmware creates/heals it with default and logs that event on serial.
- Runtime settings sync runs at startup, every 10 minutes, and on manual `sync` serial command.

## SPIFFS Dashboard Metadata
- The SPIFFS asset folder contains `dashboard.html`, `dashboard.css`, `dashboard.js`, and `version.txt`.
- Any change to those files should bump the version/date in `version.txt`.
- The dashboard reads `version.txt` and displays it in the header and About tab so operators can verify the deployed asset set.

## Acceptance Criteria
- Every runtime feature has a default.
- Every important setting can be persisted.
- Fallback behavior is defined when cloud access is unavailable.
- Missing runtime cloud keys are auto-created and operator-notified.
