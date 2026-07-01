# v8 Boot Sequence

1. Power and modem initialization.
2. IP5306 keep-on setup.
3. Serial modem start.
4. SIM module restart.
5. Attempt STA WiFi connection (tries dynamic saved list `/wifi_nets.json` → legacy fixed slots → secrets primary → secrets backup; 15 s each).
6. If STA fails, start AP fallback.
7. Start web dashboard and API.
8. Initialize DHT and display managers.
9. Sync runtime settings from Firebase (`intervalOfDhtSeconds`, `showFirebasePushLogs`).
10. Print startup summary logs including docs URL.

## WiFi Self-Healing (Post-Boot Reconnect)

If the router was not up when the device booted (e.g. power outage — router needs
5 min to warm up, device tries for ≤15 s per SSID and falls to AP mode), the device
would previously stay offline until manually rebooted.

**v8 now retries STA every 5 minutes when in AP/OFFLINE mode.**  
On a successful reconnect it:
- Re-syncs NTP
- Re-initializes Firebase (if not already connected)
- Syncs counters and runtime settings from the cloud
- Pushes a startup status update

This means after a power outage, once the router is up, the device recovers on its
own within ≤5 minutes — no manual reboot required.

## Dynamic WiFi Network List

The boot retry now tries **all networks from the dynamic list** (`/wifi_nets.json`)
on every attempt, not just two fixed slots. Networks are tried in insertion order.
See `v8/docs/13-WIFI-MANAGEMENT.md` for full details on adding/removing networks.

## Cloud Late-Init (WiFi up, cloud not ready)

A separate failure mode: the station **connects** at boot, but the uplink/DNS
isn't ready yet (router just powered on), so `firebaseManager.begin()` fails for
lack of internet. Because the WiFi self-healing block only fires when the station
is *down*, the device would otherwise stay online forever with **no Firebase and
no ThingSpeak** (ThingSpeak pushes are gated behind Firebase-ready) even though
ntfy — an independent HTTP call — still works.

**v8 runs `ensureCloudServices()` every loop** (rate-limited to one attempt per
90 s): whenever WiFi is connected but Firebase or ThingSpeak is not ready, it
re-initializes them. On Firebase success it re-bootstraps the gateway, syncs
counters + runtime settings, and pushes a startup status. It is called both at
the top of `loop()` and again **right before each telemetry push**, so the cloud
is guaranteed live at push time. On a successful WiFi reconnect the retry timer is
reset so the re-init happens immediately. This recovers telemetry after a "router
booted but internet lagged" cold start.

## Acceptance Criteria
- Boot order is deterministic.
- AP fallback is automatic.
- Startup logs are concise and professional.
- Runtime settings are fetched on startup with safe defaults when missing.
- Device recovers from AP/OFFLINE mode automatically once the router is reachable.
