# v8 Number Rules

## Baseline Behavior
- Preserve the v5-style matching behavior.
- Compare allowlists using the last 10 digits.
- Accept modem-extracted numbers in international form when present.
- Mark invalid numbers as errored instead of silently retrying forever.

## Acceptance Criteria
- Number handling is deterministic.
- Auth behavior matches legacy expectations.
- Invalid inputs produce a clear status.
