# v8 Server / App Enqueue Guide

How a backend (Rails, etc.) or the dashboard enqueues SMS and calls, and reads results. The device polls Firestore every 10 s, claims pending work, enforces block lists + global SMS limits, performs the GSM action, and writes the result back.

## How the device drains the queue

- Every 10 s the device runs a server-side query (`runQuery`) for `status == "pending"` only, so finished (`sent`/`failed`/`blocked`) jobs are never downloaded — the queue scales to any size and completed jobs are simply ignored (cleanup, if any, is a separate backend concern, not the device's).
- It grabs up to **5 pending SMS jobs** per batch and sends them **one at a time** with a random **5–30 s gap** between actual sends (anti-SIM-ban pacing). The next batch of 5 is not fetched until the current batch is fully drained.
- Each send is gated by the global rate limit and the SIM package validity; when the daily/weekly/monthly limit is hit, the remaining jobs are left `pending` (for the next window) and an ntfy alert is sent.
- If a batch cannot finish within **5 minutes** (e.g. a wedged send), it is abandoned ("rescue") with an ntfy alert and the unprocessed jobs stay `pending` for re-fetch — the device never gets stuck on one batch.
- Calls are processed one per poll (no anti-ban delay needed for low-volume missed calls).

## Firestore layout (single device)

```
sim_module/
  device                                 (doc)  config + health + block lists + counters
  sms/sms_jobs/{number}                   outgoing SMS queue   (doc id = phone number)
  sms/sms_received/{number}               incoming SMS archive (doc id = sender)
  calls/call_jobs/{number}                outgoing call queue  (doc id = phone number)
  calls/call_received/{number}            incoming call archive
```

There is **one job document per number** (the document id *is* the number). Queueing a new job to a number that already has one overwrites it. There is no allow-list and no per-number quota — any number is allowed unless it is in an outgoing block list.

## Queue an SMS

Create/overwrite `sim_module/sms/sms_jobs/{number}`:

```json
{
  "phone_number": "+923001234567",
  "message": "hello from the app",
  "status": "pending",
  "enque_by": "app:orders",
  "created_at": "<server timestamp>"
}
```

- `phone_number` should be canonical `+<countrycode><number>`. The device normalizes anyway, but the **document id must match** what the device looks up, so use the canonical number as the id.
- `enque_by` is free-form and preserved untouched — use it to link a future reply back to the originating app/user.
- Required: `message`, `status: "pending"`.

## Queue a missed call

Create/overwrite `sim_module/calls/call_jobs/{number}`:

```json
{
  "phone_number": "+923001234567",
  "status": "pending",
  "enque_by": "app:otp",
  "created_at": "<server timestamp>"
}
```

## Status lifecycle (device-managed)

`pending → in_progress → sent` (SMS) or `→ called` (call), or `→ blocked` / `→ failed`.

| status | meaning |
|---|---|
| `pending` | created by the app, waiting to be claimed |
| `in_progress` | claimed by the device, action in progress |
| `sent` | SMS handed to the modem |
| `called` | missed call placed (see `user_picked`, `duration_seconds`) |
| `blocked` | number is in the outgoing block list (`error: "blocked_outgoing"`) or device inactive (`error: "device_inactive"`) |
| `failed` | invalid number, send error, package expired (`error: "package_expired"`), or global SMS rate limit reached (`error` explains) |

Jobs stuck in `in_progress` for more than 5 minutes are reset to `pending` automatically. Rate-limited jobs are left `pending` (not failed) so they retry once the window resets.

## Read incoming

```json
// sim_module/sms/sms_received/{number}
{
  "number": "+923001234567",
  "original_message": "00310020...",        // raw, as received
  "normalized_message": "1 Paise mai 8GB",   // decoded if it was UCS2
  "was_decoded": true,
  "notified": true,    // false when muted by an incoming block list
  "blocked": false,
  "last_received_at": "2026-06-17 23:09:27 PKT"
}

// sim_module/calls/call_received/{number}
{ "number": "...", "notified": true, "blocked": false, "last_received_at": "..." }
```

Incoming SMS bodies are normalized before storage/notification — see [SMS normalization](#sms-normalization).

## Block lists (on `sim_module/device`)

Four string arrays, editable from the dashboard or directly in Firestore:

- `blockedIncomingCallers` / `blockedIncomingSms` — incoming events from these are still archived to `*_received` but do **not** trigger an ntfy notification (`notified: false`).
- `blockedOutgoingCallers` / `blockedOutgoingSms` — jobs to these are marked `blocked` and never sent/dialed.

Entries may be phone numbers or alphanumeric sender ids (e.g. `JAZZ`). On first boot each empty list is seeded with `["JAZZ","000"]` as an editable template. The device refreshes these about once a minute and instantly when **Sync** is pressed.

## SMS normalization

Incoming SMS that arrive as UCS2/UTF-16BE hex are decoded before storing/notifying. Detection: length divisible by 4, all hex, starts `00xx`, and decodes to mostly-printable text. On success the decoded text becomes `normalized_message` (`was_decoded: true`) and is what Firebase + ntfy receive; the raw payload is kept in `original_message`. On failure the original is kept unchanged. Example: `00310020005000610069007300650020006D006100690020003800470042` → `1 Paise mai 8GB`.

## What lives where (no duplication)

- **Firestore `sim_module/device`** — identity (`name`, `active`), `poll_interval_seconds`, `missed_call_mode`, the 4 block-list arrays, lifetime counters (`totalSmsSent`, `totalSmsReceived`, `totalSmsBlockedOutgoing`, `totalSmsMutedIncoming`, `totalCallsMade`, `totalCallsReceived`, `totalCallsBlockedOutgoing`, `totalCallsMutedIncoming`), and health (`battery_percent`, `signal_strength`, `network_operator`, `last_seen_epoch`).
- **RTDB `/ttgo_tcall/settings/runtime`** — operational toggles and the **SMS rate limits** (`dailySmsLimit`, `weeklySmsLimit`, `monthlySmsLimit`), DHT interval, log flags, ntfy URL, WiFi pairs. SMS limits live **only** here — never duplicated in Firestore.
- **RTDB `/ttgo_tcall/counters`** — rolling daily/weekly/monthly *rate-limit windows* (reset on rollover). Distinct from the Firestore lifetime totals above.
