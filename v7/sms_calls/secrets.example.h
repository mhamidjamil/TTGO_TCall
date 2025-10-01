#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

// Copy this file to 'secrets.h' and edit values for your device.
// This example provides default values for all variables exposed by the
// dashboard so the firmware initializes with sensible defaults.

// ----- WiFi -----
#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"

// ----- Dashboard -----
// Password used to access /dashboard (or to send X-Dashboard-Auth header)
#define DASHBOARD_PASSWORD "admin123"

// ----- API / forwarding -----
// Toggle whether the API requires the API secret by default (1 = enabled)
#define USE_API_SECRET_DEFAULT 1
// Default API secret token (can be changed from dashboard at runtime)
#define API_SECRET_DEFAULT "changeme"
// URL to POST forwarded events (incoming SMS/call, sent events)
#define FORWARD_URL_DEFAULT ""
// Optional API key/header to include when forwarding events
#define FORWARD_API_KEY_DEFAULT ""

// ----- Allow toggles -----
// Allow sending SMS from dashboard/device (1 = enabled)
#define ALLOW_SMS_DEFAULT 1
// Allow initiating calls from dashboard/device (1 = enabled)
#define ALLOW_CALL_DEFAULT 1

// ----- Remote settings (versioned) -----
// Optional remote settings URL that the device will check when requested
#define SETTINGS_URL_DEFAULT ""
// Local settings version. If a remote settings JSON with a newer
// version is found the device can apply it and persist it.
#define SETTINGS_VERSION_DEFAULT ""

// ----- Modem / Server defaults -----
// Modem Serial (Serial1) baud rate
#define MODEM_SERIAL_BAUD 115200
// Web server port
#define WEB_SERVER_PORT 420

#endif // SECRETS_EXAMPLE_H
