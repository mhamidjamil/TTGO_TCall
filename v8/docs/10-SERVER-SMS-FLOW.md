# Server Job Flow

Use this file when posting outgoing SMS or call jobs into Firestore for the TTGO T-Call v8 firmware.

## Firestore Collections
- `sim_modules/{deviceId}` stores the device record and health metrics.
- `allowed_numbers/{phone_number_safe_id}` stores the permission and quota policy for each target number.
- `sms_jobs/{auto_id}` stores outgoing SMS work.
- `call_jobs/{auto_id}` stores outgoing call work.
- `sms_logs/{auto_id}` stores all SMS audit events.
- `call_logs/{auto_id}` stores all call audit events.

## How To Queue SMS
Create a document in `sms_jobs`:

```json
{
  "device_id": "device_001",
  "phone_number": "+923001234567",
  "message": "hello from server",
  "status": "pending",
  "created_at": "timestamp",
  "processing_started_at": null,
  "completed_at": null,
  "error": null
}
```

Required fields:
- `device_id`: target device identifier.
- `phone_number`: the target number.
- `message`: SMS body text.
- `status`: set to `pending`.

## How To Queue Calls
Create a document in `call_jobs`:

```json
{
  "device_id": "device_001",
  "phone_number": "+923001234567",
  "status": "pending",
  "created_at": "timestamp",
  "processing_started_at": null,
  "completed_at": null,
  "user_picked": false,
  "duration_seconds": 0,
  "error": null
}
```

## Suggested Lifecycle
1. Server or dashboard creates the job with `status: pending`.
2. ESP32 polls Firestore every 3 seconds and claims at most one SMS job and one call job per cycle.
3. ESP32 changes a claimed job to `processing`.
4. ESP32 finishes with `sent`, `completed`, `failed`, `blocked`, or `quota_exceeded`.
5. ESP32 writes the matching audit log document and updates device heartbeat separately.

## Allowed Number and Quota Rules
- The target number must exist in `allowed_numbers`.
- `enabled` must be `true`.
- Per-number daily usage is counted from successful outgoing logs using `day_key`.
- SMS quota counts `sms_logs` documents with `status = sent`.
- Call quota counts `call_logs` documents with `status = completed`.

## Recovery
- Jobs stuck in `processing` for more than 5 minutes are reset to `pending`.
- The reset writes `error = recovered_from_stale_processing`.

## Do Not Do This
- Do not place service-account JSON on the ESP32.
- Do not write jobs without `device_id`, `phone_number`, or `status = pending`.
- Do not skip `allowed_numbers` if you expect the device to send or call.
- Do not rely on the old RTDB command queue for the new gateway flow.
