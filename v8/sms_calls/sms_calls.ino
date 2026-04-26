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
#include "ThingSpeakManager.h"
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
static ThingSpeakManager thingSpeakManager;
static WebDashboard webDashboard;

static unsigned long lastUiRefresh = 0;
static unsigned long lastCloudPoll = 0;
static unsigned long lastTelemetryPush = 0;
static unsigned long lastThingSpeakPush = 0;
static unsigned long lastRuntimeSettingsSync = 0;
static unsigned long pendingPollStartMs = 0;
static V8Config runtimeConfig;
static String startupBootTime;
static String startupIp;
static bool lastTelemetryPushOk = false;
static String lastTelemetryPushMessage = "not_attempted";
static unsigned long telemetryIntervalMs = 15000UL;
static unsigned long thingSpeakIntervalMs = 15000UL;
static const unsigned long runtimeSettingsSyncIntervalMs = 10UL * 60UL * 1000UL;
static bool showFirebasePushLogs = true;
static bool showThingSpeakPushLogs = true;

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

static void syncCountersFromCloud() {
  int daily = 0;
  int weekly = 0;
  int monthly = 0;
  if (firebaseManager.fetchCounterSnapshot(daily, weekly, monthly)) {
    rateLimitManager.loadSnapshot(daily, weekly, monthly);
    Logger::info("RATE_LIMIT", "Counters restored from Firebase");
  } else {
    Logger::warn("RATE_LIMIT", firebaseManager.lastError().c_str());
  }
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

static void printCommandHelp() {
  Serial.println("[CMD] available commands:");
  Serial.println(" - dht    : print temperature and humidity");
  Serial.println(" - status : print wifi/ip/firebase status");
  Serial.println(" - sync   : sync runtime settings from Firebase now");
  Serial.println(" - help   : show this command list");
}

static void printRuntimeSettingChange(const char *name, const String &oldValue, const String &newValue) {
  Serial.println();
  Serial.println("============================");
  Serial.print("old value of variable (");
  Serial.print(name);
  Serial.print("): ");
  Serial.print(oldValue);
  Serial.print("    |   new value is: ");
  Serial.println(newValue);
  Serial.println("============================");
  Serial.println();
}

static bool syncRuntimeSettingsFromCloud(const char *source) {
  if (!firebaseManager.isReady()) {
    Logger::warn("FIREBASE", "Runtime settings sync skipped: firebase not ready");
    return false;
  }

  const uint32_t defaultIntervalOfDhtSeconds = 15;
  const bool defaultShowFirebasePushLogs = true;
  const bool defaultShowThingSpeakPushLogs = true;

  FirebaseRuntimeSettings settings;
  if (!firebaseManager.fetchRuntimeSettings(settings, defaultIntervalOfDhtSeconds, defaultShowFirebasePushLogs, defaultShowThingSpeakPushLogs)) {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    return false;
  }

  unsigned long oldIntervalSeconds = telemetryIntervalMs / 1000UL;
  bool oldShowFirebasePushLogs = showFirebasePushLogs;
  bool oldShowThingSpeakPushLogs = showThingSpeakPushLogs;
  int oldDailyLimit = runtimeConfig.dailySmsLimit;
  int oldWeeklyLimit = runtimeConfig.weeklySmsLimit;
  int oldMonthlyLimit = runtimeConfig.monthlySmsLimit;

  telemetryIntervalMs = (unsigned long)settings.intervalOfDhtSeconds * 1000UL;
  thingSpeakIntervalMs = telemetryIntervalMs < 15000UL ? 15000UL : telemetryIntervalMs;
  showFirebasePushLogs = settings.showFirebasePushLogs;
  showThingSpeakPushLogs = settings.showThingSpeakPushLogs;
  runtimeConfig.dailySmsLimit = settings.dailySmsLimit;
  runtimeConfig.weeklySmsLimit = settings.weeklySmsLimit;
  runtimeConfig.monthlySmsLimit = settings.monthlySmsLimit;
  rateLimitManager.setLimits(runtimeConfig.dailySmsLimit, runtimeConfig.weeklySmsLimit, runtimeConfig.monthlySmsLimit);

  if (settings.createdIntervalOfDht) {
    Serial.println("[SYNC] created or healed Firebase variable: intervalOfDhtSeconds");
  }
  if (settings.createdShowFirebasePushLogs) {
    Serial.println("[SYNC] created or healed Firebase variable: showFirebasePushLogs");
  }
  if (settings.createdShowThingSpeakPushLogs) {
    Serial.println("[SYNC] created or healed Firebase variable: showThingSpeakPushLogs");
  }
  if (settings.createdDailySmsLimit) {
    Serial.println("[SYNC] created or healed Firebase variable: dailySmsLimit");
  }
  if (settings.createdWeeklySmsLimit) {
    Serial.println("[SYNC] created or healed Firebase variable: weeklySmsLimit");
  }
  if (settings.createdMonthlySmsLimit) {
    Serial.println("[SYNC] created or healed Firebase variable: monthlySmsLimit");
  }

  if (oldIntervalSeconds != settings.intervalOfDhtSeconds) {
    printRuntimeSettingChange("intervalOfDhtSeconds", String(oldIntervalSeconds), String(settings.intervalOfDhtSeconds));
  }
  if (oldShowFirebasePushLogs != showFirebasePushLogs) {
    printRuntimeSettingChange("showFirebasePushLogs", oldShowFirebasePushLogs ? "true" : "false", showFirebasePushLogs ? "true" : "false");
  }
  if (oldShowThingSpeakPushLogs != showThingSpeakPushLogs) {
    printRuntimeSettingChange("showThingSpeakPushLogs", oldShowThingSpeakPushLogs ? "true" : "false", showThingSpeakPushLogs ? "true" : "false");
  }
  if (oldDailyLimit != runtimeConfig.dailySmsLimit) {
    printRuntimeSettingChange("dailySmsLimit", String(oldDailyLimit), String(runtimeConfig.dailySmsLimit));
  }
  if (oldWeeklyLimit != runtimeConfig.weeklySmsLimit) {
    printRuntimeSettingChange("weeklySmsLimit", String(oldWeeklyLimit), String(runtimeConfig.weeklySmsLimit));
  }
  if (oldMonthlyLimit != runtimeConfig.monthlySmsLimit) {
    printRuntimeSettingChange("monthlySmsLimit", String(oldMonthlyLimit), String(runtimeConfig.monthlySmsLimit));
  }

  Serial.print("[SYNC] source=");
  Serial.print(source);
  Serial.print(" intervalOfDhtSeconds=");
  Serial.print(settings.intervalOfDhtSeconds);
  Serial.print(" showFirebasePushLogs=");
  Serial.print(showFirebasePushLogs ? "true" : "false");
  Serial.print(" showThingSpeakPushLogs=");
  Serial.println(showThingSpeakPushLogs ? "true" : "false");

  lastRuntimeSettingsSync = millis();
  return true;
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
  if (command == "help" || command == "?") {
    printCommandHelp();
    return;
  }
  if (command == "sync") {
    if (syncRuntimeSettingsFromCloud("manual")) {
      Serial.println("[SYNC] manual sync complete");
    } else {
      Serial.println("[SYNC] manual sync failed");
    }
    return;
  }
  printCommandHelp();
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

  rateLimitManager.begin(runtimeConfig);
  if (thingSpeakManager.begin(runtimeConfig)) {
    Logger::info("THINGSPEAK", "ThingSpeak manager ready");
  } else {
    Logger::warn("THINGSPEAK", thingSpeakManager.lastError().c_str());
  }

  firebaseManager.begin(runtimeConfig);
  if (firebaseManager.isReady()) {
    Logger::info("FIREBASE", "Firebase manager ready");
    syncCountersFromCloud();
    syncRuntimeSettingsFromCloud("startup");
  } else {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
  }

  if (!smsManager.begin()) {
    Logger::warn("SMS", "SMS manager init failed");
  }
  callManager.begin();
  displayManager.begin();
  dhtManager.begin();
  webDashboard.begin(runtimeConfig, wifiManager, firebaseManager);

  startupIp = wifiManager.localIp().toString();
  Logger::info("BOOT", startupIp.c_str());
  Logger::info("API", webDashboard.docsUrl().c_str());
  pendingPollStartMs = millis() + 60000UL;

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

  if (firebaseManager.isReady() && millis() - lastRuntimeSettingsSync >= runtimeSettingsSyncIntervalMs) {
    syncRuntimeSettingsFromCloud("periodic");
  }

  if (firebaseManager.isReady() && millis() >= pendingPollStartMs &&
      millis() - lastCloudPoll >= (unsigned long)runtimeConfig.pollingIntervalSeconds * 1000UL) {
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

    String telemetryState = lastTelemetryPushOk ? "ok" : "fail";
    displayManager.update(
        temperature,
        humidity,
        wifiManager.modeName().c_str(),
        firebaseManager.isReady() ? telemetryState.c_str() : "LOCAL",
        rateLimitManager.dailyCount(),
        rateLimitManager.weeklyCount(),
        rateLimitManager.monthlyCount());

    if (firebaseManager.isReady() && millis() - lastTelemetryPush > telemetryIntervalMs) {
      lastTelemetryPush = millis();
      time_t now = time(nullptr);
      unsigned long epochSeconds = (now > 1000) ? (unsigned long)now : (millis() / 1000UL);
      lastTelemetryPushOk = firebaseManager.pushTelemetry(
          temperature,
          humidity,
          epochSeconds);
      lastTelemetryPushMessage = lastTelemetryPushOk ? "Telemetry pushed" : firebaseManager.lastError();
      if (lastTelemetryPushOk) {
        if (showFirebasePushLogs) {
          Logger::info("FIREBASE", lastTelemetryPushMessage.c_str());
        }
      } else {
        Logger::warn("FIREBASE", lastTelemetryPushMessage.c_str());
      }

      if (thingSpeakManager.isReady() && millis() - lastThingSpeakPush > thingSpeakIntervalMs) {
        lastThingSpeakPush = millis();
        bool thingSpeakOk = thingSpeakManager.update(temperature, humidity);
        if (thingSpeakOk) {
          if (showThingSpeakPushLogs) {
            Logger::info("THINGSPEAK", "Temperature and humidity pushed");
          }
        } else {
          Logger::warn("THINGSPEAK", thingSpeakManager.lastError().c_str());
        }
      }

      bool landingOk = firebaseManager.pushLandingSnapshot(
          temperature,
          humidity,
          rateLimitManager.dailyCount(),
          rateLimitManager.weeklyCount(),
          rateLimitManager.monthlyCount(),
          wifiManager.modeName(),
          startupIp,
          firebaseManager.isReady(),
          lastTelemetryPushOk,
          lastTelemetryPushMessage,
          epochSeconds);
      if (landingOk) {
        if (showFirebasePushLogs) {
          Logger::info("FIREBASE", "Landing snapshot pushed");
        }
      } else {
        Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
      }
    }
  }
}
