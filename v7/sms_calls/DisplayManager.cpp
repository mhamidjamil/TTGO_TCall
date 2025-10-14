#include "DisplayManager.h"
#include <Arduino.h>

DisplayManager::DisplayManager() : display(128, 64, &Wire, -1), displayWorking(true) {}

void DisplayManager::initialize() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        displayWorking = false;
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.display();
    }
}

void DisplayManager::update(float temperature, float humidity, int messagesSent, int messagesReceived, int callsMade, int callsReceived, bool wifiConnected, bool serverConnected) {
    if (!displayWorking) return;

    display.clearDisplay();

    // Row 1: Temperature and Humidity
    display.setCursor(0, 0);
    display.printf("T: %.1fC H: %.1f%%", temperature, humidity);

    // Row 2: Message and Call Statistics
    display.setCursor(0, 17);
    display.printf("Msg: S:%d R:%d", messagesSent, messagesReceived);

    display.setCursor(0, 32);
    display.printf("Calls: M:%d R:%d", callsMade, callsReceived);

    // Row 3: Connectivity Status
    display.setCursor(0, 47);
    display.printf("WiFi: %s Server: %s", wifiConnected ? "OK" : "Fail", serverConnected ? "OK" : "Fail");

    display.display();
}
