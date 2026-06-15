# v8 Firebase Design

## Scope
Firebase Realtime Database remains the command/settings path for v8. Firestore is now also used for SIM module call/SMS archives and block lists.

## Why Realtime Database For Commands
- The device workflow is polling-based and updates small JSON documents frequently.
- Realtime Database is a better fit for command queues, counters, and telemetry snapshots.
- Firestore is used where append-only SIM event history and editable block-list documents are a better fit.

## Initial Design Intent
- Use a polling-based command model.
- Keep command processing offline-safe.
- Claim work atomically to avoid duplicate sends.
- Preserve local fallback behavior when internet is unavailable.
- Keep device-side configuration isolated from any service-account material.
- Server posting details live in [10-SERVER-SMS-FLOW.md](10-SERVER-SMS-FLOW.md).

## What The ESP32 Actually Uses
- WiFi STA mode means the ESP32 is acting as a client connecting to your router.
- AP mode means the ESP32 creates its own hotspot when normal WiFi fails.
- Firebase auth on the device uses Firebase Authentication, not the service-account JSON pasted for server-side use.
- The service-account JSON belongs only on a backend or local admin tool, never on the ESP32.

## Proposed Paths
- Commands: `/ttgo_tcall/commands/pending`
- Command history: `/ttgo_tcall/commands/history`
- Counters: `/ttgo_tcall/counters`
- Device status: `/ttgo_tcall/status`
- Telemetry: `/ttgo_tcall/telemetry`
- Runtime settings: `/ttgo_tcall/settings/runtime`
- Firestore SIM module collection: `sim_module`

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
	"dailySmsLimit": 200,
	"weeklySmsLimit": 950,
	"monthlySmsLimit": 4900,
	"ntfyUrl": "https://ntfy.innovorix.com/oracle_ntfy",
	"updatedAtMs": 1712345678000
}
```

## Data Model

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
Firestore collection `sim_module` stores call/SMS archives and editable block lists:

- `sim_module/calls/entries/{autoId}` - unblocked call events.
- `sim_module/sms/entries/{autoId}` - unblocked SMS events.
- `sim_module/blocked_calls/entries/{autoId}` - blocked call events.
- `sim_module/blocked_sms/entries/{autoId}` - blocked SMS events.
- `sim_module/blocked_callers/numbers/{docId}` - caller numbers that should not trigger ntfy.
- `sim_module/blocked_sms_senders/numbers/{docId}` - SMS sender numbers that should not trigger ntfy.

Event documents contain `type`, `number`, `message`, `blocked`, `source`, `timestamp`, and `updatedAtMs`.

Block-list documents should contain:

```json
{
	"number": "+923001234567",
	"enabled": true
}
```

If `number` is missing, the firmware treats the Firestore document ID as the number.
For compatibility with operator-created folders, the firmware also reads `sim_module/blocked_caller/numbers/{docId}` and `sim_module/blocked_sms_sender/numbers/{docId}`.

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
