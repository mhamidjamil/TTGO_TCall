# v8 Runtime Settings Sync

## Purpose
Allow selected runtime behavior to be controlled from Firebase Realtime Database without reflashing firmware.

## Paths
- Runtime settings: `/ttgo_tcall/settings/runtime`

## Runtime Variables
- `intervalOfDhtSeconds` (number)
  - Meaning: telemetry push interval in seconds.
  - Default: `60`
  - Guardrail: clamp or heal invalid values to safe default. A hard **60 s floor**
    is enforced in firmware (`kMinTelemetryIntervalMs`) — telemetry/ThingSpeak are
    never pushed more often than once per minute, regardless of a lower value here.
- `pushDhtToFirebase` (boolean)
  - Meaning: mirror the DHT reading into RTDB `/ttgo_tcall/telemetry`. When false
    the device writes nothing to that node; the reading still goes to the OLED,
    the local dashboard and ThingSpeak.
  - Default: `false`. It is off by default because a per-minute write of a value
    that is already published elsewhere was the largest single source of Realtime
    Database traffic. See [05-FIREBASE-DESIGN.md](05-FIREBASE-DESIGN.md).
- `connectedSsid` (string, device-written)
  - Meaning: the SSID the device is currently connected to (STA). Written by the
    device on each sync; empty/absent when in AP/OFFLINE mode. Read-only for operators.
- `showFirebasePushLogs` (boolean)
  - Meaning: controls periodic serial info logs for telemetry and landing snapshot push success.
  - Default: `true`
- `jobLogs` (boolean)
  - Meaning: controls serial logs for Firestore SMS/call job claiming, validation, quota checks, and final status.
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
- `ntfyUrl` (string)
  - Meaning: ntfy topic URL for user-facing notifications (incoming SMS/calls, package events).
  - Default: from `secrets.h` `NTFY_URL_DEFAULT`. Real topic URL lives only in the gitignored `secrets.h` — never commit it, since an ntfy topic name is a bearer credential (anyone who knows it can publish to or subscribe to the channel), and this channel carries WiFi passwords on save.
- `ntfyLogUrl` (string)
  - Meaning: ntfy topic URL for the operational log/error channel (job lifecycle, rate-limit/rescue alerts, boot) **and** for incoming SMS from a sender on `blockedIncomingSms`. Ignore-listed senders are routed here instead of `ntfyUrl`, never silenced: the message is still decoded, parsed for a package subscription, archived to Firestore and deleted from the SIM. Chatty - subscribe + mute.
  - Default: from `secrets.h` `NTFY_LOG_URL_DEFAULT`. Real topic URL lives only in `secrets.h`.

## Firestore Block Lists
- Stored on `sim_module/device`: `blockedIncomingCallers`, `blockedIncomingSms`, `blockedOutgoingCallers`, `blockedOutgoingSms`.
- Refreshed on startup, on every settings `sync`, and automatically about once a minute, so changes apply without a reboot.
- The same read also caches the `active` switch. Job processing uses that cached
  value instead of re-reading the device document once per job, so switching the
  gateway off applies within about a minute rather than instantly.

## Package State
- Stored at `/ttgo_tcall/package` (separate node). Holds the subscription expiry
  as a single plain-text `expiresAt` ("2026-09-28 01:44 PKT") plus the detection
  `matchTokens` and `safetyMarginDays`, all cloud-tunable.
- See `v8/docs/14-PACKAGE-SUBSCRIPTION.md` for the full contract.

## Sync Strategy
1. Startup: device fetches runtime settings and uses the Firebase value when it already exists.
2. If missing/invalid: device creates or heals only the missing key in Firebase using the default.
3. Periodic: device refreshes runtime settings every 5 minutes.
4. Manual: serial command `sync` forces immediate refresh, including Firestore block lists.

## Operator Visibility
- If a runtime variable is created or healed, print a clear serial message.
- If a runtime variable changes, print old value and new value in a readable block.
- If block-list counts change, print old and new counts.
- The Firebase root should stay folder-only; runtime settings belong under `/ttgo_tcall/settings/runtime`.

## Acceptance Criteria
- Existing runtime variables are never overwritten by startup defaults.
- Missing runtime variables are auto-created.
- `pushDhtToFirebase` is created as `false` when absent, and no telemetry is
  written while it is false.
- Invalid runtime variables are auto-healed.
- `ntfyUrl` can be changed without reflashing.
- Firestore block lists sync on startup, periodically, and through `sync`.
- Startup and periodic sync both work.
- Manual `sync` command applies changes immediately.
- `help` command documents all available serial commands, including `show sms`, `drain sms`, `delete sms <index>`, and `delete all sms`.
