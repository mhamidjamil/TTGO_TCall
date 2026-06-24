# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Firmware for the **TTGO T-Call ESP32** (SIM800 GSM modem + DHT sensor + SSD1306 OLED) that runs as a two-way SMS/call gateway with a cloud control plane. **`v8/` is the one active, self-contained Arduino sketch.** All older versions and supporting material were moved to `archive/` (see `archive/README.md`).

- **v8/** — the project. Firebase (Firestore + Realtime Database) control plane, ntfy notifications, ThingSpeak telemetry, local web dashboard, editable WiFi. **All work happens here.**
- **archive/v5/** — legacy behavior reference for SMS handling, number normalization, and allow/block rules. The compatibility baseline (hand-written by the maintainer), not a target for new features.
- **archive/v7/** — modular reference implementation; the structural template for v8's manager pattern. **archive/v6/** is an earlier modular split.
- **archive/server/** — standalone Flask bridge/logger from an earlier design (v8 talks to Firebase directly).
- **archive/Modules_test/** — throwaway hardware test sketches (I2C OLED, MPU6050, ultrasonic, RTOS). Not production.

## Version Roles (these are hard project rules)

- **archive/v5 is the behavioral source of truth** for legacy number handling and SMS/call identity rules. When changing SMS destinations, allowlists, or message processing, treat v5 as the compatibility baseline.
- **archive/v7 is the modular reference** for structure, config handling, and dashboard/API organization.
- **v8 must NOT include MQTT.** It was intentionally removed.
- **AP fallback must be intentional and documented**, not accidental.
- **LittleFS must remain the local persistence fallback** when cloud access is unavailable.
- For v8, **Firebase device access is Realtime Database first** unless the user explicitly switches database type.

## Build / Flash / Run

Each sketch folder (e.g. `v8/sms_calls/`) is compiled as an Arduino sketch. The `.ino` file must match its folder name. Board: `esp32:esp32:esp32`, partition scheme `huge_app`, 4 MB flash, 921600 upload baud (see `.vscode/arduino.json`).

```sh
# One-time core + library setup
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson Firebase_Arduino_Client_Library_for_ESP8266_and_ESP32 DHT_sensor_library Adafruit_SSD1306

# Compile / upload (run from repo root)
arduino-cli compile --fqbn esp32:esp32:esp32 v8/sms_calls
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 v8/sms_calls   # use the port from `arduino-cli board list`
```

**LittleFS web assets** (`v8/sms_calls/data/`) are flashed separately as the filesystem image so `/dashboard.html`, `/dashboard.css`, `/dashboard.js` are served by the device. Build with `mklittlefs`, flash with `esptool` (data partition at `0x290000` for this repo's 4 MB partition — see `v8/README.md` for the exact commands). If upload reports `Wrong boot mode detected (0x13)`, hold `BOOT` while tapping `EN/RESET`, then retry.

There is **no automated test suite** — verification is on-device via the serial monitor (115200 baud) and the web dashboard.

### Flask server (server/)

```sh
cd server
pip install -r requirements.txt   # Flask, flasgger, flask-sock, openpyxl, requests
python app.py                     # Swagger UI at /docs (dashboard-password protected)
```
Configured via env vars: `DEVICE_HOST`, `DEVICE_PORT`, `DASHBOARD_PASSWORD`, `USE_API_SECRET`, `API_SECRET`, `LOG_FILE`.

## v8 Firmware Architecture

`sms_calls.ino` is the orchestrator: it instantiates one static instance of each `*Manager` and wires them together in `setup()`, then drives them from `loop()`. Each manager owns one concern and exposes a `begin(...)` + per-loop tick. Follow this manager pattern (mirrored from v7) when adding features — do not inline new subsystems into the `.ino`.

| Manager | Responsibility |
|---|---|
| `ConfigManager` | Config precedence + LittleFS persistence; produces the `runtimeConfig` struct passed to every other `begin()` |
| `WiFiManager` | STA connect with AP fallback |
| `FirebaseManager` | Firestore gateway (queues, allowed numbers, logs, heartbeat) + RTDB (counters, telemetry, runtime settings). **The largest module (~62 KB)** |
| `RateLimitManager` | Per-number daily quotas + global daily/weekly/monthly SMS limits |
| `SMSManager` / `CallManager` | AT-command SMS/call send + incoming handling |
| `DHTManager` / `ThingSpeakManager` | Temperature/humidity read + ThingSpeak upload (fields 1/2) |
| `DisplayManager` | SSD1306 OLED output |
| `NtfyManager` | Push notifications to ntfy on incoming SMS/call |
| `WebDashboard` | Local HTTP server serving the LittleFS dashboard + device API |
| `Logger` | Serial logging |

### Config precedence (order of truth)
1. Compile-time defaults (`secrets.h` / `secrets.example.h`)
2. LittleFS persisted config
3. RTDB runtime settings (`/ttgo_tcall/settings/runtime`)
4. Firestore `sim_module/device` (active switch + block lists) for control-plane decisions
5. Live changes from dashboard / cloud sync (persisted back as last-known-good)

### Cloud layout
- **Firestore `sim_module/`** — exactly three children. `device` (doc: config, health, the four block-list arrays `blocked{Incoming,Outgoing}{Callers,Sms}`, and lifetime counters); `sms` (container doc) with subcollections `sms_jobs/{number}` + `sms_received/{number}`; `calls` with `call_jobs/{number}` + `call_received/{number}`. Jobs are keyed by phone number (doc id = number), one active job per number. Status flow: `pending → in_progress → sent`/`called` | `blocked` | `failed`; jobs stuck in `in_progress` > 5 min reset to `pending`. Outgoing control is block-list only (no allow-list, no per-number quota). Incoming UCS2/UTF-16BE SMS are decoded before store/notify (`normalized_message`/`original_message`/`was_decoded`). See `v8/docs/10-SERVER-SMS-FLOW.md` for the app enqueue contract.
- **RTDB `/ttgo_tcall/`** — counters (rolling rate-limit windows), telemetry snapshots, and `settings/runtime` (`intervalOfDhtSeconds`, `showFirebasePushLogs`, `jobLogs`, the SMS limits — only here, not duplicated in Firestore — `ntfyUrl`, and WiFi pairs).

## Working Rules (from .github/copilot-instructions.md & READMEs — follow these)

- **Docs come first.** The versioned spec lives in `v8/docs/` (`00-IMPLEMENTATION-CHECKLIST` … `12-THINGSPEAK`). Before changing behavior: update the pin map before hardware-facing changes, config precedence before new settings, API doc before route changes. Keep `v8/docs/` in sync with the firmware — they are the spec, not afterthoughts.
- **Self-healing runtime config.** For any Firebase-backed runtime variable, if it's missing/invalid/unreadable, create or heal it with a safe default **and print a clear serial log** so the operator knows. Runtime sync must work at startup, every 10 min, and on the manual `sync` serial command.
- **Serial commands are documented inline with `help`.** Every serial command must appear in the `help` output with a short description, in the **same change** that adds the command. Current commands include `dht`, `status`, `sync`, `help`, `show sms`, `delete sms <i>`, `delete all sms`, `ntfy test`.
- **LittleFS asset versioning.** Any change under `v8/sms_calls/data/` must bump `data/version.txt` (new version + date) — the dashboard displays it.
- **Secrets never go in markdown, examples, logs, or chat.** `secrets.h` is gitignored; `secrets.example.h` is the committed template with placeholder values. Copy it to `secrets.h` locally to build.
- **Do NOT create commits or run push/rewrite operations unless the user explicitly asks.**

## Hardware Pin Map (v8 production — see v8/docs/02-PIN-MAP.md)

| Pin | Role | | Pin | Role |
|---|---|---|---|---|
| 4 | Modem PWRKEY | | 23 | Modem POWER_ON |
| 5 | Modem RESET | | 26 | Modem RX |
| 21 | I2C SDA | | 27 | Modem TX |
| 22 | I2C SCL | | 33 | DHT sensor |

Useful SIM800 AT commands are catalogued in `archive/commands.txt`.
