# Server SMS Flow

Use this file if you are posting pending SMS jobs from a server or dashboard into Firebase Realtime Database for the TTGO T-Call v8 firmware.

## What the server writes
Write pending jobs under `ttgo_tcall/commands/pending`.

A pending SMS job should look like this:
```json
{
  "type": "sms",
  "number": "+923001234567",
  "message": "hello from server",
  "status": "pending",
  "createdAt": 1712345678,
  "createdAtMs": 1712345678000,
  "source": "server"
}
```

## Required fields
- `type`: use `sms`.
- `number`: the target phone number.
- `message`: the text to send.
- `status`: set to `pending`.
- `createdAtMs`: epoch milliseconds.

## Suggested lifecycle
1. Server creates the job with `status: pending`.
2. ESP32 boots, restores counters from `ttgo_tcall/counters`, then waits 60 seconds before polling.
3. ESP32 claims one job by changing it to `processing`.
4. ESP32 updates the job to `sent` or `errored`.
5. Server reads `ttgo_tcall/commands/history` for completed jobs.

## Counter sync
If the server also maintains counts, keep the snapshot under `ttgo_tcall/counters`.

```json
{
  "daily": 12,
  "weekly": 48,
  "monthly": 101,
  "updatedAtMs": 1712345678000
}
```

The device reads this snapshot at boot before it starts polling pending SMS.

## Status nodes
Useful status locations:
- `ttgo_tcall/status`
- `ttgo_tcall/telemetry`
- `ttgo_tcall/commands/history`

## Minimal RTDB rules for development
```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}
```

## Do not do this
- Do not place service-account JSON on the ESP32.
- Do not post jobs with missing phone number or message.
- Do not skip the `pending` status if you want the firmware to claim the job.
- Do not write commands at the RTDB root.
