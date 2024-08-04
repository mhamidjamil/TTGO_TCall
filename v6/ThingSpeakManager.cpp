#include "ThingSpeakManager.h"
#include <Arduino.h>

ThingSpeakManager::ThingSpeakManager(DebugManager& debugManager)
    : debug(debugManager) {}

void ThingSpeakManager::initialize() {
    pinMode(13, OUTPUT);
    debug.println("Initializing ThingSpeak...");
    ThingSpeak.begin(espClient);
}

void ThingSpeakManager::update(float temperature, int humidity) {
    if (humidity < 110) {
        ThingSpeak.setField(1, temperature);
        ThingSpeak.setField(2, humidity);
        debug.println("Setting fields for ThingSpeak update");

        int updateStatus = ThingSpeak.writeFields(ts_channel_id, ts_api_key);
        if (updateStatus == 200) {
            debug.println("ThingSpeak update successful");
            digitalWrite(13, HIGH); // Turn LED on
        } else {
            debug.println("Error updating ThingSpeak. Status: " + String(updateStatus));
            digitalWrite(13, LOW); // Turn LED off
        }
    } else {
        digitalWrite(13, LOW); // Turn LED off
    }
}
