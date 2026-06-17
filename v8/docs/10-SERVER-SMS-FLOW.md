# Server Job Flow

Use this when a Rails app or another backend queues outgoing SMS/call work for the TTGO T-Call v8 firmware.

## Firestore Paths
- Device and policy document: `sim_module/settings`
- Allowed numbers: `sim_module/settings/allowed_numbers/{safePhoneNumber}`
- SMS jobs: `sim_module/settings/sms_jobs/{autoId}`
- Call jobs: `sim_module/settings/call_jobs/{autoId}`
- SMS logs: `sim_module/settings/sms_logs/{autoId}`
- Call logs: `sim_module/settings/call_logs/{autoId}`

Do not write new gateway jobs to top-level `sms_jobs`, `call_jobs`, `allowed_numbers`, or `sim_modules`. Those were removed from the active flow.

## Required Policy Before Sending
Every outgoing target must exist in `sim_module/settings/allowed_numbers`:

```json
{
  "phone_number": "+923354888420",
  "enabled": true,
  "sms_limit_per_day": 5,
  "call_limit_per_day": 5,
  "notes": "Owner"
}
```

If a job finishes as `blocked` with `error = number_not_allowed`, the number was missing from this collection or `enabled` was false. Add the number and retry the job.

## Queue SMS
Create a document in `sim_module/settings/sms_jobs`:

```json
{
  "device_id": "device_001",
  "phone_number": "+923354888420",
  "message": "hello from Rails",
  "status": "pending",
  "created_at": "timestamp",
  "processing_started_at": null,
  "completed_at": null,
  "error": null
}
```

Required fields:
- `phone_number`
- `message`
- `status = pending`

`device_id` can stay `device_001` for the single-module setup.

## Queue Call
Create a document in `sim_module/settings/call_jobs`:

```json
{
  "device_id": "device_001",
  "phone_number": "+923354888420",
  "status": "pending",
  "created_at": "timestamp",
  "processing_started_at": null,
  "completed_at": null,
  "user_picked": false,
  "duration_seconds": 0,
  "error": null
}
```

## Status Flow
- `pending`: queued by Rails or dashboard.
- `processing`: claimed by the ESP32.
- `sent`: SMS sent.
- `completed`: call attempt completed.
- `failed`: modem, lookup, or execution failure.
- `blocked`: device inactive or number not allowed.
- `quota_exceeded`: daily per-number limit reached.

## Logs
Successful and failed execution attempts are written to:

```text
sim_module/settings/sms_logs
sim_module/settings/call_logs
```

Daily quota checks count successful outgoing logs for the current `day_key`.

## Runtime Settings
Runtime settings stay in Realtime Database:

```text
ttgo_tcall/settings/runtime
```

Supported keys include `intervalOfDhtSeconds`, `showFirebasePushLogs`, `showThingSpeakPushLogs`, `dailySmsLimit`, `weeklySmsLimit`, `monthlySmsLimit`, `ntfyUrl`, and `jobLogs`.

`jobLogs = true` makes the ESP32 print serial lines such as job claim, validation result, quota result, send/dial attempt, and final status. Set it false to quiet those job logs.

## Test Checklist
1. Add the target number under `sim_module/settings/allowed_numbers`.
2. Confirm `sim_module/settings.active` is true.
3. Set `ttgo_tcall/settings/runtime/jobLogs` to true.
4. Create one SMS or call job with `status = pending`.
5. Watch Serial for `[JOB]` lines.
6. Confirm the job leaves `pending`.
7. Confirm a matching log appears under `sms_logs` or `call_logs`.

## Rails Implementation Notes
- Use server timestamps for `created_at`.
- Keep phone numbers normalized to E.164 where possible.
- Use the same safe document ID rule for allowed numbers: keep letters, digits, `+`, `_`, `-`; replace other characters with `_`.
- Never place Firebase service-account JSON on the ESP32.
