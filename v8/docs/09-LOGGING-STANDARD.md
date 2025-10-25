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
