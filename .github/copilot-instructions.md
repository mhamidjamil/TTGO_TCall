# Copilot Instructions for TTGO_TCall

## Mandatory Workflow
- Create the version folder docs before implementation.
- Define the pin map before changing hardware-facing code.
- Document config precedence before adding new settings.
- Document API changes before exposing or changing routes.
- Keep versioned docs in sync with the implementation.

## Project Rules
- v5 is the behavior reference for legacy SMS and number handling.
- v7 is the modular reference.
- v8 must not include MQTT.
- AP fallback must be documented and intentional.
- SPIFFS must remain the local persistence fallback.
