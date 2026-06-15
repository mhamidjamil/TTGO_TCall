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

bool parseStringVariant(const JsonVariantConst &variant, String &outValue) {
  if (!variant.is<const char *>()) {
    return false;
  }
  String parsed = variant.as<const char *>();
  parsed.trim();
  if (!parsed.startsWith("http://") && !parsed.startsWith("https://")) {
    return false;
  }
  outValue = parsed;
  return true;
}

String firestoreStringField(const JsonObjectConst &fields, const char *name) {
  JsonVariantConst field = fields[name];
  if (field["stringValue"].is<const char *>()) {
    return String(field["stringValue"].as<const char *>());
  }
  return String();
}

bool firestoreBoolField(const JsonObjectConst &fields, const char *name, bool defaultValue) {
  JsonVariantConst field = fields[name];
  if (field["booleanValue"].is<bool>()) {
    return field["booleanValue"].as<bool>();
  }
  if (field["integerValue"].is<const char *>()) {
    return String(field["integerValue"].as<const char *>()).toInt() != 0;
  }
  if (field["stringValue"].is<const char *>()) {
    String text = field["stringValue"].as<const char *>();
    text.toLowerCase();
    if (text == "false" || text == "0" || text == "no" || text == "off") {
      return false;
    }
    if (text == "true" || text == "1" || text == "yes" || text == "on") {
      return true;
    }
  }
  return defaultValue;
}

String firestoreDocumentId(const String &name) {
  int slash = name.lastIndexOf('/');
  if (slash < 0 || slash >= (int)name.length() - 1) {
    return name;
  }
  return name.substring(slash + 1);
}

String safeFirestoreDocumentId(const String &value) {
  String id;
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '_') {
      id += c;
    } else {
      id += '_';
    }
  }
  id.trim();
  if (id.length() == 0 || id == "." || id == "..") {
    return String("unknown");
  }
  return id;
}

void appendFirestoreArrayStrings(const JsonVariantConst &field, String *numbers, size_t maxNumbers, size_t &numberCount) {
  JsonArrayConst values = field["arrayValue"]["values"].as<JsonArrayConst>();
  for (JsonVariantConst value : values) {
    if (numberCount >= maxNumbers) {
      return;
    }
    String number = value["stringValue"] | "";
    number.trim();
    if (number.length() > 0) {
      numbers[numberCount++] = number;
    }
  }
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
                                           bool defaultShowThingSpeakPushLogs,
                                           const String &defaultNtfyUrl) {
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
  outSettings.ntfyUrl = defaultNtfyUrl.length() > 0 ? defaultNtfyUrl : String(config.ntfyUrl);

  String runtimePath = rootPathFromConfig() + String("/settings/runtime");
  String response;
  int statusCode = 0;
  if (!httpGetJson(buildPathUrl(runtimePath), response, statusCode)) {
    return false;
  }

  DynamicJsonDocument writeDoc(640);
  bool shouldWriteBack = false;

  if (statusCode == 404 || response == "null") {
    outSettings.createdIntervalOfDht = true;
    outSettings.createdShowFirebasePushLogs = true;
    outSettings.createdShowThingSpeakPushLogs = true;
    outSettings.createdDailySmsLimit = true;
    outSettings.createdWeeklySmsLimit = true;
    outSettings.createdMonthlySmsLimit = true;
    outSettings.createdNtfyUrl = true;
    shouldWriteBack = true;
  } else if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "runtime settings fetch", statusCode, response);
    return false;
  } else {
    DynamicJsonDocument readDoc(1024);
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

    String parsedNtfyUrl;
    if (parseStringVariant(root["ntfyUrl"], parsedNtfyUrl)) {
      outSettings.ntfyUrl = parsedNtfyUrl;
    } else {
      outSettings.createdNtfyUrl = true;
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
    writeDoc["ntfyUrl"] = outSettings.ntfyUrl;
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

bool FirebaseManager::pushSimModuleEvent(const String &type,
                                         const String &number,
                                         const String &message,
                                         bool blocked,
                                         const String &pakistanTimestamp,
                                         int simIndex) {
  if (!ensureAuthenticated()) {
    return false;
  }

  String section;
  if (type == "call") {
    section = "calls";
  } else if (type == "sms") {
    section = "sms";
  } else {
    error = "invalid sim event type";
    return false;
  }

  String documentId = safeFirestoreDocumentId(number);
  String documentPath = String("sim_module/") + section + String("/by_number/") + documentId;

  DynamicJsonDocument doc(1792);
  JsonObject fields = doc.createNestedObject("fields");
  fields["type"]["stringValue"] = type;
  fields["number"]["stringValue"] = number;
  fields["documentId"]["stringValue"] = documentId;
  fields["message"]["stringValue"] = message;
  fields["blocked"]["booleanValue"] = blocked;
  fields["source"]["stringValue"] = "ttgo_tcall_v8";
  fields["pakistanTime"]["stringValue"] = pakistanTimestamp;
  fields["receivedAtPakistan"]["stringValue"] = pakistanTimestamp;
  fields["updatedAtPakistan"]["stringValue"] = pakistanTimestamp;
  fields["uptimeSeconds"]["integerValue"] = String(millis() / 1000UL);
  if (simIndex > 0) {
    fields["simIndex"]["integerValue"] = String(simIndex);
  }

  String payload;
  serializeJson(doc, payload);

  String response;
  int statusCode = 0;
  String url = buildFirestoreUrl(documentPath);
  if (!httpPatchBearerJson(url, payload, response, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "firestore sim event push", statusCode, response);
    return false;
  }

  error = String();
  return true;
}

bool FirebaseManager::fetchSimBlockLists(String *blockedCallers,
                                         size_t maxBlockedCallers,
                                         size_t &blockedCallerCount,
                                         String *blockedSmsSenders,
                                         size_t maxBlockedSmsSenders,
                                         size_t &blockedSmsSenderCount) {
  if (!ensureAuthenticated()) {
    return false;
  }

  blockedCallerCount = 0;
  blockedSmsSenderCount = 0;

  if (!ensureSimSettingsDocument()) {
    return false;
  }

  if (!fetchFirestoreSettingsList("blockedCallers", blockedCallers, maxBlockedCallers, blockedCallerCount)) {
    return false;
  }

  if (!fetchFirestoreSettingsList("blockedSmsSenders", blockedSmsSenders, maxBlockedSmsSenders, blockedSmsSenderCount)) {
    return false;
  }

  error = String();
  return true;
}

bool FirebaseManager::bootstrapSimModulePaths() {
  if (!ensureAuthenticated()) {
    return false;
  }

  const char *paths[] = {
      "sim_module/sms",
      "sim_module/calls",
  };

  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
    if (!ensureFirestoreDocument(paths[i])) {
      return false;
    }
  }

  if (!ensureSimSettingsDocument()) {
    return false;
  }

  error = String();
  return true;
}

bool FirebaseManager::cleanupLegacySimModulePaths() {
  if (!ensureAuthenticated()) {
    return false;
  }

  const char *legacyCollections[] = {
      "sim_module/calls/entries",
      "sim_module/sms/entries",
      "sim_module/blocked_calls/entries",
      "sim_module/blocked_sms/entries",
      "sim_module/blocked_callers/numbers",
      "sim_module/blocked_sms_senders/numbers",
      "sim_module/blocked_caller/numbers",
      "sim_module/blocked_sms_sender/numbers",
  };

  for (size_t i = 0; i < sizeof(legacyCollections) / sizeof(legacyCollections[0]); ++i) {
    if (!deleteFirestoreCollectionDocuments(legacyCollections[i])) {
      return false;
    }
  }

  const char *legacyDocuments[] = {
      "sim_module/blocked_calls",
      "sim_module/blocked_sms",
      "sim_module/blocked_callers",
      "sim_module/blocked_sms_senders",
      "sim_module/blocked_caller",
      "sim_module/blocked_sms_sender",
  };

  for (size_t i = 0; i < sizeof(legacyDocuments) / sizeof(legacyDocuments[0]); ++i) {
    if (!deleteFirestoreDocument(legacyDocuments[i])) {
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

  if (!bootstrapSimModulePaths()) {
    return false;
  }

  if (!cleanupLegacySimModulePaths()) {
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

String FirebaseManager::buildFirestoreUrl(const String &path) const {
  String normalizedPath = path;
  if (normalizedPath.startsWith("/")) {
    normalizedPath.remove(0, 1);
  }
  return String("https://firestore.googleapis.com/v1/projects/") +
         config.firebaseProjectId +
         String("/databases/(default)/documents/") +
         normalizedPath;
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

bool FirebaseManager::httpGetBearer(const String &url, String &responseBody, int &statusCode) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    error = String("http bearer get begin failed url=") + url;
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + idToken);
  statusCode = http.GET();
  responseBody = http.getString();
  http.end();
  return true;
}

bool FirebaseManager::httpPostBearerJson(const String &url, const String &payload, String &responseBody, int &statusCode) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    error = String("http bearer post begin failed url=") + url;
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + idToken);
  http.addHeader("Content-Type", "application/json");
  statusCode = http.POST(payload);
  responseBody = http.getString();
  http.end();
  return true;
}

bool FirebaseManager::httpPatchBearerJson(const String &url, const String &payload, String &responseBody, int &statusCode) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    error = String("http bearer patch begin failed url=") + url;
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + idToken);
  http.addHeader("Content-Type", "application/json");
  statusCode = http.PATCH(payload);
  responseBody = http.getString();
  http.end();
  return true;
}

bool FirebaseManager::httpDeleteBearer(const String &url, String &responseBody, int &statusCode) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    error = String("http bearer delete begin failed url=") + url;
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + idToken);
  statusCode = http.sendRequest("DELETE");
  responseBody = http.getString();
  http.end();
  return true;
}

bool FirebaseManager::appendCsvNumbers(const String &csvText, String *numbers, size_t maxNumbers, size_t &numberCount) {
  int start = 0;
  while (start < (int)csvText.length() && numberCount < maxNumbers) {
    int comma = csvText.indexOf(',', start);
    String number = comma >= 0 ? csvText.substring(start, comma) : csvText.substring(start);
    number.trim();
    if (number.startsWith("\"") && number.endsWith("\"") && number.length() >= 2) {
      number = number.substring(1, number.length() - 1);
      number.trim();
    }
    if (number.length() > 0) {
      numbers[numberCount++] = number;
    }
    if (comma < 0) {
      break;
    }
    start = comma + 1;
  }
  return true;
}

bool FirebaseManager::fetchFirestoreSettingsList(const String &fieldName, String *numbers, size_t maxNumbers, size_t &numberCount) {
  String response;
  int statusCode = 0;
  String url = buildFirestoreUrl("sim_module/settings");
  if (!httpGetBearer(url, response, statusCode)) {
    return false;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "firestore settings fetch", statusCode, response);
    return false;
  }

  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, response)) {
    error = String("firestore settings parse failed body=") + response;
    return false;
  }

  JsonObjectConst fields = doc["fields"].as<JsonObjectConst>();
  appendFirestoreArrayStrings(fields[fieldName.c_str()], numbers, maxNumbers, numberCount);

  String csvFieldName = fieldName + String("Csv");
  String csvText = firestoreStringField(fields, csvFieldName.c_str());
  if (csvText.length() > 0) {
    appendCsvNumbers(csvText, numbers, maxNumbers, numberCount);
  }

  error = String();
  return true;
}

bool FirebaseManager::ensureSimSettingsDocument() {
  String url = buildFirestoreUrl("sim_module/settings");
  String response;
  int statusCode = 0;
  if (!httpGetBearer(url, response, statusCode)) {
    return false;
  }

  if (statusCode >= 200 && statusCode < 300) {
    return true;
  }

  if (statusCode != 404) {
    setHttpStatusError(error, "firestore settings check", statusCode, response);
    return false;
  }

  Serial.println("[FIRESTORE] sim_module/settings not found; creating simple block-list arrays");

  DynamicJsonDocument doc(1024);
  JsonObject fields = doc.createNestedObject("fields");
  JsonArray callers = fields["blockedCallers"]["arrayValue"].createNestedArray("values");
  JsonArray smsSenders = fields["blockedSmsSenders"]["arrayValue"].createNestedArray("values");
  (void)callers;
  (void)smsSenders;
  fields["blockedCallersCsv"]["stringValue"] = "";
  fields["blockedSmsSendersCsv"]["stringValue"] = "";
  fields["updatedAtPakistan"]["stringValue"] = "";
  fields["source"]["stringValue"] = "ttgo_tcall_v8";

  String payload;
  serializeJson(doc, payload);

  String writeResponse;
  int writeStatusCode = 0;
  if (!httpPatchBearerJson(url, payload, writeResponse, writeStatusCode)) {
    return false;
  }

  if (writeStatusCode < 200 || writeStatusCode >= 300) {
    setHttpStatusError(error, "firestore settings create", writeStatusCode, writeResponse);
    return false;
  }

  Serial.println("[FIRESTORE] sim_module/settings created");
  return true;
}

bool FirebaseManager::deleteFirestoreCollectionDocuments(const String &collectionPath) {
  String response;
  int statusCode = 0;
  String url = buildFirestoreUrl(collectionPath);
  if (!httpGetBearer(url, response, statusCode)) {
    return false;
  }

  if (statusCode == 404) {
    error = String();
    return true;
  }

  if (statusCode < 200 || statusCode >= 300) {
    setHttpStatusError(error, "legacy collection list", statusCode, response);
    return false;
  }

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, response)) {
    error = String("legacy collection parse failed body=") + response;
    return false;
  }

  JsonArray documents = doc["documents"].as<JsonArray>();
  for (JsonVariant item : documents) {
    String name = item["name"] | "";
    if (name.length() == 0) {
      continue;
    }

    String deleteResponse;
    int deleteStatusCode = 0;
    String deleteUrl = String("https://firestore.googleapis.com/v1/") + name;
    if (!httpDeleteBearer(deleteUrl, deleteResponse, deleteStatusCode)) {
      return false;
    }
    if (deleteStatusCode != 404 && (deleteStatusCode < 200 || deleteStatusCode >= 300)) {
      setHttpStatusError(error, "legacy collection delete", deleteStatusCode, deleteResponse);
      return false;
    }
    Serial.print("[FIRESTORE] deleted legacy document ");
    Serial.println(name);
  }

  error = String();
  return true;
}

bool FirebaseManager::deleteFirestoreDocument(const String &documentPath) {
  String response;
  int statusCode = 0;
  if (!httpDeleteBearer(buildFirestoreUrl(documentPath), response, statusCode)) {
    return false;
  }

  if (statusCode == 404 || (statusCode >= 200 && statusCode < 300)) {
    error = String();
    return true;
  }

  setHttpStatusError(error, "legacy document delete", statusCode, response);
  return false;
}

bool FirebaseManager::ensureFirestoreDocument(const String &documentPath) {
  String url = buildFirestoreUrl(documentPath);
  String response;
  int statusCode = 0;
  if (!httpGetBearer(url, response, statusCode)) {
    return false;
  }

  if (statusCode >= 200 && statusCode < 300) {
    return true;
  }

  if (statusCode != 404) {
    setHttpStatusError(error, "firestore folder check", statusCode, response);
    return false;
  }

  Serial.print("[FIRESTORE] ");
  Serial.print(documentPath);
  Serial.println(" not found; creating now");

  DynamicJsonDocument doc(768);
  JsonObject fields = doc.createNestedObject("fields");
  fields["kind"]["stringValue"] = "sim_module_folder";
  fields["path"]["stringValue"] = documentPath;
  fields["source"]["stringValue"] = "ttgo_tcall_v8";
  fields["createdBy"]["stringValue"] = "ttgo_tcall_v8";

  String payload;
  serializeJson(doc, payload);

  String writeResponse;
  int writeStatusCode = 0;
  if (!httpPatchBearerJson(url, payload, writeResponse, writeStatusCode)) {
    return false;
  }

  if (writeStatusCode < 200 || writeStatusCode >= 300) {
    setHttpStatusError(error, "firestore folder create", writeStatusCode, writeResponse);
    return false;
  }

  Serial.print("[FIRESTORE] ");
  Serial.print(documentPath);
  Serial.println(" created");
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
