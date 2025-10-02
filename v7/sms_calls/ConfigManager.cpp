#include <Arduino.h>
#include "ConfigManager.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
// Ensure compile-time defaults (WIFI_SSID, FORWARD_URL_DEFAULT, etc.) are available
#include "secrets.h"
#include <WiFi.h>

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
  Serial.println("[ConfigManager] begin() - loading compile-time defaults and persisted settings");
  // Compile-time macro diagnostics: print whether these macros are available
#ifdef API_SECRET_DEFAULT
  Serial.println(String("[ConfigManager] COMPILE_API_SECRET_DEFAULT=") + String(API_SECRET_DEFAULT));
#else
  Serial.println("[ConfigManager] COMPILE_API_SECRET_DEFAULT NOT DEFINED");
#endif
#ifdef FORWARD_URL_DEFAULT
  Serial.println(String("[ConfigManager] COMPILE_FORWARD_URL_DEFAULT=") + String(FORWARD_URL_DEFAULT));
#else
  Serial.println("[ConfigManager] COMPILE_FORWARD_URL_DEFAULT NOT DEFINED");
#endif
#ifdef USE_API_SECRET_DEFAULT
  Serial.println(String("[ConfigManager] COMPILE_USE_API_SECRET_DEFAULT=") + (USE_API_SECRET_DEFAULT?"1":"0"));
#else
  Serial.println("[ConfigManager] COMPILE_USE_API_SECRET_DEFAULT NOT DEFINED");
#endif
  // SPIFFS removed: keep config in-memory and initialize defaults
  // if api secret empty, set default from compile-time
  // Apply compile-time defaults from secrets.h. These act as the initial
  // values on a first run. Persisted NVS values (loaded below) will override
  // these default values if present.

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
#define ALLOW_SMS_DEFAULT 0
#endif
#ifndef ALLOW_CALL_DEFAULT
#define ALLOW_CALL_DEFAULT 0
#endif
#ifndef SETTINGS_URL_DEFAULT
#define SETTINGS_URL_DEFAULT ""
#endif
#ifndef SETTINGS_VERSION_DEFAULT
#define SETTINGS_VERSION_DEFAULT ""
#endif

  // Apply compile-time defaults (these now always evaluate to some value,
  // either from secrets.h or the safe fallbacks above).
  cfg.useApiSecret = (USE_API_SECRET_DEFAULT != 0);
  cfg.apiSecret = String(API_SECRET_DEFAULT);
  cfg.forwardUrl = String(FORWARD_URL_DEFAULT);
  cfg.forwardApiKey = String(FORWARD_API_KEY_DEFAULT);
  cfg.allowSms = (ALLOW_SMS_DEFAULT != 0);
  cfg.allowCall = (ALLOW_CALL_DEFAULT != 0);
  cfg.settingsUrl = String(SETTINGS_URL_DEFAULT);
  cfg.settingsVersion = String(SETTINGS_VERSION_DEFAULT);
  // load persisted config from NVS (Preferences)
  Preferences prefs;
  if (prefs.begin("cfg", true)) {
    String raw = prefs.getString("config", "");
    if (raw.length()) {
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, raw) == DeserializationError::Ok) {
        cfg.useApiSecret = doc["useApiSecret"] | cfg.useApiSecret;
        if (doc.containsKey("apiSecret")) {
          const char *v = doc["apiSecret"].as<const char *>();
          if (v && v[0] != '\0') cfg.apiSecret = String(v);
        }
        if (doc.containsKey("forwardUrl")) {
          const char *v = doc["forwardUrl"].as<const char *>();
          if (v && v[0] != '\0') cfg.forwardUrl = String(v);
        }
        if (doc.containsKey("forwardApiKey")) {
          const char *v = doc["forwardApiKey"].as<const char *>();
          if (v && v[0] != '\0') cfg.forwardApiKey = String(v);
        }
        cfg.allowSms = doc["allowSms"] | cfg.allowSms;
        cfg.allowCall = doc["allowCall"] | cfg.allowCall;
        if (doc.containsKey("settingsUrl")) {
          const char *v = doc["settingsUrl"].as<const char *>();
          if (v && v[0] != '\0') cfg.settingsUrl = String(v);
        }
        if (doc.containsKey("settingsVersion")) {
          const char *v = doc["settingsVersion"].as<const char *>();
          if (v && v[0] != '\0') cfg.settingsVersion = String(v);
        }
      }
    }
    prefs.end();
  }

  // Debug: print effective configuration so startup logs show where values came from
  Serial.println(String("[ConfigManager] useApiSecret=") + (cfg.useApiSecret?"true":"false"));
  // mask apiSecret but show length
  Serial.println(String("[ConfigManager] apiSecret_len=") + cfg.apiSecret.length());
  Serial.println(String("[ConfigManager] forwardUrl=") + cfg.forwardUrl);
  Serial.println(String("[ConfigManager] forwardApiKey_len=") + cfg.forwardApiKey.length());
  Serial.println(String("[ConfigManager] allowSms=") + (cfg.allowSms?"true":"false"));
  Serial.println(String("[ConfigManager] allowCall=") + (cfg.allowCall?"true":"false"));
  Serial.println(String("[ConfigManager] settingsUrl=") + cfg.settingsUrl);
  Serial.println(String("[ConfigManager] settingsVersion=") + cfg.settingsVersion);
}

Config ConfigManager::get() { return cfg; }

bool ConfigManager::save(const Config &c) {
  // No persistent storage in this build. Save to RAM only.
  cfg = c;
  // persist to NVS
  Preferences prefs;
  if (!prefs.begin("cfg", false)) return true; // fail quietly, but RAM saved
  StaticJsonDocument<512> doc;
  doc["useApiSecret"] = cfg.useApiSecret;
  doc["apiSecret"] = cfg.apiSecret;
  doc["forwardUrl"] = cfg.forwardUrl;
  doc["forwardApiKey"] = cfg.forwardApiKey;
  doc["allowSms"] = cfg.allowSms;
  doc["allowCall"] = cfg.allowCall;
  doc["settingsUrl"] = cfg.settingsUrl;
  doc["settingsVersion"] = cfg.settingsVersion;
  String out;
  serializeJson(doc, out);
  prefs.putString("config", out);
  prefs.end();
  return true;
}

void ConfigManager::loadFromFS() {
  // persistent storage removed in this build; nothing to load
}

// Fetch settings from remote settings URL (if configured). If server returns JSON
// with a 'settingsVersion' field different from current, apply and persist.
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
  if (code <= 0) { http.end(); return false; }
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  String remoteVersion = String((const char *)doc["settingsVersion"].as<const char *>());
  if (remoteVersion == cfg.settingsVersion) return false; // nothing new
  // apply known fields but only if remote values are non-empty to avoid
  // accidentally clearing compile-time defaults or persisted values.
  if (doc.containsKey("apiSecret")) {
    const char *v = doc["apiSecret"].as<const char *>();
    if (v && v[0] != '\0') {
      cfg.apiSecret = String(v);
      Serial.println("[ConfigManager] Applied remote apiSecret");
    } else {
      Serial.println("[ConfigManager] Remote apiSecret empty; skipping");
    }
  }
  if (doc.containsKey("forwardUrl")) {
    const char *v = doc["forwardUrl"].as<const char *>();
    if (v && v[0] != '\0') {
      cfg.forwardUrl = String(v);
      Serial.println("[ConfigManager] Applied remote forwardUrl: " + cfg.forwardUrl);
    } else {
      Serial.println("[ConfigManager] Remote forwardUrl empty; skipping");
    }
  }
  if (doc.containsKey("forwardApiKey")) {
    const char *v = doc["forwardApiKey"].as<const char *>();
    if (v && v[0] != '\0') {
      cfg.forwardApiKey = String(v);
      Serial.println("[ConfigManager] Applied remote forwardApiKey");
    } else {
      Serial.println("[ConfigManager] Remote forwardApiKey empty; skipping");
    }
  }
  if (doc.containsKey("allowSms")) {
    cfg.allowSms = (bool)doc["allowSms"];
    Serial.println(String("[ConfigManager] Applied remote allowSms=") + (cfg.allowSms?"true":"false"));
  }
  if (doc.containsKey("allowCall")) {
    cfg.allowCall = (bool)doc["allowCall"];
    Serial.println(String("[ConfigManager] Applied remote allowCall=") + (cfg.allowCall?"true":"false"));
  }
  // Always update settingsVersion when remote provided one
  cfg.settingsVersion = remoteVersion;
  // persist
  save(cfg);
  // Debug: print resulting values after applying remote settings
  Serial.println(String("[ConfigManager] After remote apply forwardUrl=") + cfg.forwardUrl);
  Serial.println(String("[ConfigManager] After remote apply apiSecret_len=") + cfg.apiSecret.length());
  return true;
}

#include <Preferences.h>

Preferences preferences;

void ConfigManager::loadFromNVS() {
  Serial.println("Loading configuration from SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS. Formatting...");
    SPIFFS.format();
  }

  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.println("Configuration file not found. Creating default configuration...");
    setting1 = "default_value";
    saveToNVS();
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Failed to parse configuration file. Using default values.");
    setting1 = "default_value";
  } else {
    setting1 = doc["setting1"].as<String>();
    Serial.println("Loaded setting1: " + setting1);
  }
  file.close();
}

void ConfigManager::saveToNVS() {
  Serial.println("Saving configuration to SPIFFS...");
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open configuration file for writing.");
    return;
  }

  StaticJsonDocument<512> doc;
  doc["setting1"] = setting1;

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to configuration file.");
  } else {
    Serial.println("Configuration saved successfully.");
  }
  file.close();
}
