# v8 API Endpoints

## Active Surface
- `GET /` redirects to `/dashboard.html`.
- `GET /dashboard.html`, `GET /dashboard.css`, `GET /dashboard.js`, `GET /version.txt` serve LittleFS assets.
- `GET /wifi.html` — WiFi management page (LittleFS asset). Add/delete saved networks.
- `GET /api/status` returns WiFi mode, IP, and Firebase readiness.
- `GET /api/firebase-web-config` returns browser-safe Firebase settings.
- `POST /api/notify-test` sends a local ntfy test using the configured topic.
- `POST /api/sync-runtime` queues a Firebase runtime-settings sync.
- `POST /api/reboot` reboots the device.
- `GET /docs` returns the built-in firmware documentation page.

## WiFi Management API
- `GET /api/wifi/list` — returns `{"connected":"ssid","networks":[{"ssid":"..."},...], "max":8}`.
  `connected` is empty string when in AP/OFFLINE mode.
- `POST /api/wifi/add` — body `{"ssid":"...","pass":"..."}`. Adds or updates a network in `/wifi_nets.json`.
  Returns `{"ok":true}` or `{"ok":false,"message":"..."}`.
- `POST /api/wifi/delete` — body `{"ssid":"..."}`. Removes a network by SSID.
  Returns `{"ok":true}` or `{"ok":false,"message":"..."}`.

## Acceptance Criteria
- Public routes are clearly identified.
- Browser routes do not expose device email/password or service-account material.
- The notification test path is local to the device.
- Docs URL is printed at startup.
- WiFi API returns passwords only on add (write-only); list returns SSIDs only.
