# v8 Firebase Design

## Scope
Firebase Realtime Database is the cloud direction for v8.

## Why Realtime Database For v8
- The device workflow is polling-based and updates small JSON documents frequently.
- Realtime Database is a better fit for command queues, counters, and telemetry snapshots.
- Firestore can still be used for a future dashboard or archival layer, but it is not the first-class device path for this firmware.

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
	"daily": 12,
	"weekly": 48,
	"monthly": 101,
	"updatedAtMs": 1712345678000
}
```

### Telemetry
Telemetry should contain sensor values and a timestamp:
```json
{
	"temperature": 29.4,
	"humidity": 61.2,
	"sentToday": 12,
	"sentWeek": 48,
	"sentMonth": 101,
	"timestamp": 1712345678,
	"updatedAtMs": 1712345678000
}
```

### Runtime Settings
Runtime settings should contain telemetry interval and log verbosity controls:
```json
{
	"intervalOfDhtSeconds": 15,
	"showFirebasePushLogs": true,
	"updatedAtMs": 1712345678000
}
```

If runtime keys are missing or invalid, device firmware should create/heal them with safe defaults and print an operator-facing serial log.

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

### Firestore (reference only, not the v8 device path)
```text
Firestore rules are not used by the current ESP32 firmware path.
If Firestore is chosen later, the data model and code paths must be rewritten.
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
