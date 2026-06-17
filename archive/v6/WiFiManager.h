#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "DebugManager.h"
#include "arduino_secrets.h" // Include your secrets

class WiFiManager {
public:
    WiFiManager(DebugManager& debugManager);
    void connect();

private:
    DebugManager& debug;
    const char* ssid = MY_SSID;
    const char* password = MY_PASSWORD;
};

#endif // WIFI_MANAGER_H
