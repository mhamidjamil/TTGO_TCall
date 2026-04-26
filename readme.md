# TTGO T-Call ESP32 Module Project

This repository tracks the evolution of the TTGO T-Call ESP32 firmware across multiple versions. The current work is centered on v8, which uses Firebase Realtime Database as the cloud control plane and keeps the device behavior modular, observable, and safe to operate.

## What This Repository Contains

- v5: legacy SMS and number-handling behavior reference.
- v6: intermediate modular managers.
- v7: modular reference implementation.
- v8: current Firebase-first runtime with folder-based Realtime Database structure.

## Why v5 Still Matters

v5 is the legacy behavior reference for SMS handling, number normalization, and the older device flow. When changing how SMS destinations, allowlists, or message processing work, v5 remains the compatibility baseline.

The community should read v5 as the preserved behavior source, not as the newest architecture.

## What v8 Changes

v8 is the current direction for the project. It keeps the same TTGO T-Call hardware but modernizes the runtime:

- Firebase Realtime Database is the primary cloud path.
- Commands are polled from Firebase.
- Device telemetry is pushed to Firebase.
- Temperature and humidity are also uploaded to ThingSpeak.
- Runtime settings can be read from Firebase and healed automatically if missing.
- SPIFFS remains the local fallback for configuration.
- AP fallback remains intentional and documented.
- MQTT is not part of v8.

## Current Firebase Flow

The RTDB tree is now organized so the root node is only a container. Leaf values live under their parent folders.

### Main Tree

- `/ttgo_tcall/commands/pending`
- `/ttgo_tcall/commands/history`
- `/ttgo_tcall/counters`
- `/ttgo_tcall/status`
- `/ttgo_tcall/telemetry`
- `/ttgo_tcall/settings/runtime`

### Counters Folder

This folder stores SMS usage counters.

- `sentToday`
- `sentWeek`
- `sentMonth`
- `updatedAtMs`

### Status Folder

This folder stores device and last-sync status.

- `bootTime`
- `wifiMode`
- `ipAddress`
- `firebaseReady`
- `telemetryPushOk`
- `telemetryMessage`
- `updatedAtMs`

### Telemetry Folder

This folder stores sensor telemetry.

- `temperature`
- `humidity`
- `timestamp`
- `updatedAtMs`

### Runtime Settings Folder

This folder stores values that can be controlled from Firebase without reflashing the device.

- `intervalOfDhtSeconds`
- `showFirebasePushLogs`
- `dailySmsLimit`
- `weeklySmsLimit`
- `monthlySmsLimit`
- `updatedAtMs`

## ThingSpeak Upload Flow

The firmware also publishes temperature and humidity to ThingSpeak so the sensor data can be graphed separately from Firebase.

- `field1`: temperature
- `field2`: humidity

This upload uses the configured ThingSpeak write key and runs alongside the normal telemetry cycle when WiFi is connected.

If a runtime key is missing, invalid, or unreadable, the firmware creates or heals it with a safe default and prints a serial message so the operator knows what happened.

## Runtime Behavior

At startup the device will:

1. Initialize hardware and modem components.
2. Connect to WiFi or fall back to AP mode if needed.
3. Authenticate to Firebase.
4. Bootstrap missing RTDB folders and safe defaults.
5. Read runtime settings from `/ttgo_tcall/settings/runtime`.
6. Apply the current telemetry interval and SMS limits.
7. Print startup summary logs.

Runtime settings are then refreshed:

- on startup
- every 10 minutes
- immediately when the user types `sync` in the serial terminal

If `showFirebasePushLogs` is set to `false`, the device keeps sending telemetry but stops printing the repeated success lines for Firebase pushes.

## Serial Commands

The serial terminal currently supports these commands:

- `dht`: print the current temperature and humidity.
- `status`: print WiFi mode, IP address, and Firebase readiness.
- `sync`: force an immediate refresh of runtime settings from Firebase.
- `help`: list all available serial commands.

Firebase remains the control plane, while ThingSpeak acts as an additional sensor-data sink.

The rule for this project is simple: every new serial command must also be added to the `help` output in the same change.

## Firebase Console Notes

For v8, Realtime Database should be enabled and the device should have a valid Firebase Authentication path.

Recommended development rules:

```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}
```

The device uses its configured RTDB URL directly. If Firebase returns a region-corrected URL, the firmware can now detect that and self-correct.

## Project Rules Worth Remembering

- v5 is the legacy behavior reference.
- v7 is the modular reference.
- v8 must not include MQTT.
- AP fallback must be intentional.
- SPIFFS must remain the local persistence fallback.
- Firebase device access is Realtime Database first for v8.
- Missing Firebase runtime keys are auto-created or healed.
- New serial commands must be documented in `help`.

## Documentation Index

The versioned implementation docs live under `v8/docs/` and cover the implementation checklist, feature inventory, pin map, boot sequence, config precedence, Firebase design, rate limiting, number rules, API endpoints, logging standards, server SMS flow, and runtime settings sync.

If you are working on the code, start with the v8 docs first and keep them aligned with the firmware behavior.

## Hardware Summary

The project targets the TTGO T-Call ESP32 module with SIM800 modem support, DHT sensor telemetry, display output, and local web dashboard access.

## Library Notes

The firmware uses Arduino-compatible libraries, WiFi, HTTP client support, Firebase authentication, and SPIFFS-backed local persistence.

## Final Note

This repository is no longer just a demo sketch. It now contains a layered firmware history and a structured v8 runtime that can be expanded safely without turning the Firebase tree into a flat, hard-to-maintain key dump.
