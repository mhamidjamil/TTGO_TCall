# v8 SIM Package Subscription Tracking

Auto-detects the operator's package-subscription SMS, tracks the expiry, blocks
outgoing SMS once the package has lapsed, and reminds the operator before expiry.
Owned by `PackageManager` (`PackageManager.h/.cpp`).

---

## Why

A prepaid SIM only sends SMS while an SMS bundle is active. When the bundle
expires the modem silently fails every send. Instead of letting jobs pile up as
`send_failed`, the device learns the expiry from the operator's own confirmation
SMS and refuses to send after it lapses — and pings you (ntfy) a day before.

---

## Detection (on-device, no external AI)

When any SMS arrives, the message is checked against a **configurable token list**.
Detection succeeds only when **every** token is present (case-insensitive) **and**
a validity-day count can be extracted.

- **`matchTokens`** — comma-separated substrings that must all appear.
  Default: `been subscribed,SMS,validity`. Edit this in Firebase (see below) when
  the operator changes wording — no reflash needed.
- **Validity days** — the number immediately before `day` is extracted, robust to
  `30day`, `30 day`, and `30days`.
- **SMS allowance** — the number immediately before `SMS` is extracted (e.g. `10000`).

Example (Jazz):

```
Your Offer with 0 Onnet Mins 0 Other Network Mins 0 MBs 10000 SMS with 30day
validity has been subscribed for Rs. Status:*303*30#
```

→ tokens `been subscribed` / `SMS` / `validity` all present, `validityDays = 30`,
`smsAllowance = 10000` → **detected**.

On detection the device sends an ntfy notification with the extracted values and a
confidence score, e.g.:

```
Package detected. validity=30d sms=10000 confidence=100% (3/3 tokens) daysLeft=29
```

> **Changing the expected values later:** if the operator switches from 10000 to
> 5000 SMS, nothing needs to change — the count is extracted dynamically. If they
> change the *wording*, edit `matchTokens` in Firebase. If you want to *require* a
> specific value (e.g. only accept "10000"), add it as a token: `10000,SMS,validity`.

---

## Expiry math

```
expiryEpoch = subscribeTime + (validityDays − safetyMarginDays) × 1 day
```

- **`safetyMarginDays`** (default **1**) accounts for operators expiring the bundle
  early / inclusive counting. So a 30-day pack subscribed on **1 Jul** expires
  **30 Jul** (`1 Jul + 29 days`), matching Jazz's "valid till 30 Jul 11:59pm".
- Fully flexible: a 7-day pack → `+6 days`. Adjust `safetyMarginDays` in Firebase.
- Expiry is only computed once the device clock is NTP-synced; until then the
  device fails **open** (see below).

---

## Enforcement & fail-open policy

`isSmsAllowed()` gates outgoing SMS in `processPendingCommand()`. A job that is
blocked by expiry ends as **`failed`, reason `package_expired`** (so it can be
retried automatically once you re-subscribe — stuck jobs reset after 5 min).

**Fail-open** by design — sending is allowed when:
- the package state is **unknown** (never saw a subscription SMS), or
- the clock isn't NTP-synced yet, or
- no expiry has been computed.

This prevents a missed subscription SMS from bricking sending (the v5 lockout
problem). Calls are never gated by the package.

---

## Auto-renew & reminder

- **Detect renewals:** every time a matching subscription SMS arrives (manual
  re-subscribe or operator auto-renew), the expiry is recomputed and persisted.
- **Reminder:** ~1 day before expiry the device sends a one-time ntfy reminder so
  you can re-subscribe. It does **not** auto-send the subscribe code (avoids
  double-charge / loops).

---

## Firebase storage (RTDB, first source of truth)

`/ttgo_tcall/package`:

```json
{
  "known": true,
  "subscribedEpoch": 1719792000,
  "expiryEpoch": 1722297600,
  "validityDays": 30,
  "smsAllowance": 10000,
  "safetyMarginDays": 1,
  "matchTokens": "been subscribed,SMS,validity",
  "lastMessage": "Your Offer ... 10000 SMS with 30day validity ..."
}
```

On boot the device reads this node; if missing it is **created/healed** with the
compile-time defaults and a serial log is printed. `matchTokens` and
`safetyMarginDays` are read from here, so both are tunable from the cloud.

---

## Serial commands

| Command | Effect |
|---------|--------|
| `package status` | Show validity, days left, allowance, whether sending is allowed |
| `package set <days>` | Manually set validity from today (e.g. after subscribing while offline) |
| `package clear` | Clear state → back to unknown (sending allowed) |

---

## Reference: v5 behaviour

v5 used an exact string match (`"10000 SMS with 30day validity"`) and calendar-
month math, storing `packageExpiryDate` in SPIFFS and gating via `hasPackage()`.
v8 keeps the same intent but makes detection token-based/configurable, uses
day-based expiry with a safety margin, stores state in Firebase (LittleFS-agnostic),
and fails open instead of locking out on unknown state.
