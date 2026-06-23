# v8 Number Rules

## Baseline Behavior
- Preserve the v5-style matching behavior.
- Compare allowlists using the last 10 digits.
- Accept modem-extracted numbers in international form when present.
- Mark invalid numbers as errored instead of silently retrying forever.

## Outgoing Control (block-list model)
- There is no allow-list. Outgoing SMS/calls are sent to any number **unless** it is in `device.blockedOutgoingSms` / `device.blockedOutgoingCallers`, in which case the job is marked `blocked` (`error = blocked_outgoing`).
- The global SMS rate limit (`dailySmsLimit` / `weeklySmsLimit` / `monthlySmsLimit` in RTDB runtime settings) still applies; exceeding it fails the job with `error = rate_limited`. There are no per-number quotas.

## Canonical Form & Doc IDs (v8.2.1)
- Numbers are normalized to `+<countrycode><digits>` (e.g. `+923001234567`). `normalizePhoneNumber()` in `sms_calls.ino` is the reference.
- The allowed-number document id is `safeFirestoreDocumentId(normalizedNumber)`. The dashboard runs the **same** normalization (`normalizePhone()` in `dashboard.js`, country code from `/api/firebase-web-config`) before writing the doc id and the `phone_number` field, so device lookups never miss because of `0300…` vs `+92300…` formatting.

## Block-list Matching (v8.2.1)
- Incoming block-list matching (`numbersMatch`) compares by digits for numeric senders, and **case-insensitively as text** for alphanumeric sender IDs such as `JAZZ` or `Telenor` (which have no digits and previously could never match).

## Acceptance Criteria
- Number handling is deterministic.
- Auth behavior matches legacy expectations.
- Invalid inputs produce a clear status.
