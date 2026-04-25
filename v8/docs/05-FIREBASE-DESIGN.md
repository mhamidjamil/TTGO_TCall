# v8 Firebase Design

## Scope
Firebase Realtime Database is the cloud direction for v8.

## Initial Design Intent
- Use a polling-based command model.
- Keep command processing offline-safe.
- Claim work atomically to avoid duplicate sends.
- Preserve local fallback behavior when internet is unavailable.
- Keep device-side configuration isolated from any service-account material.

## Proposed Paths
- Commands: `/ttgo_tcall/commands/pending`
- Command history: `/ttgo_tcall/commands/history`
- Counters: `/ttgo_tcall/counters`
- Device status: `/ttgo_tcall/status`

## Device Auth Direction
- Realtime Database access should use a device-safe Firebase auth flow.
- Service account JSON stays server-side only and must never move onto the ESP32.

## Acceptance Criteria
- Polling workflow is defined before code starts.
- Command status transitions are explicit.
- Duplicate-send prevention is documented.
- Paths are deterministic and ready for implementation.
