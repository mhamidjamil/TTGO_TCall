#include "DisplayManager.h"
#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22

DisplayManager::DisplayManager() : u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE) {}

void DisplayManager::initialize() {
    // Initialize the OLED display
    u8g2.begin();
    u8g2.setPowerSave(0); // Wake up the display
}

void DisplayManager::updateDisplay(const String& message) {
    u8g2.clearBuffer(); // Clear the internal memory
    u8g2.setFont(u8g2_font_logisoso16_tf); // Set the font
    int yPosition = 25; // Adjust the vertical position here (in pixels)
    u8g2.setCursor(0, yPosition); // Set the cursor position
    u8g2.print(message.c_str()); // Draw the text
    u8g2.sendBuffer(); // Transfer internal memory to the display
}
