# v8 Rate Limit Design

## Default Limits
- Daily: 200
- Weekly: 950
- Monthly: 4900

Limits live only in RTDB `/ttgo_tcall/settings/runtime` (`dailySmsLimit`,
`weeklySmsLimit`, `monthlySmsLimit`) and are applied on every settings sync.

## Intent
- Enforce limits before sending SMS.
- Keep local counters for short-term resilience.
- Sync with Firebase when connectivity returns.

## Windows Are Calendar Windows
Counters roll over on the calendar, in Pakistan local time (UTC+5), not on
elapsed uptime:

- `sentToday` resets at local midnight.
- `sentWeek` resets on Monday.
- `sentMonth` resets on the 1st.

Each window has a key, stored next to the count:

| Field | Example | Meaning |
|---|---|---|
| `dayKey` | `2026-07-12` | local date the daily count belongs to |
| `weekKey` | `2026-07-06` | local date of that week's Monday |
| `monthKey` | `2026-07` | local month the monthly count belongs to |

A count whose stored key does not match the current window restores as 0, so a
`sentMonth` left over from June is never carried into July. Counters written by
older firmware carry no keys and are treated the same way.

Rollover needs a real clock. Until NTP answers, the window keys stay unset and no
window is reset: uptime seconds are not a date and must never drive a reset.

## Restore Before Write
The stored snapshot is the source of truth for the current window, and the device
will not write counters back until it has read that snapshot:

1. Boot: counters start at 0 and are **not** authoritative.
2. The device fetches `/ttgo_tcall/counters` and adopts it, adding any sends made
   between boot and the fetch on top of the stored totals.
3. Only after a successful restore does the device push counters, on each send
   and again on every window rollover.
4. If Firebase is down at boot, the restore is retried every minute until it lands.

Without step 3, a device that booted while Firebase was unreachable would send one
SMS and overwrite the real monthly total with `1`, which is the symptom reported
in issue #187.

## Offline Behavior
- No cloud: counting continues locally, limits are still enforced, nothing is
  written. Local totals merge into the stored value on the next successful restore.
- No NTP: counting continues, but no window ever resets. Over-counting is the safe
  direction; the counters correct themselves on the first rollover after the clock
  arrives.

## Acceptance Criteria
- Each limit has a clear source and persistence story.
- Offline behavior is defined.
- Counter reconciliation is explicit.
- `sentToday` / `sentWeek` / `sentMonth` survive a reboot.
- A reboot with Firebase down never lowers the stored totals.
- Counters reset at local midnight / Monday / the 1st, not 24 h / 7 d / 30 d after
  boot.
- Each stored count carries the window key it belongs to.
- Limits are enforced before every send, including OTP jobs.
