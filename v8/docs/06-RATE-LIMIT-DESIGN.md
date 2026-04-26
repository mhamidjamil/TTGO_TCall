# v8 Rate Limit Design

## Default Limits
- Daily: 200
- Weekly: 950
- Monthly: 4900

## Intent
- Enforce limits before sending SMS.
- Keep local counters for short-term resilience.
- Sync with Firebase when connectivity returns.

## Acceptance Criteria
- Each limit has a clear source and persistence story.
- Offline behavior is defined.
- Counter reconciliation is explicit.
