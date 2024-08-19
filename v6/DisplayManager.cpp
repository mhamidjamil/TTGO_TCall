#include "DisplayManager.h"
#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22

DisplayManager::DisplayManager() : u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE), display_working(true) {}

void DisplayManager::initialize() {
    if (!u8g2.begin()) {
        Serial.println(F("SSD1306 allocation failed"));
        display_working = false;
    } else {
        delay(300);
        showBootMessage();
    }
}

void DisplayManager::showBootMessage() {
    if (display_working) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.setCursor(0, 0);
        u8g2.print("Boot Code:  " + String(random(99)));
        u8g2.sendBuffer();
        delay(500);
    }
}

void DisplayManager::updateDisplay(const String& line1, const String& line2, const String& status) {
    if (!display_working) return;

    u8g2.clearBuffer(); // Clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr); // Use a smaller font

    // Display status line
    u8g2.setCursor(0, 8);
    u8g2.print(status.c_str());

    // Display first line of data
    u8g2.setCursor(0, 24); // Adjust the vertical position
    u8g2.print(line1.c_str());

    // Display second line of data
    u8g2.setCursor(0, 37); // Adjust the vertical position
    u8g2.print(line2.c_str());

    // Transfer internal memory to the display
    u8g2.sendBuffer();
}
