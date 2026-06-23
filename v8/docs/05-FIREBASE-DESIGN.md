# v8 Firebase Design

## Scope
Firestore is the primary data plane for the GSM gateway. It stores device health, allowed numbers, outgoing job queues, audit logs, quotas by log count, and SIM module archives.

Firebase Realtime Database remains in v8 only for legacy telemetry snapshots, runtime settings, and counter compatibility.

## Initial Design Intent
- Use a polling-based command model.
- Keep command processing offline-safe.
- Claim work atomically to avoid duplicate sends.
- Preserve local fallback behavior when internet is unavailable.
- Keep device-side configuration isolated from any service-account material.
- Server posting details live in [10-SERVER-SMS-FLOW.md](10-SERVER-SMS-FLOW.md). Queue writers create number-keyed Firestore jobs under `sim_module/sms/sms_jobs/{number}` and `sim_module/calls/call_jobs/{number}`.

## What The ESP32 Actually Uses
- WiFi STA mode means the ESP32 is acting as a client connecting to your router.
- AP mode means the ESP32 creates its own hotspot when normal WiFi fails.
- Firebase auth on the device uses Firebase Authentication, not the service-account JSON pasted for server-side use.
- The service-account JSON belongs only on a backend or local admin tool, never on the ESP32.

## Proposed Paths
- Legacy RTDB commands: `/ttgo_tcall/commands/pending`
- Legacy RTDB command history: `/ttgo_tcall/commands/history`
- Counters: `/ttgo_tcall/counters`
- Device status: `/ttgo_tcall/status`
- Telemetry: `/ttgo_tcall/telemetry`
- Runtime settings + SMS limits + WiFi: `/ttgo_tcall/settings/runtime`
- Firestore device doc (config, health, block lists, counters): `sim_module/device`
- Firestore SMS jobs: `sim_module/sms/sms_jobs/{number}`
- Firestore SMS received: `sim_module/sms/sms_received/{number}`
- Firestore call jobs: `sim_module/calls/call_jobs/{number}`
- Firestore calls received: `sim_module/calls/call_received/{number}`

There is no allow-list and no per-number quota: outgoing control is by block list only. The full field list and app enqueue contract live in [10-SERVER-SMS-FLOW.md](10-SERVER-SMS-FLOW.md).

## Folder-Only Shape
The root `ttgo_tcall` node is now treated as a container for folders only. Leaf variables should live under their parent folders.

### Counters
```json
{
	"sentToday": 12,
	"sentWeek": 48,
	"sentMonth": 101,
	"updatedAtMs": 1712345678000
}
```

### Status
```json
{
	"bootTime": "12345ms",
	"wifiMode": "STA",
	"ipAddress": "192.168.0.106",
	"firebaseReady": true,
	"telemetryPushOk": true,
	"telemetryMessage": "Telemetry pushed",
	"updatedAtMs": 1712345678000
}
```

### Telemetry
```json
{
	"temperature": 29.4,
	"humidity": 61.2,
	"timestamp": 1712345678,
	"updatedAtMs": 1712345678000
}
```

### Runtime Settings
```json
{
	"intervalOfDhtSeconds": 15,
	"showFirebasePushLogs": true,
	"jobLogs": true,
	"dailySmsLimit": 200,
	"weeklySmsLimit": 950,
	"monthlySmsLimit": 4900,
	"ntfyUrl": "https://ntfy.innovorix.com/oracle_ntfy",
	"wifiSsid1": "",
	"wifiPass1": "",
	"wifiSsid2": "",
	"wifiPass2": "",
	"updatedAtMs": 1712345678000
}
```

WiFi pairs are optional and managed from the dashboard WiFi tab. The device reads them on sync and persists them to SPIFFS; they apply on the next reboot. Empty values are ignored.

## Firestore Gateway Data Model

`sim_module` has exactly three children: `device`, `sms`, `calls`.

### device (doc: `sim_module/device`)
- Identity/config: `name`, `active` (master send switch), `poll_interval_seconds`, `missed_call_mode`, `source`.
- Health: `battery_percent`, `signal_strength`, `network_operator`, `last_seen_at`, `last_seen_epoch`.
- Block lists (string arrays, seeded `["JAZZ","000"]` when empty): `blockedIncomingCallers`, `blockedIncomingSms`, `blockedOutgoingCallers`, `blockedOutgoingSms`.
- Lifetime counters (ints, atomically incremented): `totalSmsSent`, `totalSmsReceived`, `totalSmsBlockedOutgoing`, `totalSmsMutedIncoming`, `totalCallsMade`, `totalCallsReceived`, `totalCallsBlockedOutgoing`, `totalCallsMutedIncoming`.
- **SMS limits are NOT here** — they live only in RTDB runtime settings.

### sms (doc: `sim_module/sms`)
- `sms_jobs/{number}` — outgoing queue, one doc per number: `phone_number`, `message`, `status`, `enque_by`, `error`, timestamps.
- `sms_received/{number}` — incoming archive: `number`, `original_message`, `normalized_message`, `was_decoded`, `notified`, `blocked`, `last_received_at`.

### calls (doc: `sim_module/calls`)
- `call_jobs/{number}` — outgoing missed-call queue: `phone_number`, `status`, `user_picked`, `duration_seconds`, `enque_by`, `error`, timestamps.
- `call_received/{number}` — incoming archive: `number`, `notified`, `blocked`, `last_received_at`.

Full field reference and the app enqueue contract: [10-SERVER-SMS-FLOW.md](10-SERVER-SMS-FLOW.md).

### Queue processing
Each poll cycle the device claims one pending SMS job and one pending call job (`pending → in_progress`), validates `device.active` and the outgoing block lists, enforces the global SMS rate limit (RTDB), performs the GSM action, then writes the final status (`sent`/`called`/`blocked`/`failed`) back to the job doc. Jobs stuck in `in_progress` > 5 minutes are reset to `pending`.

## Legacy RTDB Data Model

### Pending Commands
Each pending command should look like this:
```json
{
	"type": "sms",
	"number": "+923001234567",
	"message": "test message",
	"status": "pending",
	"createdAt": 1712345678,
	"createdAtMs": 1712345678000,
	"source": "web"
}
```

### Command Lifecycle
1. `pending` - created by the server or dashboard.
2. `processing` - claimed by the ESP32.
3. `sent` - SMS sent successfully.
4. `errored` - rejected or failed, with `errorReason`.

New implementations should prefer the Firestore `sim_module/sms/sms_jobs` queue; this RTDB command path is retained only for older tooling.

### Counters
Store counters as a single snapshot object:
```json
{
	"sentToday": 12,
	"sentWeek": 48,
	"sentMonth": 101,
	"updatedAtMs": 1712345678000
}
```

### Telemetry
Telemetry should contain sensor values and a timestamp.

### Runtime Settings
Runtime settings should contain telemetry interval, log verbosity, SMS limit controls, and `ntfyUrl`.

If runtime keys are missing or invalid, device firmware should create/heal them with safe defaults and print an operator-facing serial log.

### Firestore SIM Module
See [Firestore Gateway Data Model](#firestore-gateway-data-model) above for the current `device` / `sms` / `calls` schema. Firestore paths must alternate collection/document, which is why jobs live under the `sms`/`calls` container docs (`sim_module/sms/sms_jobs/{number}`) rather than directly under `sim_module`.

## Device Auth Direction
- Realtime Database access should use Firebase Authentication from the device.
- Default device mode in v8 is anonymous auth for quick bootstrapping.
- If anonymous auth is disabled or you need tighter control, switch to email/password in `secrets.h` and set `FIREBASE_USE_ANONYMOUS_DEFAULT` to `0`.
- Service account JSON stays server-side only and must never move onto the ESP32.

## Firebase Console Checklist
1. Create or open the Firebase project `myacademiaapp`.
2. Enable Realtime Database.
3. Choose a region and create the database.
4. Enable Authentication.
5. Enable Anonymous sign-in if you want the current device default to work immediately.
6. If you prefer email/password auth, create a dedicated device user and set it in `secrets.h`.
7. Add the RTDB rules you want for testing.
8. Verify the device can read and write the `ttgo_tcall` subtree.

## Recommended Test Rules

### Realtime Database (development only)
```json
{
	"rules": {
		".read": "auth != null",
		".write": "auth != null"
	}
}
```

### Firestore
```text
Allow authenticated device users to read/write the sim_module collection during development.
Tighten this rule for production device accounts.
```

## What `auth failed code=400` Usually Means
- Anonymous auth is disabled in Firebase Authentication.
- The API key is restricted or invalid for this project.
- The database URL does not match the active Firebase project.
- Email/password auth was selected but the credentials are empty or incorrect.

## What `WIFI STA` Means
- `STA` means station mode.
- The module is connected to an existing router instead of hosting its own hotspot.
- If STA fails, the firmware should fall back to AP mode so you can still reach the local web UI.

## Debug Checklist
1. Confirm the ESP32 can reach the internet from the selected WiFi network.
2. Confirm Firebase Authentication is enabled.
3. Confirm anonymous sign-in is enabled, or provide device email/password.
4. Confirm the RTDB URL is exactly the project database URL.
5. Confirm the API key belongs to the same Firebase project.
6. Confirm the device can write a simple test node under `/ttgo_tcall/test`.
7. If auth fails, inspect the HTTP response body for Firebase error messages.

## Acceptance Criteria
- Polling workflow is defined before code starts.
- Command status transitions are explicit.
- Duplicate-send prevention is documented.
- Paths are deterministic and ready for implementation.
- Setup, rules, and debug steps are complete enough to onboard a new device from scratch.
- Runtime settings path and healing behavior are documented.
