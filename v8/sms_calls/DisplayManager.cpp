#include "DisplayManager.h"

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

namespace {
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
}

bool DisplayManager::begin() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    return false;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TTGO v8 Booting...");
  display.display();
  return true;
}

void DisplayManager::update(float temperature, float humidity, const char *networkMode, const char *cloudMode,
                            int sentToday, int sentWeek, int sentMonth) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.print("T:");
  if (temperature <= -999.0f) {
    display.print("NA");
  } else {
    display.print(temperature, 1);
    display.print("C");
  }

  display.setCursor(64, 0);
  display.print("H:");
  if (humidity < 0.0f) {
    display.print("NA");
  } else {
    display.print(humidity, 0);
    display.print("%");
  }

  display.setCursor(0, 24);
  display.print("NET:");
  display.print(networkMode);

  display.setCursor(64, 24);
  display.print("CLD:");
  display.print(cloudMode);

  display.setCursor(0, 40);
  display.print("SENT D:");
  display.print(sentToday);

  display.setCursor(78, 40);
  display.print("W:");
  display.print(sentWeek);
  display.print(" M:");
  display.print(sentMonth);

  display.setCursor(0, 56);
  display.print("TTGO T-Call v8");

  display.display();
}
