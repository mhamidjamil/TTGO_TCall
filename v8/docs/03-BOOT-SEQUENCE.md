# v8 Boot Sequence

1. Power and modem initialization.
2. IP5306 keep-on setup.
3. Serial modem start.
4. SIM module restart.
5. Attempt STA WiFi connection (tries saved pairs → secrets primary → secrets backup; 15 s each).
6. If STA fails, start AP fallback.
7. Start web dashboard and API.
8. Initialize DHT and display managers.
9. Sync runtime settings from Firebase (`intervalOfDhtSeconds`, `showFirebasePushLogs`).
10. Print startup summary logs including docs URL.

## WiFi Self-Healing (Post-Boot Reconnect)

If the router was not up when the device booted (e.g. power outage — router needs
5 min to warm up, device tries for ≤60 s and falls to AP mode), the device would
previously stay offline until manually rebooted.

**v8 now retries STA every 2 minutes when in AP/OFFLINE mode.**  
On a successful reconnect it:
- Re-syncs NTP
- Re-initializes Firebase (if not already connected)
- Syncs counters and runtime settings from the cloud
- Pushes a startup status update

This means after a power outage, once the router is up, the device recovers on its
own within ≤2 minutes — no manual reboot required.

## Acceptance Criteria
- Boot order is deterministic.
- AP fallback is automatic.
- Startup logs are concise and professional.
- Runtime settings are fetched on startup with safe defaults when missing.
- Device recovers from AP/OFFLINE mode automatically once the router is reachable.
