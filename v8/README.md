# v8

v8 is the Firebase-backed evolution of the TTGO T-Call project.

## Goals
- Keep temperature and humidity sensor support.
- Keep display behavior and visibility.
- Keep SMS and calling workflows.
- Hang up incoming calls, notify through ntfy, and archive SIM events.
- Keep SPIFFS-backed dynamic configuration as the local fallback source of truth.
- Remove MQTT from v8.
- Add automatic AP fallback when STA WiFi fails.
- Build clean, professional, and traceable logs.

## Working Rules
- Documentation comes first.
- Pin usage must be documented before implementation.
- Every runtime feature must have a config story in both defaults and persisted storage.
- Firebase Realtime Database handles command/settings flows; Firestore handles SIM module events and block lists.
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
The ESP32 uses Firebase Auth and Firestore REST with the same Firebase project ID/API key.

- Block lists: `sim_module/settings` with `blockedCallers` and `blockedSmsSenders` string arrays.
- Optional CSV fields: `blockedCallersCsv` and `blockedSmsSendersCsv`.
- SMS documents: `sim_module/sms/by_number/{sender_or_number}`.
- Call documents: `sim_module/calls/by_number/{caller_number}`.
- Timestamps: human-readable Pakistan time, for example `2026-06-16 14:30:22 PKT`.

Firestore paths must alternate collection/document, so the smallest valid phone-number document path is `sim_module/sms/by_number/{number}` rather than `sim_module/sms/{number}`.

On startup the firmware creates `sim_module/settings`, `sim_module/sms`, and `sim_module/calls` if missing, then removes legacy `entries`, `numbers`, `_meta`, and blocked bucket documents created by the previous schema.

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

Use the actual serial port shown by `arduino-cli board list`.
