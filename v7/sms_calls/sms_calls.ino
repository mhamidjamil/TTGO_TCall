#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "secrets.h"
#include <Wire.h>
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include <TinyGsmClient.h>
#include "ConfigManager.h"
#include "WebDashboard.h"
#include "SMSManager.h"
#include "CallManager.h"
#include "SPIFFSUtils.h"

ConfigManager configManager;
SMSManager smsManager(configManager);
CallManager callManager(configManager);
WebDashboard dashboard(configManager, &smsManager, &callManager);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting SMS & Calls Project");

  // Initialize SPIFFS via helper
  if (!spiffsBegin(true)) {
    Serial.println("Failed to mount SPIFFS (helper)");
  }

  // Use helper to list root
  spiffsListRoot();

  // Check/read/create version file using helper
  const char *verPath = "/version.txt";
  if (spiffsExists(verPath)) {
    Serial.println(String("Found existing version file: ") + verPath + " — reading (will not overwrite)");
    String contents = spiffsReadRaw(verPath);
    if (contents.length()) {
      Serial.println("SPIFFS version file contents:");
      Serial.print(contents);
    } else {
      Serial.println(String("Failed to read existing version file: ") + verPath);
    }
  } else if (false) {
    // Create initial version file only if missing
    String content = String("v7 sms_calls dashboard\n") + "built:" __DATE__ " " __TIME__ "\n";
    if (spiffsWriteFileIfMissing(verPath, content)) {
      Serial.println(String("Created SPIFFS version file: ") + verPath);
    } else {
      Serial.println(String("Failed to create SPIFFS version file: ") + verPath);
    }
  }

  // Dump /data directory with helper
  spiffsDumpDataDir();
  // Print SPIFFS usage info
  Serial.println("--- SPIFFS info ---");
  Serial.print(spiffsInfo());

  // Look for dashboard.html in several common locations (prefer /data/...)
  const char *candidates[] = {"/data/dashboard.html", "/dashboard.html", "/data/index.html", "/index.html"};
  String found = spiffsFindFile(candidates, 4);
  String firstLines = "";
  if (found.length()) {
    Serial.println(String("Found dashboard candidate: ") + found);
    firstLines = spiffsReadFirstLines(found.c_str(), 5);
  }
  if (firstLines.length()) {
    Serial.println("--- First 5 lines of dashboard.html ---");
    Serial.print(firstLines);
    Serial.println("--- end first lines ---");
  } else {
    Serial.println("dashboard.html not found in common locations (first-lines check)");
  }

  // Print first 5 lines of every file at SPIFFS root (helpful when uploader placed files at root)
  Serial.println("--- First 5 lines of root files ---");
  Serial.print(spiffsDumpRootFirstLines(5));

  configManager.begin();

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
  } else {
    Serial.println("WiFi not connected");
  }

  // Start dashboard (web server + API)
  dashboard.begin();

  // Initialize modem-related managers (they'll use Serial1 by default)
  smsManager.begin();
  callManager.begin();
}

void loop() {
  dashboard.handleClient();
  smsManager.loop();
  callManager.loop();
  delay(1);
}
