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
#include "NtfyManager.h"

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
static NtfyManager ntfyManager;

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
static const size_t maxBlockedNumbers = 32;
static bool showFirebasePushLogs = true;
static bool showThingSpeakPushLogs = true;
static String blockedCallers[maxBlockedNumbers];
static String blockedSmsSenders[maxBlockedNumbers];
static size_t blockedCallerCount = 0;
static size_t blockedSmsSenderCount = 0;
static String modemLineBuffer;
static bool awaitingSmsBody = false;
static String pendingSmsNumber;
static int pendingSmsIndex = -1;
static String lastCallNumber;
static unsigned long lastCallHandledMs = 0;
static const unsigned long duplicateCallWindowMs = 30000UL;

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

static String displayPhoneNumber(const String &raw) {
  String normalized = normalizePhoneNumber(raw);
  return normalized.length() > 0 ? normalized : raw;
}

static bool numbersMatch(const String &left, const String &right) {
  String leftDigits = digitsOnly(left);
  String rightDigits = digitsOnly(right);
  if (leftDigits.length() == 0 || rightDigits.length() == 0) {
    return false;
  }
  if (leftDigits == rightDigits) {
    return true;
  }
  String leftNormalized = normalizePhoneNumber(left);
  String rightNormalized = normalizePhoneNumber(right);
  if (leftNormalized.length() > 0 && rightNormalized.length() > 0 && leftNormalized == rightNormalized) {
    return true;
  }
  return leftDigits.length() >= 10 &&
         rightDigits.length() >= 10 &&
         leftDigits.substring(leftDigits.length() - 10) == rightDigits.substring(rightDigits.length() - 10);
}

static bool isBlockedNumber(const String &number, String *blockedNumbers, size_t blockedCount) {
  for (size_t i = 0; i < blockedCount; ++i) {
    if (numbersMatch(number, blockedNumbers[i])) {
      return true;
    }
  }
  return false;
}

static unsigned long currentEpochSeconds() {
  time_t now = time(nullptr);
  if (now > 1000) {
    return (unsigned long)now;
  }
  return millis() / 1000UL;
}

static String formatEventTime(unsigned long epochSeconds) {
  if (epochSeconds > 1000) {
    time_t eventTime = (time_t)epochSeconds;
    struct tm timeInfo;
    if (localtime_r(&eventTime, &timeInfo)) {
      char buffer[24];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
      return String(buffer);
    }
  }
  return String("uptime ") + String(millis() / 1000UL) + String("s");
}

static String quotedField(const String &line, int fieldIndex) {
  int searchFrom = 0;
  for (int i = 0; i <= fieldIndex; ++i) {
    int startQuote = line.indexOf('"', searchFrom);
    if (startQuote < 0) {
      return String();
    }
    int endQuote = line.indexOf('"', startQuote + 1);
    if (endQuote < 0) {
      return String();
    }
    if (i == fieldIndex) {
      return line.substring(startQuote + 1, endQuote);
    }
    searchFrom = endQuote + 1;
  }
  return String();
}

static bool pushSimEvent(const String &type, const String &number, const String &message, bool blocked, unsigned long epochSeconds) {
  if (!firebaseManager.isReady()) {
    Logger::warn("FIREBASE", "SIM event skipped: firebase not ready");
    return false;
  }
  if (!firebaseManager.pushSimModuleEvent(type, number, message, blocked, epochSeconds)) {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    return false;
  }
  return true;
}

static void processIncomingCall(const String &rawNumber) {
  String number = displayPhoneNumber(rawNumber);
  unsigned long epochSeconds = currentEpochSeconds();
  if (numbersMatch(number, lastCallNumber) && millis() - lastCallHandledMs < duplicateCallWindowMs) {
    Serial.print("[CALL] duplicate ignored number=");
    Serial.println(number);
    return;
  }
  lastCallNumber = number;
  lastCallHandledMs = millis();

  bool blocked = isBlockedNumber(number, blockedCallers, blockedCallerCount);
  bool firebaseOk = pushSimEvent("call", number, String(), blocked, epochSeconds);

  Serial.print("[CALL] incoming number=");
  Serial.print(number);
  Serial.print(" blocked=");
  Serial.print(blocked ? "yes" : "no");
  Serial.print(" time=");
  Serial.print(formatEventTime(epochSeconds));
  Serial.print(" firestore=");
  Serial.println(firebaseOk ? "ok" : "failed");

  if (blocked) {
    Logger::info("CALL", "Blocked caller logged without ntfy");
    return;
  }

  String message = String("Incoming call at ") + formatEventTime(epochSeconds);
  if (ntfyManager.notify(String("call from ") + number, message)) {
    Serial.print("[NTFY] call notification sent number=");
    Serial.println(number);
  } else {
    Logger::warn("NTFY", ntfyManager.lastError().c_str());
  }
}

static void processIncomingSms(const String &rawNumber, const String &message, int smsIndex) {
  String number = displayPhoneNumber(rawNumber);
  unsigned long epochSeconds = currentEpochSeconds();
  bool blocked = isBlockedNumber(number, blockedSmsSenders, blockedSmsSenderCount);
  bool firebaseOk = pushSimEvent("sms", number, message, blocked, epochSeconds);

  Serial.print("[SMS] incoming index=");
  Serial.print(smsIndex);
  Serial.print(" number=");
  Serial.print(number);
  Serial.print(" blocked=");
  Serial.print(blocked ? "yes" : "no");
  Serial.print(" firestore=");
  Serial.print(firebaseOk ? "ok" : "failed");
  Serial.print(" message=");
  Serial.println(message);

  if (blocked) {
    Logger::info("SMS", "Blocked SMS sender logged without ntfy");
  } else {
    if (ntfyManager.notify(String("sms from ") + number, message)) {
      Serial.print("[NTFY] sms notification sent number=");
      Serial.print(number);
      Serial.print(" message=");
      Serial.println(message);
    } else {
      Logger::warn("NTFY", ntfyManager.lastError().c_str());
    }
  }

  if (firebaseOk && smsIndex > 0) {
    if (smsManager.deleteMessage(smsIndex)) {
      Serial.print("[SMS] deleted from SIM index=");
      Serial.println(smsIndex);
    } else {
      Serial.print("[SMS] delete failed index=");
      Serial.println(smsIndex);
    }
  } else if (!firebaseOk && smsIndex > 0) {
    Serial.print("[SMS] kept on SIM because Firestore upload failed index=");
    Serial.println(smsIndex);
  }
}

static void handleModemLine(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (awaitingSmsBody) {
    awaitingSmsBody = false;
    processIncomingSms(pendingSmsNumber, line, pendingSmsIndex);
    pendingSmsNumber = String();
    pendingSmsIndex = -1;
    return;
  }

  if (line.startsWith("+CLIP:")) {
    String callerNumber = quotedField(line, 0);
    callManager.hangUp();
    processIncomingCall(callerNumber);
    return;
  }

  if (line.startsWith("+CMT:")) {
    pendingSmsNumber = quotedField(line, 0);
    awaitingSmsBody = pendingSmsNumber.length() > 0;
    return;
  }

  if (line.startsWith("+CMGR:")) {
    pendingSmsNumber = quotedField(line, 1);
    awaitingSmsBody = pendingSmsNumber.length() > 0;
    return;
  }

  if (line.startsWith("+CMTI:")) {
    int comma = line.lastIndexOf(',');
    if (comma > 0) {
      String index = line.substring(comma + 1);
      index.trim();
      pendingSmsIndex = index.toInt();
      Serial1.print("AT+CMGR=");
      Serial1.println(index);
    }
  }
}

static void handleModemEvents() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      handleModemLine(modemLineBuffer);
      modemLineBuffer = String();
      continue;
    }
    modemLineBuffer += c;
    if (modemLineBuffer.length() > 512) {
      modemLineBuffer = String();
    }
  }
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
  Serial.println(" - ntfy test | test ntfy : send ntfy test notification");
  Serial.println(" - show sms : list all SMS messages with SIM indexes");
  Serial.println(" - delete sms <index> : delete one SMS by SIM index");
  Serial.println(" - delete all sms : delete every SMS from SIM memory");
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
  if (!firebaseManager.fetchRuntimeSettings(settings, defaultIntervalOfDhtSeconds, defaultShowFirebasePushLogs, defaultShowThingSpeakPushLogs, String(runtimeConfig.ntfyUrl))) {
    Logger::warn("FIREBASE", firebaseManager.lastError().c_str());
    return false;
  }

  unsigned long oldIntervalSeconds = telemetryIntervalMs / 1000UL;
  bool oldShowFirebasePushLogs = showFirebasePushLogs;
  bool oldShowThingSpeakPushLogs = showThingSpeakPushLogs;
  int oldDailyLimit = runtimeConfig.dailySmsLimit;
  int oldWeeklyLimit = runtimeConfig.weeklySmsLimit;
  int oldMonthlyLimit = runtimeConfig.monthlySmsLimit;
  String oldNtfyUrl = runtimeConfig.ntfyUrl;

  telemetryIntervalMs = (unsigned long)settings.intervalOfDhtSeconds * 1000UL;
  thingSpeakIntervalMs = telemetryIntervalMs < 15000UL ? 15000UL : telemetryIntervalMs;
  showFirebasePushLogs = settings.showFirebasePushLogs;
  showThingSpeakPushLogs = settings.showThingSpeakPushLogs;
  runtimeConfig.dailySmsLimit = settings.dailySmsLimit;
  runtimeConfig.weeklySmsLimit = settings.weeklySmsLimit;
  runtimeConfig.monthlySmsLimit = settings.monthlySmsLimit;
  strlcpy(runtimeConfig.ntfyUrl, settings.ntfyUrl.c_str(), sizeof(runtimeConfig.ntfyUrl));
  ntfyManager.setUrl(settings.ntfyUrl);
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
  if (settings.createdNtfyUrl) {
    Serial.println("[SYNC] created or healed Firebase variable: ntfyUrl");
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
  if (oldNtfyUrl != String(runtimeConfig.ntfyUrl)) {
    printRuntimeSettingChange("ntfyUrl", oldNtfyUrl, String(runtimeConfig.ntfyUrl));
  }

  size_t oldBlockedCallerCount = blockedCallerCount;
  size_t oldBlockedSmsSenderCount = blockedSmsSenderCount;
  if (firebaseManager.fetchSimBlockLists(
          blockedCallers,
          maxBlockedNumbers,
          blockedCallerCount,
          blockedSmsSenders,
          maxBlockedNumbers,
          blockedSmsSenderCount)) {
    if (oldBlockedCallerCount != blockedCallerCount) {
      printRuntimeSettingChange("blocked_callers_count", String(oldBlockedCallerCount), String(blockedCallerCount));
    }
    if (oldBlockedSmsSenderCount != blockedSmsSenderCount) {
      printRuntimeSettingChange("blocked_sms_senders_count", String(oldBlockedSmsSenderCount), String(blockedSmsSenderCount));
    }
  } else {
    Logger::warn("FIRESTORE", firebaseManager.lastError().c_str());
  }

  Serial.print("[SYNC] source=");
  Serial.print(source);
  Serial.print(" intervalOfDhtSeconds=");
  Serial.print(settings.intervalOfDhtSeconds);
  Serial.print(" showFirebasePushLogs=");
  Serial.print(showFirebasePushLogs ? "true" : "false");
  Serial.print(" showThingSpeakPushLogs=");
  Serial.print(showThingSpeakPushLogs ? "true" : "false");
  Serial.print(" blockedCallers=");
  Serial.print(blockedCallerCount);
  Serial.print(" blockedSmsSenders=");
  Serial.println(blockedSmsSenderCount);

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
  if (command == "ntfy test" || command == "test ntfy") {
    if (ntfyManager.test()) {
      Serial.println("[NTFY] test notification sent");
    } else {
      Serial.print("[NTFY] test failed: ");
      Serial.println(ntfyManager.lastError());
    }
    return;
  }
  if (command == "show sms") {
    Serial.println("[SMS] listing SIM messages");
    Serial.println(smsManager.listMessages());
    return;
  }
  if (command == "delete all sms" || command == "delete sms all") {
    if (smsManager.deleteAllMessages()) {
      Serial.println("[SMS] all SIM messages deleted");
    } else {
      Serial.println("[SMS] delete all failed");
    }
    return;
  }
  if (command.startsWith("delete sms ")) {
    String indexText = command.substring(String("delete sms ").length());
    indexText.trim();
    int index = indexText.toInt();
    if (index <= 0) {
      Serial.println("[SMS] invalid delete index");
      return;
    }
    if (smsManager.deleteMessage(index)) {
      Serial.print("[SMS] deleted index=");
      Serial.println(index);
    } else {
      Serial.print("[SMS] delete failed index=");
      Serial.println(index);
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
  ntfyManager.begin(runtimeConfig.ntfyUrl);

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
  handleModemEvents();
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
