# v8

v8 is the Firebase-backed evolution of the TTGO T-Call project.

## Goals
- Keep temperature and humidity sensor support.
- Keep display behavior and visibility.
- Keep SMS and calling workflows.
- Hang up incoming calls, notify through ntfy, and archive SIM events.
- Process outgoing SMS and missed-call jobs from Firestore.
- Enforce outgoing block lists and the global SMS rate limit before dialing or sending.
- Push device heartbeat, signal, battery, operator, and queue health fields to Firestore.
- Keep SPIFFS-backed dynamic configuration as the local fallback source of truth.
- Remove MQTT from v8.
- Add automatic AP fallback when STA WiFi fails.
- Build clean, professional, and traceable logs.

## Working Rules
- Documentation comes first.
- Pin usage must be documented before implementation.
- Every runtime feature must have a config story in both defaults and persisted storage.
- Firestore handles the GSM gateway queues, allowed numbers, logs, heartbeat, and SIM archives.
- Firebase Realtime Database remains only for legacy counters, telemetry snapshots, and runtime settings.
- Secrets never belong in markdown, example files, or logs.

## Reference Baseline
- v5 is the behavioral source of truth for legacy number handling and SMS/call identity rules.
- v7 is the reference for modular structure, config handling, and dashboard/API organization.

## ntfy Notifications
- Default topic URL: `https://ntfy.innovorix.com/oracle_ntfy`.
- Runtime setting: `/ttgo_tcall/settings/runtime/ntfyUrl`.
- Incoming call title: `call from <number>`.
- Incoming SMS title: `sms from <number>`, with the SMS body as the notification description.
- Serial test commands: `ntfy test` or `test ntfy`.

## Firestore `sim_module`
The ESP32 uses Firebase Auth and Firestore REST with the same Firebase project ID/API key. `sim_module` has exactly three children:

- `sim_module/device` — config, health, the four block-list arrays, and lifetime counters.
- `sim_module/sms/sms_jobs/{number}` (outgoing queue) and `sim_module/sms/sms_received/{number}` (incoming archive).
- `sim_module/calls/call_jobs/{number}` (outgoing queue) and `sim_module/calls/call_received/{number}` (incoming archive).

Job/received documents are keyed by phone number (the doc id *is* the number). Timestamps are human-readable Pakistan time, e.g. `2026-06-16 14:30:22 PKT`. The full field list + app enqueue contract is in [docs/10-SERVER-SMS-FLOW.md](docs/10-SERVER-SMS-FLOW.md).

On startup the firmware creates `sim_module/device`, `sim_module/sms`, and `sim_module/calls` if missing and seeds each empty block-list array with `["JAZZ","000"]`.

## Two-Way GSM Gateway
- Poll interval: one cloud cycle every 3 seconds or the configured value if higher.
- The cycle claims at most one pending `sms_jobs` doc and one pending `call_jobs` doc.
- Status flow: `pending -> in_progress -> sent` (SMS) / `called` (call), or `blocked` / `failed`.
- Jobs stuck in `in_progress` for more than 5 minutes are reset to `pending`.
- Outgoing is allowed for any number **unless** it is in `device.blockedOutgoingSms` / `device.blockedOutgoingCallers` (then the job is `blocked`). The global SMS rate limit (RTDB runtime) still applies.
- Missed-call mode rings briefly; if the modem reports an answer, firmware hangs up immediately and records `user_picked = true`.

## Block lists (incoming mute vs outgoing block)
Four string arrays on `sim_module/device`, editable from the dashboard or Firestore:

- `blockedIncomingCallers` / `blockedIncomingSms` — incoming events are still archived but do **not** trigger an ntfy notification (muted).
- `blockedOutgoingCallers` / `blockedOutgoingSms` — the device refuses to dial/send; the job is marked `blocked`.

Entries may be numbers or alphanumeric sender ids (e.g. `JAZZ`, matched case-insensitively). The device refreshes the lists about once a minute and instantly when you press **Sync**. Phone numbers are stored canonically (`+<countrycode><number>`); the dashboard normalizes what you type so the device matches regardless of `0300…`, `92300…`, or `+92300…`.

## Incoming SMS normalization
UCS2/UTF-16BE hex messages are decoded to text before being stored or notified. `sms_received` keeps both `original_message` (raw) and `normalized_message` (decoded, `was_decoded:true`); ntfy gets the normalized text. Example: `00310020…0042` → `1 Paise mai 8GB`.

## Editable WiFi (no reflash to change networks)
The **WiFi tab** lets you set up to two SSID/password pairs. They are saved to Firebase (the runtime settings node) like the other dashboard settings; the device reads them on sync and stores them in SPIFFS. On boot the device tries, in order: saved pair 1 → saved pair 2 → the `secrets.h` networks → its own AP. Set the new network while the device is still online, then use the **Reboot Device** button to reconnect. Serial prints which network it connects through.

## Dashboard
Upload `v8/sms_calls/data` to SPIFFS with the sketch. The local web UI is served from:

```text
http://<device-ip>:<webServerPort>/dashboard.html
```

The dashboard uses Firebase anonymous auth from the browser and manages:

- `sim_module/device` (active/name, block lists, counters)
- `sim_module/sms/sms_jobs` and `sim_module/sms/sms_received`
- `sim_module/calls/call_jobs` and `sim_module/calls/call_received`
- RTDB `/ttgo_tcall/settings/runtime` for runtime flags such as `jobLogs`, SMS limits, and WiFi

If anonymous auth is disabled, use a separate hosted admin dashboard or adapt `dashboard.js` to your preferred sign-in method. Device email/password is intentionally not exposed to the browser.

### SPIFFS Upload Rule
- Every time anything under `v8/sms_calls/data` changes, bump `v8/sms_calls/data/version.txt` with a new version and date.
- The dashboard reads that file and shows it in the header and About tab.
- For the default ESP32 4 MB partition in this repo, SPIFFS starts at `0x290000`.
- Build the image with `mkspiffs` and flash it with `esptool` when you want to verify the upload from the console.

```sh
IMG=/tmp/ttgo_spiffs.bin
ROOT=/home/megatron/Desktop/projects/TTGO_TCall/v8/sms_calls/data
MKSPIFFS=~/.arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs
ESPTOOL=~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool
$MKSPIFFS -c "$ROOT" -b 4096 -p 256 -s 0x160000 "$IMG"
$ESPTOOL --chip esp32 --port /dev/ttyACM0 --baud 921600 write-flash 0x290000 "$IMG"
```

If the board uses a different partition scheme, check `arduino-cli board details -b esp32:esp32:esp32` before flashing.
If you see `Wrong boot mode detected (0x13)`, the board is not in download mode. Hold `BOOT` while tapping `EN/RESET`, then rerun the upload.

## Reduced 8-Commit Delivery Plan
1. `feat(firestore): add gateway schema and device identity`
2. `feat(device): publish heartbeat and health metrics`
3. `feat(numbers): manage incoming/outgoing block lists`
4. `feat(sms): add outgoing SMS queue processing and logs`
5. `feat(calls): add missed-call queue processing and logs`
6. `feat(recovery): reset stale processing jobs`
7. `feat(dashboard): add gateway admin screens`
8. `docs(project): document deployment and operations`

## Serial SMS Memory Commands
- `show sms` prints `AT+CMGL="ALL"` output with SIM indexes.
- `delete sms <index>` deletes one SIM message with `AT+CMGD=<index>`.
- `delete all sms` deletes all SIM messages with `AT+CMGD=1,4`.
- Incoming SMS uses the stored-message flow: receive index, read with `AT+CMGR=<index>`, upload to Firestore, then delete that index only after upload succeeds.

## Arduino CLI Setup
Install Arduino CLI, add the ESP32 core, then compile or upload from this folder:

```sh
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson Firebase_Arduino_Client_Library_for_ESP8266_and_ESP32 DHT_sensor_library Adafruit_SSD1306
arduino-cli compile --fqbn esp32:esp32:esp32 v8/sms_calls
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 v8/sms_calls
```

Use the actual serial port shown by `arduino-cli board list`. Upload `v8/sms_calls/data` as the SPIFFS/LittleFS data folder with your ESP32 filesystem upload tool so `/dashboard.html`, `/dashboard.css`, and `/dashboard.js` are available on the device.
