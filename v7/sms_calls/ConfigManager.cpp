#include <Arduino.h>
#include "ConfigManager.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include "secrets.h"
#include <WiFi.h>

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
  Serial.println("[ConfigManager] Initializing SPIFFS and loading configuration...");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("[ConfigManager] SPIFFS Mount Failed! Formatting SPIFFS...");
    SPIFFS.format();
    if (!SPIFFS.begin(true)) {
      Serial.println("[ConfigManager] SPIFFS Failed to format and mount!");
      return;
    }
  }
  Serial.println("[ConfigManager] SPIFFS mounted successfully");
  
  // Load compile-time defaults first
  loadDefaults();
  
  // Try to load from SPIFFS, fall back to NVS if SPIFFS config doesn't exist
  if (!loadFromSPIFFS()) {
    Serial.println("[ConfigManager] No SPIFFS config found, trying NVS...");
    loadFromNVS();
    // Save to SPIFFS for future use
    saveToSPIFFS();
  }
  
  // Print loaded configuration
  printConfig();
}

void ConfigManager::loadDefaults() {
  // Provide safe fallbacks in case the macros are not defined by the build
  #ifndef USE_API_SECRET_DEFAULT
  #define USE_API_SECRET_DEFAULT 0
  #endif
  #ifndef API_SECRET_DEFAULT
  #define API_SECRET_DEFAULT ""
  #endif
  #ifndef FORWARD_URL_DEFAULT
  #define FORWARD_URL_DEFAULT ""
  #endif
  #ifndef FORWARD_API_KEY_DEFAULT
  #define FORWARD_API_KEY_DEFAULT ""
  #endif
  #ifndef ALLOW_SMS_DEFAULT
  #define ALLOW_SMS_DEFAULT 1
  #endif
  #ifndef ALLOW_CALL_DEFAULT
  #define ALLOW_CALL_DEFAULT 1
  #endif
  #ifndef SETTINGS_URL_DEFAULT
  #define SETTINGS_URL_DEFAULT ""
  #endif
  #ifndef SETTINGS_VERSION_DEFAULT
  #define SETTINGS_VERSION_DEFAULT "1.0"
  #endif
  #ifndef NTFY_SERVER_URL_DEFAULT
  #define NTFY_SERVER_URL_DEFAULT ""
  #endif
  #ifndef NTFY_TOPIC_DEFAULT
  #define NTFY_TOPIC_DEFAULT ""
  #endif
  #ifndef NTFY_ENABLED_DEFAULT
  #define NTFY_ENABLED_DEFAULT 0
  #endif
  #ifndef OWNER_NAME_DEFAULT
  #define OWNER_NAME_DEFAULT ""
  #endif
  #ifndef MY_NUMBER_DEFAULT
  #define MY_NUMBER_DEFAULT ""
  #endif
  #ifndef DEVICE_NAME_DEFAULT
  #define DEVICE_NAME_DEFAULT "TTGO T-Call"
  #endif
  #ifndef GATE_ESP_URL_DEFAULT
  #define GATE_ESP_URL_DEFAULT ""
  #endif
  #ifndef GATE_ESP_ENABLED_DEFAULT
  #define GATE_ESP_ENABLED_DEFAULT 0
  #endif
  #ifndef ADMIN_NUMBERS_DEFAULT
  #define ADMIN_NUMBERS_DEFAULT ""
  #endif
  #ifndef AUTO_REPLY_MESSAGE_DEFAULT
  #define AUTO_REPLY_MESSAGE_DEFAULT "This is an automated response. The device is currently unable to take calls."
  #endif
  #ifndef LOG_MESSAGES_DEFAULT
  #define LOG_MESSAGES_DEFAULT 1
  #endif
  #ifndef DELETE_AFTER_FORWARD_DEFAULT
  #define DELETE_AFTER_FORWARD_DEFAULT 1
  #endif

  // Apply compile-time defaults
  cfg.useApiSecret = (USE_API_SECRET_DEFAULT != 0);
  cfg.apiSecret = String(API_SECRET_DEFAULT);
  cfg.forwardUrl = String(FORWARD_URL_DEFAULT);
  cfg.forwardApiKey = String(FORWARD_API_KEY_DEFAULT);
  cfg.allowSms = (ALLOW_SMS_DEFAULT != 0);
  cfg.allowCall = (ALLOW_CALL_DEFAULT != 0);
  cfg.settingsUrl = String(SETTINGS_URL_DEFAULT);
  cfg.settingsVersion = String(SETTINGS_VERSION_DEFAULT);
  
  // NTFY Configuration
  cfg.ntfyEnabled = (NTFY_ENABLED_DEFAULT != 0);
  cfg.ntfyServerUrl = String(NTFY_SERVER_URL_DEFAULT);
  cfg.ntfyTopic = String(NTFY_TOPIC_DEFAULT);
  
  // Owner and Device Info
  cfg.ownerName = String(OWNER_NAME_DEFAULT);
  cfg.myNumber = String(MY_NUMBER_DEFAULT);
  cfg.deviceName = String(DEVICE_NAME_DEFAULT);
  
  // Gate ESP Configuration
  cfg.gateEspEnabled = (GATE_ESP_ENABLED_DEFAULT != 0);
  cfg.gateEspUrl = String(GATE_ESP_URL_DEFAULT);
  
  // Admin Numbers
  cfg.adminNumbers = String(ADMIN_NUMBERS_DEFAULT);
  
  // Auto-reply Configuration
  cfg.autoReplyMessage = String(AUTO_REPLY_MESSAGE_DEFAULT);
  
  // Message Management
  cfg.logMessages = (LOG_MESSAGES_DEFAULT != 0);
  cfg.deleteAfterForward = (DELETE_AFTER_FORWARD_DEFAULT != 0);
  
  Serial.println("[ConfigManager] Loaded compile-time defaults");
}

bool ConfigManager::loadFromSPIFFS() {
  if (!SPIFFS.exists(path)) {
    Serial.println("[ConfigManager] Config file does not exist in SPIFFS");
    return false;
  }
  
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.println("[ConfigManager] Failed to open config file for reading");
    return false;
  }
  
  size_t size = file.size();
  if (size == 0) {
    Serial.println("[ConfigManager] Config file is empty");
    file.close();
    return false;
  }
  
  // Allocate a buffer to store contents of the file
  std::unique_ptr<char[]> buf(new char[size]);
  file.readBytes(buf.get(), size);
  file.close();
  
  // Parse JSON
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    Serial.println(String("[ConfigManager] deserializeJson() failed: ") + error.c_str());
    return false;
  }
  
  // Load configuration from JSON
  jsonToConfig(doc, cfg);
  
  Serial.println("[ConfigManager] Configuration loaded from SPIFFS successfully");
  return true;
}

bool ConfigManager::saveToSPIFFS() {
  // Create JSON document
  DynamicJsonDocument doc(2048);
  configToJson(cfg, doc);
  
  // Open file for writing
  File file = SPIFFS.open(path, "w");
  if (!file) {
    Serial.println("[ConfigManager] Failed to open config file for writing");
    return false;
  }
  
  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println("[ConfigManager] Failed to write config to file");
    file.close();
    return false;
  }
  
  file.close();
  Serial.println("[ConfigManager] Configuration saved to SPIFFS successfully");
  return true;
}

template<typename T>
void ConfigManager::configToJson(const Config &config, T &doc) {
  // API Configuration
  doc["useApiSecret"] = config.useApiSecret;
  doc["apiSecret"] = config.apiSecret;
  doc["forwardUrl"] = config.forwardUrl;
  doc["forwardApiKey"] = config.forwardApiKey;
  
  // Device Permissions
  doc["allowSms"] = config.allowSms;
  doc["allowCall"] = config.allowCall;
  
  // Remote Settings
  doc["settingsUrl"] = config.settingsUrl;
  doc["settingsVersion"] = config.settingsVersion;
  
  // NTFY Configuration
  doc["ntfyEnabled"] = config.ntfyEnabled;
  doc["ntfyServerUrl"] = config.ntfyServerUrl;
  doc["ntfyTopic"] = config.ntfyTopic;
  
  // Owner and Device Info
  doc["ownerName"] = config.ownerName;
  doc["myNumber"] = config.myNumber;
  doc["deviceName"] = config.deviceName;
  
  // Gate ESP Configuration
  doc["gateEspEnabled"] = config.gateEspEnabled;
  doc["gateEspUrl"] = config.gateEspUrl;
  
  // Admin Numbers
  doc["adminNumbers"] = config.adminNumbers;
  
  // Auto-reply Configuration
  doc["autoReplyMessage"] = config.autoReplyMessage;
  
  // Message Management
  doc["logMessages"] = config.logMessages;
  doc["deleteAfterForward"] = config.deleteAfterForward;
}

template<typename T>
void ConfigManager::jsonToConfig(const T &doc, Config &config) {
  // API Configuration
  if (doc.containsKey("useApiSecret")) config.useApiSecret = doc["useApiSecret"];
  if (doc.containsKey("apiSecret") && !doc["apiSecret"].isNull()) {
    const char* val = doc["apiSecret"];
    if (val) config.apiSecret = val;
  }
  if (doc.containsKey("forwardUrl") && !doc["forwardUrl"].isNull()) {
    const char* val = doc["forwardUrl"];
    if (val) config.forwardUrl = val;
  }
  if (doc.containsKey("forwardApiKey") && !doc["forwardApiKey"].isNull()) {
    const char* val = doc["forwardApiKey"];
    if (val) config.forwardApiKey = val;
  }
  
  // Device Permissions
  if (doc.containsKey("allowSms")) config.allowSms = doc["allowSms"];
  if (doc.containsKey("allowCall")) config.allowCall = doc["allowCall"];
  
  // Remote Settings
  if (doc.containsKey("settingsUrl") && !doc["settingsUrl"].isNull()) {
    const char* val = doc["settingsUrl"];
    if (val) config.settingsUrl = val;
  }
  if (doc.containsKey("settingsVersion") && !doc["settingsVersion"].isNull()) {
    const char* val = doc["settingsVersion"];
    if (val) config.settingsVersion = val;
  }
  
  // NTFY Configuration
  if (doc.containsKey("ntfyEnabled")) config.ntfyEnabled = doc["ntfyEnabled"];
  if (doc.containsKey("ntfyServerUrl") && !doc["ntfyServerUrl"].isNull()) {
    const char* val = doc["ntfyServerUrl"];
    if (val) config.ntfyServerUrl = val;
  }
  if (doc.containsKey("ntfyTopic") && !doc["ntfyTopic"].isNull()) {
    const char* val = doc["ntfyTopic"];
    if (val) config.ntfyTopic = val;
  }
  
  // Owner and Device Info
  if (doc.containsKey("ownerName") && !doc["ownerName"].isNull()) {
    const char* val = doc["ownerName"];
    if (val) config.ownerName = val;
  }
  if (doc.containsKey("myNumber") && !doc["myNumber"].isNull()) {
    const char* val = doc["myNumber"];
    if (val) config.myNumber = val;
  }
  if (doc.containsKey("deviceName") && !doc["deviceName"].isNull()) {
    const char* val = doc["deviceName"];
    if (val) config.deviceName = val;
  }
  
  // Gate ESP Configuration
  if (doc.containsKey("gateEspEnabled")) config.gateEspEnabled = doc["gateEspEnabled"];
  if (doc.containsKey("gateEspUrl") && !doc["gateEspUrl"].isNull()) {
    const char* val = doc["gateEspUrl"];
    if (val) config.gateEspUrl = val;
  }
  
  // Admin Numbers
  if (doc.containsKey("adminNumbers") && !doc["adminNumbers"].isNull()) {
    const char* val = doc["adminNumbers"];
    if (val) config.adminNumbers = val;
  }
  
  // Auto-reply Configuration
  if (doc.containsKey("autoReplyMessage") && !doc["autoReplyMessage"].isNull()) {
    const char* val = doc["autoReplyMessage"];
    if (val) config.autoReplyMessage = val;
  }
  
  // Message Management
  if (doc.containsKey("logMessages")) config.logMessages = doc["logMessages"];
  if (doc.containsKey("deleteAfterForward")) config.deleteAfterForward = doc["deleteAfterForward"];
}

Config ConfigManager::get() { 
  return cfg; 
}

bool ConfigManager::save(const Config &c) {
  cfg = c;
  
  // Save to both SPIFFS and NVS
  bool spiffsResult = saveToSPIFFS();
  saveToNVS();
  
  return spiffsResult;
}

void ConfigManager::printConfig() {
  Serial.println("[ConfigManager] Current Configuration:");
  Serial.println("  API Configuration:");
  Serial.println("    useApiSecret: " + String(cfg.useApiSecret ? "true" : "false"));
  Serial.print("    apiSecret: "); Serial.println(cfg.apiSecret.length() > 0 ? "[SET]" : "[EMPTY]");
  Serial.print("    forwardUrl: "); Serial.println(cfg.forwardUrl);
  Serial.print("    forwardApiKey: "); Serial.println(cfg.forwardApiKey.length() > 0 ? "[SET]" : "[EMPTY]");
  
  Serial.println("  Device Permissions:");
  Serial.println("    allowSms: " + String(cfg.allowSms ? "true" : "false"));
  Serial.println("    allowCall: " + String(cfg.allowCall ? "true" : "false"));
  
  Serial.println("  NTFY Configuration:");
  Serial.println("    ntfyEnabled: " + String(cfg.ntfyEnabled ? "true" : "false"));
  Serial.println("    ntfyServerUrl: " + cfg.ntfyServerUrl);
  Serial.println("    ntfyTopic: " + cfg.ntfyTopic);
  
  Serial.println("  Owner Info:");
  Serial.println("    ownerName: " + cfg.ownerName);
  Serial.println("    myNumber: " + cfg.myNumber);
  Serial.println("    deviceName: " + cfg.deviceName);
  
  Serial.println("  Gate ESP:");
  Serial.println("    gateEspEnabled: " + String(cfg.gateEspEnabled ? "true" : "false"));
  Serial.println("    gateEspUrl: " + cfg.gateEspUrl);
  
  Serial.println("  Admin Numbers: " + cfg.adminNumbers);
  Serial.println("  Auto-reply: " + (cfg.autoReplyMessage.length() > 20 ? cfg.autoReplyMessage.substring(0, 20) + "..." : cfg.autoReplyMessage));
  
  Serial.println("  Message Management:");
  Serial.println("    logMessages: " + String(cfg.logMessages ? "true" : "false"));
  Serial.println("    deleteAfterForward: " + String(cfg.deleteAfterForward ? "true" : "false"));
}

// Legacy NVS methods for backward compatibility
void ConfigManager::loadFromNVS() {
  Preferences prefs;
  if (prefs.begin("cfg", true)) {
    String raw = prefs.getString("config", "");
    if (raw.length()) {
      DynamicJsonDocument doc(2048);
      if (deserializeJson(doc, raw) == DeserializationError::Ok) {
        jsonToConfig(doc, cfg);
        Serial.println("[ConfigManager] Configuration loaded from NVS");
      }
    }
    prefs.end();
  }
}

void ConfigManager::saveToNVS() {
  Preferences prefs;
  if (!prefs.begin("cfg", false)) return;
  
  DynamicJsonDocument doc(2048);
  configToJson(cfg, doc);
  
  String output;
  serializeJson(doc, output);
  prefs.putString("config", output);
  prefs.end();
  Serial.println("[ConfigManager] Configuration saved to NVS");
}

bool ConfigManager::checkAndApplyRemoteSettings() {
  // If no settings URL configured or WiFi not connected, skip
  if (cfg.settingsUrl.length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ConfigManager] WiFi not connected; skipping remote settings check");
    return false;
  }
  
  HTTPClient http;
  http.begin(cfg.settingsUrl);
  int code = http.GET();
  if (code <= 0 || code != 200) { 
    http.end(); 
    return false; 
  }
  
  String body = http.getString();
  http.end();
  
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  
  const char* remoteVersionVal = doc["settingsVersion"];
  String remoteVersion = remoteVersionVal ? String(remoteVersionVal) : "";
  if (remoteVersion == cfg.settingsVersion) return false; // nothing new
  
  // Apply remote settings
  jsonToConfig(doc, cfg);
  
  // Save updated configuration
  save(cfg);
  
  Serial.println("[ConfigManager] Applied remote settings, version: " + remoteVersion);
  return true;
}

// Deprecated method kept for compatibility
void ConfigManager::loadFromFS() {
  loadFromSPIFFS();
}

bool ConfigManager::resetToDefaults() {
  loadDefaults();
  return save(cfg);
}

// Template method instantiations (required for templates in .cpp files)
template void ConfigManager::configToJson<DynamicJsonDocument>(const Config &config, DynamicJsonDocument &doc);
template void ConfigManager::jsonToConfig<DynamicJsonDocument>(const DynamicJsonDocument &doc, Config &config);