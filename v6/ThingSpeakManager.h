#ifndef THINGSPEAK_MANAGER_H
#define THINGSPEAK_MANAGER_H

#include <WiFi.h>
#include <ThingSpeak.h>
#include "arduino_secrets.h" // Include your secrets
#include "DebugManager.h"

class ThingSpeakManager {
public:
    ThingSpeakManager(DebugManager& debugManager);
    void initialize();
    void update(float temperature, int humidity);

private:
    DebugManager& debug;
    WiFiClient espClient;
    unsigned long ts_channel_id = MY_CHANNEL_ID;
    const char *ts_api_key = THINGSPEAK_API;
};

#endif // THINGSPEAK_MANAGER_H
