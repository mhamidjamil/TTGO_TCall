# v8 Number Rules

## Baseline Behavior
- Preserve the v5-style matching behavior.
- Compare allowlists using the last 10 digits.
- Accept modem-extracted numbers in international form when present.
- Mark invalid numbers as errored instead of silently retrying forever.

## Firestore Allowed Numbers
- Outgoing SMS and calls are only allowed when the number exists in `allowed_numbers`.
- The number document must have `enabled = true`.
- Daily usage is counted from `sms_logs` and `call_logs`.
- The device falls back to the device-wide default quota only if the number-specific limit is missing or zero.

## Acceptance Criteria
- Number handling is deterministic.
- Auth behavior matches legacy expectations.
- Invalid inputs produce a clear status.
