#include "OrangePiCommManager.h"

OrangePiCommManager::OrangePiCommManager(DebugManager& debugManager)
    : debug(debugManager) {}

void OrangePiCommManager::sendData(const String& data) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverName);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(data);

        if (httpResponseCode > 0) {
            String response = http.getString();
            debug.println("HTTP Response code: " + String(httpResponseCode));
            debug.println("Response: " + response);
        } else {
            debug.println("Error in sending POST request");
            debug.println("HTTP Response code: " + String(httpResponseCode));
        }

        http.end();
    } else {
        debug.println("WiFi not connected");
    }
}
