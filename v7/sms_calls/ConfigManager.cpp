#include "ConfigManager.h"
#include <Preferences.h>
#include <HTTPClient.h>

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
  // SPIFFS removed: keep config in-memory and initialize defaults
  // if api secret empty, set default from compile-time
#ifdef API_SECRET_DEFAULT
  if (cfg.apiSecret.length() == 0) cfg.apiSecret = String(API_SECRET_DEFAULT);
#endif
// apply compile-time defaults for remote settings
#ifdef SETTINGS_URL_DEFAULT
  if (cfg.settingsUrl.length() == 0) cfg.settingsUrl = String(SETTINGS_URL_DEFAULT);
#endif
#ifdef SETTINGS_VERSION_DEFAULT
  if (cfg.settingsVersion.length() == 0) cfg.settingsVersion = String(SETTINGS_VERSION_DEFAULT);
#endif
  // load persisted config from NVS (Preferences)
  Preferences prefs;
  if (prefs.begin("cfg", true)) {
    String raw = prefs.getString("config", "");
    if (raw.length()) {
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, raw) == DeserializationError::Ok) {
        cfg.useApiSecret = doc["useApiSecret"] | cfg.useApiSecret;
        cfg.apiSecret = String((const char *)doc["apiSecret"].as<const char *>());
        cfg.forwardUrl = String((const char *)doc["forwardUrl"].as<const char *>());
        cfg.forwardApiKey = String((const char *)doc["forwardApiKey"].as<const char *>());
        cfg.allowSms = doc["allowSms"] | cfg.allowSms;
        cfg.allowCall = doc["allowCall"] | cfg.allowCall;
        cfg.settingsUrl = String((const char *)doc["settingsUrl"].as<const char *>());
        cfg.settingsVersion = String((const char *)doc["settingsVersion"].as<const char *>());
      }
    }
    prefs.end();
  }
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
  if (cfg.settingsUrl.length() == 0) return false;
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
  // apply known fields
  if (doc.containsKey("apiSecret")) cfg.apiSecret = String((const char *)doc["apiSecret"].as<const char *>());
  if (doc.containsKey("forwardUrl")) cfg.forwardUrl = String((const char *)doc["forwardUrl"].as<const char *>());
  if (doc.containsKey("forwardApiKey")) cfg.forwardApiKey = String((const char *)doc["forwardApiKey"].as<const char *>());
  if (doc.containsKey("allowSms")) cfg.allowSms = (bool)doc["allowSms"];
  if (doc.containsKey("allowCall")) cfg.allowCall = (bool)doc["allowCall"];
  cfg.settingsVersion = remoteVersion;
  // persist
  save(cfg);
  return true;
}
