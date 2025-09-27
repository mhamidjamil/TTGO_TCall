#include <Arduino.h>
#include <Wire.h>
#include <time.h>

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#include "ConfigManager.h"
#include "WiFiManager.h"
#include "FirebaseManager.h"
#include "RateLimitManager.h"
#include "SMSManager.h"
#include "CallManager.h"
#include "DisplayManager.h"
#include "DHTManager.h"
#include "WebDashboard.h"
#include "Logger.h"

static ConfigManager configManager;
static WiFiManager wifiManager;
static FirebaseManager firebaseManager;
static RateLimitManager rateLimitManager;
static SMSManager smsManager;
static CallManager callManager;
static DisplayManager displayManager;
static DHTManager dhtManager;
static WebDashboard webDashboard;

static unsigned long lastUiRefresh = 0;
static unsigned long lastCloudPoll = 0;
static unsigned long lastTelemetryPush = 0;
static V8Config runtimeConfig;
static String startupBootTime;

static void initializeModemHardware() {
  Wire.begin(I2C_SDA_PIN_DEFAULT, I2C_SCL_PIN_DEFAULT);

  Wire.beginTransmission(0x75);
  Wire.write(0x00);
  Wire.write(0x37);
  Wire.endTransmission();

  pinMode(MODEM_PWKEY_PIN_DEFAULT, OUTPUT);
  pinMode(MODEM_RST_PIN_DEFAULT, OUTPUT);
  pinMode(MODEM_POWER_ON_PIN_DEFAULT, OUTPUT);
  digitalWrite(MODEM_PWKEY_PIN_DEFAULT, LOW);
  digitalWrite(MODEM_RST_PIN_DEFAULT, HIGH);
  digitalWrite(MODEM_POWER_ON_PIN_DEFAULT, HIGH);

  Serial1.begin(MODEM_SERIAL_BAUD_DEFAULT, SERIAL_8N1, MODEM_RX_PIN_DEFAULT, MODEM_TX_PIN_DEFAULT);
  delay(3000);
}

static String digitsOnly(const String &input) {
  String out;
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    if (c >= '0' && c <= '9') {
      out += c;
    }
  }
  return out;
}

static String normalizePhoneNumber(const String &raw) {
  String digits = digitsOnly(raw);
  if (digits.startsWith("00")) {
    digits = digits.substring(2);
  }
  if (digits.startsWith("0") && digits.length() == 11) {
    digits = String(runtimeConfig.defaultCountryCode) + digits.substring(1);
  }
  if (digits.length() == 10) {
    digits = String(runtimeConfig.defaultCountryCode) + digits;
  }
  if (digits.length() < 12) {
    return String();
  }
  return String("+") + digits;
}

static void processPendingCommand() {
  FirebaseCommand cmd;
  if (!firebaseManager.fetchNextCommand(cmd)) {
    return;
  }

  if (cmd.type != "sms") {
    firebaseManager.updateCommandStatus(cmd, "errored", "unsupported_command_type");
    return;
  }

  String normalizedNumber = normalizePhoneNumber(cmd.number);
  if (normalizedNumber.length() == 0) {
    firebaseManager.updateCommandStatus(cmd, "errored", "number_invalid");
    return;
  }

  String limitReason;
  rateLimitManager.sync();
  if (!rateLimitManager.canSend(limitReason)) {
    firebaseManager.updateCommandStatus(cmd, "errored", limitReason);
    return;
  }

  bool sent = smsManager.sendMessage(normalizedNumber, cmd.message);
  if (!sent) {
    firebaseManager.updateCommandStatus(cmd, "errored", "send_failed");
    return;
  }

  rateLimitManager.recordSend();
  firebaseManager.updateCounterSnapshot(rateLimitManager.dailyCount(), rateLimitManager.weeklyCount(), rateLimitManager.monthlyCount());
  firebaseManager.updateCommandStatus(cmd, "sent", String());
}

static void printDhtStatus(const char *source) {
  float temperature = dhtManager.readTemperature();
  float humidity = dhtManager.readHumidity();
  Serial.print("[DHT] ");
  Serial.print(source);
  Serial.print(" temp=");
  if (temperature <= -999.0f) {
    Serial.print("NA");
  } else {
    Serial.print(temperature, 1);
  }
  Serial.print(" hum=");
  if (humidity < 0.0f) {
    Serial.println("NA");
  } else {
    Serial.println(humidity, 1);
  }
}

static void handleSerialCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command == "dht") {
    printDhtStatus("serial");
    return;
  }
  if (command == "status") {
    Serial.print("[STATUS] wifi=");
    Serial.print(wifiManager.modeName());
    Serial.print(" ip=");
    Serial.print(wifiManager.localIp().toString());
    Serial.print(" firebase=");
    Serial.println(firebaseManager.isReady() ? "ready" : "not_ready");
    return;
  }
  Serial.println("[CMD] use: dht | status");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Logger::info("BOOT", "Starting v8 runtime");
  startupBootTime = String(millis()) + String("ms");

  configManager.begin();
  runtimeConfig = configManager.get();

  initializeModemHardware();
  Logger::info("MODEM", "Hardware initialized");

  wifiManager.begin(runtimeConfig);
  Logger::info("WIFI", wifiManager.modeName().c_str());
  if (wifiManager.isStationConnected()) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  }

  firebaseManager.begin(runtimeConfig);
  if (firebaseManager.isReady()) {
    Logger::info("FIREBASE", "Firebase manager ready");
  } else {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
  }

  rateLimitManager.begin(runtimeConfig);

  if (!smsManager.begin()) {
    Logger::warn("SMS", "SMS manager init failed");
  }
  callManager.begin();
  displayManager.begin();
  dhtManager.begin();
  webDashboard.begin(runtimeConfig, wifiManager, firebaseManager);

  String startupIp = wifiManager.localIp().toString();
  Logger::info("BOOT", startupIp.c_str());
  Logger::info("API", webDashboard.docsUrl().c_str());

  if (firebaseManager.isReady()) {
    if (firebaseManager.pushStartupStatus(startupBootTime, wifiManager.modeName(), startupIp, true)) {
      Logger::info("FIREBASE", "Startup status pushed");
    } else {
      Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    }
  }
}

void loop() {
  firebaseManager.pollCommands();
  if (millis() - lastCloudPoll >= (unsigned long)runtimeConfig.pollingIntervalSeconds * 1000UL) {
    lastCloudPoll = millis();
    processPendingCommand();
  }

  callManager.loop();
  webDashboard.loop();

  while (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleSerialCommand(command);
  }

  if (millis() - lastUiRefresh > 3000) {
    lastUiRefresh = millis();
    float temperature = dhtManager.readTemperature();
    float humidity = dhtManager.readHumidity();
    displayManager.update(
        temperature,
        humidity,
        wifiManager.modeName().c_str(),
        firebaseManager.isReady() ? "FIREBASE" : "LOCAL",
        rateLimitManager.dailyCount(),
        rateLimitManager.weeklyCount(),
        rateLimitManager.monthlyCount());

    if (firebaseManager.isReady() && millis() - lastTelemetryPush > 15000) {
      lastTelemetryPush = millis();
      time_t now = time(nullptr);
      unsigned long epochSeconds = (now > 1000) ? (unsigned long)now : (millis() / 1000UL);
      firebaseManager.pushTelemetry(
          temperature,
          humidity,
          rateLimitManager.dailyCount(),
          rateLimitManager.weeklyCount(),
          rateLimitManager.monthlyCount(),
          epochSeconds);
      Logger::info("FIREBASE", "Telemetry pushed");
    }
  }
}
