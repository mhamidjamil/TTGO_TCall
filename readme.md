# TTGO T-Call ESP32 — GSM SMS/Call Gateway

Firmware for the **LilyGO TTGO T-Call ESP32** (SIM800 GSM modem + DHT sensor + SSD1306 OLED) that runs as a **two-way SMS and call gateway** with a Firebase control plane, a local web dashboard, ntfy push notifications, and ThingSpeak telemetry.

Queue an SMS or call in Firestore (from the dashboard, or later from a Rails backend) → the device claims it, enforces allow-list and per-number daily quotas, sends/dials over GSM, and writes the result back. Incoming SMS and calls are archived to Firestore and pushed to ntfy.

> **Active version: [`/v8`](v8).** Everything else lives in [`/archive`](archive) for reference — including **`archive/v5`, the behavioral source of truth** for legacy phone-number and SMS/call identity rules.

## Hardware

| Pin | Role | | Pin | Role |
|---|---|---|---|---|
| 4 | Modem PWRKEY | | 23 | Modem POWER_ON |
| 5 | Modem RESET | | 26 | Modem RX |
| 21 | I2C SDA | | 27 | Modem TX |
| 22 | I2C SCL | | 33 | DHT sensor |

Full map: [`v8/docs/02-PIN-MAP.md`](v8/docs/02-PIN-MAP.md). Diagram: `archive/TTGO-T-call-Pinout-Diagram-Large.jpg`.

## Quick start

```sh
# 1. Toolchain + libraries
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson Firebase_Arduino_Client_Library_for_ESP8266_and_ESP32 DHT_sensor_library Adafruit_SSD1306

# 2. Secrets — copy the template and fill in WiFi / Firebase / ntfy / ThingSpeak
cp v8/sms_calls/secrets.example.h v8/sms_calls/secrets.h

# 3. Compile + upload (use the port from `arduino-cli board list`)
arduino-cli compile --fqbn esp32:esp32:esp32 v8/sms_calls
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 v8/sms_calls
```

The web UI assets in `v8/sms_calls/data/` are flashed **separately** as the SPIFFS image (see [`v8/README.md`](v8/README.md) for the `mkspiffs`/`esptool` commands). Then open `http://<device-ip>:<port>/dashboard.html`.

## What v8 does

- **Outgoing queue** — polls Firestore `sms_jobs` / `call_jobs`, claims one of each per cycle, drains the queue quickly when work is pending.
- **Block-list + rate limit** — any number is sent/dialed unless it's in an outgoing block list; a global daily/weekly/monthly SMS limit (RTDB runtime) still applies. No allow-list, no per-number quota.
- **Block-list** — incoming calls/SMS from blocked senders (numeric **or** alphanumeric IDs like `JAZZ`) are archived without notifying.
- **Editable WiFi** — set two WiFi networks from the dashboard; saved to SPIFFS and tried before the `secrets.h` networks on next boot, then AP fallback. No reflash needed to change networks.
- **Runtime settings sync** — `jobLogs`, push-log toggles, DHT interval, SMS limits, and ntfy URL live in RTDB `/ttgo_tcall/settings/runtime`; edit them in the dashboard and the device re-reads on startup, every 5 min, or on demand via the **Sync** button / serial `sync`. Telemetry is pushed at most once per minute.
- **SIM package tracking** — auto-detects the operator's subscription SMS (configurable token match), records expiry in RTDB `/ttgo_tcall/package`, blocks outgoing SMS once the bundle lapses (`failed`/`package_expired`), and ntfy-reminds ~1 day before expiry. Fails open when the package state is unknown. See [`v8/docs/14-PACKAGE-SUBSCRIPTION.md`](v8/docs/14-PACKAGE-SUBSCRIPTION.md).
- **Self-healing connectivity** — retries STA WiFi every 5 min in AP/OFFLINE mode, and re-initializes **Firebase + ThingSpeak** (rate-limited to every 90 s, and re-checked right before each telemetry push) whenever WiFi is up but a cloud service never came online (router up but uplink not ready at boot).
- **Serial `[JOB]` logs** — see every claim, validation, quota check, send/dial, and final status (toggle with the `jobLogs` flag).
- **Telemetry** — temperature/humidity to Firebase RTDB and ThingSpeak; device heartbeat (battery, signal, operator) to Firestore.
- **No MQTT.** SPIFFS is the local config fallback. AP fallback is intentional.

## Documentation

Specs live in [`v8/docs/`](v8/docs) — implementation checklist, feature inventory, pin map, boot sequence, config precedence, Firestore design, rate limiting, number rules, API endpoints, logging standard, server SMS flow, runtime-settings sync, and ThingSpeak. Start there before changing firmware, and keep them in sync with the code.

## Roadmap

A Rails backend (and, later, a mobile front-end) will enqueue SMS/call jobs into the same Firestore collections the dashboard uses, so the device stays a thin executor.

## Future Ideas

### Over-the-Air (OTA) Firmware Updates — esp32FOTA

[`chrisjoyce911/esp32FOTA`](https://github.com/chrisjoyce911/esp32FOTA) is the recommended approach for wireless firmware updates:

- **Unattended**: polls a JSON manifest URL, compares semver, downloads and self-flashes with no serial cable.
- **Verified**: supports RSA signature verification so only your signed binaries can be applied.
- **HTTPS out of the box**: works against GitHub Releases, S3, or Firebase Storage; Arduino 3.x bundles root CAs, no manual certificate embedding needed.
- Install via Arduino Library Manager: search `esp32FOTA`.

The manifest JSON specifies the firmware version, type, and binary URL. The device checks it periodically and applies the update if the remote version is newer. This would eliminate the need to physically access the device for firmware upgrades.
