# v8 API Endpoints

## Active Surface
- `GET /` redirects to `/dashboard.html`.
- `GET /dashboard.html`, `GET /dashboard.css`, `GET /dashboard.js`, `GET /version.txt` serve LittleFS assets.
- `GET /api/status` returns WiFi mode, IP, and Firebase readiness.
- `GET /api/firebase-web-config` returns browser-safe Firebase settings.
- `POST /api/notify-test` sends a local ntfy test using the configured topic.
- `GET /docs` returns the built-in firmware documentation page.

## Acceptance Criteria
- Public routes are clearly identified.
- Browser routes do not expose device email/password or service-account material.
- The notification test path is local to the device.
- Docs URL is printed at startup.
