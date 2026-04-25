# v8 Config Precedence

## Order of Truth
1. Compile-time defaults.
2. SPIFFS persisted config.
3. Runtime changes from dashboard or cloud sync.
4. Persist the last known good configuration.

## Acceptance Criteria
- Every runtime feature has a default.
- Every important setting can be persisted.
- Fallback behavior is defined when cloud access is unavailable.
