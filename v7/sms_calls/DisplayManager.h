#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_SSD1306.h>
#include <Wire.h>

class DisplayManager {
public:
    DisplayManager();
    void initialize();
    void update(float temperature, float humidity, int messagesSent, int messagesReceived, int callsMade, int callsReceived, bool wifiConnected, bool serverConnected);

private:
    Adafruit_SSD1306 display;
    bool displayWorking;
};

#endif