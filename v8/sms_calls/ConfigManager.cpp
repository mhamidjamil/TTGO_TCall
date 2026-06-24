#include "ConfigManager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets.example.h")
#include "secrets.example.h"
#endif

#ifndef NTFY_URL_DEFAULT
#define NTFY_URL_DEFAULT "https://ntfy.innovorix.com/oracle_ntfy"
#endif

#ifndef DEVICE_ID_DEFAULT
#define DEVICE_ID_DEFAULT "device_001"
#endif

namespace {
constexpr const char *kConfigPath = "/v8_config.json";
}

static void copyText(char *target, size_t targetSize, const char *source) {
  if (!target || targetSize == 0) {
    return;
  }
  if (!source) {
    target[0] = '\0';
    return;
  }
  strlcpy(target, source, targetSize);
}

void ConfigManager::begin() {
  if (!LittleFS.begin(true)) {
    LittleFS.format();
    LittleFS.begin(true);
  }

  loadDefaults();
  if (!loadFromLittleFS()) {
    saveToLittleFS();
  }
}

bool ConfigManager::save() {
  return saveToLittleFS();
}

const V8Config &ConfigManager::get() const {
  return config;
}

bool ConfigManager::updateUserWifi(const String &ssid1, const String &pass1,
                                   const String &ssid2, const String &pass2) {
  copyText(config.userWifiSsid1, sizeof(config.userWifiSsid1), ssid1.c_str());
  copyText(config.userWifiPass1, sizeof(config.userWifiPass1), pass1.c_str());
  copyText(config.userWifiSsid2, sizeof(config.userWifiSsid2), ssid2.c_str());
  copyText(config.userWifiPass2, sizeof(config.userWifiPass2), pass2.c_str());
  return saveToLittleFS();
}

void ConfigManager::loadDefaults() {
  config.wifiEnabled = true;
  config.apFallbackEnabled = true;
  config.logVerbose = LOG_VERBOSE_DEFAULT != 0;
  config.firebaseUseAnonymous = FIREBASE_USE_ANONYMOUS_DEFAULT != 0;
  copyText(config.wifiSsid, sizeof(config.wifiSsid), WIFI_SSID_DEFAULT);
  copyText(config.wifiPass, sizeof(config.wifiPass), WIFI_PASS_DEFAULT);
  copyText(config.wifiSsidBackup, sizeof(config.wifiSsidBackup), WIFI_SSID_BACKUP_DEFAULT);
  copyText(config.wifiPassBackup, sizeof(config.wifiPassBackup), WIFI_PASS_BACKUP_DEFAULT);
  // User WiFi overrides start empty; only set from the dashboard.
  config.userWifiSsid1[0] = '\0';
  config.userWifiPass1[0] = '\0';
  config.userWifiSsid2[0] = '\0';
  config.userWifiPass2[0] = '\0';
  copyText(config.apSsid, sizeof(config.apSsid), AP_SSID_DEFAULT);
  copyText(config.apPass, sizeof(config.apPass), AP_PASS_DEFAULT);
  config.webServerPort = WEB_SERVER_PORT_DEFAULT;
  config.pollingIntervalSeconds = POLLING_INTERVAL_SECONDS_DEFAULT;
  config.dailySmsLimit = DAILY_SMS_LIMIT_DEFAULT;
  config.weeklySmsLimit = WEEKLY_SMS_LIMIT_DEFAULT;
  config.monthlySmsLimit = MONTHLY_SMS_LIMIT_DEFAULT;
  copyText(config.dashboardPassword, sizeof(config.dashboardPassword), DASHBOARD_PASSWORD_DEFAULT);
  copyText(config.deviceId, sizeof(config.deviceId), DEVICE_ID_DEFAULT);
  copyText(config.deviceName, sizeof(config.deviceName), DEVICE_NAME_DEFAULT);
  copyText(config.ownerName, sizeof(config.ownerName), OWNER_NAME_DEFAULT);
  copyText(config.myNumber, sizeof(config.myNumber), MY_NUMBER_DEFAULT);
  copyText(config.defaultCountryCode, sizeof(config.defaultCountryCode), DEFAULT_COUNTRY_CODE_DEFAULT);
  copyText(config.adminNumbers, sizeof(config.adminNumbers), ADMIN_NUMBERS_DEFAULT);
  copyText(config.authenticNumbers, sizeof(config.authenticNumbers), AUTHENTIC_NUMBERS_DEFAULT);
  copyText(config.bypassKey, sizeof(config.bypassKey), BYPASS_KEY_DEFAULT);
  copyText(config.firebaseProjectId, sizeof(config.firebaseProjectId), FIREBASE_PROJECT_ID_DEFAULT);
  copyText(config.firebaseDatabaseUrl, sizeof(config.firebaseDatabaseUrl), FIREBASE_DATABASE_URL_DEFAULT);
  copyText(config.firebaseApiKey, sizeof(config.firebaseApiKey), FIREBASE_API_KEY_DEFAULT);
  copyText(config.firebaseUserEmail, sizeof(config.firebaseUserEmail), FIREBASE_USER_EMAIL_DEFAULT);
  copyText(config.firebaseUserPassword, sizeof(config.firebaseUserPassword), FIREBASE_USER_PASSWORD_DEFAULT);
  copyText(config.firebaseCommandPath, sizeof(config.firebaseCommandPath), FIREBASE_COMMAND_PATH_DEFAULT);
  copyText(config.firebaseHistoryPath, sizeof(config.firebaseHistoryPath), FIREBASE_HISTORY_PATH_DEFAULT);
  copyText(config.firebaseCounterPath, sizeof(config.firebaseCounterPath), FIREBASE_COUNTER_PATH_DEFAULT);
  copyText(config.firebaseStatusPath, sizeof(config.firebaseStatusPath), FIREBASE_STATUS_PATH_DEFAULT);
  copyText(config.firebaseTelemetryPath, sizeof(config.firebaseTelemetryPath), FIREBASE_TELEMETRY_PATH_DEFAULT);
  copyText(config.ntfyUrl, sizeof(config.ntfyUrl), NTFY_URL_DEFAULT);
  config.thingSpeakChannelId = THINGSPEAK_CHANNEL_ID_DEFAULT;
  copyText(config.thingSpeakWriteApiKey, sizeof(config.thingSpeakWriteApiKey), THINGSPEAK_WRITE_API_KEY_DEFAULT);
}

bool ConfigManager::loadFromLittleFS() {
  if (!LittleFS.exists(kConfigPath)) {
    return false;
  }

  File file = LittleFS.open(kConfigPath, "r");
  if (!file) {
    return false;
  }

  String jsonText = file.readString();
  file.close();
  if (jsonText.isEmpty()) {
    return false;
  }

  readJsonConfig(jsonText);
  return true;
}

bool ConfigManager::saveToLittleFS() {
  File file = LittleFS.open(kConfigPath, "w");
  if (!file) {
    return false;
  }

  String jsonText = writeJsonConfig();
  bool ok = file.print(jsonText) > 0;
  file.close();
  return ok;
}

void ConfigManager::readJsonConfig(const String &jsonText) {
  DynamicJsonDocument doc(3072);
  if (deserializeJson(doc, jsonText)) {
    return;
  }

  config.wifiEnabled = doc["wifiEnabled"] | config.wifiEnabled;
  config.apFallbackEnabled = doc["apFallbackEnabled"] | config.apFallbackEnabled;
  config.logVerbose = doc["logVerbose"] | config.logVerbose;
  config.firebaseUseAnonymous = doc["firebaseUseAnonymous"] | config.firebaseUseAnonymous;
  strlcpy(config.wifiSsid, doc["wifiSsid"] | config.wifiSsid, sizeof(config.wifiSsid));
  strlcpy(config.wifiPass, doc["wifiPass"] | config.wifiPass, sizeof(config.wifiPass));
  strlcpy(config.wifiSsidBackup, doc["wifiSsidBackup"] | config.wifiSsidBackup, sizeof(config.wifiSsidBackup));
  strlcpy(config.wifiPassBackup, doc["wifiPassBackup"] | config.wifiPassBackup, sizeof(config.wifiPassBackup));
  strlcpy(config.userWifiSsid1, doc["userWifiSsid1"] | config.userWifiSsid1, sizeof(config.userWifiSsid1));
  strlcpy(config.userWifiPass1, doc["userWifiPass1"] | config.userWifiPass1, sizeof(config.userWifiPass1));
  strlcpy(config.userWifiSsid2, doc["userWifiSsid2"] | config.userWifiSsid2, sizeof(config.userWifiSsid2));
  strlcpy(config.userWifiPass2, doc["userWifiPass2"] | config.userWifiPass2, sizeof(config.userWifiPass2));
  strlcpy(config.apSsid, doc["apSsid"] | config.apSsid, sizeof(config.apSsid));
  strlcpy(config.apPass, doc["apPass"] | config.apPass, sizeof(config.apPass));
  config.webServerPort = doc["webServerPort"] | config.webServerPort;
  config.pollingIntervalSeconds = doc["pollingIntervalSeconds"] | config.pollingIntervalSeconds;
  config.dailySmsLimit = doc["dailySmsLimit"] | config.dailySmsLimit;
  config.weeklySmsLimit = doc["weeklySmsLimit"] | config.weeklySmsLimit;
  config.monthlySmsLimit = doc["monthlySmsLimit"] | config.monthlySmsLimit;
  strlcpy(config.dashboardPassword, doc["dashboardPassword"] | config.dashboardPassword, sizeof(config.dashboardPassword));
  strlcpy(config.deviceId, doc["deviceId"] | config.deviceId, sizeof(config.deviceId));
  strlcpy(config.deviceName, doc["deviceName"] | config.deviceName, sizeof(config.deviceName));
  strlcpy(config.ownerName, doc["ownerName"] | config.ownerName, sizeof(config.ownerName));
  strlcpy(config.myNumber, doc["myNumber"] | config.myNumber, sizeof(config.myNumber));
  strlcpy(config.defaultCountryCode, doc["defaultCountryCode"] | config.defaultCountryCode, sizeof(config.defaultCountryCode));
  strlcpy(config.adminNumbers, doc["adminNumbers"] | config.adminNumbers, sizeof(config.adminNumbers));
  strlcpy(config.authenticNumbers, doc["authenticNumbers"] | config.authenticNumbers, sizeof(config.authenticNumbers));
  strlcpy(config.bypassKey, doc["bypassKey"] | config.bypassKey, sizeof(config.bypassKey));
  strlcpy(config.firebaseProjectId, doc["firebaseProjectId"] | config.firebaseProjectId, sizeof(config.firebaseProjectId));
  strlcpy(config.firebaseDatabaseUrl, doc["firebaseDatabaseUrl"] | config.firebaseDatabaseUrl, sizeof(config.firebaseDatabaseUrl));
  strlcpy(config.firebaseApiKey, doc["firebaseApiKey"] | config.firebaseApiKey, sizeof(config.firebaseApiKey));
  strlcpy(config.firebaseUserEmail, doc["firebaseUserEmail"] | config.firebaseUserEmail, sizeof(config.firebaseUserEmail));
  strlcpy(config.firebaseUserPassword, doc["firebaseUserPassword"] | config.firebaseUserPassword, sizeof(config.firebaseUserPassword));
  strlcpy(config.firebaseCommandPath, doc["firebaseCommandPath"] | config.firebaseCommandPath, sizeof(config.firebaseCommandPath));
  strlcpy(config.firebaseHistoryPath, doc["firebaseHistoryPath"] | config.firebaseHistoryPath, sizeof(config.firebaseHistoryPath));
  strlcpy(config.firebaseCounterPath, doc["firebaseCounterPath"] | config.firebaseCounterPath, sizeof(config.firebaseCounterPath));
  strlcpy(config.firebaseStatusPath, doc["firebaseStatusPath"] | config.firebaseStatusPath, sizeof(config.firebaseStatusPath));
  strlcpy(config.firebaseTelemetryPath, doc["firebaseTelemetryPath"] | config.firebaseTelemetryPath, sizeof(config.firebaseTelemetryPath));
  strlcpy(config.ntfyUrl, doc["ntfyUrl"] | config.ntfyUrl, sizeof(config.ntfyUrl));
  config.thingSpeakChannelId = doc["thingSpeakChannelId"] | config.thingSpeakChannelId;
  strlcpy(config.thingSpeakWriteApiKey, doc["thingSpeakWriteApiKey"] | config.thingSpeakWriteApiKey, sizeof(config.thingSpeakWriteApiKey));
}

String ConfigManager::writeJsonConfig() const {
  DynamicJsonDocument doc(3072);
  doc["wifiEnabled"] = config.wifiEnabled;
  doc["apFallbackEnabled"] = config.apFallbackEnabled;
  doc["logVerbose"] = config.logVerbose;
  doc["firebaseUseAnonymous"] = config.firebaseUseAnonymous;
  doc["wifiSsid"] = config.wifiSsid;
  doc["wifiPass"] = config.wifiPass;
  doc["wifiSsidBackup"] = config.wifiSsidBackup;
  doc["wifiPassBackup"] = config.wifiPassBackup;
  doc["userWifiSsid1"] = config.userWifiSsid1;
  doc["userWifiPass1"] = config.userWifiPass1;
  doc["userWifiSsid2"] = config.userWifiSsid2;
  doc["userWifiPass2"] = config.userWifiPass2;
  doc["apSsid"] = config.apSsid;
  doc["apPass"] = config.apPass;
  doc["webServerPort"] = config.webServerPort;
  doc["pollingIntervalSeconds"] = config.pollingIntervalSeconds;
  doc["dailySmsLimit"] = config.dailySmsLimit;
  doc["weeklySmsLimit"] = config.weeklySmsLimit;
  doc["monthlySmsLimit"] = config.monthlySmsLimit;
  doc["dashboardPassword"] = config.dashboardPassword;
  doc["deviceId"] = config.deviceId;
  doc["deviceName"] = config.deviceName;
  doc["ownerName"] = config.ownerName;
  doc["myNumber"] = config.myNumber;
  doc["defaultCountryCode"] = config.defaultCountryCode;
  doc["adminNumbers"] = config.adminNumbers;
  doc["authenticNumbers"] = config.authenticNumbers;
  doc["bypassKey"] = config.bypassKey;
  doc["firebaseProjectId"] = config.firebaseProjectId;
  doc["firebaseDatabaseUrl"] = config.firebaseDatabaseUrl;
  doc["firebaseApiKey"] = config.firebaseApiKey;
  doc["firebaseUserEmail"] = config.firebaseUserEmail;
  doc["firebaseUserPassword"] = config.firebaseUserPassword;
  doc["firebaseCommandPath"] = config.firebaseCommandPath;
  doc["firebaseHistoryPath"] = config.firebaseHistoryPath;
  doc["firebaseCounterPath"] = config.firebaseCounterPath;
  doc["firebaseStatusPath"] = config.firebaseStatusPath;
  doc["firebaseTelemetryPath"] = config.firebaseTelemetryPath;
  doc["ntfyUrl"] = config.ntfyUrl;
  doc["thingSpeakChannelId"] = config.thingSpeakChannelId;
  doc["thingSpeakWriteApiKey"] = config.thingSpeakWriteApiKey;

  String jsonText;
  serializeJsonPretty(doc, jsonText);
  return jsonText;
}
