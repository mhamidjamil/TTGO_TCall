# v8 Feature Inventory

| Feature | Status | Notes |
|---|---|---|
| Temperature and humidity sensor | Planned | Keep working exactly as core telemetry. |
| Display manager | Planned | Preserve existing display behavior. |
| SMS support | Implemented | Sends outgoing SMS and handles incoming SMS ntfy/archive flow. |
| Call support | Implemented | Hangs up incoming calls and handles ntfy/archive flow. |
| SPIFFS configuration | Planned | Use as fallback and local persistence layer. |
| Firebase polling | Implemented | RTDB command/settings flow plus Firestore SIM module archive/block lists. |
| AP fallback | Planned | Start AP automatically on WiFi failure. |
| Web dashboard | Planned | Keep local UI/config/API access. |
| API docs endpoint | Planned | Add startup-visible docs URL. |
| Number normalization | Planned | Preserve v5-compatible behavior. |
| Rate limiting | Planned | Daily/weekly/monthly enforcement. |
| Structured logging | Planned | Clean, concise, professional logs. |
| Runtime settings from Firebase | Implemented | Reads telemetry/log/limit settings and `ntfyUrl` from RTDB, with defaults if missing. |
| Manual runtime sync command | Implemented | `sync` command forces immediate cloud settings and Firestore block-list refresh. |
| Serial help command | Implemented | `help` command lists supported serial commands, including ntfy test commands. |
| ntfy notifications | Implemented | Incoming call/SMS notifications use the configured ntfy topic unless blocked. |
| ThingSpeak upload | Planned | Post temperature and humidity to ThingSpeak field 1 and field 2. |
| MQTT | Removed | Not part of v8. |

## Notes
- v5 behavior is the source of truth for legacy phone number handling.
- v7 is the modular structure reference.
- Firebase details will be captured in a separate design doc before implementation.
