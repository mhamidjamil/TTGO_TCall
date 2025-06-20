#define RED_PIN 12
#define GREEN_PIN 14
#define BLUE_PIN 27

uint8_t brightness = 100; // Percentage (1-100)
uint8_t currentR = 0, currentG = 0, currentB = 0;

void setup() {
  Serial.begin(115200);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  Serial.println("🎨 ESP32 RGB Color Controller Ready");
  printHelp();
}

void printHelp() {
  Serial.println("\nAvailable Commands:");
  Serial.println("1  - Red");
  Serial.println("2  - Green");
  Serial.println("3  - Blue");
  Serial.println("4  - Yellow");
  Serial.println("5  - Cyan");
  Serial.println("6  - Magenta");
  Serial.println("7  - White");
  Serial.println("8  - Orange");
  Serial.println("9  - Pink");
  Serial.println("10 - Purple");
  Serial.println("11 - Lime");
  Serial.println("12 - Teal");
  Serial.println("13 - Sky Blue");
  Serial.println("14 - Brown");
  Serial.println("15 - Gray");
  Serial.println("420 - Off");
  Serial.println("B:<value> or b:<value> - Set brightness (1-100)");
  Serial.println("#RRGGBB - Custom color in HEX format (e.g., #FF00AA)");
}

void applyColor(uint8_t r, uint8_t g, uint8_t b) {
  currentR = r;
  currentG = g;
  currentB = b;
  updatePWM();
}

void updatePWM() {
  // Apply brightness scaling
  analogWrite(RED_PIN, map(currentR * brightness, 0, 255 * 100, 0, 255));
  analogWrite(GREEN_PIN, map(currentG * brightness, 0, 255 * 100, 0, 255));
  analogWrite(BLUE_PIN, map(currentB * brightness, 0, 255 * 100, 0, 255));
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Brightness Command
    if (input.startsWith("B:") || input.startsWith("b:")) {
      int newBrightness = input.substring(2).toInt();
      if (newBrightness >= 1 && newBrightness <= 100) {
        brightness = newBrightness;
        Serial.print("🔆 Brightness set to: ");
        Serial.print(brightness);
        Serial.println("%");
        updatePWM(); // Re-apply current color with new brightness
      } else {
        Serial.println("⚠️ Brightness must be between 1 and 100.");
      }
      return;
    }

    // Hex color support (#RRGGBB)
    if (input.startsWith("#") && input.length() == 7) {
      long hexColor = strtol(input.c_str() + 1, NULL, 16);
      uint8_t r = (hexColor >> 16) & 0xFF;
      uint8_t g = (hexColor >> 8) & 0xFF;
      uint8_t b = hexColor & 0xFF;
      Serial.printf("🎨 Custom color - R:%d G:%d B:%d\n", r, g, b);
      applyColor(r, g, b);
      return;
    }

    // Color code command
    int code = input.toInt();
    switch (code) {
    case 1:
      applyColor(255, 0, 0);
      Serial.println("🔴 Red");
      break;
    case 2:
      applyColor(0, 255, 0);
      Serial.println("🟢 Green");
      break;
    case 3:
      applyColor(0, 0, 255);
      Serial.println("🔵 Blue");
      break;
    case 4:
      applyColor(255, 255, 0);
      Serial.println("🟡 Yellow");
      break;
    case 5:
      applyColor(0, 255, 255);
      Serial.println("🟦 Cyan");
      break;
    case 6:
      applyColor(255, 0, 255);
      Serial.println("🟪 Magenta");
      break;
    case 7:
      applyColor(255, 255, 255);
      Serial.println("⚪ White");
      break;
    case 8:
      applyColor(255, 165, 0);
      Serial.println("🟧 Orange");
      break;
    case 9:
      applyColor(255, 105, 180);
      Serial.println("💗 Pink");
      break;
    case 10:
      applyColor(128, 0, 128);
      Serial.println("💜 Purple");
      break;
    case 11:
      applyColor(191, 255, 0);
      Serial.println("🟩 Lime");
      break;
    case 12:
      applyColor(0, 128, 128);
      Serial.println("🟫 Teal");
      break;
    case 13:
      applyColor(135, 206, 235);
      Serial.println("🌤 Sky Blue");
      break;
    case 14:
      applyColor(139, 69, 19);
      Serial.println("🟤 Brown");
      break;
    case 15:
      applyColor(128, 128, 128);
      Serial.println("⚙️ Gray");
      break;
    case 420:
      applyColor(0, 0, 0);
      Serial.println("Off");
      break;
    default:
      Serial.println("❌ Invalid input. Please enter a valid code, B:<1-100>, "
                     "or #RRGGBB.");
      printHelp();
    }
  }
}
