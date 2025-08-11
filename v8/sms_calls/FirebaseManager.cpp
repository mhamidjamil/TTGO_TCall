#include "FirebaseManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

#if __has_include(<Firebase_ESP_Client.h>)
#include <Firebase_ESP_Client.h>
#define V8_HAS_FIREBASE_CLIENT 1
#else
#define V8_HAS_FIREBASE_CLIENT 0
class FirebaseAuth {};
class FirebaseConfig {};
class FirebaseData {};
class Firebase_ESP_Client {
public:
  void begin(FirebaseConfig *, FirebaseAuth *) {}
  bool ready() const { return false; }
};
#endif

namespace {
#if V8_HAS_FIREBASE_CLIENT
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbconfig;
Firebase_ESP_Client firebase;
#endif

void setHttpStatusError(String &error, const char *op, int statusCode, const String &body) {
  error = String(op) + String(" failed code=") + statusCode + String(" body=") + body;
}

bool parseBoolVariant(const JsonVariantConst &variant, bool &outValue) {
  if (variant.is<bool>()) {
    outValue = variant.as<bool>();
    return true;
  }
  if (variant.is<int>()) {
    outValue = variant.as<int>() != 0;
    return true;
  }
  if (variant.is<const char *>()) {
    String text = variant.as<const char *>();
    text.toLowerCase();
    if (text == "true" || text == "1" || text == "yes" || text == "on") {
      outValue = true;
      return true;
    }
    if (text == "false" || text == "0" || text == "no" || text == "off") {
      outValue = false;
      return true;
    }
  }
  return false;
}

bool parseIntervalVariant(const JsonVariantConst &variant, uint32_t &outValue) {
  long parsed = -1;
  if (variant.is<int>()) {
    parsed = variant.as<int>();
  } else if (variant.is<const char *>()) {
    parsed = String(variant.as<const char *>()).toInt();
  } else {
    return false;
  }

  if (parsed < 3 || parsed > 3600) {
    return false;
  }

  outValue = (uint32_t)parsed;
  return true;
}

bool parseLimitVariant(const JsonVariantConst &variant, int &outValue) {
  long parsed = -1;
  if (variant.is<int>()) {
    parsed = variant.as<int>();
  } else if (variant.is<const char *>()) {
    parsed = String(variant.as<const char *>()).toInt();
  } else {
    return false;
  }

  if (parsed < 0 || parsed > 100000) {
    return false;
  }

  outValue = (int)parsed;
  return true;
}
}

bool FirebaseManager::begin(const V8Config &incomingConfig) {
  config = incomingConfig;
  ready = false;
  tokenExpiresAtMs = 0;
  idToken = String();
  error = String();

  if (String(config.firebaseProjectId).isEmpty() || String(config.firebaseDatabaseUrl).isEmpty() ||
      String(config.firebaseApiKey).isEmpty()) {
    error = "Firebase config missing";
    return false;
  }

  if (!config.firebaseUseAnonymous &&
      (String(config.firebaseUserEmail).isEmpty() || String(config.firebaseUserPassword).isEmpty())) {
    error = "Firebase email/password missing";
    return false;
  }

#if V8_HAS_FIREBASE_CLIENT
  fbconfig.api_key = config.firebaseApiKey;
  fbconfig.database_url = config.firebaseDatabaseUrl;
  auth.user.email = config.firebaseUserEmail;
  auth.user.password = config.firebaseUserPassword;
  firebase.begin(&fbconfig, &auth);
  ready = authenticate();
#else
  // We still support Firebase through HTTPS REST in this build.
  ready = authenticate();
#endif

  if (ready && !bootstrapPaths()) {
    ready = false;
  }

  return ready;
}

bool FirebaseManager::isReady() const {
  return ready;
}

bool FirebaseManager::pollCommands() {
  if (!ready) {
    return false;
  }

  return ensureAuthenticated();
}

bool FirebaseManager::fetchNextCommand(FirebaseCommand &outCommand) {
  if (!ensureAuthenticated()) {
    return false;
  }

  String url = buildPathUrl(String(config.firebaseCommandPath));
  String body;
  int statusCode = 0;
  if (!httpGetJson(url, body, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300 || body == "null") {
    return false;
  }

  DynamicJsonDocument doc(6144);
  if (deserializeJson(doc, body)) {
    error = "command parse failed";
    return false;
  }

  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    JsonObject cmdObj = kv.value().as<JsonObject>();
    String status = cmdObj["status"] | "pending";
    if (!(status == "pending" || status == "unsent" || status == "new")) {
      continue;
    }

    outCommand.id = String(kv.key().c_str());
    outCommand.type = cmdObj["type"] | "sms";
    outCommand.number = cmdObj["number"] | "";
    outCommand.message = cmdObj["message"] | "";
    outCommand.status = status;
    outCommand.errorReason = String();

    String claimUrl = buildPathUrl(String(config.firebaseCommandPath) + "/" + outCommand.id);
    DynamicJsonDocument claimDoc(256);
    claimDoc["status"] = "processing";
    claimDoc["claimedAtMs"] = millis();
    String payload;
    serializeJson(claimDoc, payload);
    String claimResp;
    int claimCode = 0;
    if (!httpPatchJson(claimUrl, payload, claimResp, claimCode) || claimCode < 200 || claimCode >= 300) {
      error = "claim failed";
      return false;
    }
    return true;
  }

  return false;
}

bool FirebaseManager::updateCommandStatus(const FirebaseCommand &command, const String &status, const String &errorReason) {
  if (!ensureAuthenticated()) {
    return false;
  }

  String commandUrl = buildPathUrl(String(config.firebaseCommandPath) + "/" + command.id);
  DynamicJsonDocument statusDoc(512);
  statusDoc["status"] = status;
  statusDoc["processedAtMs"] = millis();
  if (errorReason.length() > 0) {
    statusDoc["errorReason"] = errorReason;
  }
  String commandPayload;
  serializeJson(statusDoc, commandPayload);

  String commandResp;
  int commandCode = 0;
  if (!httpPatchJson(commandUrl, commandPayload, commandResp, commandCode) || commandCode < 200 || commandCode >= 300) {
    if (commandCode < 200 || commandCode >= 300) {
      setHttpStatusError(error, "status update", commandCode, commandResp);
    }
    return false;
  }

  DynamicJsonDocument historyDoc(768);
  historyDoc["commandId"] = command.id;
  historyDoc["type"] = command.type;
  historyDoc["number"] = command.number;
  historyDoc["status"] = status;
  historyDoc["processedAtMs"] = millis();
  if (errorReason.length() > 0) {
    historyDoc["errorReason"] = errorReason;
  }
  String historyPayload;
  serializeJson(historyDoc, historyPayload);

  String historyResp;
  int historyCode = 0;
  String historyUrl = buildPathUrl(String(config.firebaseHistoryPath));
  if (!httpPatchJson(historyUrl, String("{\"") + command.id + String("\":") + historyPayload + String("}"), historyResp, historyCode)) {
    return false;
  }

  if (historyCode < 200 || historyCode >= 300) {
    setHttpStatusError(error, "history update", historyCode, historyResp);
    return false;
  }

  return true;
}

bool FirebaseManager::updateCounterSnapshot(int dailyCount, int weeklyCount, int monthlyCount) {
  if (!ensureAuthenticated()) {
    return false;
  }

  DynamicJsonDocument doc(256);
  doc["sentToday"] = dailyCount;
  doc["sentWeek"] = weeklyCount;
  doc["sentMonth"] = monthlyCount;
  doc["updatedAtMs"] = millis();

  String payload;
  serializeJson(doc, payload);

  String response;
  int statusCode = 0;
  if (!httpPatchJson(buildPathUrl(String(config.firebaseCounterPath)), payload, response, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "counter update", statusCode, response);
    return false;
  }

  return true;
}

bool FirebaseManager::fetchCounterSnapshot(int &dailyCount, int &weeklyCount, int &monthlyCount) {
  if (!ensureAuthenticated()) {
    return false;
  }

  String response;
  int statusCode = 0;
  if (!httpGetJson(buildPathUrl(String(config.firebaseCounterPath)), response, statusCode)) {
    return false;
  }

  if (statusCode == 404 || response == "null") {
    dailyCount = 0;
    weeklyCount = 0;
    monthlyCount = 0;
    if (!updateCounterSnapshot(0, 0, 0)) {
      return false;
    }
    error = String();
    return true;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "counter fetch", statusCode, response);
    return false;
  }

  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, response)) {
    error = String("counter parse failed body=") + response;
    return false;
  }

  dailyCount = doc["sentToday"] | 0;
  weeklyCount = doc["sentWeek"] | 0;
  monthlyCount = doc["sentMonth"] | 0;
  return true;
}

String FirebaseManager::lastError() const {
  return error;
}

bool FirebaseManager::ensureAuthenticated() {
  if (!ready) {
    return false;
  }
  if (idToken.length() == 0 || millis() > tokenExpiresAtMs) {
    return authenticate();
  }
  return true;
}

bool FirebaseManager::authenticate() {
  if (WiFi.status() != WL_CONNECTED) {
    error = "wifi disconnected";
    return false;
  }

  auto authWithMode = [&](bool useAnonymous, String &outBody, int &outCode) -> bool {
    String url;
    DynamicJsonDocument authDoc(512);
    if (useAnonymous) {
      url = String("https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=") + config.firebaseApiKey;
      authDoc["returnSecureToken"] = true;
    } else {
      url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + config.firebaseApiKey;
      authDoc["email"] = config.firebaseUserEmail;
      authDoc["password"] = config.firebaseUserPassword;
      authDoc["returnSecureToken"] = true;
    }

    String payload;
    serializeJson(authDoc, payload);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, url)) {
      error = "auth begin failed";
      return false;
    }

    http.addHeader("Content-Type", "application/json");
    outCode = http.POST(payload);
    outBody = http.getString();
    http.end();
    return true;
  };

  String body;
  int code = 0;
  if (!authWithMode(config.firebaseUseAnonymous, body, code)) {
    return false;
  }

  if (code < 200 || code >= 300) {
    error = String("auth failed code=") + code + String(" body=") + body;
    if (config.firebaseUseAnonymous && String(config.firebaseUserEmail).length() > 0 && String(config.firebaseUserPassword).length() > 0) {
      String fallbackBody;
      int fallbackCode = 0;
      if (authWithMode(false, fallbackBody, fallbackCode) && fallbackCode >= 200 && fallbackCode < 300) {
        body = fallbackBody;
        code = fallbackCode;
      } else {
        error = String("auth failed code=") + code + String(" body=") + body + String(" | fallback code=") + fallbackCode + String(" body=") + fallbackBody;
        return false;
      }
    } else {
      return false;
    }
  }

  DynamicJsonDocument tokenDoc(1536);
  if (deserializeJson(tokenDoc, body)) {
    error = String("auth parse failed body=") + body;
    return false;
  }

  String token = tokenDoc["idToken"] | "";
  long expiresIn = String((const char *)(tokenDoc["expiresIn"] | "0")).toInt();
  if (token.length() == 0 || expiresIn <= 0) {
    error = String("auth token missing body=") + body;
    return false;
  }

  idToken = token;
  tokenExpiresAtMs = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
  error = String();
  return true;
}

bool FirebaseManager::pushTelemetry(float temperature, float humidity, unsigned long epochSeconds) {
  if (!ensureAuthenticated()) {
    return false;
  }

  DynamicJsonDocument doc(512);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["timestamp"] = epochSeconds;
  doc["updatedAtMs"] = millis();

  String payload;
  serializeJson(doc, payload);

  String response;
  int statusCode = 0;
  if (!httpPatchJson(buildPathUrl(String(config.firebaseTelemetryPath)), payload, response, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "telemetry push", statusCode, response);
    return false;
  }

  return true;
}

bool FirebaseManager::pushStartupStatus(const String &bootTime, const String &wifiMode, const String &ipAddress, bool firebaseReady) {
  if (!ensureAuthenticated()) {
    return false;
  }

  DynamicJsonDocument doc(512);
  doc["bootTime"] = bootTime;
  doc["wifiMode"] = wifiMode;
  doc["ipAddress"] = ipAddress;
  doc["firebaseReady"] = firebaseReady;
  doc["updatedAtMs"] = millis();

  String payload;
  serializeJson(doc, payload);

  String response;
  int statusCode = 0;
  if (!httpPatchJson(buildPathUrl(String(config.firebaseStatusPath)), payload, response, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "startup status push", statusCode, response);
    return false;
  }

  return true;
}

bool FirebaseManager::pushLandingSnapshot(float temperature,
                                         float humidity,
                                         int sentToday,
                                         int sentWeek,
                                         int sentMonth,
                                         const String &wifiMode,
                                         const String &ipAddress,
                                         bool firebaseReady,
                                         bool telemetryPushOk,
                                         const String &telemetryMessage,
                                         unsigned long epochSeconds) {
  if (!ensureAuthenticated()) {
    return false;
  }

  DynamicJsonDocument doc(1024);
  doc["counters"]["sentToday"] = sentToday;
  doc["counters"]["sentWeek"] = sentWeek;
  doc["counters"]["sentMonth"] = sentMonth;
  doc["counters"]["updatedAtMs"] = millis();
  doc["telemetry"]["temperature"] = temperature;
  doc["telemetry"]["humidity"] = humidity;
  doc["telemetry"]["timestamp"] = epochSeconds;
  doc["telemetry"]["updatedAtMs"] = millis();
  doc["status"]["wifiMode"] = wifiMode;
  doc["status"]["ipAddress"] = ipAddress;
  doc["status"]["firebaseReady"] = firebaseReady;
  doc["status"]["telemetryPushOk"] = telemetryPushOk;
  doc["status"]["telemetryMessage"] = telemetryMessage;
  doc["status"]["updatedAtMs"] = millis();

  String payload;
  serializeJson(doc, payload);

  String response;
  int statusCode = 0;
  if (!httpPatchJson(buildPathUrl(rootPathFromConfig()), payload, response, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "landing snapshot push", statusCode, response);
    return false;
  }

  return true;
}

bool FirebaseManager::fetchRuntimeSettings(FirebaseRuntimeSettings &outSettings,
                                           uint32_t defaultIntervalOfDhtSeconds,
                                           bool defaultShowFirebasePushLogs,
                                           bool defaultShowThingSpeakPushLogs) {
  if (!ensureAuthenticated()) {
    return false;
  }

  outSettings = FirebaseRuntimeSettings();
  outSettings.intervalOfDhtSeconds = defaultIntervalOfDhtSeconds;
  outSettings.showFirebasePushLogs = defaultShowFirebasePushLogs;
  outSettings.showThingSpeakPushLogs = defaultShowThingSpeakPushLogs;
  outSettings.dailySmsLimit = config.dailySmsLimit;
  outSettings.weeklySmsLimit = config.weeklySmsLimit;
  outSettings.monthlySmsLimit = config.monthlySmsLimit;

  String runtimePath = rootPathFromConfig() + String("/settings/runtime");
  String response;
  int statusCode = 0;
  if (!httpGetJson(buildPathUrl(runtimePath), response, statusCode)) {
    return false;
  }

  DynamicJsonDocument writeDoc(384);
  bool shouldWriteBack = false;

  if (statusCode == 404 || response == "null") {
    outSettings.createdIntervalOfDht = true;
    outSettings.createdShowFirebasePushLogs = true;
    outSettings.createdShowThingSpeakPushLogs = true;
    outSettings.createdDailySmsLimit = true;
    outSettings.createdWeeklySmsLimit = true;
    outSettings.createdMonthlySmsLimit = true;
    shouldWriteBack = true;
  } else if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "runtime settings fetch", statusCode, response);
    return false;
  } else {
    DynamicJsonDocument readDoc(768);
    if (deserializeJson(readDoc, response)) {
      error = String("runtime settings parse failed body=") + response;
      return false;
    }

    JsonObject root = readDoc.as<JsonObject>();
    uint32_t intervalParsed = 0;
    if (parseIntervalVariant(root["intervalOfDhtSeconds"], intervalParsed)) {
      outSettings.intervalOfDhtSeconds = intervalParsed;
    } else {
      outSettings.createdIntervalOfDht = true;
      shouldWriteBack = true;
    }

    bool logsValue = defaultShowFirebasePushLogs;
    if (parseBoolVariant(root["showFirebasePushLogs"], logsValue)) {
      outSettings.showFirebasePushLogs = logsValue;
    } else {
      outSettings.createdShowFirebasePushLogs = true;
      shouldWriteBack = true;
    }

    bool tsLogsValue = defaultShowThingSpeakPushLogs;
    if (parseBoolVariant(root["showThingSpeakPushLogs"], tsLogsValue)) {
      outSettings.showThingSpeakPushLogs = tsLogsValue;
    } else {
      outSettings.createdShowThingSpeakPushLogs = true;
      shouldWriteBack = true;
    }

    int dailyLimitParsed = 0;
    if (parseLimitVariant(root["dailySmsLimit"], dailyLimitParsed)) {
      outSettings.dailySmsLimit = dailyLimitParsed;
    } else {
      outSettings.createdDailySmsLimit = true;
      shouldWriteBack = true;
    }

    int weeklyLimitParsed = 0;
    if (parseLimitVariant(root["weeklySmsLimit"], weeklyLimitParsed)) {
      outSettings.weeklySmsLimit = weeklyLimitParsed;
    } else {
      outSettings.createdWeeklySmsLimit = true;
      shouldWriteBack = true;
    }

    int monthlyLimitParsed = 0;
    if (parseLimitVariant(root["monthlySmsLimit"], monthlyLimitParsed)) {
      outSettings.monthlySmsLimit = monthlyLimitParsed;
    } else {
      outSettings.createdMonthlySmsLimit = true;
      shouldWriteBack = true;
    }
  }

  if (shouldWriteBack) {
    writeDoc["intervalOfDhtSeconds"] = outSettings.intervalOfDhtSeconds;
    writeDoc["showFirebasePushLogs"] = outSettings.showFirebasePushLogs;
    writeDoc["showThingSpeakPushLogs"] = outSettings.showThingSpeakPushLogs;
    writeDoc["dailySmsLimit"] = outSettings.dailySmsLimit;
    writeDoc["weeklySmsLimit"] = outSettings.weeklySmsLimit;
    writeDoc["monthlySmsLimit"] = outSettings.monthlySmsLimit;
    writeDoc["updatedAtMs"] = millis();

    String payload;
    serializeJson(writeDoc, payload);

    String writeResp;
    int writeCode = 0;
    if (!httpPatchJson(buildPathUrl(runtimePath), payload, writeResp, writeCode)) {
      return false;
    }
    if (writeCode < 200 || writeCode >= 300) {
      setHttpStatusError(error, "runtime settings write", writeCode, writeResp);
      return false;
    }
  }

  error = String();
  return true;
}

bool FirebaseManager::bootstrapPaths() {
  if (!ensureAuthenticated()) {
    return false;
  }

  error = String();
  return true;
}

String FirebaseManager::rootPathFromConfig() const {
  String commandPath = String(config.firebaseCommandPath);
  if (!commandPath.startsWith("/")) {
    commandPath = String("/") + commandPath;
  }

  int firstSlashAfterRoot = commandPath.indexOf('/', 1);
  if (firstSlashAfterRoot <= 0) {
    return String("/ttgo_tcall");
  }

  return commandPath.substring(0, firstSlashAfterRoot);
}

String FirebaseManager::buildPathUrl(const String &path) const {
  String base = String(config.firebaseDatabaseUrl);
  if (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }

  String normalizedPath = path;
  if (!normalizedPath.startsWith("/")) {
    normalizedPath = String("/") + normalizedPath;
  }

  return base + normalizedPath + ".json?auth=" + idToken;
}

bool FirebaseManager::httpGetJson(const String &url, String &responseBody, int &statusCode) {
  String requestUrl = url;
  for (int attempt = 0; attempt < 2; ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, requestUrl)) {
      error = String("http get begin failed url=") + requestUrl;
      return false;
    }

    statusCode = http.GET();
    responseBody = http.getString();
    http.end();

    if (statusCode == 404 && attempt == 0 && tryUpdateDatabaseUrlFromBody(responseBody)) {
      requestUrl = rebuildUrlWithCurrentBase(requestUrl);
      continue;
    }
    break;
  }

  return true;
}

bool FirebaseManager::httpPatchJson(const String &url, const String &payload, String &responseBody, int &statusCode) {
  String requestUrl = url;
  for (int attempt = 0; attempt < 2; ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, requestUrl)) {
      error = String("http patch begin failed url=") + requestUrl;
      return false;
    }

    http.addHeader("Content-Type", "application/json");
    statusCode = http.PATCH(payload);
    responseBody = http.getString();
    http.end();

    if (statusCode == 404 && attempt == 0 && tryUpdateDatabaseUrlFromBody(responseBody)) {
      requestUrl = rebuildUrlWithCurrentBase(requestUrl);
      continue;
    }
    break;
  }

  return true;
}

bool FirebaseManager::tryUpdateDatabaseUrlFromBody(const String &responseBody) {
  if (responseBody.length() == 0) {
    return false;
  }

  DynamicJsonDocument doc(768);
  if (deserializeJson(doc, responseBody)) {
    return false;
  }

  String correctUrl = doc["correctUrl"] | "";
  if (correctUrl.length() == 0 || !correctUrl.startsWith("http")) {
    return false;
  }

  if (correctUrl.endsWith("/")) {
    correctUrl.remove(correctUrl.length() - 1);
  }

  String current = String(config.firebaseDatabaseUrl);
  if (current == correctUrl) {
    return false;
  }

  strlcpy(config.firebaseDatabaseUrl, correctUrl.c_str(), sizeof(config.firebaseDatabaseUrl));
  error = String("database url auto-corrected to ") + correctUrl;
  return true;
}

String FirebaseManager::rebuildUrlWithCurrentBase(const String &originalUrl) const {
  String base = String(config.firebaseDatabaseUrl);
  if (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }

  int schemePos = originalUrl.indexOf("://");
  if (schemePos < 0) {
    return originalUrl;
  }

  int pathStart = originalUrl.indexOf('/', schemePos + 3);
  if (pathStart < 0) {
    return base;
  }

  return base + originalUrl.substring(pathStart);
}
