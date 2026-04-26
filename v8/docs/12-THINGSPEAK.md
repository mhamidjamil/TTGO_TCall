# v8 ThingSpeak Upload

## Purpose
Upload temperature and humidity readings from the TTGO T-Call to ThingSpeak alongside the existing Firebase flow.

## Data Posted
- `field1`: temperature
- `field2`: humidity

## Transport
- The device uses the ThingSpeak update endpoint over HTTPS.
- The write API key is required.
- A read API key is not needed for upload-only firmware behavior.

## Runtime Behavior
1. When WiFi is connected, the firmware attempts to upload the latest DHT readings.
2. Uploads follow the existing telemetry cadence.
3. If the WiFi connection is unavailable, the upload is skipped.
4. If the ThingSpeak credentials are missing, the firmware logs a clear startup warning.

## Configuration
- `thingSpeakChannelId`
- `thingSpeakWriteApiKey`

These values are loaded from the normal v8 config path and/or local secrets defaults.

## Acceptance Criteria
- Temperature is written to field 1.
- Humidity is written to field 2.
- The feature does not interfere with Firebase telemetry.
- Errors are logged clearly on the serial terminal.
