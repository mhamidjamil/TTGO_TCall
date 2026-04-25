# v8 Feature Inventory

| Feature | Status | Notes |
|---|---|---|
| Temperature and humidity sensor | Planned | Keep working exactly as core telemetry. |
| Display manager | Planned | Preserve existing display behavior. |
| SMS support | Planned | Keep setup and message workflows. |
| Call support | Planned | Preserve inbound call handling. |
| SPIFFS configuration | Planned | Use as fallback and local persistence layer. |
| Firebase polling | Planned | Replace previous cloud direction. |
| AP fallback | Planned | Start AP automatically on WiFi failure. |
| Web dashboard | Planned | Keep local UI/config/API access. |
| API docs endpoint | Planned | Add startup-visible docs URL. |
| Number normalization | Planned | Preserve v5-compatible behavior. |
| Rate limiting | Planned | Daily/weekly/monthly enforcement. |
| Structured logging | Planned | Clean, concise, professional logs. |
| MQTT | Removed | Not part of v8. |

## Notes
- v5 behavior is the source of truth for legacy phone number handling.
- v7 is the modular structure reference.
- Firebase details will be captured in a separate design doc before implementation.
