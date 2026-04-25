# v8

v8 is the Firebase Realtime Database evolution of the TTGO T-Call project.

## Goals
- Keep temperature and humidity sensor support.
- Keep display behavior and visibility.
- Keep SMS and calling workflows.
- Keep SPIFFS-backed dynamic configuration as the local fallback source of truth.
- Remove MQTT from v8.
- Add automatic AP fallback when STA WiFi fails.
- Build clean, professional, and traceable logs.

## Working Rules
- Documentation comes first.
- Pin usage must be documented before implementation.
- Every runtime feature must have a config story in both defaults and persisted storage.
- Firebase integration will be designed for offline-safe polling and command handling.
- Secrets never belong in markdown, example files, or logs.

## Reference Baseline
- v5 is the behavioral source of truth for legacy number handling and SMS/call identity rules.
- v7 is the reference for modular structure, config handling, and dashboard/API organization.
