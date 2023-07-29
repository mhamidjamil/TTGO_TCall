//$ 23/JULY/2023 08:22 AM
// # First success with the OLED display
//~ Version 1.0
#include <U8g2lib.h>
#include <Wire.h>

#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>

// TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA,
                                         U8X8_PIN_NONE);

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  Serial.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  Serial1.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("Initializing modem...");
  TinyGsm modem(Serial1);
  modem.restart();

  // Initialize the OLED display
  u8g2.begin();
  u8g2.setPowerSave(0); // Wake up the display

  // Print a message on the OLED display
  u8g2.setFont(u8g2_font_logisoso16_tf); // Set the font
  String displayText = String(random(99)) + ", ESP32-S3!";
  int yPosition = 25;           // Adjust the vertical position here (in pixels)
  u8g2.setCursor(0, yPosition); // Set the cursor position
  u8g2.print(displayText.c_str()); // Draw the text
  u8g2.sendBuffer();               // Send the buffer to display
}

void loop() {
  // Change display on OLED display
  u8g2.clearBuffer(); // Clear the internal memory
  u8g2.setFont(u8g2_font_logisoso16_tf);
  int yPosition = 25;           // Adjust the vertical position here (in pixels)
  u8g2.setCursor(0, yPosition); // Set the cursor position
  u8g2.print(("Hello World!" + String(random(99))).c_str()); // Draw the text
  u8g2.sendBuffer(); // Transfer internal memory to the display
  delay(1000);
}
