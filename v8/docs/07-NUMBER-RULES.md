# v8 Number Rules

## Baseline Behavior
- Preserve the v5-style matching behavior.
- Compare allowlists using the last 10 digits.
- Accept modem-extracted numbers in international form when present.
- Mark invalid numbers as errored instead of silently retrying forever.

## Firestore Allowed Numbers
- Outgoing SMS and calls are only allowed when the number exists in `sim_module/allowed_numbers/items`.
- The number document must have `enabled = true`.
- Daily usage is counted from `sim_module/sms_logs/items` and `sim_module/call_logs/items`.
- The device falls back to the device-wide default quota only if the number-specific limit is missing or zero.

## Canonical Form & Doc IDs (v8.2.1)
- Numbers are normalized to `+<countrycode><digits>` (e.g. `+923001234567`). `normalizePhoneNumber()` in `sms_calls.ino` is the reference.
- The allowed-number document id is `safeFirestoreDocumentId(normalizedNumber)`. The dashboard runs the **same** normalization (`normalizePhone()` in `dashboard.js`, country code from `/api/firebase-web-config`) before writing the doc id and the `phone_number` field, so device lookups never miss because of `0300…` vs `+92300…` formatting.

## Block-list Matching (v8.2.1)
- Incoming block-list matching (`numbersMatch`) compares by digits for numeric senders, and **case-insensitively as text** for alphanumeric sender IDs such as `JAZZ` or `Telenor` (which have no digits and previously could never match).

## Acceptance Criteria
- Number handling is deterministic.
- Auth behavior matches legacy expectations.
- Invalid inputs produce a clear status.
