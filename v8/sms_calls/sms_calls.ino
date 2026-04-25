#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#include "ConfigManager.h"
#include "WiFiManager.h"
#include "FirebaseManager.h"
#include "SMSManager.h"
#include "CallManager.h"
#include "DisplayManager.h"
#include "DHTManager.h"
#include "WebDashboard.h"
#include "Logger.h"

static ConfigManager configManager;
static WiFiManager wifiManager;
static FirebaseManager firebaseManager;
static SMSManager smsManager;
static CallManager callManager;
static DisplayManager displayManager;
static DHTManager dhtManager;
static WebDashboard webDashboard;

static unsigned long lastUiRefresh = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Logger::info("BOOT", "Starting v8 runtime");

  configManager.begin();
  const V8Config &config = configManager.get();

  wifiManager.begin(config);
  Logger::info("WIFI", wifiManager.modeName().c_str());

  firebaseManager.begin(config);
  if (firebaseManager.isReady()) {
    Logger::info("FIREBASE", "Firebase manager ready");
  } else {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
  }

  smsManager.begin();
  callManager.begin();
  displayManager.begin();
  dhtManager.begin();
  webDashboard.begin(config, wifiManager, firebaseManager);

  String startupIp = wifiManager.localIp().toString();
  Logger::info("BOOT", startupIp.c_str());
  Logger::info("API", webDashboard.docsUrl().c_str());
}

void loop() {
  firebaseManager.pollCommands();
  callManager.loop();
  webDashboard.loop();

  if (millis() - lastUiRefresh > 3000) {
    lastUiRefresh = millis();
    float temperature = dhtManager.readTemperature();
    float humidity = dhtManager.readHumidity();
    displayManager.update(temperature, humidity, wifiManager.modeName().c_str(), firebaseManager.isReady() ? "FIREBASE" : "LOCAL");
  }
}
