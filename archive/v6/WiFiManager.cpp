#include "WiFiManager.h"
#include "arduino_secrets.h"

WiFiManager::WiFiManager(DebugManager& debugManager)
    : debug(debugManager) {}

void WiFiManager::connect() {
    WiFi.begin(ssid, password); // Connect to Wi-Fi
    for (int i = 0; WiFi.status() != WL_CONNECTED && i < 10; i++) {
        delay(500);
        debug.println("Connecting to WiFi...");
    }

    if (WiFi.status() == WL_CONNECTED) {
        debug.println("Wi-Fi connected successfully");
    } else {
        debug.println("Wi-Fi connection failed");
    }
}
