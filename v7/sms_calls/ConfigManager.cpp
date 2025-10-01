#include "ConfigManager.h"

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
  loadFromFS();

  // if api secret empty, set default from compile-time
#ifdef API_SECRET_DEFAULT
  if (cfg.apiSecret.length() == 0) cfg.apiSecret = String(API_SECRET_DEFAULT);
#endif
}

Config ConfigManager::get() { return cfg; }

bool ConfigManager::save(const Config &c) {
  File f = SPIFFS.open(path, FILE_WRITE);
  if (!f) return false;
  StaticJsonDocument<512> doc;
  doc["useApiSecret"] = c.useApiSecret;
  doc["apiSecret"] = c.apiSecret;
  doc["forwardUrl"] = c.forwardUrl;
  doc["forwardApiKey"] = c.forwardApiKey;
  doc["allowSms"] = c.allowSms;
  doc["allowCall"] = c.allowCall;
  if (serializeJson(doc, f) == 0) {
    f.close();
    return false;
  }
  f.close();
  cfg = c;
  return true;
}

void ConfigManager::loadFromFS() {
  if (!SPIFFS.exists(path)) {
    // write default
    save(cfg);
    return;
  }
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  if (err) {
    f.close();
    return;
  }
  cfg.useApiSecret = doc["useApiSecret"] | cfg.useApiSecret;
  cfg.apiSecret = String((const char *)doc["apiSecret"].as<const char *>());
  cfg.forwardUrl = String((const char *)doc["forwardUrl"].as<const char *>());
  cfg.forwardApiKey = String((const char *)doc["forwardApiKey"].as<const char *>());
  cfg.allowSms = doc["allowSms"] | cfg.allowSms;
  cfg.allowCall = doc["allowCall"] | cfg.allowCall;
  f.close();
}
