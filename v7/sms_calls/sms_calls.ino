#include <Arduino.h>
#include <WiFi.h>
// SPIFFS removed for v7 runtime; using embedded/dashboard defaults and secrets.h
#include "secrets.h"
#include <Wire.h>
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include <TinyGsmClient.h>
#include "ConfigManager.h"
#include "WebDashboard.h"
#include "SMSManager.h"
#include "CallManager.h"
#include "DisplayManager.h"
// SPIFFSUtils removed from v7 runtime

ConfigManager configManager;
SMSManager smsManager(configManager);
CallManager callManager(configManager);
WebDashboard dashboard(configManager, &smsManager, &callManager);
DisplayManager displayManager;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting SMS & Calls Project");

  // SPIFFS disabled for v7 build — using embedded dashboard and secrets.h values
  Serial.println("SPIFFS disabled in this build; using embedded dashboard and secrets.h for defaults");

  configManager.begin();
  // After loading defaults from secrets.h and persisted settings.
  // NOTE: remote settings may perform HTTP requests — defer that until
  // after we have a WiFi connection (see below) to avoid running network
  // code while network stack isn't ready.

  // --- Modem / power initialization (copied from v6 ModemManager)
  // pin definitions (adjust if your board wiring differs)
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

  // Ensure power boost keep on (IP5306) so module stays powered from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  // write to IP5306 register 0x00 to set keep-on bit
  Wire.beginTransmission(0x75);
  Wire.write(0x00);
  Wire.write(0x37);
  Wire.endTransmission();

  // setup modem power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // start serial1 for modem with pins
  Serial1.begin(MODEM_SERIAL_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  // TinyGSM modem instance and restart to initialize SIM module
  TinyGsm modem(Serial1);
  delay(3000);
  Serial.println("Initializing modem...");
  modem.restart();

  // Now connect to WiFi using secrets.h values
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    // Now that WiFi is connected, it's safe to check remote settings
    if (configManager.checkAndApplyRemoteSettings()) {
      Serial.println("Applied remote settings on startup");
    } else {
      Serial.println("No remote settings applied on startup");
    }
  } else {
    Serial.println("WiFi not connected");
  }

  // Start dashboard (web server + API)
  dashboard.begin();

  // Initialize modem-related managers (they'll use Serial1 by default)
  smsManager.begin();
  callManager.begin();

  // Initialize the display
  displayManager.initialize();

  // Example: Update the display with dummy data
  displayManager.update(25.5, 60.0, 10, 5, 3, 2, true, true);
}

void loop() {
  dashboard.handleClient();
  smsManager.loop();
  callManager.loop();
  delay(1);
}
