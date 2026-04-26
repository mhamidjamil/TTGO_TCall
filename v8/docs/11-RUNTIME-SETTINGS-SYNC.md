# v8 Runtime Settings Sync

## Purpose
Allow selected runtime behavior to be controlled from Firebase Realtime Database without reflashing firmware.

## Paths
- Runtime settings: `/ttgo_tcall/settings/runtime`

## Runtime Variables
- `intervalOfDhtSeconds` (number)
  - Meaning: telemetry push interval in seconds.
  - Default: `15`
  - Guardrail: clamp or heal invalid values to safe default.
- `showFirebasePushLogs` (boolean)
  - Meaning: controls periodic serial info logs for telemetry and landing snapshot push success.
  - Default: `true`
- `dailySmsLimit` (number)
  - Meaning: daily SMS cap used by the rate limiter.
  - Default: `200`
- `weeklySmsLimit` (number)
  - Meaning: weekly SMS cap used by the rate limiter.
  - Default: `950`
- `monthlySmsLimit` (number)
  - Meaning: monthly SMS cap used by the rate limiter.
  - Default: `4900`

## Sync Strategy
1. Startup: device fetches runtime settings and uses the Firebase value when it already exists.
2. If missing/invalid: device creates or heals only the missing key in Firebase using the default.
3. Periodic: device refreshes runtime settings every 10 minutes.
4. Manual: serial command `sync` forces immediate refresh.

## Operator Visibility
- If a runtime variable is created or healed, print a clear serial message.
- If a runtime variable changes, print old value and new value in a readable block.
- The Firebase root should stay folder-only; runtime settings belong under `/ttgo_tcall/settings/runtime`.

## Acceptance Criteria
- Existing runtime variables are never overwritten by startup defaults.
- Missing runtime variables are auto-created.
- Invalid runtime variables are auto-healed.
- Startup and periodic sync both work.
- Manual `sync` command applies changes immediately.
- `help` command documents all available serial commands.
