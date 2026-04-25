# v8 Firebase Design

## Scope
Firebase is the cloud direction for v8.

## Initial Design Intent
- Use a polling-based command model.
- Keep command processing offline-safe.
- Claim work atomically to avoid duplicate sends.
- Preserve local fallback behavior when internet is unavailable.

## Open Items
- Final Firebase product choice.
- Final command path or collection layout.
- Device auth method.
- Counter storage layout.

## Acceptance Criteria
- Polling workflow is defined before code starts.
- Command status transitions are explicit.
- Duplicate-send prevention is documented.
