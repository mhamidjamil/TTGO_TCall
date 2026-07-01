# v8 Logging Standard

## Tags
- BOOT
- MODEM
- WIFI
- CONFIG
- FIREBASE
- RATE_LIMIT
- SMS
- CALL
- API
- THINGSPEAK
- JOB (queue claim/validation/send/dial lifecycle)
- LOG (mirrored to the `ttgo_stuff` ntfy log channel)
- PACKAGE (SIM subscription detection/expiry)

## ntfy log channel
Job lifecycle and error events are pushed to a second ntfy topic (`ttgo_stuff`,
URL from `NTFY_LOG_URL_DEFAULT` / RTDB `ntfyLogUrl`) in addition to serial, via
`pushLog(title, message)`. This is separate from the user-facing `oracle_ntfy`
topic. It is best-effort (no-ops when the URL is unset or WiFi is down) and chatty
by design — subscribe and mute. Events include: pending batch received, per-message
processing/sent/failed, rate-limit hit, batch rescue, and boot.

## Rules
- One meaningful line per event.
- No noisy repetition.
- Errors must explain what failed and what happens next.
- `showFirebasePushLogs` controls recurring success logs for telemetry and landing snapshot.
- Runtime setting create/heal events must always be visible on serial.
- ThingSpeak upload errors and success events should use the `THINGSPEAK` tag.

## Acceptance Criteria
- Logs are readable at a glance.
- Startup summary is always printed.
- Debug output is structured and professional.
- Operator can disable recurring Firebase success logs from cloud settings.
