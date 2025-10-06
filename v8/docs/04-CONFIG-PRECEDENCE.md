# v8 Config Precedence

## Order of Truth
1. Compile-time defaults.
2. SPIFFS persisted config.
3. Runtime cloud settings from Firebase (`/ttgo_tcall/settings/runtime`).
4. Runtime changes from dashboard or cloud sync.
5. Persist the last known good configuration.

## Runtime Cloud Settings (v8.1)
- `intervalOfDhtSeconds` controls telemetry push interval.
- `showFirebasePushLogs` controls periodic success-log visibility.
- If a key is missing or invalid, firmware creates/heals it with default and logs that event on serial.
- Runtime settings sync runs at startup, every 10 minutes, and on manual `sync` serial command.

## Acceptance Criteria
- Every runtime feature has a default.
- Every important setting can be persisted.
- Fallback behavior is defined when cloud access is unavailable.
- Missing runtime cloud keys are auto-created and operator-notified.
