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

- Call events: `sim_module/calls/entries/{autoId}`.
- SMS events: `sim_module/sms/entries/{autoId}`.
- Blocked call events: `sim_module/blocked_calls/entries/{autoId}`.
- Blocked SMS events: `sim_module/blocked_sms/entries/{autoId}`.
- Blocked callers list: `sim_module/blocked_callers/numbers/{docId}`.
- Blocked SMS senders list: `sim_module/blocked_sms_senders/numbers/{docId}`.

For block-list documents, set `number` to the phone number and optionally set `enabled` to `false` to disable it without deleting the document. The firmware also accepts the document ID as the number when the `number` field is missing, and it also reads singular aliases `blocked_caller` and `blocked_sms_sender`.

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
