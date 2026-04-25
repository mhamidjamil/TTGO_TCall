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
}

bool FirebaseManager::begin(const V8Config &incomingConfig) {
  config = incomingConfig;
  ready = false;
  tokenExpiresAtMs = 0;
  idToken = String();
  error = String();

  if (String(config.firebaseProjectId).isEmpty() || String(config.firebaseDatabaseUrl).isEmpty() ||
      String(config.firebaseApiKey).isEmpty() || String(config.firebaseUserEmail).isEmpty() ||
      String(config.firebaseUserPassword).isEmpty()) {
    error = "Firebase config missing";
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
    error = "status update failed";
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

  return historyCode >= 200 && historyCode < 300;
}

bool FirebaseManager::updateCounterSnapshot(int dailyCount, int weeklyCount, int monthlyCount) {
  if (!ensureAuthenticated()) {
    return false;
  }

  DynamicJsonDocument doc(256);
  doc["daily"] = dailyCount;
  doc["weekly"] = weeklyCount;
  doc["monthly"] = monthlyCount;
  doc["updatedAtMs"] = millis();

  String payload;
  serializeJson(doc, payload);

  String response;
  int statusCode = 0;
  if (!httpPatchJson(buildPathUrl(String(config.firebaseCounterPath)), payload, response, statusCode)) {
    return false;
  }

  return statusCode >= 200 && statusCode < 300;
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

  const String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + config.firebaseApiKey;
  DynamicJsonDocument authDoc(512);
  authDoc["email"] = config.firebaseUserEmail;
  authDoc["password"] = config.firebaseUserPassword;
  authDoc["returnSecureToken"] = true;
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
  int code = http.POST(payload);
  String body = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    error = String("auth failed code=") + code;
    return false;
  }

  DynamicJsonDocument tokenDoc(1536);
  if (deserializeJson(tokenDoc, body)) {
    error = "auth parse failed";
    return false;
  }

  String token = tokenDoc["idToken"] | "";
  long expiresIn = String((const char *)(tokenDoc["expiresIn"] | "0")).toInt();
  if (token.length() == 0 || expiresIn <= 0) {
    error = "auth token missing";
    return false;
  }

  idToken = token;
  tokenExpiresAtMs = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
  error = String();
  return true;
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
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    error = "http get begin failed";
    return false;
  }

  statusCode = http.GET();
  responseBody = http.getString();
  http.end();
  return true;
}

bool FirebaseManager::httpPatchJson(const String &url, const String &payload, String &responseBody, int &statusCode) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    error = "http patch begin failed";
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  statusCode = http.PATCH(payload);
  responseBody = http.getString();
  http.end();
  return true;
}
