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
- Server posting details live in [10-SERVER-SMS-FLOW.md](10-SERVER-SMS-FLOW.md). Queue writers should create Firestore jobs under `sim_module/sms_jobs/items` and `sim_module/call_jobs/items`.

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
- Runtime settings: `/ttgo_tcall/settings/runtime`
- Firestore device/settings document: `sim_module/config`
- Firestore allowed numbers: `sim_module/allowed_numbers/items`
- Firestore SMS jobs: `sim_module/sms_jobs/items`
- Firestore call jobs: `sim_module/call_jobs/items`
- Firestore SMS logs: `sim_module/sms_logs/items`
- Firestore call logs: `sim_module/call_logs/items`
- Firestore SIM module archive paths: `sim_module/sms/by_number` and `sim_module/calls/by_number`

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

### Single Module Shape
This project currently uses one TTGO module. Gateway state lives under the existing `sim_module` collection to avoid multiple competing roots.

### Config Document
Path: `sim_module/config` — gateway policy and editable lists.

```json
{
	"name": "TTGO Bedroom",
	"active": true,
	"poll_interval_seconds": 3,
	"missed_call_mode": true,
	"daily_sms_default_limit": 5,
	"daily_call_default_limit": 5,
	"blockedCallers": ["JAZZ"],
	"blockedSmsSenders": ["JAZZ"]
}
```

`active = false` blocks pending outgoing jobs. There is no per-device identity — this is a single-device gateway, so any pending job is processed.

### Device Document
Path: `sim_module/device` — heartbeat / health, written by the firmware.

```json
{
	"name": "TTGO Bedroom",
	"active": true,
	"last_seen_at": "timestamp",
	"last_seen_epoch": 1780000000,
	"signal_strength": 22,
	"battery_percent": 84,
	"network_operator": "Jazz",
	"poll_interval_seconds": 3,
	"missed_call_mode": true
}
```

### Allowed Numbers
Path: `sim_module/allowed_numbers/items/{safe_phone_number}`

```json
{
	"phone_number": "+923001111111",
	"enabled": true,
	"sms_limit_per_day": 5,
	"call_limit_per_day": 5,
	"notes": "Owner",
	"updated_at": "timestamp"
}
```

Document IDs are sanitized the same way as firmware: letters, digits, `+`, `_`, and `-` are preserved; other characters become `_`.

### SMS Jobs
Path: `sim_module/sms_jobs/items/{auto_id}`

```json
{
	"phone_number": "+923001111111",
	"message": "Hello",
	"status": "pending",
	"created_at": "timestamp",
	"processing_started_at": null,
	"completed_at": null,
	"error": null
}
```

Statuses: `pending`, `processing`, `sent`, `failed`, `blocked`, `quota_exceeded`.

### Call Jobs
Path: `sim_module/call_jobs/items/{auto_id}`

```json
{
	"phone_number": "+923001111111",
	"status": "pending",
	"created_at": "timestamp",
	"processing_started_at": null,
	"completed_at": null,
	"user_picked": false,
	"duration_seconds": 0,
	"error": null
}
```

Statuses: `pending`, `processing`, `completed`, `failed`, `blocked`, `quota_exceeded`.

### SMS Logs
Path: `sim_module/sms_logs/items/{auto_id}`

```json
{
	"direction": "outgoing",
	"phone_number": "+923001111111",
	"message": "Hello",
	"status": "sent",
	"error": "",
	"timestamp": "timestamp",
	"timestamp_epoch": 1780000000,
	"day_key": "2026-06-16"
}
```

Directions: `incoming`, `outgoing`.

### Call Logs
Path: `sim_module/call_logs/items/{auto_id}`

```json
{
	"direction": "outgoing",
	"phone_number": "+923001111111",
	"duration_seconds": 8,
	"answered": false,
	"status": "completed",
	"error": "",
	"timestamp": "timestamp",
	"timestamp_epoch": 1780000000,
	"day_key": "2026-06-16"
}
```

Directions: `incoming`, `outgoing`.

## Queue Processing
Every poll cycle:

1. Fetch one pending SMS job.
2. Fetch one pending call job.
3. Validate `sim_module/config.active`.
4. Validate `sim_module/allowed_numbers/items/{phone_number}` exists and is enabled.
5. Count successful outgoing logs for the current `day_key`.
6. Execute the GSM action.
7. Write an audit log.
8. Update final job status.

The firmware keeps one polling cycle; it does not create separate SMS/call intervals.

### Stuck Job Recovery
Once per minute, the device scans `sim_module/sms_jobs/items` and `sim_module/call_jobs/items`. Any `processing` job with `processing_started_epoch` older than 5 minutes is reset to `pending` and marked with `error = recovered_from_stale_processing`.

### Quotas
Daily quota is counted from logs, not a separate counter collection:

- SMS usage: successful outgoing `sim_module/sms_logs/items` with `status = sent`.
- Call usage: successful outgoing `sim_module/call_logs/items` with `status = completed`.
- Reset: automatic through `day_key`, based on UTC date from device time.

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

New implementations should prefer Firestore `sim_module/sms_jobs/items`; this RTDB path is retained for older tooling.

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
Firestore collection `sim_module` stores one simple settings document, nested gateway collections, and phone-number-based event documents.

- `sim_module/config` - device state and editable block lists.
- `sim_module/allowed_numbers/items` - outgoing permission and quotas.
- `sim_module/sms_jobs/items` - outgoing SMS queue.
- `sim_module/call_jobs/items` - outgoing call queue.
- `sim_module/sms_logs/items` - SMS audit trail.
- `sim_module/call_logs/items` - call audit trail.
- `sim_module/sms/by_number/{sender_or_number}` - latest SMS details for that sender.
- `sim_module/calls/by_number/{caller_number}` - latest call details for that caller.

`sim_module/config` should contain:

```json
{
	"active": true,
	"blockedCallers": ["+923001234567", "100"],
	"blockedSmsSenders": ["Jazz", "samosa", "100"]
}
```

Event documents contain `type`, `number`, `documentId`, `message`, `blocked`, `source`, `pakistanTime`, `receivedAtPakistan`, `updatedAtPakistan`, and `uptimeSeconds`. SMS documents also include `simIndex` when the message came from SIM memory.

Firestore paths must alternate collection/document. Because of that, `sim_module/sms/{number}` is not a valid document path, so the firmware uses `sim_module/sms/by_number/{number}`.

On startup, firmware creates `sim_module/config`, `sim_module/sms`, and `sim_module/calls` if missing. It also deletes legacy documents under `entries`, `numbers`, `_meta`, blocked buckets, and old top-level gateway collections.

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
